#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <semaphore.h>
#include <time.h>
#include <stdarg.h>


uint8_t input = 0x01;
uint8_t output = 0x00;
sem_t sem_input, sem_output;
uint8_t  previous_sensor_data;
uint8_t next_output_station;

uint8_t eject1_flag, eject2_flag, eject3_flag = 0;

void set_output(uint8_t mask) {
    sem_wait(&sem_output);
    output |= mask;
    sem_post(&sem_output);
}

void clear_output(uint8_t mask) {
    sem_wait(&sem_output);
    output &= ~mask;
    sem_post(&sem_output);
}

void print_with_timestamp(const char *format, ...) {
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
}


_Noreturn void* sensors_task(void* arg) {
    while(1) {
        sem_wait(&sem_input);
        print_with_timestamp("Reading sensor values");
        sleep(4);
        input = input << 1;
        sem_post(&sem_input);
    }
}

_Noreturn void* eject_task1(void* arg) {
    while(1) {
        if ((input & 0x01) == 0x01) {
            print_with_timestamp("Ejector number %d HIGH", 1);
            set_output(0x01);
            sleep(4);
            clear_output(0x01);
            print_with_timestamp("Ejector number %d LOW", 1);
        }
    }
}

_Noreturn void* eject_task2(void* arg) {
    while(1) {
        print_with_timestamp("Ejector number %d HIGH", 2);
        set_output(0x02);
        sleep(4);
        clear_output(0x02);
        print_with_timestamp("Ejector number %d LOW", 2);
    }
}


_Noreturn void* eject_task3(void* arg) {
    while(1) {
        print_with_timestamp("Ejector number %d HIGH", 2);
        set_output(0x04);
        sleep(4);
        clear_output(0x04);
        print_with_timestamp("Ejector number %d LOW", 2);
    }
}

uint8_t read_scanner(uint8_t index) {
    uint8_t arr[] = {3, 1, 2, 254, 1, 3, 3, 2, 254, 1, 254, 2, 3};
    return arr[index];
}

_Noreturn void* barcode_task(void* arg) {
    uint8_t i = 0;
    while(1) {
        i = (i + 1) % 12;
        uint8_t parcel_num = read_scanner(i);
        if(parcel_num != 254) {
            print_with_timestamp("I got packet for position: %d", parcel_num);
        }
        sleep(2);
    }
}

_Noreturn void* belt_task() {
    //band1, band2, slider
    while(1) {
        print_with_timestamp("I control the slider and the belts.");
        sleep(2);
    }
}

int main(int count, char **args) {
    int thread_arg = 1;
    pthread_t sensor_thread, eject_thread1, eject_thread2, eject_thread3, barcode_thread, belt_thread;

    if (pthread_create(&sensor_thread, NULL,  sensors_task, &thread_arg) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&eject_thread1, NULL, eject_task1, &thread_arg) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&eject_thread2, NULL, eject_task2, &thread_arg) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&eject_thread3, NULL, eject_task3, &thread_arg) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }
    if (pthread_create(&barcode_thread, NULL, barcode_task, &thread_arg) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&belt_thread, NULL, belt_task, &thread_arg) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    getchar();

    return 0;
}