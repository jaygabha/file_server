
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
#include <netdb.h>
#include <ctype.h>

#define MAX_BUFFER_SIZE 1024
#define MAX_TOKENS 10
int UNZIP = 0;
int TAR = 0;

void error(const char *msg) {
    perror(msg);
    exit(1);
}
void recv_tar_file(int sockfd) {
    char buf[MAX_BUFFER_SIZE];
    ssize_t recv_len;
    FILE *fp;
    //Open the temp.tar.gz file
    fp = fopen("temp.tar.gz", "wb");
    if (fp == NULL) {
        printf("fopen() error");
    }
    //write on the file the recieved data
    while ((recv_len = recv(sockfd, buf, MAX_BUFFER_SIZE, 0)) > 0) {
        if (fwrite(buf, sizeof(char), recv_len, fp) != recv_len) {
            printf("fwrite() error");
        }
    }

    if (recv_len < 0) {
        printf("recv() error");
    }

    fclose(fp);
    printf("File recieved successully\n");
}

int parse_command(char *command){

    char *token;
    int num_tokens = 0;
    char *tokens[MAX_TOKENS];
    token = strtok(command, " \t\n");
    //tokenize input
    while (token != NULL) {
        tokens[num_tokens] = token;
        num_tokens++;
        token = strtok(NULL, " \t\n");
    }

    tokens[num_tokens] = NULL;
    
    if (num_tokens == 0) {
        printf("Error: empty command\n");
    } 
    else if (strcmp(tokens[0], "\n") == 0) {
        printf("Error: empty command\n");
    } 

    if(strcmp(tokens[num_tokens-1],"-u") == 0){
        UNZIP = 1;
    }

    if (strcmp(tokens[0], "findfile") == 0) {
        if (num_tokens < 2 || num_tokens > 2) {
            printf("Error: invalid syntax for %s command.\nExpected syntax: %s filename\n",tokens[0],tokens[0]);
            return 1;
        }  
        else{
            return 0;
        }
    }

    else if (strcmp(tokens[0], "sgetfiles") == 0) {
        TAR = 1;
        off_t size1;
        off_t size2;

        if (num_tokens < 3 || num_tokens > 4) {
            printf("Error: invalid syntax for %s command.\nExpected syntax: %s size1 size2 <-u>\n",tokens[0],tokens[0]);
            return 1;
        }
        // Check if size1 and size2 is a number
        size1 = strtoll(tokens[1], NULL,10);
        size2 = strtoll(tokens[2], NULL,10);
        if(size1 && size2){
            if (size1 <= size2 && size1 >= 0 && size2 >= 0) {
                return 0;
            }  
            else{
                printf("Invalid size range: %lld %lld\n", size1, size2);
                return 1;
            }
        }
        else{
            printf("Invalid input. Size should be in bytes.");
            return 1;
        }
    }

    else if (strcmp(tokens[0], "dgetfiles") == 0) {
        TAR = 1;
        char *date1 = tokens[1];
        char *date2 = tokens[2];
        struct tm time_info1, time_info2;
        const char *format = "%Y-%m-%d";

        if (num_tokens < 3 || num_tokens > 4) {
            printf("Error: invalid syntax for %s command.\nExpected syntax: %s date1 date2 <-u>\n",tokens[0],tokens[0]);
            return 1;
        }

        strptime(date1, format, &time_info1);
        strptime(date2, format, &time_info2);

        time_t time1 = mktime(&time_info1);
        time_t time2 = mktime(&time_info2);
        
        // Check if date1 and date2 is in correct format
        if( date1 != NULL && date2 != NULL ) {
            if (difftime(time1,time2) >= 0) {
                return 0;
            }  
            else{
                printf("Invalid date range: %ld %ld\n", time1, time2);
                return 1;
            }
        }

        else{
            printf("Invalid input. Size should be in bytes.");
            return 1;
        }
    }

    else if(strcmp(tokens[0],"getfiles") ==0 ){
        if(num_tokens < 2 || num_tokens > 8){
            TAR = 1;
            printf("Error: invalid syntax for %s command.\nExpected syntax: %s file1 file2 file3 file4 file5 file6 <-u>\n",tokens[0],tokens[0]);
            return 1;
        }
        else{
            return 0;
        }
    }

    else if(strcmp(tokens[0],"gettargz") == 0){
        if(num_tokens < 2 || num_tokens > 8){
            TAR = 1;
            printf("Error: invalid syntax for %s command.\nExpected syntax: %s <extension list> <-u>\n",tokens[0],tokens[0]);
            return 1;
        }
        else{
            return 0;
        }
    }

    else if (strcmp(tokens[0], "quit") == 0) {
        if(num_tokens == 1){
            return 0;
        }
        else
        {
            printf("Error: Invalid syntax for %s command\nExpected syntax: %s",tokens[0], tokens[0]);
            return 1;
        }
    } 
    
    else{
            printf("Invalid command\n");
    }
    return 0;
}

void run_client(int sockfd){
    char command[MAX_BUFFER_SIZE] = {0}; 
    char buffer[MAX_BUFFER_SIZE];

    int n;
    while (1) {
        UNZIP = 0;
        TAR = 0;
        bzero(buffer, MAX_BUFFER_SIZE); //clean the buffer
        printf("\nC$ ");
        fgets(buffer, MAX_BUFFER_SIZE - 1, stdin);
        char buffer_cpy[sizeof(buffer)];
        //create a deep copy of the original buffer for tokenization and parsing
        memcpy(buffer_cpy,buffer,sizeof(buffer));
        // Remove the newline character from the end of the command
        int len = strlen(buffer);
        if (buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        }
        if(parse_command(buffer_cpy) == 0){ //check the syntax of the command
            // Send the command to the server
            n = write(sockfd, buffer, strlen(buffer));
            if (n < 0) {
                error("ERROR writing to socket");
            }
        }
        else{ //If the syntax is not correct repromp the input command msg.
            continue;
        }

        bzero(buffer, MAX_BUFFER_SIZE); //clean the buffer
        if(TAR==1){
            recv_tar_file(sockfd);
        }
        else{
            // Read the response from the server
            n = read(sockfd, buffer, MAX_BUFFER_SIZE - 1);
            if (n <= 0) {
                error("ERROR reading from socket");
            }

            if(strcmp(buffer, "quit") == 0)
            {
                exit(0);
            }
            // Print the response from the server
            printf("Server: %s\n", buffer);
        }
        
        //Unzip if Unzip
        if(UNZIP == 1){
            char cmd[] = "tar -xvzf temp.tar.gz";
            system(cmd);
            printf("File Unzipped Successfully");
        }
    }
}

int main(int argc, char *argv[]) {
    //Create Socket 
    int sockD = socket(AF_INET, SOCK_STREAM, 0);
    int portno;
    struct sockaddr_in servAddr;
    struct hostent *server;
    servAddr.sin_family = AF_INET; //Internet 
    if(argc != 3){
        printf("Usage: ./client server_ip portno");
        exit(1);
    }
    server = gethostbyname(argv[1]);
    sscanf(argv[2], "%d", &portno);
    bcopy((char *)server->h_addr, (char *)&servAddr.sin_addr.s_addr, server->h_length);
    servAddr.sin_port = htons(portno); 
    //Connect to Server
    int connectStatus = connect(sockD, (struct sockaddr*)&servAddr, sizeof(servAddr));
    if (connectStatus < 0) {
        error("ERROR opening socket");
    }
    printf("\nClient connected successfully.");
    //Read initial Handshake message
    char buffer[25];
    int n = read(sockD, buffer, 25);
    if(n<=0){
        perror("ERROR reading from socket");
    }
    else{
        if(buffer[0] == '1'){
            //Connected to original server
            run_client(sockD);
        }
        else{
            //Redirected to Mirror
            char* token = strtok(buffer, " ");
            char * ip_addr = strtok(NULL, " "); //Extract IP of mirror
            char * port = strtok(NULL, " "); //Extract Portno of mirror
            int mirrD = socket(AF_INET, SOCK_STREAM, 0);
            int mirrport = atoi(port);
            struct sockaddr_in mirrorAddr;
            struct hostent *mirror;
            mirrorAddr.sin_family = AF_INET; //Internet 
            mirror = gethostbyname(ip_addr);
            bcopy((char *)mirror->h_addr, (char *)&mirrorAddr.sin_addr.s_addr, mirror->h_length);
            mirrorAddr.sin_port = htons(mirrport); 
            //Connect to mirror
            int connectStatus = connect(mirrD, (struct sockaddr*)&mirrorAddr, sizeof(mirrorAddr));
            if (connectStatus < 0) {
                error("ERROR opening socket");
            }
            printf("\nClient connected to mirror successfully.");
            run_client(mirrD);
        }
    }
    

    close(sockD);
    return 0;
}
