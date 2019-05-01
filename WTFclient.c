#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <inttypes.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include "WTFclient.h"
#include "utility.h"

int sockfd, port;
volatile char globalStop = 0;

void output_error(int errnum){
    switch (errnum) {
        case 0:
        fprintf(stderr, "%s\n", "Usage: WTFclient <command> <command args>\nAvailable commands: configure, checkout, update, upgrade, commit, push, create, destroy, add, remove, currentversion, history, rollback.\nRead docs for more info.");
        break;
        case 1:
        fprintf(stderr,"%s\n", "Usage: WTFclient configure <IP> <port>");
        break;
        case 2:
        fprintf(stderr,"%s\n", "Unable to open configure file\n");
        break;
        case 3:
        fprintf(stderr,"%s\n", "Invalid configure file. Please reconfigure.\n");
    }
    exit(0);
}

void on_sig_intp(int sig_num){
    globalStop = 1;
    shutdown(sockfd,2);
    return;
}

void connect_server(char * domainStr,char * portStr){
    port = parse_port(portStr);

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = on_sig_intp;
    sigaction(SIGINT, &act, NULL);

    struct addrinfo hints, *servinfo, *p, *first;
    int rv;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if((rv = getaddrinfo(domainStr, portStr, &hints, &servinfo)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    }

    char connected = 0;

    do{
        for(p = servinfo; p != NULL && !globalStop; p = p->ai_next) {
            if((sockfd = socket(p -> ai_family, p -> ai_socktype, p -> ai_protocol)) == -1){
                perror("socket");
                continue;
            }
            if(connect(sockfd, p->ai_addr, p->ai_addrlen) == -1){
                close(sockfd);
                continue;
            }
            connected = 1;
            break;
        }
        if(!connected){
            printf("Fail connected. Will try again 3 seconds later.\n");
            sleep(3);
        }
    } while(!connected && !globalStop);

    if(connected){
        printf("Connected to server\n");
        freeaddrinfo(servinfo);
    }
}

void process_create(int argc, char **argv){
    char *repo_name = argv[2];
    if(IsProject(repo_name) == 0){
      printf("The repository %s has been created\n", repo_name);
      return;
    }

    int i, msg_len = 8, str_size = strlen(repo_name);
    send_all(sockfd, &msg_len, sizeof(int), 0);
    char msg[8] = {'c', 'r', 'e', 't'};
    memcpy(msg + 4, &str_size, sizeof(int));
    send_all(sockfd, msg, msg_len, 0);
    send_all(sockfd, repo_name, str_size, 0);


    if(mkdir(repo_name, 0777) == -1){
      printf("%s\n",strerror(errno));
    }
    int file_pointer = dirfd(opendir(repo_name));
    int manifest_pointer = openat(file_pointer, ".Manifest", O_WRONLY | O_CREAT,0666);
    write(manifest_pointer, "1\n0 2 0 -1 -1\n",14);
    write(manifest_pointer, "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\n",48);
    write(manifest_pointer, "\n",1);
}

// void process_send(int argc, char *argv){
//
// }

int main(int argc,char ** argv){
    if(argc < 2){output_error(0);}
    if(strcmp(argv[1],"configure") == 0){
        if(argc != 4){output_error(1);}
        FILE *f = fopen(".configure","w");
        if(f == NULL){output_error(2);}
        fprintf(f,"%s\n",argv[2]);
        fprintf(f,"%s\n",argv[3]);
        fclose(f);
        return 0;
    } else {
        FILE *f = fopen(".configure","r");
        if(f == NULL){output_error(2);}
        char domain[64],port[64];
        fgets(domain,64,f);
        fgets(port,64,f);
        int t;
        t = strlen(domain);
        if(domain[t - 1] != '\n'){output_error(3);} else {domain[t - 1] = 0;}
        t = strlen(port);
        if(port[t - 1] != '\n'){output_error(3);} else {port[t - 1] = 0;}
        connect_server(domain,port);
    }

    if(globalStop) return 0;

    int flags = fcntl(sockfd,F_GETFL,0);
    fcntl(sockfd,F_SETFL,flags | O_NONBLOCK);
    // process_create(argc,argv);

    //pthread_create(&receive_thread, NULL, receiveFromServer, (void *) NULL);
    SendFile(sockfd, "ppp", ".Manifest");
    /*
    if(strcmp(argv[1], "checkout") == 0){
        process_checkout(argc, argv);
    }else if(strcmp(argv[1], "create") == 0){
        process_create(argc, argv);
    }else if(strcmp(argv[1], "add") == 0){
        process_add(argc, argv);
    }else if(strcmp(argv[1], "push") == 0){
        process_push(argc, argv);
    }else if(strcmp(argv[1], "update") == 0){
        process_update(argc, argv);
    }
    */

    //pthread_join(receive_thread,NULL);

    printf("finish\n");
    return 0;
}
