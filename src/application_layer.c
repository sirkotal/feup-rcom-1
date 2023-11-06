// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

//Creates a control packet
int buildControlPacket(int controlfield, const char* filename, int length){
    int lensize = 0;
    int tmp = length;

    while (tmp > 0) {
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
    return llwrite(control, size);
}

//Reads a control packet
int readControlPacket(unsigned char* name){
    unsigned char control[MAX_PAYLOAD_SIZE];
    int reada;
    while ((reada = llread(control)) == -1);
    if (reada == -2){
        return -2;
    }

    int size = 0;
    int filesize = control[2];
    int i;
    for (i = 3; i < 3+filesize; i++){
        size += control[i];
        if (i+1 < 3+filesize){
            size <<= 8;
        }
    }

    int namesize = control[++i];
    memcpy(name,control+(++i), namesize);
    name[namesize]='\0';

    int format_pos = -1;
    for (int i = 0; i < namesize; i++) {
        if (name[i] == '.' && (i + 3) < namesize && name[i + 1] == 'g' && name[i + 2] == 'i' && name[i + 3] == 'f') {
            format_pos = i;
            break;
        }
    }

    if (format_pos != -1) {
        memmove(name + format_pos, "-received.gif", 14);
    }
    return 0;
}

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

    int statistics = 0; /*change to 1 if statistics are pretended*/
    if (llopen(parameters) < 0){
        perror("Connection failed\n");
        exit(EXIT_FAILURE);
    }
    if (parameters.role == LlRx) {  
        unsigned char name[MAX_PAYLOAD_SIZE];
        if (readControlPacket(name) == -2){
            perror("Error transfering the control\n");
            llclose(statistics);
            exit(EXIT_FAILURE);
        }
        FILE *fptr = fopen(name, "wb+");
        int read;
        unsigned char data[MAX_PAYLOAD_SIZE];   
        do {
            while ((read = llread(data)) == -1);
            if (read == -2){
                perror("Error transfering the data\n");
                fclose(fptr);
                llclose(statistics);
                exit(EXIT_FAILURE);
            }
            if (data[0] == 3) break;
            fwrite(data+3, 1, read-3, fptr);
            fflush(fptr);
        } while (1); 

        fclose(fptr);
        llclose(statistics);
    }
    else if (parameters.role == LlTx) {
        FILE *fptr;
        unsigned int start_ctrl = 2;
        unsigned int end_ctrl = 3;

        fptr = fopen(filename, "rb");

        if (fptr == NULL) {
            perror("This file wasn't found\n");
            exit(EXIT_FAILURE);
        }

        fseek(fptr, 0, SEEK_END);
        int len = ftell(fptr);
        fseek(fptr, 0, SEEK_SET);

        if (buildControlPacket(start_ctrl, filename, len) == -1){
            perror("Control packet error\n");
            fclose(fptr);
            llclose(statistics);
            exit(EXIT_FAILURE);
        }

        int bytesleft = len;
        int datasize;
        unsigned char data[MAX_PAYLOAD_SIZE-3];
        unsigned char data_packet[MAX_PAYLOAD_SIZE];

        while (bytesleft > 0){
            data_packet[0] = 1;
            if (bytesleft > MAX_PAYLOAD_SIZE-3){
                datasize = MAX_PAYLOAD_SIZE-3;
                bytesleft -= datasize;
            }
            else{
                datasize = bytesleft;
                bytesleft -= datasize;
            }

            fread(data, 1, datasize, fptr);
            data_packet[1] = datasize >> 8 & 0xFF;
            data_packet[2] = datasize & 0xFF;
            memcpy(data_packet + 3, data, datasize);
            int written = llwrite(data_packet, datasize+3);
            if (written == -1)
                break;
        }

        if (buildControlPacket(end_ctrl, filename, len) == -1){
            perror("Control packet error\n");
            fclose(fptr);
            llclose(statistics);
            exit(EXIT_FAILURE);
        }

        fclose(fptr);

        if (llclose(statistics) == -1){
            perror("Error disconnecting\n");
            exit(EXIT_FAILURE);
        }
    }
    else {
        perror("Unidentified Role\n");
        exit(EXIT_FAILURE);
    }
}