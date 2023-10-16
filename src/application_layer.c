// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>



void applicationLayer(const char *serialPort, const char *role, int baudRate, int nTries, int timeout, const char *filename) {
    LinkLayer parameters;
    strcpy(parameters.serialPort, serialPort);
    if (strcmp(role, "rx") == 0) {
        parameters.role = LlRx;
    } 
    else {
        parameters.role = LlTx;
    }
    parameters.baudRate = baudRate;
    parameters.nRetransmissions = nTries;
    parameters.timeout = timeout;
    llopen(parameters);

    if (parameters.role == LlRx) {
        unsigned char control[MAX_PAYLOAD_SIZE];
        llread(control);
        int size = 0;
        for (int i = 3; i < 3+control[2]; i++){
            size += control[i];
            if (i+1<5)
                size <<= 8;
        }    
        printf("J:%d\n", size);
        int namesize = control[5+control[2]];
        unsigned char name[namesize];
        memcpy(name,control+5+control[2], namesize);
        printf("Name: %s\n", name);
        unsigned char data[MAX_PAYLOAD_SIZE];
        llread(data);
    }
    else if (parameters.role == LlTx) {
        FILE *fptr;
        unsigned int packet_size;
        unsigned int start = 2;

        fptr = fopen(filename, "rb");

        if (fptr == NULL) {
            perror("This file wasn't found\n");
            exit(-1);
        }

        fseek(fptr, 0, SEEK_END);
        int len = ftell(fptr);
        unsigned char data[len];
        fseek(fptr, 0, SEEK_SET);
        fread(data, sizeof(unsigned char), len, fptr);
        int tmp = len;
        int lensize = 0;
        while (tmp > 0){
            tmp >>= 8;
            lensize++;
        }
        int namesize = strlen(filename);
        int size = 5+lensize+namesize;
        unsigned char control[size];
        int i = 0;
        control[i++] = 2;
        control[i++] = 0;
        control[i] = lensize; //or i++ but then i-1 + lensize
        tmp = len;
        for (int j = i + lensize; j > i; j--){
            printf("J:%d\n", j);
            control[j] = tmp & 0xFF;
            printf("control:%d\n", control[j]);
            tmp >>= 8;
            printf("len:%d\n", tmp);
        }
        i+=lensize+1;
        control[i++] = 1;
        control[i++] = namesize;
        memcpy(control+i,filename,namesize);
        llwrite(control, size);

        int bytesleft = len;
        int datasize;
        unsigned char data_packet[MAX_PAYLOAD_SIZE];
        data_packet[0] = 1;
        if (bytesleft > MAX_PAYLOAD_SIZE)
            datasize = MAX_PAYLOAD_SIZE;
        else
            datasize = bytesleft;
        
        data_packet[1] = datasize >> 8 & 0xFF;
        data_packet[2] = datasize & 0xFF;
        
        memcpy(data_packet+3, data, datasize-3);
        printf("data %d", datasize);
        llwrite(data_packet, datasize);
        fclose(fptr);
    }
    else {
        perror("Unidentified Role\n");
        exit(-1);
    }
}