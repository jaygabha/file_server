# file_server
A Server, Mirror and Client in C which works by communicating with Sockets and has capabilities to run on different machines. The server and mirror provide load balancing so that the first 4 clients are served by Server, the next 4 clients by the Mirror and clients after that are served alternatively by the Server and Mirror respectively.

Update the mirror's Ip and portno at the line no. 20,21 and 336 in Server.c

Setup:
Compile all the files:
1. gcc -o server.c server -pthread
2. gcc -o mirror.c mirror
3. gcc -o client.c client

Usage:
First run the mirror with a port on a machine:
./mirror portno
Then run the server with a portno on another machine:
./server portno
The last run the clients on client machines and provide the Server's Ip address and port number.
./client server_ip server_portno
