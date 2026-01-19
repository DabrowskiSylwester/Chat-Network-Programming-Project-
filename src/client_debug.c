#define _GNU_SOURCE

#include <stdio.h>      // printf, perror
#include <stdlib.h>     // exit
#include <string.h>     // memset
#include <unistd.h>     // close
#include <stdint.h>     // uint16_t
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "protocol.h"

#define SERVER_IP   "127.0.0.1"
#define SERVER_PORT 6000

int main( void ) {

    int sock;
    struct sockaddr_in server_addr;

    /* 1. Socket TCP */
    sock = socket( AF_INET, SOCK_STREAM, 0 );
    if ( sock < 0 ) {
        perror( "socket" );
        exit( EXIT_FAILURE );
    }

    /* 2. Server address */
    memset( &server_addr, 0, sizeof( server_addr ) );
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons( SERVER_PORT );
    inet_pton( AF_INET, SERVER_IP, &server_addr.sin_addr );

    /* 3. Connect */
    if ( connect( sock,
                  ( struct sockaddr * ) &server_addr,
                  sizeof( server_addr ) ) < 0 ) {
        perror( "connect" );
        close( sock );
        exit( EXIT_FAILURE );
    }

    printf( "Connected to TCP server %s:%d\n",
            SERVER_IP, SERVER_PORT );

    /* 4. Send TLV (test message) */
    const char * msg = "HELLO_TLV";
    send_tlv(
        sock,
        1,                  // przykładowy typ TLV
        msg,
        strlen( msg )
    );

    printf( "Sent TLV\n" );

    /* 5. Receive echo */
    uint16_t type;
    uint16_t len;
    void * data = NULL;

    if ( recv_tlv( sock, &type, &data, &len ) < 0 ) {
        perror( "recv_tlv" );
        close( sock );
        exit( EXIT_FAILURE );
    }

    printf(
        "Received echo TLV: type=%u len=%u value='%.*s'\n",
        type,
        len,
        len,
        ( char * ) data
    );

    free( data );
    close( sock );
    return 0;
}
