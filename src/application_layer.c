// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"

#include <string.h>
#include <stdio.h>

LinkLayer parameters;

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
        // more stuff to do
    }
    else {
        perror("Unidentified Role");
        exit(-1);
    }
}
