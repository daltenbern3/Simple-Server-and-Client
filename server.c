#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#define CONNQ 10     // Queue of incoming connections
#define MAXDATASIZE 100 // Max size of recieve data
#define PORT "3490"  // The port we will connect through

void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void)
{
    int main_sock, temp_sock;  // listen on main_sock, new connection on temp_sock
    struct sockaddr_storage client_addr; // connector's address information
    socklen_t client_size;
    struct addrinfo getaddrparam, *addrinfo; // variables for getaddrinfo()
    struct addrinfo *p; // temporary addrinfo pointer
    int getaddr_rv; // return value for getaddrinfo()
    char buffer[MAXDATASIZE];
    char to_send[] = "                                           ";
    const char weather_options[5][20] = {"Sunny and 75","Cloudy and 60","Raining and 55","Snowing and 25","Hailing and 38"};
    int bytes_recieved;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    time_t rawtime;
    struct tm * timeinfo;

    // set up log file
    FILE *f = fopen("server_log.txt", "w");
    if (f == NULL){
        printf("Error opening file! Exiting...\n");
        exit(1);
    }

    // set params for getaddrinfo()
    memset(&getaddrparam, 0, sizeof getaddrparam); // allocate memory for getaddrparam
    getaddrparam.ai_flags = AI_PASSIVE; // use my IP
    getaddrparam.ai_socktype = SOCK_STREAM;
    getaddrparam.ai_family = AF_UNSPEC;

    if ((getaddr_rv = getaddrinfo(NULL, PORT, &getaddrparam, &addrinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(getaddr_rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = addrinfo; p != NULL; p = p->ai_next) {
        if ((main_sock = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("Error when creating socket");
            continue;
        }

        if (setsockopt(main_sock, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("Error during setsockopt");
            exit(1);
        }

        if (bind(main_sock, p->ai_addr, p->ai_addrlen) == -1) {
            close(main_sock);
            perror("Error binding to socket");
            continue;
        }

        break;
    }

    freeaddrinfo(addrinfo); // all done with this structure

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind. Exiting...\n");
        exit(1);
    }

    if (listen(main_sock, CONNQ) == -1) {
        perror("Failed to listen(). Exiting...\n");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    while(1) {  // main accept() loop
        client_size = sizeof client_addr;
        temp_sock = accept(main_sock, (struct sockaddr *)&client_addr, &client_size);
        if (temp_sock == -1) {
            perror("Error when accepting connection.");
            continue;
        }

        inet_ntop(client_addr.ss_family,
            get_in_addr((struct sockaddr *)&client_addr),
            s, sizeof s);
        printf("Got connection from %s\n", s);
        fprintf(f, "Got connection from %s\n", s);

        if (!fork()) { // this is the child process
            close(main_sock); // child doesn't need the listener anymore
            if ((bytes_recieved = recv(temp_sock, buffer, MAXDATASIZE-1, 0)) == -1) {
                perror("Error recieving packet. Exiting...");
                exit(1);
            }
            buffer[bytes_recieved] = '\0';
            fprintf(f,"Recieved '%s' from %s\n", buffer, s);
            // set up time data
            time(&rawtime);
            timeinfo = localtime(&rawtime);
            if(strcmp(buffer,"time")==0){ // if the client is requesting time
                if(timeinfo->tm_min <10)
                    sprintf(to_send,"%d:0%d",timeinfo->tm_hour,timeinfo->tm_min);
                else
                    sprintf(to_send,"%d:%d",timeinfo->tm_hour,timeinfo->tm_min);
            }else if(strcmp(buffer,"weather")==0){// if the client is requesting weather
                strcpy(to_send, weather_options[timeinfo->tm_min % 5]);
            }else{// client has sent an Unknown request
                strcpy(to_send, "Unknown option, Please try again.");
            }
            if (send(temp_sock, to_send, 20, 0) == -1){// Send the final info
                perror("Error sending packet. Exiting...");
                exit(1);
            }
            fprintf(f, "Sent '%s' to %s\n", to_send, s);
            close(temp_sock);// close the connection
            fprintf(f, "Ending connection with %s\n",s);
            exit(0);
        }
        close(temp_sock);  // parent doesn't need this
    }

    return 0;
}