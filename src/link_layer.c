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
int llreadDisc = 0;
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

// Alarm function handler
void alarmHandler(int signal)
{
   alarmCount++;
   alarmEnabled = FALSE;
   printf("Alarm %d\n", alarmCount);
}

void establishSerialPort(LinkLayer connectionParameters) {
    // Program usage: Uses either COM1 or COM2
    const char *serialPortName = connectionParameters.serialPort;

    // Open serial port device for reading and writing, and not as controlling tty
    // because we don't want to get killed if linenoise sends CTRL-C.
    fd = open(serialPortName, O_RDWR | O_NOCTTY);

    retransmissions = connectionParameters.nRetransmissions;
    role = connectionParameters.role;

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
    
    if (role == LlTx) {
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

int llSetFrame() {
   // Set alarm function handler
   (void)signal(SIGALRM, alarmHandler);

   buf[0] = 0x7E;
   buf[1] = 0x03;
   buf[2] = 0x03;
   buf[3] = buf[1]^buf[2];
   buf[4] = 0x7E;
   
   int bytes = write(fd, buf, BUF_SIZE);
   printf("%d bytes written\n", bytes);
   
   alarm(3);
   alarmEnabled = TRUE;
   int count = 0;
   unsigned char byte;
   enum message_state state = START;

   while (state != END) {
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
               printf("Connection Established...\n");
            }
            else
               state = START;
            break;
         }
      
      if (alarmEnabled == FALSE && state != END){
         if (alarmCount > retransmissions) {
            alarm(0);
            alarmCount = 0;
            state = END;
            printf("Program Terminated...\n");
            return -1;
         }
         else {
            int bytes = write(fd, buf, BUF_SIZE);
            printf("%d bytes written\n", bytes);
            alarm(3);
            alarmEnabled = TRUE;
         }
      }
   }
   alarm(0);
   alarmCount = 0;
   alarmEnabled = FALSE;
   return 0;
}

void llUaFrame() {
    memset(buf, 0, BUF_SIZE);
    enum message_state state = START;
    unsigned char byte;

    while (state != END) {
        int bytes = read(fd, &byte, 1);
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
                  buf[BCC_OK] = byte;
               }
               else
                  state = START;
               break;
            }
    }
    int bytes = write(fd, buf, BUF_SIZE);
    printf("%d bytes written\n", bytes);
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llopen(LinkLayer connectionParameters)
{    
   establishSerialPort(connectionParameters);
   int connection;
   if (role == LlTx) {
      connection = llSetFrame();
   }
   else {
      llUaFrame();
   }
   return connection;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
void stuffing(unsigned char* frame, unsigned int packet_location, unsigned char special, unsigned int *size) {
   frame = realloc(frame, ++(*size));
   frame[packet_location] = 0x7D;
   frame[packet_location+1] = special^0x20;
}

int llwrite(const unsigned char *buf, int bufSize)
{  
   int bufSizeParam = bufSize+6;
   unsigned char* frame = (unsigned char*) malloc (bufSizeParam);
   memset(frame, 0, bufSize+6);
   *frame = 0x7E;
   *(frame+1) = 0x03;
   if (trans_frame == 0) {
      *(frame+2) = 0x00;
   }
   else if (trans_frame == 1) {
      *(frame+2) = 0x40;
   }
   *(frame+3) = *(frame+1)^*(frame+2);

   unsigned char bcc_2;
   bcc_2 = *buf;
   for (unsigned int i = 1 ; i < bufSize ; i++) {
      bcc_2 ^= *(buf+i);
   }
   unsigned int packet_loc = 4;

   for (unsigned int i = 0 ; i < bufSize ; i++) {
      if (*(buf+i) == 0x7E) {
         stuffing(frame, packet_loc, 0x7E, &bufSizeParam);
         packet_loc++;
      }
      else if (*(buf+i) == 0x7D) {
         stuffing(frame, packet_loc, 0x7D, &bufSizeParam);
         packet_loc++;
      }
      else {
         *(frame+packet_loc) = *(buf+i);
      }
      packet_loc++;
   }

   if (bcc_2 == 0x7E) {
      stuffing(frame, packet_loc, 0x7E, &bufSizeParam);
      packet_loc+=2;
   }
   else if (bcc_2 == 0x7D) {
      stuffing(frame, packet_loc, 0x7D, &bufSizeParam);
      packet_loc+=2;
   }
   else {
      *(frame+packet_loc) = bcc_2;
      packet_loc++;
   }
   *(frame+packet_loc) = 0x7E;
   packet_loc++;
   
   alarm(3);
   alarmEnabled = TRUE;
   unsigned char byte;
   unsigned char cbyte;
   int accepted = FALSE;
   enum message_state state = START;
   write(fd, frame, packet_loc);
   while (accepted != TRUE && state != END) { 
      read(fd, &byte, 1);
      switch (state) {
         case START:
               if (byte == 0x7E) {
                  state = FLAG_RCV;
               }    
               break;
         case FLAG_RCV:
               if (byte == 0x03) {
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
               if (byte == 0x05 || byte == 0x85){  // RR0, RR1 
                  accepted = TRUE;
                  state = C_RCV;
                  cbyte = byte;
               }
               else if(byte == 0x01 || byte == 0x81) {   //REJ0, REJ1
                  state = C_RCV;
                  cbyte = byte;
               }
               else if (byte == 0x7E) {
                  state = FLAG_RCV;
               }
               else {
                  state = START;
               }
               break;
         case C_RCV:
               if (byte == (0x01^cbyte)) {
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
      //printf("state: %d\n",state);
      if (alarmEnabled == FALSE && state != END){
         if (alarmCount > retransmissions) {
            alarm(0);
            state = END;
            printf("Program Terminated...\n");
         }
         else{
            int bytes = write(fd, frame, packet_loc);
            printf("%d bytes written\n", bytes);
            alarm(3);
            alarmEnabled = TRUE;
         }
      }
   }
   alarm(0);
   alarmCount = 0;
   alarmEnabled = FALSE;
   if (accepted) {
      trans_frame = 1 - trans_frame;
      return packet_loc;
   }
   else {
      printf("here\n");
      return -1;
   }
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{  
   printf("llread\n");
   unsigned char tmp[2050];
   enum message_state state = START;
   unsigned char byte;
   int bcc = 0;
   int size = 0;
   while (state != END) {
      int bytes = read(fd, &byte, 1);
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
            else if (byte == 0x0B) {
               llreadDisc = 1;
               return -2;
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
            else if (byte == 0x7E)
               state = FLAG_RCV;
            else
               state = START;
            break;
         case BCC_OK:
            if (byte == 0x7D)
               state = ESC;
            else if (byte == 0x7E){
               int bcc;
               size-=1;
               bcc = tmp[0];
               for (unsigned int i = 1 ; i < size; i++) {
                  bcc ^= tmp[i];
               }
               if (bcc == tmp[size]){
                  memcpy(packet,tmp,MAX_PAYLOAD_SIZE);
                  state = END;
                  buf[0]=0x7E;
                  buf[1]=0x03;
                  if (frame)
                     buf[2]=0x05;
                  else
                     buf[2]=0x85;
                  buf[3]=buf[1]^buf[2];
                  buf[4]=0x7E;
                  write(fd,buf,BUF_SIZE); 
                  printf("llread bem %d\n", size);
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
                  printf("llread mal\n");
                  return -1;
               }

            }
            else {
               tmp[size] = byte;
               size++;
            }
            break;
         case ESC:
            state = BCC_OK;
            if (byte == 0x5E){
               tmp[size]=0x7E;
               size++;
            }
            else if (byte == 0x5D){
               tmp[size]=0x7D;
               size++;
            }
            break;
      }
   }
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llcloseTx(){
   buf[0] = 0x7E;
   buf[1] = 0x03;
   buf[2] = 0x0B;
   buf[3] = buf[1]^buf[2];
   buf[4] = 0x7E;

   int bytes = write(fd, buf, BUF_SIZE);
   printf("%d bytes written\n", bytes);
   
   alarm(3);
   alarmEnabled=TRUE;
   int count = 0;
   unsigned char byte;
   enum message_state state = START;

   while (state != END) {
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
            }
            else
               state = START;
            break;
      }   

      if (alarmEnabled == FALSE && state != END){
         if (alarmCount > retransmissions) {
            alarm(0);
            state = END;
            printf("Program Terminated...\n");
            return -1;
         }
         else{
            int bytes = write(fd, buf, BUF_SIZE);
            printf("%d bytes written\n", bytes);
            alarm(3);
            alarmEnabled = TRUE;
         }
      }
   }
   alarm(0);
   alarmEnabled = FALSE;
   alarmCount = 0;

   buf[0] = 0x7E;
   buf[1] = 0x03;
   buf[2] = 0x07;
   buf[3] = buf[1]^buf[2];
   buf[4] = 0x7E;

   bytes = write(fd, buf, BUF_SIZE);
   printf("Program Terminated...\n");
   return 0;
}

void llcloseRx(){
   enum message_state state = START;
   unsigned char byte;
   if (llreadDisc){
      state = C_RCV;
      }
   while (state != END) {
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
   state = START;
   while (state != END) {
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
            }
            else
               state = START;
            break;
         }
   }
   printf("Program Terminated...\n");
}

int llclose(int showStatistics){
   int connection;  
    if (role == LlTx) {
        connection = llcloseTx();
    }
    else {
        llcloseRx();
    }
    if (showStatistics) {
        printStatistics();
    }

    resetPortSettings();

    return connection;;
}

void printStatistics() {
    double t_prop;
}