#include <linux/init.h>
#include <linux/module.h>
#include <linux/pci.h>

#include <rtai.h>
#include <rtai_sched.h>
#include <rtai_sem.h>
// times:
#define TICK_PERIOD 500000000
#define our1p5ms 1200000000

#define SCANNER_RUHEZEIT 13 * 1000 * 10000
// devices: Zum Aktivieren eines Device muss das aktuelle Bit-Muster
//	   mit seiner Adresse verORt werden.
//	   Zum Deaktivieren eines Device muss das aktuelle Bit-Muster
//	   mit seiner Adresse verXORt werden.
#define PCI_VENDOR_ID_KOLTER 0x1001
#define PCI_DEVICE_ID_KOLTER_DIO 0x11

#define band1 0x01    // 1
#define band2 0x02    // 2
#define schieber 0x04 // 4
#define ausw1 0x08    // 8
#define ausw2 0x10    // 16
#define ausw3 0x20    // 32
#define scanner 0x80  // 128
#define keinLS e0
#define LS0 e1
#define LS1 e2
#define LS2 e4
#define LS3 e8
#define LENGTH_EAN 20
#define PCI_DEVICE_ID_KOLTER_DIO 0x11
#define CARD 0xc000 // absolute PCI-Kartenadresse

// globalvars:
RT_TASK polltask, rs232task;
int bitmuster = 0x00; // state der ganzen Maschine
int stateLS = 0;      // state der Lichtschranken
int lost = 0;         // Pakete ohne Adresse
unsigned char port_a;
unsigned char port_b;
char ch, eancode[LENGTH_EAN];
int charcounter = 0;
SEM sem_bit, s1; // Semaphore for Bitmuster
char fifo1[3] = "xx", fifo2[3] = "xx", fifo3[3] = "xx"; // for 2 packets + \0
char fifo4[3] =
    "xx"; // this project doesn't use fifo4, but we don't have a pop() :)

void activate(int val) { // schreibe 32 Bit auf Karte
  rt_sem_wait(&sem_bit);
  // rt_printk("val: %x\n",val);
  // rt_printk("bitmusteralt: %x\n",bitmuster);
  bitmuster = bitmuster | val;
  outb(bitmuster, CARD + 0);
  // rt_printk("bitmusterneu: %x\n",bitmuster);
  rt_sem_signal(&sem_bit);
}

void deactivate(int val) { // schreibe 32 Bit auf Karte
  rt_sem_wait(&sem_bit);
  // rt_printk("deactiv val: %x\n",val);
  bitmuster = bitmuster ^ val;
  // rt_printk("bitmuster deac: %x\n",bitmuster);
  outb(bitmuster, CARD + 0);
  rt_sem_signal(&sem_bit);
}

void test_machine(void) {

  rt_printk(" -->Test Machine start\n");
  activate(band1);
  deactivate(scanner);
  rt_sleep(nano2count(300 * 1000 * 1000)); // 15 Sekunden bitte???
  deactivate(band1);
  activate(band2);
  rt_sleep(nano2count(300 * 1000 * 1000)); // 15 Sekunden bitte???
  deactivate(band2);
  activate(schieber);
  rt_sleep(nano2count(300 * 1000 * 1000)); // 15 Sekunden bitte???
  deactivate(schieber);
  activate(ausw1);
  rt_sleep(nano2count(300 * 1000 * 1000)); // 15 Sekunden bitte???
  deactivate(ausw1);
  activate(ausw2);
  rt_sleep(nano2count(300 * 1000 * 1000)); // 15 Sekunden bitte???
  deactivate(ausw2);
  activate(ausw3);
  rt_sleep(nano2count(300 * 1000 * 1000)); // 15 Sekunden bitte???
  deactivate(ausw3);
  rt_sleep(nano2count(300 * 1000 * 1000)); // 15 Sekunden bitte???
  activate(band1 + band2 + scanner);
  rt_sleep(nano2count(600 * 1000 * 1000)); // 30 Sekunden bitte???
}
unsigned short getDigitalInput(void) {
  unsigned short tmp = 0;
  tmp = inb(0xc000 + 5);
  tmp = tmp << 8;
  tmp = tmp | inb(0xc000 + 4);
  return tmp;
};

void LSpoller(void) { // states pruefen
  unsigned short temp, oben;
  while (1) {
    temp = inb(CARD + 4);
    oben = inb(CARD + 5);
    rt_printk("LSpoller in = %3d \n ", temp);
    rt_printk("LSoben in = %3d \n ", oben);
    test_machine();
    rt_task_wait_period();
  }
}

static __init int parallel_init(void) {
  // init_pci();

  deactivate(schieber);

  RTIME tperiod, tstart1, now;
  rt_mount();
  rt_task_init(&polltask, LSpoller, 0x0, 3000, 4, 0, 0);
  rt_typed_sem_init(&sem_bit, 1, RES_SEM);
  rt_typed_sem_init(&s1, 1, RES_SEM);
  rt_set_periodic_mode();
  start_rt_timer(0);

  now = rt_get_time();
  tstart1 = now + nano2count(10000000);
  rt_task_make_periodic(&polltask, tstart1, nano2count(our1p5ms * 2));
  rt_printk("Module loadedzor\n");
  return 0;
}

static __exit void parallel_exit(void) {
  stop_rt_timer();
  rt_sem_delete(&s1);
  rt_sem_delete(&sem_bit);
  rt_task_delete(&polltask);
  outb(0, CARD + 0); // 00000000
  outb(0, CARD + 1); // 00000000
  bitmuster= 0x00;
  activate(scanner);
  rt_umount();
  rt_printk("Unloading modulski\n------------------\n");
}

module_init(parallel_init);
module_exit(parallel_exit);

MODULE_LICENSE("GPL");
