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
#include <signal.h>

#define CHILDDATA "HIII321"
#define SERVER_PORT 14100
#define NUM_CHILDREN 3
#define NUM_BYTES 4
#define DATALOOP "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJLKMNOPQURSTVWXYZ"
#define NUM_REPS 16
#define TERM_STR "****"
#define DEBUG_LOCAL 0

int main(int argc, char* argv[])
{
    //--------------------------------------VARIABLE DECLARATIONS-----------------------------------//
    //socket pairs and descriptors for IPC
    int socket_pair_ipc[3][2], read_from_child[3];
    //support variable for when we fork()
    pid_t pid;

    //for temporary testing purposes
    char buf[3][256];

    //support variables
    int isChild, opt, flags, n;
    //for storing data received from client temporarily
    char data_buf_child[4];
    //socket identifier for the control socket to which the server is bound
    int control_sock;
    //socket identifier for the data socket to which the child server is bound
    int data_sock_child[NUM_CHILDREN];
    //identifier for accepting connection from client when he pings the server
    int acc;
    //identifier for accepting connection from child client when he pings the server child
    int acc_child[NUM_CHILDREN];
    //socket structures for storing info on server and client(when he pings server)
    struct sockaddr_in server, client;
    //socket structures for storing info on server and client(when he pings server) for child processes
    struct sockaddr_in server_child[NUM_CHILDREN], client_child[NUM_CHILDREN];
    //storing length of client structure
    int client_len;
    //storing length of client structure for child processes
    int client_len_child[NUM_CHILDREN];
    //structure for storing socket structure informaqtion when IP address/name of server is entered from command line
    struct hostent* cmd_addr;
    //for storing DSS from client
    int dss[3];//char dss[3];
    //for storing parsed values from dss;
    int DSS_seq_num=0, DSS_byte_num, subflow_seq_num, child_num;//should be char??
    //mapping arrays for getting the output in right order
    int mapping_arrays[NUM_CHILDREN][83];
    //trackers for assisting in reconstructing back the output
    int DSS_pointer[NUM_CHILDREN], Read_pointer[NUM_CHILDREN];
    //for storing data received from child temporarily
    char data_buf_parent[NUM_CHILDREN][400];
    //final output string
    char final_output[992];

    //--------------------------------------INITIAL SERVER SOCKET SETUP-----------------------------------//
    //if IP address not entered from command line
    if(argc == 1){
		fprintf(stderr,"\n Please enter the IP address or host name of the server, i.e the PC you're working on right now \n");
		return -1;
	}

    //IP address of server taken from command line
    cmd_addr = gethostbyname(argv[1]);

    //assigning IP address from command line to server socket structure
	if(!(cmd_addr == NULL)){
	 	printf("\n host name/ip address is: %s \n",inet_ntoa(*((struct in_addr*) cmd_addr -> h_addr_list[0])));
        /*//IP address of server is stored in server socket structure
	 	server_send.sin_addr = *((struct in_addr*) (cmd_addr -> h_addr_list[0]));*/
 	}else{
	 	perror("\n Failed to retrieve IP Address \n");
		return -1;
	}

    //storing length of client structure
    client_len = sizeof(client);
    for(int i = 0; i < NUM_CHILDREN; i++){
        client_len_child[i] = sizeof(client_child[i]);
    }

    //-------------------------------IPC SOCKETS CREATION--------------------------------------//
    //creating 3 sockets for IPC i.e. communicating with child processes
    for(int i = 0; i < NUM_CHILDREN; i++){
        read_from_child[i] = socketpair(AF_UNIX, SOCK_STREAM, 0, socket_pair_ipc[i]);
        if(read_from_child[i] == -1){
            perror("\nError opening socket pair\n");
            exit(0);
        }
    }

    //set all 3 pairs of sockets as non blocking sockets
    // for(int i = 0; i < NUM_CHILDREN; i++){
    //     for(int j = 0; j < 2; j++){
    //         flags = fcntl(socket_pair_ipc[i][j], F_GETFL);
    //         flags |= O_NONBLOCK;
    //         fcntl(socket_pair_ipc[i][j], F_SETFL, flags);
    //     }
    // }

    //-----------------------------------------------------------------------------------------//
    //-----------------------------------MPTCP CORE LOGIC--------------------------------------//
    //-----------------------------------------------------------------------------------------//

    //-------------------------------CREATING 3 CHILD PROCESSES--------------------------------//
    isChild = 0;
    for(child_num = 0; child_num < NUM_CHILDREN; child_num++){
        pid = fork();
        if(pid == -1){
            perror("\nfork error\n");
            exit(0);
        }
        if(pid == 0){
            isChild = 1;
            break;
        }else{
            continue;
        }
    }

    //------------------------------PARENT VS CHILD DIFFERENTIATION-----------------------------//
    if(isChild){
        printf("\nI am a child %d\n",child_num);
        //close the parent’s socket
        close(socket_pair_ipc[child_num][0]);

        //assigning values for child server socket structure
        server_child[child_num].sin_family = AF_INET;
        server_child[child_num].sin_port = htons(SERVER_PORT + child_num + 1);
        server_child[child_num].sin_addr = *((struct in_addr*) (cmd_addr -> h_addr_list[0]));

        //data socket of child server is now set up
    	data_sock_child[child_num] = socket(AF_INET, SOCK_STREAM, 0);
    	if(data_sock_child[child_num] < 0){
    	 	perror("\nsocket() failed\n");
    	 	return -1;
    	}

        //for reusing child server socket without an issue
    	setsockopt(data_sock_child[child_num], SOL_SOCKET,SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    	/*
        //child server binds the required port and IP address to data socket
    	if(bind(data_sock_child[child_num], (struct sockaddr*) &server_child[child_num], sizeof(server_child[child_num])) < 0){
    	 	perror("\nbind() 2 failed\n");
    	 	return -1;
    	}
    	*/
    	if (connect(data_sock_child[child_num], (struct sockaddr *)&server_child[child_num], sizeof(server_child[child_num])) < 0) 
	    { 
	        printf("\nChild %d connection Failed \n",child_num); 
	        return -1; 
	    } 

	    
        while(1){
            

            //write to parent
            read_from_child[child_num] = read(socket_pair_ipc[child_num][1], data_buf_child, sizeof(data_buf_child));
            if (read_from_child[child_num] == -1){
                perror("\nError on writing to parent\n");
            }
            //for safety
            usleep(1000);

            if(read_from_child[child_num] % 4 != 0){
            	printf("mismatch in sublfow data: %*.s\n",read_from_child[child_num],data_buf_child);
            }

            printf("child %d writing to server: %.*s\n",child_num,(int)sizeof(data_buf_child),data_buf_child);
            //read data from child data socket
            if((n = write(data_sock_child[child_num], data_buf_child, read_from_child[child_num])) < read_from_child[child_num]){
    			perror("\n read() failed \n");
    			return -1;
    		}
            //for safety
            usleep(1000);

			//Terminating condition
			if(data_buf_child[0] == '*'){
				break;
			}
        }
		

        close(data_sock_child[child_num]);
		printf("\nChild %d process is done...\n",child_num);

    }else/*-----------------------------------------I AM PARENT----------------------------------------*/{

        printf("\nI am the parent\n");
        //close 3 IPC child sockets
        for(int i = 0; i < NUM_CHILDREN; i++){
            close(socket_pair_ipc[i][1]);
        }

        //create the data
		char data[992];
		int data_index = 0;
		for(int rep = 0; rep < NUM_REPS; rep++){
		    for(int i = 0; i < strlen(DATALOOP);i++){
		        data[data_index] = DATALOOP[i];
		        data_index++;
		    }
		}

		printf("data: %.*s\n",992,data);

        //assigning values for server socket structure
        server.sin_family = AF_INET;
        server.sin_port = htons(SERVER_PORT);
	 	server.sin_addr = *((struct in_addr*) (cmd_addr -> h_addr_list[0]));

        // setting up control socket of server
    	control_sock = socket(AF_INET, SOCK_STREAM, 0);
    	if(control_sock < 0){
    	 	perror("\nsocket() failed\n");
    	 	return -1;
    	}

        //for reusing server socket without an issue
    	setsockopt(control_sock,SOL_SOCKET,SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    	if (connect(control_sock, (struct sockaddr *)&server, sizeof(server)) < 0) 
	    { 
	        printf("\nParent Connection Failed \n"); 
	        return -1; 
	    } 



        while(1){

        	//create dss
        	DSS_byte_num = DSS_seq_num*4;
        	subflow_seq_num = DSS_seq_num/3;
        	child_num = DSS_seq_num%3;

        	dss[0] = htonl(DSS_byte_num);
        	dss[1] = htonl(subflow_seq_num);
        	dss[2] = htonl(child_num);

            //write client's DSS message
            if((n = write(control_sock, dss, sizeof(dss))) < 0){
                perror("\n read() failed \n");
                return -1;
            }

            
            //for safety
            usleep(1000);
            
             
            //copy data into buffer
            for(int i = 0; i < sizeof(data_buf_child);i++){
            	data_buf_child[i] = data[DSS_byte_num + i];
            }
            //send buffer to child
            printf("parent writing to child %d: %.*s\n",child_num,(int)sizeof(data_buf_child),data_buf_child);
            read_from_child[child_num] = write(socket_pair_ipc[child_num][0], data_buf_child, sizeof(data_buf_child));
            
            printf("DSS_seq_num: %d DSS_byte_num: %d\n",DSS_seq_num,DSS_byte_num);


            DSS_seq_num ++;//increment DSS seq number after we send over ctrl connection
            
			//Terminating condition
			if(DSS_seq_num == 248){
				printf("exiting\n");
				//copy data into buffer
	            for(int i = 0; i < sizeof(TERM_STR);i++){
	            	data_buf_child[i] = TERM_STR[i];
	            }
				//send terminating conditions to children
				for(child_num = 0; child_num < 3; child_num++){
					read_from_child[child_num] = write(socket_pair_ipc[child_num][0], data_buf_child, sizeof(data_buf_child));
				}
				//for safety
            	usleep(1000);
				//send terminating condition to the server
            	//create dss
	        	DSS_byte_num = 1;
	        	subflow_seq_num = 1;
	        	child_num = 1;

	        	dss[0] = htonl(DSS_byte_num);
	        	dss[1] = htonl(subflow_seq_num);
	        	dss[2] = htonl(child_num);

				if((n = write(control_sock, dss, sizeof(dss))) < 0){
                	perror("\n read() failed \n");
                	return -1;
            	}
				break;
			}
        }
        close(acc);

		// //print out the output string
		// printf("\nFinal Output String is:\n");
		// for(int i = 0; i < 992; i++){
		// 	if(final_output[i]<10){
		// 		printf("%hhd\n",final_output[i]);
		// 	}else{
		// 		printf("%c\n",final_output[i]);
		// 	}
		// }

        //prevent zombie children
        wait(NULL);
    }
    return 0;
}
