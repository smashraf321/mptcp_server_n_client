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
#define SERVER_PORT 14085
#define NUM_CHILDREN 3
#define NUM_BYTES 4

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
    int DSS_seq_num, subflow_seq_num, child_num;//should be char??
    //mapping arrays for getting the output in right order
    int mapping_arrays[NUM_CHILDREN][83];
    //trackers for assisting in reconstructing back the output
    int DSS_pointer[NUM_CHILDREN], Read_pointer[NUM_CHILDREN];
    //for storing data received from child temporarily
    char data_buf_parent[NUM_CHILDREN][400], data_buf_ss[NUM_CHILDREN][400];
    //secondary storage fill level
    int dbss_tracker[NUM_CHILDREN];
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
    for(int i = 0; i < NUM_CHILDREN; i++){
        for(int j = 0; j < 2; j++){
            flags = fcntl(socket_pair_ipc[i][j], F_GETFL);
            flags |= O_NONBLOCK;
            fcntl(socket_pair_ipc[i][j], F_SETFL, flags);
        }
    }

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

        //child server binds the required port and IP address to data socket
    	if(bind(data_sock_child[child_num], (struct sockaddr*) &server_child[child_num], sizeof(server_child[child_num])) < 0){
    	 	perror("\nbind() 2 failed\n");
    	 	return -1;
    	}

    	// informs the OS the child server socket is for listening for connections
    	if(listen(data_sock_child[child_num], 100) < 0 ){
    	 	perror("\nlisten() failed\n");
    	 	return -1;
    	}

        while(1){
            printf("\nWaiting for child client to ping me on data socket\n");
            //accepts the child client's connection and stores client's information in client structure
            if((acc_child[child_num] = accept(data_sock_child[child_num], (struct sockaddr*) &client_child[child_num], (socklen_t *) &client_len_child[child_num])) < 0){
            //get the child client's IP address and port from this ^ client structure.
                perror("\naccept failed\n");
                return -1;
            }
            printf("\nAccepted the child client's connection\n");

            //read data from child data socket
            if((n = read(acc_child[child_num], data_buf_child, NUM_BYTES)) < 0){
    			perror("\n read() failed \n");
    			return -1;
    		}
            //for safety
            usleep(1000);

			//Terminating condition
			if(data_buf_child[0] == '*'){
				break;
			}

            //write to parent
            read_from_child[child_num] = write(socket_pair_ipc[child_num][1], data_buf_child, sizeof(data_buf_child));
            if (read_from_child[child_num] == -1){
                perror("\nError on writing to parent\n");
            }
            //for safety
            usleep(1000);
        }

        close(acc_child[child_num]);
		printf("\nChild %d process is done...\n",child_num);

    }else/*-----------------------------------------I AM PARENT----------------------------------------*/{

        printf("\nI am the parent\n");
        //close 3 IPC child sockets
        for(int i = 0; i < NUM_CHILDREN; i++){
            close(socket_pair_ipc[i][1]);
        }

        //prepare the mapping arrays and trackers, i.e. set all of them as -1,0,etc.
        for(int i = 0; i < NUM_CHILDREN; i++){
            DSS_pointer[i] = -1;
            Read_pointer[i] = -1;
            dbss_tracker[i] = 0;
            for(int j = 0; j < 83; j++){
                mapping_arrays[i][j] = -1;
            }
        }

		//initializing final_output full of spaces
		for(int i = 0; i < 992; i++){
			final_output[i] = ' ';
		}

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

        //server binds the required port and IP address to server socket
    	if(bind(control_sock, (struct sockaddr*) &server, sizeof(server)) < 0){
    	 	perror("\nbind() 1 failed\n");
    	 	return -1;
    	}

    	// informs the OS the server socket is for listening for connections
    	if(listen(control_sock, 100) < 0 ){
    	 	perror("\nlisten() failed\n");
    	 	return -1;
    	}

        while(1){
            printf("\nWaiting for client to ping me on control socket\n");
    		//accepts the client's connection and stores client's information in client structure
    		if((acc = accept(control_sock, (struct sockaddr*) &client, (socklen_t *) &client_len)) < 0){
    		//get the client's IP address and port from this ^ client structure.
    			perror("\naccept failed\n");
    			return -1;
    		}
    		printf("\nAccepted the client's connection\n");

            //read client's DSS message
            if((n = read(acc, dss, 3*sizeof(int))) < 0){
                perror("\n read() failed \n");
                return -1;
            }
            //for safety
            usleep(1000);
            //parse DSS from client
            DSS_seq_num = ntohl(dss[0]);
            subflow_seq_num = ntohl(dss[1]);
            child_num = ntohl(dss[2]);

            //update mapping arrays and pointers based from DSS from client
            mapping_arrays[child_num][subflow_seq_num] = DSS_seq_num;
            DSS_pointer[child_num] = subflow_seq_num;

            //get data from all children based on dss pointers and read pointers
            for(int i = 0; i < NUM_CHILDREN; i++){
                int chunks_to_be_read;
                chunks_to_be_read = DSS_pointer[i] - Read_pointer[i];
                //this ensures we read only if there's mapping for the current child's IP
                if(chunks_to_be_read > 0){
                    read_from_child[i] = read(socket_pair_ipc[i][0], data_buf_parent[i], sizeof(data_buf_parent[i]));
                    if(read_from_child[i] == -1){
                        perror("\nError on Read from child\n");
                    }
                    //for safety
                    usleep(1000);
                    //transfer data from data_buf_parent to secondary storage
                    for(int m = 0; m < read_from_child[i]; m++)/*no \r for characters*/{
                    //for(int m = 0; m < (read_from_child[i]/5)*4; m++)/*for tackling \r*/{
                        data_buf_ss[i][dbss_tracker[i]] = data_buf_parent[i][m];
                        dbss_tracker[i]++;
                    }
                    /*
                    The following logic is to read from child IPC socket
                    (that data is now in secondary storage) and put it
                    in right position in final output string
                    */
                    int j;
                    int k = 0;
                    for(j = 1; j <= chunks_to_be_read && j <= dbss_tracker[i]/NUM_BYTES; j++){
                        int l = 0;
                        while((k % 4) || !k){
                            //add it to final string
                            final_output[mapping_arrays[i][Read_pointer[i]+j] + l] = data_buf_ss[i][k];
                            k++;
                            l++;
                        }
                    }
                    //pushback the unread elements to the beginning of secondary storage
                    int o = 0;
                    for(int m = k; m < dbss_tracker[i]; m++)/*(k should be = chunks_to_be_read * NUM_BYTES)*/{
                        data_buf_ss[i][o] = data_buf_ss[i][m];
                        o++;
                    }
                    //update fill level of secondary storage
                    dbss_tracker[i] = o;
                    //update read pointer, shows you've successfully put these bytes in output string
                    Read_pointer[i] += j;
                }

            }

			//Terminating condition
			if(DSS_seq_num == subflow_seq_num && subflow_seq_num == child_num && child_num == 1){
				break;
			}
        }
        close(acc);

		//print out the output string
		printf("\nFinal Output String is:\n");
		for(int i = 0; i < 992; i++){
			if(final_output[i]<10){
				printf("%hhd\n",final_output[i]);
			}else{
				printf("%c\n",final_output[i]);
			}
		}

        //prevent zombie children
        wait(NULL);
    }
    return 0;
}
