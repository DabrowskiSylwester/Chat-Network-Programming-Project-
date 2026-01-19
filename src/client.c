#define _GNU_SOURCE

#include <stdio.h>      // printf, perror
#include <stdlib.h>     // exit
#include <string.h>     // memset, memcpy
#include <unistd.h>     // close
#include <stdint.h>     // uint16_t
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include "protocol.h"

#define MCAST_ADDR "239.0.0.1"
#define MCAST_PORT 5000
#define BUF_SIZE   256

int main( void ) {

    int sock;
    struct sockaddr_in local_addr;
    struct sockaddr_in mcast_addr;
    uint8_t buf[ BUF_SIZE ];

    /* 1. Socket UDP */
    sock = socket( AF_INET, SOCK_DGRAM, 0 );
    if ( sock < 0 ) {
        perror( "socket" );
        exit( EXIT_FAILURE );
    }

    /* 2. Bind local port (for unicast reply) */
    memset( &local_addr, 0, sizeof( local_addr ) );
    local_addr.sin_family = AF_INET;
    local_addr.sin_port   = htons( 0 );          // kernel wybierze port
    local_addr.sin_addr.s_addr = htonl( INADDR_ANY );

    if ( bind( sock, ( struct sockaddr * ) &local_addr,
               sizeof( local_addr ) ) < 0 ) {
        perror( "bind" );
        close( sock );
        exit( EXIT_FAILURE );
    }

    /* 3. Multicast address */
    memset( &mcast_addr, 0, sizeof( mcast_addr ) );
    mcast_addr.sin_family = AF_INET;
    mcast_addr.sin_port   = htons( MCAST_PORT );
    inet_pton( AF_INET, MCAST_ADDR, &mcast_addr.sin_addr );

    /* 4. Send TLV_DISCOVER */
    tlv_header_t hdr;
    hdr.type   = htons( TLV_DISCOVER );
    hdr.length = htons( 0 );

    sendto(
        sock,
        &hdr,
        sizeof( hdr ),
        0,
        ( struct sockaddr * ) &mcast_addr,
        sizeof( mcast_addr )
    );

    printf( "TLV_DISCOVER sent\n" );

    /* 5. Receive TLV_SERVER_INFO */
    ssize_t n = recvfrom( sock, buf, BUF_SIZE, 0, NULL, NULL );
    if ( n < ( ssize_t ) sizeof( tlv_header_t ) ) {
        fprintf( stderr, "Received packet too small\n" );
        close( sock );
        return 1;
    }

    tlv_header_t * rhdr = ( tlv_header_t * ) buf;
    uint16_t type = ntohs( rhdr->type );
    uint16_t len  = ntohs( rhdr->length );

    if ( type == TLV_SERVER_INFO && len == sizeof( server_info_t ) ) {

        server_info_t info;
        memcpy(
            &info,
            buf + sizeof( tlv_header_t ),
            sizeof( info )
        );

        char ip_str[ INET_ADDRSTRLEN ];
        inet_ntop( AF_INET, &info.ip, ip_str, sizeof( ip_str ) );

        printf(
            "Server TCP at %s:%d\n",
            ip_str,
            ntohs( info.port )
        );
    } else {
        fprintf( stderr, "Unexpected TLV\n" );
    }

    close( sock );
    return 0;
}
