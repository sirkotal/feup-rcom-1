// Link layer protocol implementation

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include "link_layer.h"

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define BAUDRATE B38400

#define BUF_SIZE 5

int alarmEnabled = FALSE;
int alarmCount = 0;
int fd;
int frame = 0;
struct termios oldtio;
LinkLayerRole role;
unsigned char buf[BUF_SIZE];
enum message_state {
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK,
    DATA_READ,
    BCC_OK_2,
    ESC,
    END
};
int retransmissions;
unsigned int trans_frame = 0;

volatile int STOP = FALSE;

// Alarm function handler
void alarmHandler(int signal)
{
    int bytes = write(fd, buf, BUF_SIZE);
    alarmCount++;

    printf("Alarm #%d\n", alarmCount);
    //talvez fazer verificação aqui
    alarm(3);
}

void establishSerialPort(LinkLayer connectionParameters) {
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = connectionParameters.serialPort;

    /*if (argc < 2)
    {
        printf("Incorrect program usage\n"
               "Usage: %s <SerialPort>\n"
               "Example: %s /dev/ttyS1\n",
               argv[0],
               argv[0]);
        exit(1);
    }*/

    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    fd = open(serialPortName, O_RDWR | O_NOCTTY);

    retransmissions = connectionParameters.nRetransmissions;

    if (fd < 0)
    {
        perror(serialPortName);
        exit(-1);
    }

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
    
    if (connectionParameters.role == LlTx) {
        newtio.c_cc[VMIN] = 0; 
    }
    else {
        newtio.c_cc[VMIN] = 1; 
    }

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
}

void resetPortSettings() {
    // Restore the old port settings
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);
}

void llSetFrame() {
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
               else if (byte != 0x7E)
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
               else if (byte == 0x7E)
                  state = FLAG_RCV;
               else
                  state = START;
               break;
            case BCC_OK:
               if (byte == 0x7E) {
                  state = END;
                  STOP = TRUE;
                  printf("Connection Established...\n");
               }
               else
                  state = START;
               break;
            }

        if (alarmCount == 4) {
            alarm(0);
            STOP = TRUE;
            printf("Program Terminated...\n");
        }
        
        /*for (int i = 0; i < BUF_SIZE; i++) {
            printf("var = 0x%02X\n", (unsigned int)(buf[i] & 0xFF));
        }*/

    }
}

void llUaFrame() {
    memset(buf, 0, BUF_SIZE);
    enum message_state state = START;
    unsigned char byte;

    while (STOP == FALSE) {
        // Returns after 5 chars have been input
        int bytes = read(fd, &byte, 1);
         // Set end of string to '\0', so we can printf
        //printf("%s:%d\n", buf, bytes);
        switch(state) {
            case START:
               if (byte == 0x7E){
                  state = FLAG_RCV;
                  buf[START] = byte;
                  }
               break;
            case FLAG_RCV:
               if (byte == 0x03){
                  state = A_RCV;
                  buf[FLAG_RCV] = 0x01;}
               else if (byte != 0x7E)
                  state = START;
               break;
            case A_RCV:
               if (byte == 0x03){
                  state = C_RCV;
                  buf[A_RCV] = 0x07;}
               else if (byte == 0x7E)
                  state = FLAG_RCV;
               else
                  state = START;
               break;
            case C_RCV:
               if (byte == 0x03^0x03){
                  state = BCC_OK;
                  buf[C_RCV] = buf[FLAG_RCV]^buf[A_RCV];}
               else if (byte == 0x7E)
                  state = FLAG_RCV;
               else
                  state = START;
               break;
            case BCC_OK:
               if (byte == 0x7E) {
                  state = END;
                  STOP = TRUE;
                  buf[BCC_OK] = byte;
               }
               else
                  state = START;
               break;
            }
         printf("state: %d",state);
        /*for (int i = 0; i<BUF_SIZE; i++){
            printf("var = 0x%02X\n", (unsigned int)(buf[i] & 0xFF));
        }*/
        /*if (buf[1]^buf[2]==buf[3]){
            STOP = TRUE;}*/
               
        //printf(":%s:%d\n", buf, bytes);
        //if (buf[0] == 'z')
          //  STOP = TRUE;
    }
    int bytes = write(fd, buf, BUF_SIZE);
    printf("%d bytes written\n", bytes);
    // The while() cycle should be changed in order to respect the specifications
    // of the protocol indicated in the Lab guide
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{    
   printf("a entrar open\n");
   establishSerialPort(connectionParameters);
   if (connectionParameters.role == LlTx) {
      llSetFrame();
   }
   else {
      llUaFrame();
   }
   printf("a sair open\n");
   return 0;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
void stuffing(unsigned char* frame, unsigned int packet_location, unsigned char special) {
   size_t frame_size = sizeof(frame) / sizeof(frame[0]);
   frame = realloc(frame, ++frame_size);

   frame[packet_location] = 0x7D;
   packet_location++;

   frame[packet_location] = special^0x20;
   packet_location++;
}

unsigned char supervisionFrameRead() {
    unsigned char byte;
    unsigned char cByte = 0;
    enum message_state state = START;
    
    while (state != END && alarmEnabled == FALSE) {  
        read(fd, &byte, 1);
        switch (state) {
            case START:
                if (byte == 0x7E) {
                    state = FLAG_RCV;
                }    
                break;
            case FLAG_RCV:
                if (byte == 0x01) {
                    state = A_RCV;
                }
                else if (byte == 0x7E) {
                    state = FLAG_RCV;
                }  
                else {
                    state = START;
                }  
                break;
            case A_RCV:
                if (byte == 0x05 || byte == 0x85 || byte == 0x01 || byte == 0x81 || byte == 0x0B) {   // RR0,RR1, REJ0, REJ1, DISC
                    state = C_RCV;
                    cByte = byte;   
                }
                else if (byte == 0x7E) {
                    state = FLAG_RCV;
                }
                else {
                    state = START;
                }
                break;
            case C_RCV:
                if (byte == (0x01^cByte)) {
                    state = BCC_OK;
                }
                else if (byte == 0x7E) {
                    state = FLAG_RCV;
                }
                else {
                    state = START;
                }
                break;
            case BCC_OK:
                if (byte == 0x7E){
                    state = END;
                }
                else {
                    state = START;
                }
                break;
            default: 
                break;
        }
    } 
    return cByte;
}

int llwrite(const unsigned char *buf, int bufSize)
{  
   printf("entered llwrite\n");
   unsigned char *frame;
   frame = (unsigned char *)malloc(bufSize + 6);
   frame[0] = 0x7E;
   frame[1] = 0x03;
   if (trans_frame == 0) {
      frame[2] = 0x00;
   }
   else if (trans_frame == 1) {
      frame[2] = 0x40;
   }
   frame[3] = frame[1]^frame[2];
   memcpy(frame+4, buf, bufSize);

   unsigned char bcc_2;
   bcc_2 = buf[0];
   for (unsigned int i = 1 ; i < bufSize ; i++) {
   bcc_2 ^= buf[i];
   }
   unsigned int packet_loc = 4;

   for (unsigned int i = 0 ; i < bufSize ; i++) {
      if (buf[i] == 0x7E) {
         stuffing(frame, &packet_loc, 0x7E);
      }
      else if (buf[i] == 0x7D) {
         stuffing(frame, &packet_loc, 0x7D);
      }
      else {
         frame[packet_loc] = buf[i];
         packet_loc++;
      }
   }

   if (bcc_2 == 0x7E) {
      stuffing(frame, &packet_loc, 0x7E);
   }
   else if (bcc_2 == 0x7D) {
      stuffing(frame, &packet_loc, 0x7D);
   }
   else {
      frame[packet_loc] = bcc_2;
      packet_loc++;
   }

   frame[packet_loc] = 0x7E;
   packet_loc++;

   int n_transmission = 0;
   int accepted = FALSE;
   int rejected = FALSE;

   while (n_transmission < retransmissions) { 
      // alarmCount = 0;  --> do we need the alarm loop in here?
      alarmEnabled = FALSE;
      alarm(3);
      rejected = FALSE;
      accepted = FALSE;

      while (alarmEnabled == FALSE && !accepted && !rejected) {
         write(fd, frame, packet_loc);
         unsigned char cByte = supervisionFrameRead();
         
         if (cByte == 0x00) {
               continue;
         }
         else if (cByte == 0x05 || cByte == 0x85) {    // RR0 and RR1
               trans_frame = 1 - trans_frame;
               accepted = TRUE;
         }
         else if (cByte == 0x01 || cByte == 0x81) {   // REJ0 and REJ1
               rejected = FALSE;
         }
         else {
               continue;
         }
      }

      if (accepted) {
         break;
      }
      n_transmission++;
   }

   free(frame);

   if (accepted) {
      printf("left llwrite nicely\n");
      return packet_loc;
   }
   else {
      printf("left llwrite\n");
      return -1;
   }
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{  
   printf("enter llread\n");
   enum message_state state = START;
   unsigned char byte;
   int bcc = 0;
   int size = 0;
   STOP = FALSE;
   while (STOP == FALSE) {
      int bytes = read(fd, &byte, 1);
      printf("var = 0x%02X\n", (unsigned int)(byte & 0xFF));
      unsigned char buf[BUF_SIZE];
      switch(state) {
         case START:
            if (byte == 0x7E){
               state = FLAG_RCV;
               }
            break;
         case FLAG_RCV:
            if (byte == 0x03){
               state = A_RCV;
               }
            else if (byte != 0x7E)
               state = START;
            break;
         case A_RCV:
            if (byte == 0x00 || byte == 0x40){
               state = C_RCV;
            }
            else if (byte == 0x7E)
               state = FLAG_RCV;
            else
               state = START;
            break;
         case C_RCV:
            if (byte == 0x03^0x00 || byte == 0x03^0x40){
               state = BCC_OK;
               }
            else if (buf[0] == 0x7E)
               state = FLAG_RCV;
            else
               state = START;
            break;
         case BCC_OK:
            if (byte == 0x7D)
               state = ESC;
            else if (byte == 0x7E){
               int bcc;
               size--;
               for (int i = 0; i < size; i++){
                  if (i == 0)
                     bcc = packet[i];
                  else
                     bcc ^= packet[i];
               }
               if (bcc == packet[size]){
                  state = STOP;
                  buf[0]=0x7E;
                  buf[1]=0x01;
                  if (frame)
                     buf[2]=0x05;
                  else
                     buf[2]=0x85;
                  buf[3]=buf[1]^buf[2];
                  buf[4]=0x7E;
                  write(fd,buf,BUF_SIZE);
                  packet[size] = '/0'; 
                  printf("left llread nice\n");
                  return size;
               }
               else{
                  buf[0]=0x7E;
                  buf[1]=0x01;
                  if (frame)
                     buf[2]=0x01;
                  else
                     buf[2]=0x81;
                  buf[3]=buf[1]^buf[2];
                  buf[4]=0x7E;
                  write(fd,buf,BUF_SIZE);
                  packet[size-1] = '/0';
                  printf("packed %s", packet); 
                  printf("left llread\n");
                  return -1;
               }

            }
            else {
               packet[size] = byte;
               size++;
            }
            break;
         case ESC:
            state = BCC_OK;
            if (byte == 0x5E){
               packet[size]==0x7E;
               size++;
            }
            else if (byte == 0x5D){
               packet[size] == 0x7D;
               size++;
            }
            else{
               packet[size]==0x7D;
               size++;
               packet[size]==byte;
               size++;
            }
      }
   }
   return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
void llcloseTx(){

   buf[0] = 0x7E;
   buf[1] = 0x03;
   buf[2] = 0x0B;
   buf[3] = buf[1]^buf[2];
   buf[4] = 0x7E;

   int bytes = write(fd, buf, BUF_SIZE);
   printf("%d bytes written\n", bytes);
   
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
            else if (byte != 0x7E)
               state = START;
            break;
         case A_RCV:
            if (byte == 0x0B)
               state = C_RCV;
            else if (byte == 0x7E)
               state = FLAG_RCV;
            else
               state = START;
            break;
         case C_RCV:
            if (byte == 0x01^0x0B)
               state = BCC_OK;
            else if (byte == 0x7E)
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

      if (alarmCount == 4) {
         alarm(0);
         STOP = TRUE;
      }
   }

   buf[0] = 0x7E;
   buf[1] = 0x03;
   buf[2] = 0x07;
   buf[3] = buf[1]^buf[2];
   buf[4] = 0x7E;

   bytes = write(fd, buf, BUF_SIZE);
}

void llcloseRx(){
   enum message_state state = START;
   unsigned char byte;

   while (STOP == FALSE) {
      int bytes = read(fd, &byte, 1);

      switch(state) {
         case START:
            if (byte == 0x7E)
               state = FLAG_RCV;
            break;
         case FLAG_RCV:
            if (byte == 0x03)
               state = A_RCV;
            else if (byte != 0x7E)
               state = START;
            break;
         case A_RCV:
            if (byte == 0x0B)
               state = C_RCV;
            else if (byte == 0x7E)
               state = FLAG_RCV;
            else
               state = START;
            break;
         case C_RCV:
            if (byte == 0x03^0x0B)
               state = BCC_OK;
            else if (byte == 0x7E)
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
   }
   buf[0] = 0x7E;
   buf[1] = 0x01;
   buf[2] = 0x0B;
   buf[3] = buf[1]^buf[2];
   buf[4] = 0x7E;
   int bytes = write(fd, buf, BUF_SIZE);
   
   while (STOP == FALSE) {
      int bytes = read(fd, &byte, 1);

      switch(state) {
         case START:
            if (byte == 0x7E)
               state = FLAG_RCV;
            break;
         case FLAG_RCV:
            if (byte == 0x03)
               state = A_RCV;
            else if (byte != 0x7E)
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
            if (byte == 0x03^0x07)
               state = BCC_OK;
            else if (byte == 0x7E)
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
   }
   printf("%d bytes written\n", bytes);
}

int llclose(int showStatistics)
{  

    if (role == LlTx) {
        llcloseTx();
    }
    else {
        llcloseRx();
    }

    resetPortSettings();

    return 1;
}

