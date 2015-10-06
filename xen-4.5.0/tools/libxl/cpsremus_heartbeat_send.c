#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define PERCENTAGE 50 // percentage of timeout to be used as period for heartbeat signal
#define UNITS 1000 // use units of 1000 microseconds for period

int main(int argc, char *argv[]) {

    unsigned int timeout;
    unsigned int period;

    if (argc != 2) {
        fprintf(stderr, "error. sender must be started with an interval specification.\n");
        return -1;
    }

    timeout = atoi(argv[1]);
    period = timeout*PERCENTAGE/100;

    while(1) {
        fprintf(stdout, "h\n");
        fflush(stdout);
        usleep(timeout*UNITS);
    }
    return -1;
}
