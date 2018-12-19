//#define _DEFAULT_SOURCE
//#define _USE_BSD

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
//#include <signal.h>
//#include <machine/byte_order.h>
#include <libkern/OSByteOrder.h>
#include <stdint.h>

#define DATALOOP "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJLKMNOPQURSTVWXYZ"
#define NUM_REPS 16


#define CHILDDATA "Hello"

int main(int argc, char* argv[]){



    //create the data
    char data[992];
    int data_index = 0;
    for(int rep = 0; rep < NUM_REPS; rep++){
        for(int i = 0; i < strlen(DATALOOP);i++){
            data[data_index] = DATALOOP[i];
            data_index++;
        }
    }
    printf("strlen: %d data %d\n",strlen(DATALOOP), strlen(data));
    printf("data_index: %d\n",data_index);
    for(int j = 0; j < 992; j++){
        printf("%c",data[j]);
    }
    printf("\n");
    return 0;

    /*

    int sockets[3][2], rc[3];
    pid_t pid;
    uint32_t buf[3][256];
    int child;
    int isChild = 0;
    int flags;
    uint32_t data[5];

    rc[0] = socketpair(AF_UNIX, SOCK_STREAM, 0, sockets[0]);
    if (rc[0] == -1) {
        //printf("ERROR OPENING SOCKET PAIR = %d\n", sock_errno());
        exit(0);
    }
    rc[1] = socketpair(AF_UNIX, SOCK_STREAM, 0, sockets[1]);
    if (rc[1] == -1) {
        //printf("ERROR OPENING SOCKET PAIR = %d\n", sock_errno());
        exit(0);
    }
    rc[2] = socketpair(AF_UNIX, SOCK_STREAM, 0, sockets[2]);
    if (rc[2] == -1) {
        //printf("ERROR OPENING SOCKET PAIR = %d\n", sock_errno());
        exit(0);
    }

    for(int i = 0; i < 3; i++){
        for(int j = 0; j < 2; j++){
            flags = fcntl(sockets[i][j], F_GETFL);
            flags |= O_NONBLOCK;
            fcntl(sockets[i][j], F_SETFL, flags);
        }
    }

    for(child = 0; child < 3; child++){
        pid = fork();
        if(pid == -1){
            printf("fork error");
            exit(0);
        }
        if(pid == 0){
            isChild = 1;
            break;
        }
        else {
            continue;
        }
    }

    if(isChild){
        printf("I am a child %d\n",child);

        //data[0]=OSSwapHostToBigInt32(1);
        data[0]=htonl(1);
        //data[1]=OSSwapHostToBigInt32(2);
        data[1]=htonl(2);
        //data[2]=OSSwapHostToBigInt32(3);
        data[2]=htonl(3);
        //data[3]=OSSwapHostToBigInt32(4);
        data[3]=htonl(4);
        //data[4]=OSSwapHostToBigInt32(5);
        data[4]=htonl(5);

        close(sockets[child][0]);      // close the parent’s socket
        rc[child] = write(sockets[child][1], data, sizeof(data));
                                    // write to parent
        if (rc[child] == -1){
            //printf("ERROR ON WRITE = %d\n", sock_errno());
        }
        rc[child] = write(sockets[child][1], data, sizeof(data));
                                    // write to parent
        if (rc[child] == -1){
            //printf("ERROR ON WRITE = %d\n", sock_errno());
        }
        rc[child] = write(sockets[child][1], data, sizeof(data));
                                    // write to parent
        if (rc[child] == -1){
            //printf("ERROR ON WRITE = %d\n", sock_errno());
        }
    }
    else{
        printf("I am a parent\n");
        usleep(500000);

        for(int i = 0; i < 3; i++){
            close(sockets[i][1]);
            rc[i] = read(sockets[i][0], buf[i],5*sizeof(uint32_t));
            if(rc[i] == -1){
                perror("\nError on Read\n");
            }

            printf("parent got of size %d,%lu from child %d:\n", rc[i], sizeof(buf[i]), i);
            for(int j = 0; j < rc[i]/sizeof(uint32_t); j++){
                //printf("%d",OSSwapBigToHostInt32(buf[i][j]));
                printf("%d",ntohl(buf[i][j]));
            }
            printf("\n");
            usleep(500000);
            rc[i] = read(sockets[i][0], buf[i],10*sizeof(uint32_t));
            if(rc[i] == -1){
                perror("\nError on Read\n");
            }
            printf("parent got of size %d,%lu from child %d:\n", rc[i], sizeof(buf[i]), i);
            int j,k;
            for(j = 0; j < rc[i]/sizeof(uint32_t); j++){
                //printf("%d",OSSwapBigToHostInt32(buf[i][j]));
                printf("%d",ntohl(buf[i][j]));
            }
            printf("\n");
            usleep(500000);
        }
        wait(NULL);
    }
    */
    return 0;
}
