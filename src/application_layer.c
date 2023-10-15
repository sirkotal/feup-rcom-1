// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

LinkLayer parameters;

unsigned char* createControlPacket(unsigned int ctrl, const char* filename, unsigned int* size) {

}

void applicationLayer(const char *serialPort, const char *role, int baudRate, int nTries, int timeout, const char *filename) {
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

    int port = llopen(parameters);

    if (port < 0) {
        perror(serialPort);
        exit(-1);
    }

    if (parameters.role == LlRx) {
        // stuff to do
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
        // TODO

        unsigned char *starterControlPacket = createControlPacket(start, fptr, &packet_size);

    }
    else {
        perror("Unidentified Role\n");
        exit(-1);
    }
}
