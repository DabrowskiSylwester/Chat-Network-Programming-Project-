#define _GNU_SOURCE

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>      /* For NULL */
#include <string.h>     /* For memset */
#include <unistd.h>     /* For close */
#include <sys/types.h>  /* For socklen_t */
#include <sys/socket.h> /* For socket, connect, getsockname */
#include <netinet/in.h> /* For struct sockaddr_in, htons */
#include <arpa/inet.h>  /* For inet_pton, inet_ntop */

#include "protocol.h"
#include "tcp_server.h"



int start_tcp_server( uint16_t port ) {

    int sock;
    struct sockaddr_in addr;
    int reuse = 1;

    /* 1. Create the socket endpoint */
    sock = socket( AF_INET, SOCK_STREAM, 0 );
    if ( sock < 0 ) {
        perror( "socket TCP" );
        return -1;
    }

    /* 2. Allow the port to be reused immediately after the server stops -TIME WAIT */
    if ( setsockopt( sock, SOL_SOCKET, SO_REUSEADDR,
                     &reuse, sizeof( reuse ) ) < 0 ) {
        perror( "setsockopt TCP" );
        close( sock );
        return -1;
    }

    /* 3. Configure the address struct */
    memset( &addr, 0, sizeof( addr ) );
    addr.sin_family = AF_INET;
    addr.sin_port   = htons( port );
    addr.sin_addr.s_addr = htonl( INADDR_ANY );

    /* 4. Bind the socket to the port */
    if ( bind( sock, ( struct sockaddr * ) &addr, sizeof( addr ) ) < 0 ) {
        perror( "bind TCP" );
        close( sock );
        return -1;
    }

    /* 5. Start listening for incoming connections */
    if ( listen( sock, BACKLOG ) < 0 ) {
        perror( "listen" );
        close( sock );
        return -1;
    }

    printf( "TCP server listening on port %u\n", port );
    return sock;
}



void handle_tcp_client( int client_fd ) {

    uint16_t type;
    uint16_t len;
    void * data = NULL;

    printf( "Client connected (fd=%d)\n", client_fd );

    while (1) {

        if ( recv_tlv( client_fd, &type, &data, &len ) < 0 ) {  //!!!!!REMEMBER TO CLEAN
            printf( "Client disconnected (fd=%d)\n", client_fd );
            break;
        }

        printf(
            "Received TLV: type=%u len=%u\n",
            type,
            len
        );

        //echo
        send_tlv( client_fd, type, data, len );

        free( data );           //NO MEMORY LEAK
    }

    close( client_fd );
}
