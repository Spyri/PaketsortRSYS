#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/string.h>

#include <rtai.h>
#include <rtai_sched.h>
#include <rtai_sem.h>
#include <rtai_fifos.h>

#include <linux/ktime.h>
#include <linux/rtc.h>

// Macros:
#define BYTE_TO_BINARY_PATTERN "%s 0b%c%c%c%c%c%c%c%c\n"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0')

// Hardware constants:
#define BELT1   (uint8_t) (1)
#define BELT2   (uint8_t) (1 << 1)
#define EJECT1  (uint8_t) (1 << 3)
#define EJECT2  (uint8_t) (1 << 4)
#define EJECT3  (uint8_t) (1 << 5)
#define SLIDER  (uint8_t) (1 << 6)
#define SCANNER (uint8_t) (1 << 7)
#define LS0     (uint8_t) (1 << 1)
#define LS1     (uint8_t) (1 << 2)
#define LS2     (uint8_t) (1 << 3)
#define LS3     (uint8_t) (1 << 4)
#define LS4     (uint8_t) (1 << 5)

#define LENGTH_EAN 20
#define PCI_DEVICE_ID_KOLTER_DIO 0x11
#define CARD 0xc000 // absolute PCI-Kartenadresse
#define PCI_VENDOR_ID_KOLTER 0x1001
#define PCI_DEVICE_ID_KOLTER_DIO 0x11
#define BUFFERSIZE 50

// FIFO constants: 
#define FIFO_SIZE 1024
#define FIFO_NR 3

// Time constants:
#define EJECTION_TASK_PERIOD_NS nano2count(25000000)
#define SENSOR_TASK_NS		nano2count(50000000)
#define BARCODE_SCANNER_TASK_NS nano2count(100000000)

#define COOLDOWN 10

// Periodic tasks
RT_TASK ejection_task_1, ejection_task_2, ejection_task_3;
RT_TASK sensor_ejection_task, sensor_belt_task;
RT_TASK barcode_scanner_task;

// Semaphores
SEM ctl_register_sem;
SEM eject_flags_sem;
SEM sensor_data_sem;
SEM current_parcel_id_sem;
SEM fifo_sem;
SEM parcels_info_sem;

// Variables
uint8_t ctl_register = 0x00;
uint8_t sensor_data = 0x00;
uint8_t eject_flags = 0x00;
uint8_t current_parcel_id = 0x00;
uint8_t barcode_flags = 0x00;

struct parcels_info_struct {
  uint8_t parcels_on_belt2;
  uint8_t sensor_countdown;
};

struct parcels_info_struct parcels_info;


// Function headers
void activate(uint8_t val);
void deactivate(uint8_t val);
uint8_t get_digital_input(void);
void initialize_system(void);
void deinitialize_system(void);
void ejection_handler(uint8_t mask, uint8_t ctl, int8_t* cooldown);
void set_eject_flags(uint8_t val);
void clear_eject_flags(uint8_t val);
int init_scanner(void);
void increment_ts(uint8_t* val, SEM* sem);
uint8_t decrement_ts(uint8_t* val, SEM* sem);

// FIFO implementation 

uint64_t fifo_data = 0;

uint8_t fifo_read(uint8_t index) {
    uint8_t n_byte_shift = index * 8;
    rt_sem_wait(&fifo_sem);
    uint64_t value = (fifo_data & (((uint64_t) 0xFF) << n_byte_shift)) >> n_byte_shift;
    rt_sem_signal(&fifo_sem);
    return value;
}

void fifo_write(uint8_t index, uint8_t value) {
    uint8_t n_byte_shift = 8 * index;
    uint64_t shifted_value = ((uint64_t) value) << n_byte_shift;
    rt_sem_wait(&fifo_sem);
    fifo_data &= ~((uint64_t)(0xFF << n_byte_shift));
    fifo_data |= shifted_value;
    rt_sem_signal(&fifo_sem);
}

void fifo_clear(uint8_t index) {
    uint8_t n_byte_shift = index * 8;
    rt_sem_wait(&fifo_sem);
    fifo_data &= ~(0xFF << n_byte_shift);
    rt_sem_signal(&fifo_sem);
}

uint8_t fifo_pop(void) {
    rt_sem_wait(&fifo_sem);
    uint64_t shifted = (fifo_data >> 8 * 7);
    uint8_t element = shifted;
    fifo_data <<= 8;
    rt_sem_signal(&fifo_sem);
    return element;
}

void fifo_push(uint8_t value) {
    rt_sem_wait(&fifo_sem);
    fifo_data <<= 8;
    fifo_data |= value;
    rt_sem_signal(&fifo_sem);
}

// Implementation

void ejection_handler(uint8_t mask, uint8_t ctl, int8_t* cooldown) {
  if (*cooldown == -1) {
    rt_sem_wait(&eject_flags_sem);
    if((eject_flags & mask) == mask) {
      rt_sem_signal(&eject_flags_sem);
      rt_sleep(nano2count(EJECTION_TASK_PERIOD_NS * 3 / 2));
      activate(ctl);
      *cooldown = COOLDOWN;
    } else {
      rt_sem_signal(&eject_flags_sem);
    }
  } else if (*cooldown == 0) {
    deactivate(ctl);
    clear_eject_flags(mask);
    *cooldown = -1;
    
    decrement_ts(&parcels_info.parcels_on_belt2, &parcels_info_sem);
    decrement_ts(&parcels_info.sensor_countdown, &parcels_info_sem);
    //fifo_write(mask, 0xFF);
    // deactivate(SLIDER);
  } else if (*cooldown > 0) {
    (*cooldown)--;
  }
}

int32_t fifo_handler(uint32_t fifo) {
  int32_t fifo_id = FIFO_NR;
  char read_value[512];
  
  int i;
  for(i = 0; i < 512; i++) {
    read_value[i] = 0;
  }
  
  if(rtf_create(fifo_id, 4096) < 0) {
    return -1;
  }
  
  //rt_printk("\n\nFIFO TRIGGERED!!!! \n\n");
  
  int32_t bytes_read = rtf_get(fifo_id, read_value, sizeof(read_value));
    if (bytes_read > 0) {
      rt_printk("GOT FROM FIFO %s\n", read_value);
      if(strcmp(read_value,"1") == 0) {
        fifo_push(1);
      } 
      else if(strcmp(read_value,"2") == 0) {
        fifo_push(2);
      }
      else if(strcmp(read_value,"3") == 0) {
        fifo_push(3);
      }
      else {
        fifo_push(4);
      }
      deactivate(SCANNER);
      deactivate(SLIDER);
    }
    return 0;	
}


// Tasks
static void ejection_1(long t) {
  int8_t cooldown = -1;
  while(1) {
    ejection_handler(0x02, EJECT1, &cooldown);
    rt_task_wait_period();
  }
}

static void ejection_2(long t) {
  int8_t cooldown = -1;
  while(1) {
    ejection_handler(0x04, EJECT2, &cooldown);
    rt_task_wait_period();
  }
}

static void ejection_3(long t) {
  int8_t cooldown = -1;
  while(1) {
    ejection_handler(0x08, EJECT3, &cooldown);
    rt_task_wait_period();
  }
}

void increment_ts(uint8_t* val, SEM* sem) {
  rt_sem_wait(&sem);
  (*val)++;
  
  rt_printk("Sind:\t%d\tCountdown:\t%d\n", parcels_info.parcels_on_belt2, parcels_info.sensor_countdown);
  
  rt_sem_signal(&sem);
}

uint8_t decrement_ts(uint8_t* val, SEM* sem) {
  rt_sem_wait(&sem);
  (*val)--;
  
  rt_printk("Sind:\t%d\tCountdown:\t%d\n", parcels_info.parcels_on_belt2, parcels_info.sensor_countdown);
  
  rt_sem_signal(&sem);
  
  return *val;
}

static void sensor_ejection(long t) {
  uint64_t start_time_ns;
  uint64_t end_time_ns;
  uint64_t delta_time;
  
  uint8_t rbyte;
  uint8_t falling_edge;
  uint8_t rising_edge;
  uint8_t mask;
  
  while(1) { 
    start_time_ns = rt_get_time_ns(); 
    //rt_printk("TIME_START: %llu\n", start_time_ns);  
  
    rbyte = ~get_digital_input();
    falling_edge = ~rbyte & sensor_data;
    rising_edge = rbyte & ~sensor_data;
    
    mask = 0x01;
    if((rising_edge & mask) == mask) {
    	activate(SCANNER);
    }
    
    mask = 0x02;
    if((rising_edge & mask) == mask) {
    	increment_ts(&parcels_info.parcels_on_belt2, &parcels_info_sem);
    	increment_ts(&parcels_info.sensor_countdown, &parcels_info_sem);
    }
    
    mask = 0x02;
    if((falling_edge & mask) == mask) {
    	activate(SLIDER);
    }
    
    if((rising_edge & ~0x01) != 0) {
    	if (decrement_ts(&parcels_info.sensor_countdown, &parcels_info_sem) == 0) {
    	  //wenn neuer wert pushe neuen wert in fifo sonst
    	  fifo_push(0x00);
    	  rt_printk("FIFO_VALUE: %016llx\n", fifo_data);
    	  
    	  rt_sem_wait(&parcels_info_sem);
  	  parcels_info.sensor_countdown = parcels_info.parcels_on_belt2;
          rt_sem_signal(&parcels_info_sem);
    	}
    }
    
    /*if((rising_edge & mask) == mask) {
    	fifo_write(1)=fifo_read(0);
    }*/
    
    if((falling_edge & mask) == mask && (fifo_read(1) == 0x01)) {
    	set_eject_flags(mask);
    }
    
    mask = 0x04;
    /*if((rising_edge & mask) == mask) {
    	fifo_write(2)=fifo_read(1);
    }*/
    if((falling_edge & mask) == mask && (fifo_read(2) == 0x02)) {
    	set_eject_flags(mask);
    }
    

    mask = 0x08;
    /*if((rising_edge & mask) == mask) {
    	fifo_write(3)=fifo_read(2);
    }*/
    if((falling_edge & mask) == mask && (fifo_read(3) == 0x03)) {
    	set_eject_flags(mask);
    }
    
    sensor_data = rbyte;
    
    //rt_printk(BYTE_TO_BINARY_PATTERN, "SENSOR:", BYTE_TO_BINARY(rbyte));
    
    end_time_ns = rt_get_time_ns();
    delta_time = end_time_ns - start_time_ns;
    //rt_printk("DELTA_TIME: %llu\n", delta_time);
    rt_task_wait_period();
  }
}

static void sensor_belt(long t) {
  while(1) {
    //rt_printk("TASK: Sensor belt\n");
    rt_task_wait_period();
  }
}

static void barcode_scanner(long t) {
  while(1) {
    //deactivate(SCANNER);
    //rt_sleep(100000000);
    //activate(SCANNER);
    //rt_printk("FIFO_VALUE: %016llx\n", fifo_data);
    rt_sleep(BARCODE_SCANNER_TASK_NS / 2);
    
    rt_task_wait_period();
  }
}

void activate(uint8_t val) { // schreibe 32 Bit auf Karte
  rt_sem_wait(&ctl_register_sem);
  ctl_register |= val;
  outb(ctl_register, CARD + 0);
  rt_sem_signal(&ctl_register_sem);
}

void deactivate(uint8_t val) { // schreibe 32 Bit auf Karte
  rt_sem_wait(&ctl_register_sem);
  ctl_register &= ~val;
  outb(ctl_register, CARD + 0);
  rt_sem_signal(&ctl_register_sem);
}


void set_eject_flags(uint8_t val) {
  rt_sem_wait(&eject_flags_sem);
  eject_flags |= val;
  rt_sem_signal(&eject_flags_sem);
}

void clear_eject_flags(uint8_t val) {
  rt_sem_wait(&ctl_register_sem);
  eject_flags &= ~val;
  rt_sem_signal(&ctl_register_sem);
}


uint8_t get_digital_input(void) {
  uint8_t tmp = 0;
  tmp = inb(0xc000 + 5);
  tmp = tmp << 8;
  tmp = tmp | inb(0xc000 + 4);
  return tmp;
};


int init_scanner(void) {
  return 1;
}

void initialize_system(void) {
  //activate(BELT1 | BELT2);
  //activate(BELT1);
  parcels_info.parcels_on_belt2 = 0;
  parcels_info.sensor_countdown = 0;
}

void deinitialize_system(void) {
  deactivate(0xFF);
}

static __init int parallel_init(void) {
  rt_mount();
  
  rt_printk("MODULE loaded\n");

  // Init tasks:
  rt_task_init(&ejection_task_1, ejection_1, 0, 1024, 2, 0, 0);
  rt_task_init(&ejection_task_2, ejection_2, 0, 1024, 2, 0, 0);
  rt_task_init(&ejection_task_3, ejection_3, 0, 1024, 2, 0, 0);
  rt_task_init(&sensor_ejection_task, sensor_ejection, 0, 1024, 3, 0, 0);
  rt_task_init(&sensor_belt_task, sensor_belt, 0, 1024, 3, 0, 0);
  rt_task_init(&barcode_scanner_task, barcode_scanner, 0, 1024, 4, 0, 0);
  
  //FIFO CREATION:
  rtf_create(FIFO_NR, FIFO_SIZE);
  rtf_create_handler(FIFO_NR, &fifo_handler);
  
  
  // Configuration:
  rt_set_periodic_mode();
  start_rt_timer(EJECTION_TASK_PERIOD_NS);
  
  // Init semaphores:
  rt_typed_sem_init(&ctl_register_sem, 1, RES_SEM);
  rt_typed_sem_init(&eject_flags_sem, 1, RES_SEM);
  rt_typed_sem_init(&sensor_data_sem, 1, RES_SEM);
  rt_typed_sem_init(&current_parcel_id_sem, 1, RES_SEM);
  rt_typed_sem_init(&parcels_info_sem, 1, RES_SEM);
  
  // Initialize the starting state of the system
  initialize_system();
  
  // Make tasks periodic:
  rt_task_make_periodic(
    &ejection_task_1,
    rt_get_time() + EJECTION_TASK_PERIOD_NS,
    nano2count(EJECTION_TASK_PERIOD_NS)
  );
  
  rt_task_make_periodic(
    &ejection_task_2,
    rt_get_time() + EJECTION_TASK_PERIOD_NS,
    nano2count(EJECTION_TASK_PERIOD_NS)
  );
  
  rt_task_make_periodic(
    &sensor_ejection_task,
    rt_get_time() + EJECTION_TASK_PERIOD_NS,
    nano2count(EJECTION_TASK_PERIOD_NS)
  );
  
  rt_task_make_periodic(
    &ejection_task_3,
    rt_get_time() + SENSOR_TASK_NS,
    nano2count(SENSOR_TASK_NS)
  );
  
  rt_task_make_periodic(
    &sensor_belt_task,
    rt_get_time() + SENSOR_TASK_NS,
    nano2count(SENSOR_TASK_NS)
  );
  
  rt_task_make_periodic(
    &barcode_scanner_task,
    rt_get_time() + BARCODE_SCANNER_TASK_NS,
    nano2count(BARCODE_SCANNER_TASK_NS)
  );

  return 0;
}

static __exit void parallel_exit(void) {
  stop_rt_timer();
  
  deinitialize_system();
  
  rtf_destroy(FIFO_NR);
  
  // Delete semaphores:
  rt_sem_delete(&ctl_register_sem);
  rt_sem_delete(&eject_flags_sem);
  rt_sem_delete(&sensor_data_sem);
  rt_sem_delete(&current_parcel_id_sem);
  rt_sem_delete(&parcels_info_sem);

  // Delete tasks:
  rt_task_delete(&ejection_task_1);
  rt_task_delete(&ejection_task_2);
  rt_task_delete(&ejection_task_3);
  rt_task_delete(&sensor_ejection_task);
  rt_task_delete(&sensor_belt_task);
  rt_task_delete(&barcode_scanner_task);
  
  rt_umount();
}

module_init(parallel_init);
module_exit(parallel_exit);

MODULE_LICENSE("GPL");
