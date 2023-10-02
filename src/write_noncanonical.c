// Write to serial port in non-canonical mode
//
// Modified by: Eduardo Nuno Almeida [enalmeida@fe.up.pt]

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

// Baudrate settings are defined in <asm/termbits.h>, which is
// included by <termios.h>
#define BAUDRATE B38400
#define _POSIX_SOURCE 1 // POSIX compliant source

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 5

int alarmEnabled = FALSE;
int alarmCount = 0;
int fd;
unsigned char buf[BUF_SIZE];
enum message_state {
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK,
    END
};


volatile int STOP = FALSE;

// Alarm function handler
void alarmHandler(int signal)
{
    int bytes = write(fd, buf, BUF_SIZE);
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
    
    alarm(3);
}

int main(int argc, char *argv[])
{
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = argv[1];

    if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }

    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    fd = open(serialPortName, O_RDWR | O_NOCTTY);

    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 5;  // Blocking read until 5 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");
    
    // Set alarm function handler
    (void)signal(SIGALRM, alarmHandler);

    // Create string to send

    buf[0] = 0x7E;
    buf[1] = 0x03;
    buf[2] = 0x03;
    buf[3] = buf[1]^buf[2];
    buf[4] = 0x7E;

    /*
    for (int i = 0; i < BUF_SIZE; i++)
    {
        buf[i] = 'a' + i % 26;
    }

    // In non-canonical mode, '\n' does not end the writing.
    // Test this condition by placing a '\n' in the middle of the buffer.
    // The whole buffer must be sent even with the '\n'.
    buf[5] = '\n';*/

    int bytes = write(fd, buf, BUF_SIZE);
    printf("%d bytes written\n", bytes);

    // Wait until all bytes have been written to the serial port
    // sleep(1);
    
    alarm(3);
    int count = 0;
    unsigned char byte;
    enum message_state state = START;


    while (STOP == FALSE) {
        int reception = read(fd, &byte, 1);     
        
        switch(state) {
            case START:
               if (byte == 0x7E)
                  state = FLAG_RCV;
               break;
            case FLAG_RCV:
               if (byte == 0x01)
                  state = A_RCV;
               else if (byte == 0x7E)
                  state = FLAG_RCV;
               else
                  state = START;
               break;
            case A_RCV:
               if (byte == 0x07)
                  state = C_RCV;
               else if (byte == 0x7E)
                  state = FLAG_RCV;
               else
                  state = START;
               break;
            case C_RCV:
               if (byte == 0x01^0x07)
                  state = BCC_OK;
               else if (buf[0] == 0x7E)
                  state = FLAG_RCV;
               else
                  state = START;
               break;
            case BCC_OK:
               if (byte == 0x7E) {
                  state = END;
                  STOP = TRUE;
               }
               else
                  state = START;
               break;
            }
        printf("state: %d",state);


        if (alarmCount == 4) {
            alarm(0);
            STOP = TRUE;
            printf("Program Terminated...\n");
        }
        
        /*for (int i = 0; i < BUF_SIZE; i++) {
            printf("var = 0x%02X\n", (unsigned int)(buf[i] & 0xFF));
        }

        if (buf[1]^buf[2] == buf[3]) {
            printf("Connection Established...\n");
            STOP = TRUE;
        }*/
    }

    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);

    return 0;
}