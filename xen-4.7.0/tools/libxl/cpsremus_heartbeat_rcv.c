#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

#define UNITS 1000 // unit is milliseconds

char** parse_pids(char *pids);

char** parse_pids(char *pids) {
    char **ret;
    
    fprintf(stderr,"Entered parsing function\n");
    ret = (char**)malloc(sizeof(char*)*2);
    ret[0] = pids;  

    for (;*pids != '#' && *pids != '\0'; pids++)
      fprintf(stderr, "%c",*pids);
    *pids = '\0';
    ret[1] = pids+1;
    
    fprintf(stderr,"Leaving parsing function\n");
    return ret;
} 

int main(int argc, char** argv)
{
    char hb;

    pid_t remus_pid;
    pid_t migrate_rcv_pid;
    fd_set rdfs;
    struct timeval tv;
    int ret;
    int bytes_read = 0;
    int i = 0;
    struct stat st_info;
    char pids[13];
    int fifo_fd;
    char **pidss;
    int timeout;

    if (argc != 2) {
        fprintf(stderr, "Not enough arguments");
        return -1;
    }

    timeout = (pid_t)atoi(argv[1]);   

    if (stat("/tmp/cpsremus_fifo", &st_info)) {
        if (errno == ENOENT) {
            mkfifo("/tmp/cpsremus_fifo", 0666);
        }
    }
   
    fprintf(stderr, "Opening fifo for reading.\n"); 
    fifo_fd = open("/tmp/cpsremus_fifo", O_RDONLY);
    
    fprintf(stderr, "Reading pids from fifo\n");
    bytes_read = read(fifo_fd, pids, 12);

    close(fifo_fd);

    fprintf(stderr, "Content of pids is: %s after %i bytes read.\n", pids, bytes_read);
    
    pidss = parse_pids(pids);
    fprintf(stderr, "pidss[0] = %s\n",pidss[0]);
    fprintf(stderr, "pidss[1] = %s\n",pidss[1]);
    fflush(stderr);
    remus_pid = atoi(pidss[0]);
    migrate_rcv_pid = atoi(pidss[1]);

    free(pidss);
    fprintf(stderr, "Pid of save-helper is %i\n",remus_pid);

    do {
        FD_ZERO(&rdfs);
        FD_SET(0, &rdfs);

        tv.tv_sec = timeout/UNITS;
        tv.tv_usec = timeout%UNITS*1000;
        ret = select(1, &rdfs, NULL, NULL, &tv);
        
        if (!ret) {
            fprintf(stderr, "No heartbeat from primary within 2 seconds. Failover.\n");
            kill(remus_pid, SIGUSR1);
            return 1;
        }

        printf("%i",i);
        if (FD_ISSET(0, &rdfs)) {
            bytes_read = read(0, &hb, 1);
            printf("%c",hb);
        }
        i++;
    } while ( bytes_read > 0 );
    
    fprintf(stderr, "No heartbeat from primary within 2 seconds. Failover.\n");
    kill(remus_pid, SIGTERM);

    return 1;
}   
