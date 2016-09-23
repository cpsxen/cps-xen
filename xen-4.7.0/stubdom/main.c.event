#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <mini-os/xenbus.h>

#define BILLION 1E9
#define BUFLEN 512
#define NPACK 10
#define PORT 9935
#define MILLION 1E9
#define PAGE_SIZE 4096

void serror(char *s)
{
    perror(s);
    exit(1);
}

int main (int argc, char *argv[])
{
    int iter = 1000000; 
    char buf[BUFLEN];
    unsigned cycles_low, cycles_high, cycles_low1, cycles_high1; 
    unsigned long start, end;
    struct sockaddr_in si_me, si_other; 
    int s, slen=sizeof(si_other);
    xenbus_transaction_t xbt;
    int retry = 0;
    char path[256];
    char *err;

    printf("Latency Benchmark v.1\n");
    printf("Workload: %d iterations. Waiting for packets. \n", iter);  

    if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
      serror("socket");

    memset((char *) &si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(PORT);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, &si_me, sizeof(si_me))==-1)
        perror("bind");

    snprintf(path, sizeof(path), "data");
    err = xenbus_transaction_start(&xbt);
    if (err) {
        printf("starting transaction\n");
        free(err);
    }

    err = xenbus_printf(xbt, path, "ha","%d",0);
    if (err) {
        printf("writing ha node\n");
        free(err);
    }
    err = xenbus_transaction_end(xbt, 0, &retry); 

    while(1) {
	volatile int i=0;
        if (recvfrom(s, buf, BUFLEN, 0, &si_other, &slen)==-1)
        serror("recvfrom()");

	/* Warm up caches */
	asm volatile ("CPUID\n\t"
	"RDTSC\n\t"
	"mov %%edx, %0\n\t"
	"mov %%eax, %1\n\t": "=r" (cycles_high), "=r" (cycles_low):: "%rax", "%rbx", "%rcx", "%rdx");
	asm volatile ("RDTSCP\n\t"
	"mov %%edx, %0\n\t"
	"mov %%eax, %1\n\t"
	"CPUID\n\t": "=r" (cycles_high1), "=r" (cycles_low1):: "%rax", "%rbx", "%rcx", "%rdx");
	asm volatile ("CPUID\n\t"
	"RDTSC\n\t"
	"mov %%edx, %0\n\t"
	"mov %%eax, %1\n\t": "=r" (cycles_high), "=r" (cycles_low):: "%rax", "%rbx", "%rcx", "%rdx");
	asm volatile ("RDTSCP\n\t"
	"mov %%edx, %0\n\t"
	"mov %%eax, %1\n\t"
	"CPUID\n\t": "=r" (cycles_high1), "=r" (cycles_low1):: "%rax", "%rbx", "%rcx", "%rdx");

	asm volatile ("CPUID\n\t"
	"RDTSC\n\t"
	"mov %%edx, %0\n\t"
	"mov %%eax, %1\n\t": "=r" (cycles_high), "=r" (cycles_low):: "%rax", "%rbx", "%rcx", "%rdx");

	/*call the function to measure here*/
	while(i<iter){
	i++;
	}

	asm volatile ("RDTSCP\n\t"
	"mov %%edx, %0\n\t"
	"mov %%eax, %1\n\t"
	"CPUID\n\t": "=r" (cycles_high1), "=r" (cycles_low1):: "%rax", "%rbx", "%rcx", "%rdx");

	start = ( ((unsigned long)cycles_high << 32) | cycles_low );
 	end = ( ((unsigned long)cycles_high1 << 32) | cycles_low1 );

	sprintf(buf, "%s %ld\0", buf, end-start);
        if(sendto(s,buf,BUFLEN, 0, &si_other, slen)==-1) serror("sendto");

    err = xenbus_transaction_start(&xbt);
    if (err) {
        printf("starting transaction for cpsremus\n");
        free(err);
    }

    err = xenbus_printf(xbt, path, "ha", "%s", buf);
    if (err) {
        printf("writing to ha\n");
        free(err);
    }

    err = xenbus_transaction_end(xbt, 0, &retry);
    free(err);

    }
    close(s);
    exit(0);
}
