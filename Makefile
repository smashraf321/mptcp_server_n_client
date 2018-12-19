default: client_mptcp.o server_mptcp_v1.o
	gcc -o client client_mptcp.o
	gcc -o server server_mptcp_v1.o

clean: 
	rm client server client_mptcp.o server_mptcp_v1.o
