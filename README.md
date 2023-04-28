# file_server
A Server, Mirror and Client in C which works by communicating with Sockets. The server and mirror provide load balancing so that the first 4 clients are served by Server, the next 4 clients by the Mirror and clients after that are served alternatively by the Server and Mirror respectively.
