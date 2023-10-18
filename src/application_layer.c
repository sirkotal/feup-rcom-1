// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void buildControlPacket(int controlfield, const char* filename, int length){
    int lensize = 0;
    int tmp = length;
    while (tmp > 0){
        tmp >>= 8;
        lensize++;
    }
    int namesize = strlen(filename);
    int size = 5+lensize+namesize;
    unsigned char control[size];
    int i = 0;
    control[i++] = controlfield;
    control[i++] = 0;
    control[i] = lensize;
    for (int j = i + lensize; j > i; j--){
        control[j] = length & 0xFF;
        length >>= 8;
    }
    i+=lensize+1;
    control[i++] = 1;
    control[i++] = namesize;
    memcpy(control+i,filename,namesize);
    llwrite(control, size);
}

void readControlPacket(unsigned char* name){
    unsigned char control[MAX_PAYLOAD_SIZE];
    llread(control);
    int size = 0;
    int filesize = control[2];
    int i;
    for (i = 3; i < 3+filesize; i++){
        size += control[i];
        if (i+1 < 3+filesize){
            size <<= 8;
        }
    }
    printf("Size:%d\n", size);
    int namesize = control[++i];
    for (int j = 0; j < namesize; j++){
        printf("var = 0x%02X\n", (unsigned int)(*(control +7 + j) & 0xFF));
    }
    memcpy(name,control+7, namesize);
    name[namesize]='\0';
    //remove on
    name[0]='t';
}

void applicationLayer(const char *serialPort, const char *role, int baudRate, int nTries, int timeout, const char *filename) {
    LinkLayer parameters;
    int statistics = 0;
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
        unsigned char name[MAX_PAYLOAD_SIZE];
        readControlPacket(name);
        
        FILE *fptr = fopen(name, "wb+");
        int bytes = 0;
        int reada;
        unsigned char data[MAX_PAYLOAD_SIZE];    
        do {
            reada = llread(data);
            if (data[0] == 3) break;
            fwrite(data+3, 1, reada-3, fptr);
            fflush(fptr);
        } while (reada > 0);
        fclose(fptr);
        llclose(statistics);
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
        fseek(fptr, 0, SEEK_SET);
        buildControlPacket(2, filename, len);
        int bytesleft = len;
        int datasize;
        unsigned char data[MAX_PAYLOAD_SIZE-3];
        unsigned char data_packet[MAX_PAYLOAD_SIZE];
        while (bytesleft > 0){
            data_packet[0] = 1;
            if (bytesleft > MAX_PAYLOAD_SIZE-3){
                datasize = MAX_PAYLOAD_SIZE-3;
                bytesleft -= datasize;}
            else{
                datasize = bytesleft;
                bytesleft -= datasize;}
            fread(data, 1, datasize, fptr);
            data_packet[1] = datasize >> 8 & 0xFF;
            data_packet[2] = datasize & 0xFF;
            memcpy(data_packet + 3, data, datasize);
            /*for(int m = 0; m<datasize; m++){
               printf("mandei = 0x%02X\n", (unsigned int)(data_packet[m] & 0xFF));
            }*/
            printf("size: %d\n", datasize);
            llwrite(data_packet, datasize+3);
        }
        buildControlPacket(3, filename, len);
        printf("left loop");
        fclose(fptr);
        llclose(statistics);
    }
    else {
        perror("Unidentified Role\n");
        exit(-1);
    }
}