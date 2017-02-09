#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/types.h>

#define MAXDATASIZE 200 // max number of bytes we can get at once 

#define PORT "3490" // the port client will be connecting to 

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
    int main_sock, bytes_recieved;  
    struct addrinfo getaddrparam, *addrinfo; // variables for getaddrinfo()
    struct addrinfo *p; // temporary addrinfo pointer
    int getaddr_rv; // return value for getaddrinfo()
    char buffer[MAXDATASIZE];
    char s[INET6_ADDRSTRLEN];
    char to_send[] = "          ";

    if (argc != 4) {
        fprintf(stderr,"usage: client hostname [-t,-w] [City]\n");
        exit(1);
    }

    memset(&getaddrparam, 0, sizeof getaddrparam);
    getaddrparam.ai_family = AF_UNSPEC;
    getaddrparam.ai_socktype = SOCK_STREAM;

    if ((getaddr_rv = getaddrinfo(argv[1], PORT, &getaddrparam, &addrinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(getaddr_rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = addrinfo; p != NULL; p = p->ai_next) {
        if ((main_sock = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("socket creation error");
            continue;
        }

        if (connect(main_sock, p->ai_addr, p->ai_addrlen) == -1) {
            close(main_sock);
            perror("main_sock connect error");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(addrinfo); // all done with this structure

    if(strcmp(argv[2],"-t") == 0){// if user wants time
        strcpy(to_send,"time");
    }else if(strcmp(argv[2],"-w") == 0){// if user wants weather
        strcpy(to_send,"weather");
    }else{// if user gives unknown option
        printf("Unknown option given, Please use -t for time or -w for weather. Exiting...");
        exit(1);
    }
    if (send(main_sock, to_send, 13, 0) == -1)
        perror("Error sending packet. Exiting...");

    if ((bytes_recieved = recv(main_sock, buffer, MAXDATASIZE-1, 0)) == -1) {
        perror("Error recieving packet. Exiting...");
        exit(1);
    }

    buffer[bytes_recieved] = '\0';

    printf("The %s in %s is %s\n",to_send,argv[3],buffer);

    close(main_sock);// finished close the socket

    return 0;
}