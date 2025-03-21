#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>

#define PORT 2969

int main(int argc, char *argv[]) {
    int sd;
    struct sockaddr_in server;
    char command[100], response[1000];
    char ip[50];

    if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1) {perror ("Socket error\n");}

    if (argc < 2) {
        printf("IP adress: ");
        scanf("%s", ip);
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = inet_addr(ip);
        server.sin_port = htons (PORT);
    }
    else {
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = inet_addr(argv[1]);
        server.sin_port = htons (PORT);
    }

    if (connect (sd, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1) {perror ("Connect error\n");}
    else {printf("Succesfully connected to server\n");}

    while (1) {
        scanf(" %[^\n]", command);
        if (write (sd, command, strlen(command) * sizeof(char)) <= 0) {perror ("Write error\n");}

        ssize_t size = read (sd, response, sizeof(response) - 1);
        if (size < 0) {perror ("Read error\n");}
        else {response[size] = '\0';}

        printf("%s", response);

        if (!strcmp(response, "Exiting the client application...\n")) {
            close (sd);
            return 0;
        }
    }
}
