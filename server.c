//NOTE: Update the MIRROR_IP and MIRROR_PORT with your current mirror IP and port 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <pthread.h>
#include <sys/types.h>
#include <fcntl.h>
#include <netdb.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAXTOKENS 8
#define BUFFER_SIZE 1024
#define MAX_FILE_SIZE 999999
#define MIRROR_IP "{mirror's ip}" //Update the mirror IP and port before running
#define MIRROR_PORT "{mirror's portno}"
int CLIENT_NO=0; //Total No. of Clients
int SERVER_CLIENTS=0; //Clients connected to the server only
int FILE_NOT_FOUND=0;

void error(const char *msg) {
    perror(msg);
    exit(1);
}

void quit_handler(int sig){
    //Decrement Clients every time a process dies i.e if connection closes
    SERVER_CLIENTS--; //Decrement count of clients
    CLIENT_NO--;
    printf("No. of Clients Connected: %d\n", CLIENT_NO);
}

void send_temp_tar(int client_socket){
    //Open the archive file and send it to the client
    int file = open("temp.tar.gz",O_RDONLY);
    if (file == -1) {
        perror("Error opening file\n");
        exit(1);
    }
    char * buffer[BUFFER_SIZE];

    int nread;
    while ((nread = read(file, buffer, BUFFER_SIZE)) > 0) {
        if (send(client_socket, buffer, nread, 0) < 0) {
            perror("Error sending file\n");
            exit(EXIT_FAILURE);
        }
    }
}
char * findfile(const char *dir_path, const char* filename){
    DIR* dir = opendir(dir_path);
    if (dir == NULL) {
        // Failed to open directory
        return NULL;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            // Skip current directory (".") and parent directory ("..")
            continue;
        }
        
        char entry_path[1024];
        snprintf(entry_path, sizeof(entry_path), "%s/%s", dir_path, entry->d_name);
        
        if (entry->d_type == DT_DIR) {
            // Recurse into subdirectory
            char* result = findfile(entry_path, filename);
            if (result != NULL) {
                // Found file in subdirectory
                closedir(dir);
                return result;
            }
        } else if (entry->d_type == DT_REG && strcmp(entry->d_name, filename) == 0) {
            // Found target file
            char* file_path = (char*) malloc(strlen(entry_path) + 1);
            strcpy(file_path, entry_path);
            closedir(dir);
            return file_path;
        }
    }
    
    closedir(dir);
    return NULL;
}
void gettargz(char* extensions[6], int num_ext ,int client_socket) {
    char* home_dir = getenv("HOME");
    if (!home_dir) {
        perror("Error: could not find home directory\n");
        return;
    }

    DIR* dir = opendir(home_dir);
    if (!dir) {
        perror("Error: could not open home directory\n");
        return;
    }

    if (num_ext == 0) {
        FILE_NOT_FOUND=1;
        return;
    }

    char cmd[1024];
    snprintf(cmd, 1024, "tar czf temp.tar.gz");

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        char path[1024];
        snprintf(path, 1024, "%s/%s", home_dir, entry->d_name);
        //get stat for each entry
        struct stat st;
        if (stat(path, &st) == -1) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            continue;
        }
        //match extensions
        int ext_match = 0;
        for (int i = 0; i < num_ext; i++) {
            if (strstr(entry->d_name, extensions[i]) != NULL) {
                ext_match = 1;
                break;
            }
        }

        if (!ext_match) {
            continue;
        }

        strncat(cmd, " ", 1024 - strlen(cmd) - 1);
        strncat(cmd, path, 1024 - strlen(cmd) - 1);
    }

    closedir(dir);
    //If no files found
    if (strlen(cmd) == strlen("tar czf temp.tar.gz")) {
        FILE_NOT_FOUND=1;
        return;
    }
    //run the command
    system(cmd);

}

void getfiles(char * files[6], int num_files, int client_socket){
    char cmd[1000];
    int i, found = 0;

    // Check if any of the files exist in the directory tree
    for (i = 0; i < num_files; i++) {
        sprintf(cmd, "find ~ -type f -name '%s' | grep -q .", files[i]);
        if (system(cmd) == 0) {
            found = 1;
            break;
        }
    }

    if (found) {
        // Construct the find command to locate files with the specified names
        sprintf(cmd, "find ~ -type f -name '%s'", files[i]);

        // Execute the command to create the tar archive containing the files
        sprintf(cmd + strlen(cmd), " | xargs tar czf temp.tar.gz");
        system(cmd);

        send_temp_tar(client_socket);
    } else {
        FILE_NOT_FOUND=1;
    }
}

void dgetfiles(char * date1, char * date2, int client_socket){
    char cmd[1000];
    time_t t1, t2;

    // Convert date1 and date2 strings to time_t values
    struct tm tm1 = {0};
    strptime(date1, "%Y-%m-%d", &tm1);
    t1 = mktime(&tm1);
    struct tm tm2 = {0};
    strptime(date2, "%Y-%m-%d", &tm2);
    t2 = mktime(&tm2);

    // Construct the find command to locate files in the date range
    sprintf(cmd, "find ~ -type f -newermt '%s' ! -newermt '%s' -print0 | xargs -0 tar czf temp.tar.gz", date1, date2);

    // Execute the command to create the tar archive
    system(cmd);
    send_temp_tar(client_socket);

}

void sgetfiles(long long size1, long long size2, int client_socket) {
    char cmd[1000];

    // Construct the find command to locate files in the size range
    sprintf(cmd, "find ~ -type f -size +%lldc -size -%lldc -print0 | xargs -0 tar czf temp.tar.gz", size1, size2);

    // Execute the command to create the tar archive
    system(cmd);
    send_temp_tar(client_socket);
}

void process_command(char * cmd, int client_socket){
    //Process each command
    FILE_NOT_FOUND=0;
    char * tokens[MAXTOKENS];
    int count=0;
    char* token = strtok(cmd, " ");
    //tokenize the input
    while (token != NULL) {
        tokens[count++] = token;
        token = strtok(NULL, " ");
    }
    tokens[count] = NULL;

    //Remove new lines
    if (cmd[strlen(cmd) - 1] == '\n')
        cmd[strlen(cmd) - 1] = '\0';
    //Run according to each command
    if(strcmp(tokens[0],"findfile") == 0){
        char *dir_path = getenv("HOME");
        //Get the filepath recursively
        char *file_path = findfile(dir_path,tokens[1]);
        //Get the stats of the file
        struct stat file_info;
        int exists = stat(file_path, &file_info);
        
        if (exists == 0) {
            // file exists, send its information to client
            char response[256];
            sprintf(response, "Filename: %s\nMAX_BUFFER_SIZE: %llu bytes\nCreated: %s\n", tokens[1], file_info.st_size, ctime(&file_info.st_ctime));
            int n = write(client_socket, response, strlen(response));
            if (n < 0) {
                perror("ERROR writing to socket\n");
            }
        }
        else {
           FILE_NOT_FOUND=1;
        }
    }
    else if(strcmp(tokens[0],"sgetfiles") == 0){
        char *eptr;
        //convert string sizes to long long
        long long size1 = strtoll(tokens[1],&eptr,10);
        long long size2 = strtoll(tokens[2],&eptr,10);
        sgetfiles(size1, size2, client_socket);
    }
    else if(strcmp(tokens[0],"dgetfiles") == 0){
        //Call dgetfiles
        dgetfiles(tokens[1], tokens[2], client_socket);
    }
    else if(strcmp(tokens[0],"getfiles") == 0){
        //count the number of files
        int num_files=0;
        //check if the -u was set
        if(strcmp(tokens[count-1], "-u") == 0){
            num_files=count-2;
        }
        else{
            num_files=count-1;
        }
        //get files
        char * files[6];
        for(int i=0;i<num_files;i++){
            files[i] = tokens[i+1];
        }
        //Call getfiles
        getfiles(files, num_files, client_socket);
    }
    else if(strcmp(tokens[0],"gettargz") == 0){
        //get count of args
        int num_args=0;
        //check for the -u flag
        if(strcmp(tokens[count-1], "-u")==0){
            num_args=count-2;
        }
        else{
            num_args=count-1;
        }
        //get all the args
        char * args[6];
        for(int i=0;i<num_args;i++){
            args[num_args] = tokens[i+1];
        }
        //call gettargz
        gettargz(args, num_args, client_socket);
    }
    else{
        //Terminate 
        error("Unknown command");
    }
    //Check if the file was not found for this command
    if(FILE_NOT_FOUND==1){
        send(client_socket,"File Not Found",14,0);
    }
}

void processclient(int client_socket) {
    //send server handshake complete message
    char buffer[BUFFER_SIZE];
    send(client_socket, "1", 1,0);
    bzero(buffer, BUFFER_SIZE);
    while (1) {
        // Wait for client to send a command
        int bytes_read = read(client_socket, buffer, BUFFER_SIZE -1);
        if (bytes_read <= 0) {
            break; // Connection closed or error
        }
        //Check for quit message
        if (strcmp(buffer, "quit") == 0){
            send(client_socket,"quit",4,0);
            //Break after sending quit message
            break;
        }
        else{

            process_command(buffer, client_socket); 
        }
        bzero(buffer, BUFFER_SIZE);
    }
    close(client_socket);
}

void mirror_process(int client_socket){
    //Send Mirror redirect message
    char buffer[] = "Mirror: {mirror's ip} {mirror's portno}";
    //Send the mirror address to client for mirror connection
    send(client_socket, buffer, 25, 0);
    char buffer1[BUFFER_SIZE];
    int n=read(client_socket, buffer1, BUFFER_SIZE -1);
    printf("%s\n",buffer1);
    close(client_socket);
}

void * mirror_sync(){
    //Connect to the Mirror to sync the Client Count
    int sockD = socket(AF_INET, SOCK_STREAM, 0);
    int portno;
    struct sockaddr_in servAddr;
    struct hostent *server;
    servAddr.sin_family = AF_INET; //Internet 
    server = gethostbyname(MIRROR_IP);
    sscanf(MIRROR_PORT, "%d", &portno);
    bcopy((char *)server->h_addr, (char *)&servAddr.sin_addr.s_addr, server->h_length);
    servAddr.sin_port = htons(portno); 
    int connectStatus = connect(sockD, (struct sockaddr*)&servAddr, sizeof(servAddr));
    if (connectStatus < 0) {
        error("ERROR opening socket");
    }
    while(1){
        char buffer[BUFFER_SIZE];
        //Read client count from mirror
        int n = read(sockD, buffer, BUFFER_SIZE -1);
        if(n>0){
            int mirror_clients = atoi(buffer);
            //Update the total client no by adding the mirror clients and server clients
            CLIENT_NO = SERVER_CLIENTS + mirror_clients; 
            printf("Client updated from Mirror: %d\n", CLIENT_NO);
        }
        else{
            break; //Connection closed
        }
    }
    close(sockD);
    pthread_exit(0);
}

int main(int argc, char const* argv[])
{   
    //Create a thread to keep in sync with mirror clients
    pthread_t th;
    pthread_create(&th, NULL, &mirror_sync, NULL);

    int portno;
    int opt = 1;
    signal(SIGCHLD,quit_handler);
    // create server socket 
    int servSockD = socket(AF_INET, SOCK_STREAM, 0);
    if(servSockD < 0){
        error("Could not create socket");
    }
    // Set socket options to reuse address and port
    if (setsockopt(servSockD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        error("setsockopt failed");
    }
    struct sockaddr_in servAddr, cliAddr;
    // define server address
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = INADDR_ANY;
    if(argc != 2){
        printf("Usage: ./server portno");
        exit(1);
    }
    sscanf(argv[1], "%d", &portno);
    servAddr.sin_port = htons((uint16_t)portno);//Host to network short- byte order of the port number 

    // bind socket to the specified IP and port
    if(bind(servSockD, (struct sockaddr*)&servAddr, sizeof(servAddr)) < 0){
        error("Bind failed");
    }
    printf("Listening on %d\n",portno);

    // listen for connections
    if (listen(servSockD, 5) < 0) { //Max 5 connections in buffer.
        error("listen failed");
    }
    

    while(1)
    {   
        int client_socket;
        // Accept incoming connection
        if ((client_socket = accept(servSockD, (struct sockaddr *)&cliAddr, (socklen_t*)&cliAddr)) < 0) {
            perror("accept failed");
            continue;
        }
        //Condition to handle main server or mirror
        if(CLIENT_NO < 4 || (CLIENT_NO >= 8 && CLIENT_NO%2 == 0)){
            // Fork child process to handle client request
            pid_t pid = fork();
            if (pid == -1) {
                close(client_socket);
                perror("fork failed");
                continue;
            } else if (pid == 0) {
                // Child process
                close(servSockD); // Close unused server socket
                processclient(client_socket);
                exit(EXIT_SUCCESS);
            } else {
                SERVER_CLIENTS++;
                CLIENT_NO++; //Increment count of clients.
                printf("No. of Clients Connected: %d\n", CLIENT_NO);
                // Parent process
                close(client_socket); // Close unused client socket
            }
        }
        else{
            //Mirror
            pid_t pid = fork();
            if (pid == -1) {
                close(client_socket);
                perror("fork failed");
                continue;
            } else if (pid == 0) {
                // Child process
                close(servSockD); // Close unused server socket
                mirror_process(client_socket);
                exit(EXIT_SUCCESS);
            } else {
                SERVER_CLIENTS++;
                CLIENT_NO++; //Increment count of clients by 1 for mirror which is then immediately decremented.
                printf("No. of Clients Connected: %d\n", CLIENT_NO);
                // Parent process
                close(client_socket); // Close unused client socket
            }
        }
        
    }
    close(servSockD);
    return 0;
}


