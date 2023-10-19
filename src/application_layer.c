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
        int namesize = control[4+control[2]];
        unsigned char name[namesize];
        memcpy(name,control+5+control[2], namesize);
        name[0] = 'T';
        //printf("Name: %s\n", name);
        //to do later
        FILE *fptr = fopen("tenguin.gif", "wb+");
        int bytes = 0;
        int reada = 1;
        int bytesread = 0;
        unsigned char data[MAX_PAYLOAD_SIZE];    
        while(reada > 0){
            reada = llread(data);
            for (int k = 0; k < reada; k++){
                printf("data = 0x%02X\n", (unsigned int)(data[k] & 0xFF));
            }
            bytesread += reada;
            //int cur = ftell(fptr);
            //printf("data: %d\n", reada-3);
            fwrite(data+3, 1, reada-3, fptr);
            fflush(fptr);
        }


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
        /*for (int k = 0; k< len; k++){
            printf("var = 0x%02X\n", (unsigned int)(data[k] & 0xFF));
        }*/
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
            //printf("J:%d\n", j);
            control[j] = tmp & 0xFF;
            //printf("control:%d\n", control[j]);
            tmp >>= 8;
            //printf("len:%d\n", tmp);
        }
        //printf(" data :%d\n", data[194]);
        i+=lensize+1;
        control[i++] = 1;
        control[i++] = namesize;
        memcpy(control+i,filename,namesize);
        llwrite(control, size);
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
        printf("left loop");
        printf("file size = %d", len);
        printf("name size = %d", namesize);
        fclose(fptr);

        // end control packet
        unsigned int end = 3;
        int end_tmp = len;  // 10968
        int end_lensize = 0;

        while (end_tmp > 0) {
            end_tmp >>= 8;
            end_lensize++;
        }

        int end_size = 5 + end_lensize + namesize;
        unsigned char end_control[end_size];
        int x = 0;

        end_control[x++] = end;
        end_control[x++] = 0;
        end_control[x] = end_lensize; 
        end_tmp = len;

        for (int y = x + end_lensize; y > x; y--){
            control[y] = end_tmp & 0xFF;
            end_tmp >>= 8;
        }

        x += end_lensize + 1;
        end_control[x++] = 1;
        end_control[x++] = namesize;
        memcpy(end_control + x, filename, namesize);
        llwrite(end_control, size);
    }
    else {
        perror("Unidentified Role\n");
        exit(-1);
    }
}