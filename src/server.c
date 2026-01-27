#define _GNU_SOURCE

#include <stdio.h>      // printf, perror, fprintf
#include <stdlib.h>     // malloc, free, exit
#include <string.h>     // memset, memcpy, strlen
#include <unistd.h>     // write, read, close, fork, daemon
#include <stdint.h>     // uint8_t, uint16_t
#include <sys/types.h>  // Definicje typów systemowych
#include <sys/socket.h> // socket(), bind(), connect()
#include <netinet/in.h> // struct sockaddr_in, IPPROTO_TCP
#include <arpa/inet.h>  // inet_ntop, htons
#include <netdb.h>      // getaddrinfo (DNS)
#include <pthread.h>    // pthread_create, pthread_mutex        https://en.wikipedia.org/wiki/Pthreads
#include <errno.h>      // errors(errno)
#include <sys/select.h> // 
#include <sys/stat.h>
#include <syslog.h>     // syslog() (demona)
#include <fcntl.h>
#include <signal.h>

// It is only usefull set of includes - it should be verified through work time if all of them are needed

#include "protocol.h"
#include "tcp_server.h"
#include "multicast_server.h"
#include "groups.h"


#define MCAST_ADDR          "239.0.0.1"      // multicast addres
#define MCAST_PORT          5000            // multicast port
#define BUF_SIZE            256
#define SERVER_TCP_PORT     6000            // tcp port

void ensure_directories(void)
{
    if (mkdir("/var/lib/chat_server", 0755) < 0 && errno != EEXIST){
        perror("mkdir chat_server");
    }

    if (mkdir("/var/lib/chat_server/users", 0755) < 0 && errno != EEXIST){
        perror("mkdir users");
    }

    if (mkdir("/var/lib/chat_server/history", 0755) < 0 && errno != EEXIST){
        perror("mkdir history");
    }

    if (mkdir("/var/lib/chat_server/groups", 0755) < 0 && errno != EEXIST){
        perror("mkdir groups");
    }
}


void daemonize(void)
{
    pid_t pid;

    /* fork #1 */
    pid = fork();
    if (pid < 0)
        exit(EXIT_FAILURE);
    if (pid > 0)
        exit(EXIT_SUCCESS);   // parent leaves

    /* new session */
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    /* fork #2  */
    pid = fork();
    if (pid < 0)
        exit(EXIT_FAILURE);
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* files and working dir. */
    umask(0);

    chdir("/");

    
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY);
    open("/dev/null", O_WRONLY);
}


static volatile int running = 1;

void handle_sig(int sig)
{
    running = 0;
}

int main( void ){  

    daemonize();
    openlog("chat_server", LOG_PID | LOG_NDELAY, LOG_DAEMON);

    signal(SIGTERM, handle_sig);
    signal(SIGINT, handle_sig);

    ensure_directories();

    pthread_t mcast_tid;

    multicast_ctx_t mcast_ctx = {   //configuration for multicast server
        .mcast_addr = MCAST_ADDR,
        .mcast_port = MCAST_PORT,
        .tcp_port   = SERVER_TCP_PORT
    };

    // int pthread_create(pthread_t *restrict thread,
    //                       const pthread_attr_t *restrict attr,
    //                       typeof(void *(void *)) *start_routine,
    //                       void *restrict arg);

    if ( pthread_create(        //create a POSIX tread for multicast server
            &mcast_tid,
            NULL,
            multicast_thread,
            &mcast_ctx
        ) != 0 ) {
        perror( "pthread_create multicast" );
        exit( EXIT_FAILURE );
    }

    pthread_detach( mcast_tid ); //give him his own live 

    int tcp_sock = start_tcp_server( SERVER_TCP_PORT ); //make tcp socket
    if ( tcp_sock < 0 ) {
        exit( EXIT_FAILURE );
    }

    //ensure_directories();

    // //test
    // group_info_t g;
    // memset(&g, 0, sizeof(g));
    
    // if (group_create("grupa2", "test1", &g) == 0) {
    //     printf("Created group '%s' %s:%d\n",
    //            g.name, g.mcast_ip, g.mcast_port);
    // } else {
    //     printf("ERROR: group_create failed\n");
    // }

    // group_add_user("grupa2", "test2");
    // group_add_user("grupa2", "test3");
    // //test

    while (running) {

        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof( client_addr );

        int client_fd = accept(
            tcp_sock,
            ( struct sockaddr * ) &client_addr,
            &client_len
        );

        if ( client_fd < 0 ) {
            perror( "accept" );
            continue;
        }

        syslog(LOG_INFO,
            "Accepted TCP client from %s:%d\n",
            inet_ntoa( client_addr.sin_addr ),
            ntohs( client_addr.sin_port )
        );

        /* Create a thread to deal with client and retun to listening. */
        pthread_t tid;

        client_ctx_t * ctx = malloc( sizeof( client_ctx_t ) );
        if ( !ctx ) {
            close( client_fd );
            continue;
        }

        ctx->client_fd = client_fd;

        if ( pthread_create(
                &tid,
                NULL,
                client_thread,
                ctx
            ) != 0 ) {
            
            perror( "pthread_create client" );
            close( client_fd );
            free( ctx );
            continue;
        }

        pthread_detach( tid );

    }


    
    return 0;
}
