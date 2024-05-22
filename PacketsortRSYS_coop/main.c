#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <semaphore.h>
#include <time.h>
#include <stdarg.h>

#define BYTE_TO_BINARY_PATTERN "%s 0b%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0')


#pragma clang diagnostic push
#pragma ide diagnostic ignored "LoopDoesntUseConditionVariableInspection"

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

typedef struct {
    uint8_t id;
    uint8_t position;
} parcel;

uint8_t eject_flags = 0x00;
sem_t eject_flags_mutex;
uint8_t slider_belts_flags = SLIDER | BELT1;
uint8_t output = 0x00;
sem_t sem_input, sem_output;
uint8_t previous_sensor_data = 0x01;
uint8_t tasks_active = 0x01;
parcel current_parcel = {0, 0};
uint8_t ready_for_next_parcel = 1;

pthread_mutex_t rt_printk_mutex = PTHREAD_MUTEX_INITIALIZER;

void rt_printk(const char *format, ...) {
    // Lock the mutex to ensure exclusive access
    pthread_mutex_lock(&rt_printk_mutex);

    // Get the current time
    time_t rawtime;
    struct tm *timeinfo;
    char time_buffer[80];

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    // Format the time as desired
    strftime(time_buffer, sizeof(time_buffer), "%H:%M:%S", timeinfo);

    // Print the timestamp
    printf("[%s] ", time_buffer);

    // Handle the variable arguments for the main message
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    // Print a newline at the end
    printf("\n");

    // Unlock the mutex
    pthread_mutex_unlock(&rt_printk_mutex);
}

void set_output(uint8_t mask) {
    sem_wait(&sem_output);
    output |= mask;
    rt_printk(BYTE_TO_BINARY_PATTERN, "out:", BYTE_TO_BINARY(output));
    sem_post(&sem_output);
}

void clear_output(uint8_t mask) {
    sem_wait(&sem_output);
    output &= ~mask;
    rt_printk(BYTE_TO_BINARY_PATTERN, "out:", BYTE_TO_BINARY(output));
    sem_post(&sem_output);
}


void *sensors_task(void *arg) {
    uint8_t sensor_data = 0;
    uint8_t index = 0;
    uint8_t mask = 0;

    while (tasks_active) {
        sem_wait(&sem_input);
        // rt_printk("Reading sensor values");
        sleep(4);

        if (sensor_data == 0) {
            sensor_data = 1;
        } else {
            sensor_data <<= 1;
        }

        rt_printk(BYTE_TO_BINARY_PATTERN, "in:", BYTE_TO_BINARY(sensor_data));

        mask = 0x1;
        for (index = 0; index < 5; index++) {
            if (((sensor_data & mask) == mask) && ((previous_sensor_data & mask) == 0)) {
                // rt_printk("Sensor [%d] activated + maks %d", index, mask);
                if (current_parcel.id == index) {
                    eject_flags |= mask;
                }
            }
            mask <<= 1;
        }

        previous_sensor_data = sensor_data;
        sem_post(&sem_input);
    }

    return NULL;
}

void handle_ejection_for_mask(uint8_t mask) {
    if ((eject_flags & mask) == mask) {
        // rt_printk("Ejector number %d HIGH", mask);
        set_output(mask);
        sleep(4); // For the amount it takes for the physical ejection to finish
        eject_flags &= ~mask;
        clear_output(mask);
        // rt_printk("Ejector number %d LOW", mask);
        ready_for_next_parcel = 1;
    }
}

void *eject_task1(void *arg) {
    while (tasks_active) {
        handle_ejection_for_mask(0x02);
    }

    return NULL;
}

void *eject_task2(void *arg) {
    while (tasks_active) {
        handle_ejection_for_mask(0x04);
    }

    return NULL;
}


void *eject_task3(void *arg) {
    while (tasks_active) {
        handle_ejection_for_mask(0x08);
    }

    return NULL;
}

uint8_t read_scanner(uint8_t index) {
    uint8_t arr[] = {3, 1, 2, 254, 1, 3, 3, 2, 254, 1, 254, 2, 3};
    return arr[index];
}

void *barcode_task(void *arg) {
    uint8_t i = 0;
    while (tasks_active) {
        if (ready_for_next_parcel) {
            uint8_t parcel_num = read_scanner(i);
            i = (i + 1) % 12;
            if (parcel_num != 254) {
                current_parcel.id = parcel_num;
                current_parcel.position = 0;
                // rt_printk("Got parcel with id: %d", current_parcel.id);
                
                sem_wait(&eject_flags_mutex);
                slider_belts_flags |= BELT2;
                slider_belts_flags &= ~SLIDER;
                sem_post(&eject_flags_mutex);
            } else {
                current_parcel.id = 4;
                current_parcel.position = 0;
                // rt_printk("Error in parcel scan.");
            }
            ready_for_next_parcel = 0;
            sleep(2);
        }
    }

    return NULL;
}

void *belt_task() {
    //band1, band2, slider
    while (tasks_active) {
        sem_wait(&eject_flags_mutex);

        if ((slider_belts_flags & BELT1) == BELT1) {
            set_output(BELT1);
            // rt_printk("belt1 set");
        } else {
            clear_output(BELT1);
            // rt_printk("belt1 unset");
        }

        if ((slider_belts_flags & BELT2) == BELT2) {
            set_output(BELT2);
            // rt_printk("belt2 set");
        } else {
            clear_output(BELT2);
            // rt_printk("belt2 unset");
        }

        if (((slider_belts_flags & SLIDER) == SLIDER)) {
            set_output(SLIDER);
            // rt_printk("slider set");
        } else {
            clear_output(SLIDER);
            // rt_printk("slider unset");
        }

        sleep(2);
        sem_post(&eject_flags_mutex);
    }

    return NULL;
}

int main() {
    int thread_arg = 1;
    pthread_t sensor_thread, eject_thread1, eject_thread2, eject_thread3, barcode_thread, belt_thread;

    if (pthread_create(&sensor_thread, NULL, sensors_task, &thread_arg) != 0 ||
        pthread_create(&eject_thread1, NULL, eject_task1, &thread_arg) != 0 ||
        pthread_create(&eject_thread2, NULL, eject_task2, &thread_arg) != 0 ||
        pthread_create(&eject_thread3, NULL, eject_task3, &thread_arg) != 0 ||
        pthread_create(&barcode_thread, NULL, barcode_task, &thread_arg) != 0 ||
        pthread_create(&belt_thread, NULL, belt_task, &thread_arg) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    getchar();

    tasks_active = 0x00;

    return 0;
}

#pragma clang diagnostic pop