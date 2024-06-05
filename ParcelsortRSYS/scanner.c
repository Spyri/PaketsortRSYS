//
// Testing laser-scanner
// Daniel Engelhart
// 2008-10-23
//

// includes
#include <errno.h>   /* Error number definitions */
#include <fcntl.h>   /* File control definitions */
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <unistd.h>  /* UNIX standard function definitions */


#define BUFFERSIZE 50

int init_port(void) {
  int fd; /* File descriptor for the port */
  struct termios options;

  fd = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY | O_NDELAY);
  if (fd == -1) {
    /*
        * Could not open the port.
    */

    perror("open_port: Unable to open /dev/ttyS0 - ");
  } else {

    fcntl(fd, F_SETFL, 0);

    /*
    * Get the current options for the port...
    */
    tcgetattr(fd, &options);

    /*
    * Set the baud rates to 19200...
    */
    cfsetispeed(&options, B9600);
    cfsetospeed(&options, B9600);

    /*
    * Enable the receiver and set local mode...
    */
    options.c_cflag |= (CLOCAL | CREAD);

    /*
    * Set the new options for the port...
    */
    tcsetattr(fd, TCSANOW, &options);

    options.c_cflag &= ~CSIZE; /* Mask the character size bits */
    options.c_cflag |= CS7;    /* Select 7 data bits */

    /* setting no parity-mode */
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;

    // options.c_cflag &= ~CNEW_CRTSCTS; /* Disable Hardware-Flow-Control */
    options.c_iflag &=
        ~(IXON | IXOFF | IXANY); /* Disable Software-Flow-Control */
  }

  return (fd);
}

int computePosition(char *ean) {
  int position = 0;
  position = atoi(ean);
  if (position >= 0 && position < 30) {
    return 1;
  } else if (position >= 30 && position < 60) {
    return 2;
  } else if (position >= 60 && position < 100) {
    return 3;
  } else {
    return 0;
  }
}

int main() {

  int fd_serial = 0;
  int fd_fifo = 0;
  int charsRead = 0;
  char buffer[BUFFERSIZE];
  char ean[3] = {0};
  fd_serial = init_port();

  if (fd_serial != -1 && fd_fifo != -1) {

    fcntl(fd_serial, F_SETFL, 0);

    while (1) {
      charsRead = read(fd_serial, buffer, BUFFERSIZE);

      if (charsRead > 13) {
        int i = 1;
        //printf("ean-code: ", charsRead);
        for (i; i < charsRead - 1; i++) {
          // printf("%c", buffer[i]);
          if (i == 12) {
            ean[0] = buffer[i];
          }
          if (i == 13) {
            ean[1] = buffer[i];
          }
        }
        //printf("\n");
        ean[2] = 0;
        int pos = 0;
        pos = computePosition(ean);
        printf("position: %d\n", pos);
        fd_fifo = open("/dev/rtf3", O_RDWR | O_NOCTTY | O_NDELAY);
        if (pos == 1) {
          write(fd_fifo, "1", 1);
        } else if (pos == 2) {
          write(fd_fifo, "2", 1);
        } else if (pos == 3) {
          write(fd_fifo, "3", 1);
        } else {
          write(fd_fifo, "4", 1);
        }
        close(fd_fifo);
      }
      
      //sleep(1);
    }
    close(fd_serial);
  } else {
    printf("error on initialising fifo or serial port\n");
  }

  return 0;
}

