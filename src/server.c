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
#include <pthread.h>    // pthread_create, pthread_mutex (Dla serwera)
#include <errno.h>      // errors(errno)
#include <sys/select.h> // select() (Dla klienta)
#include <syslog.h>     // syslog() (Dla demona)
// It is only usefull set of includes - it should be verified through work time if all of them are needed

#include "protocol.h"
#include "tcp_server.h"
#include "multicast_server.h"


#define MCAST_ADDR          "239.0.0.1"      // multicast addres
#define MCAST_PORT          5000            // multicast port
#define BUF_SIZE            256
#define SERVER_TCP_PORT     6000            // tcp port


int main( void ){

    pthread_t mcast_tid;

    multicast_ctx_t mcast_ctx = {   //configuration for multicast server
        .mcast_addr = MCAST_ADDR,
        .mcast_port = MCAST_PORT,
        .tcp_port   = SERVER_TCP_PORT
    };

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

    while (1) {

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

        printf(
            "Accepted TCP client from %s:%d\n",
            inet_ntoa( client_addr.sin_addr ),
            ntohs( client_addr.sin_port )
        );

        handle_tcp_client( client_fd );
    }

    
    return 0;
}
