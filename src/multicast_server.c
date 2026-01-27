#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h> 

#include "protocol.h"
#include "multicast_server.h"


int get_local_ip( char * ip_buf, size_t buf_len ) {

    int sock;
    struct sockaddr_in tmp_addr;
    socklen_t addr_len = sizeof( tmp_addr );

    sock = socket( AF_INET, SOCK_DGRAM, 0 );
    if ( sock < 0 ) {
        return -1;
    }

    /* Fake connect  */
    memset( &tmp_addr, 0, sizeof( tmp_addr ) );
    tmp_addr.sin_family = AF_INET;
    tmp_addr.sin_port   = htons( 53 );                 // DNS
    inet_pton( AF_INET, "8.8.8.8", &tmp_addr.sin_addr );

    connect( sock, ( struct sockaddr * ) &tmp_addr, sizeof( tmp_addr ) );

    if ( getsockname( sock, ( struct sockaddr * ) &tmp_addr, &addr_len ) < 0 ) {
        close( sock );
        return -1;
    }

    inet_ntop( AF_INET, &tmp_addr.sin_addr, ip_buf, buf_len );
    close( sock );
    return 0;
}


void * multicast_thread( void * arg ) {

    multicast_ctx_t * ctx = ( multicast_ctx_t * ) arg;

    int sock;
    struct sockaddr_in addr;
    struct ip_mreq mreq;

    /* Create socket*/
    sock = socket( AF_INET, SOCK_DGRAM, 0 );
    if ( sock < 0 ) {
        perror( "socket multicast" );
        return NULL;
    }

    /* Set options - reuse*/
    int reuse = 1;
    setsockopt(
        sock,
        SOL_SOCKET,
        SO_REUSEADDR,
        &reuse,
        sizeof( reuse )
    );

    /* Prepare address structure*/
    memset( &addr, 0, sizeof( addr ) );
    addr.sin_family = AF_INET;
    addr.sin_port   = htons( ctx->mcast_port );
    addr.sin_addr.s_addr = htonl( INADDR_ANY );

    if ( bind( sock, ( struct sockaddr * ) &addr, sizeof( addr ) ) < 0 ) {
        perror( "bind multicast" );
        close( sock );
        return NULL;
    }

    /* Prepare structure and join multicast group*/
    mreq.imr_multiaddr.s_addr = inet_addr( ctx->mcast_addr );
    mreq.imr_interface.s_addr = htonl( INADDR_ANY );

    if ( setsockopt(
            sock,
            IPPROTO_IP,
            IP_ADD_MEMBERSHIP,
            &mreq,
            sizeof( mreq )
        ) < 0 ) {
        perror( "IP_ADD_MEMBERSHIP" );
        close( sock );
        return NULL;
    }

    syslog( LOG_INFO,
        "[multicast] listening on %s:%u\n",
        ctx->mcast_addr,
        ctx->mcast_port
    );

    /* Structure for client*/
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof( client_addr );
    uint8_t buf[ 256 ];

    /* Main loop - taking care of clients*/
    while (1) {

        ssize_t n = recvfrom(
            sock,
            buf,
            sizeof( buf ),
            0,
            ( struct sockaddr * ) &client_addr,
            &client_len
        );  //receive message

        /* Read TLV header*/
        if ( n < ( ssize_t ) sizeof( tlv_header_t ) ){
            continue;
        }

        tlv_header_t * hdr = ( tlv_header_t * ) buf;

        uint16_t type = ntohs( hdr->type );
        uint16_t len  = ntohs( hdr->length );

        /*Answer only on TLV_DISCOVER*/
        if ( type != TLV_DISCOVER || len != 0 ){
            continue;
        }
        /* Get ip address for TCP server*/
        char ip_str[ INET_ADDRSTRLEN ];
        if ( get_local_ip( ip_str, sizeof( ip_str ) ) < 0 )
            continue;

        /*Prepare and send it to client*/
        server_info_t info;
        inet_pton( AF_INET, ip_str, &info.ip );
        info.port = htons( ctx->tcp_port );

        tlv_header_t out_hdr;
        out_hdr.type   = htons( TLV_SERVER_INFO );
        out_hdr.length = htons( sizeof( info ) );

        uint8_t out_buf[ sizeof( out_hdr ) + sizeof( info ) ];
        memcpy( out_buf, &out_hdr, sizeof( out_hdr ) );
        memcpy( out_buf + sizeof( out_hdr ), &info, sizeof( info ) );

        sendto(
            sock,
            out_buf,
            sizeof( out_buf ),
            0,
            ( struct sockaddr * ) &client_addr,
            client_len
        );

        syslog( LOG_INFO,
            "[multicast] replied to %s:%u\n",
            inet_ntoa( client_addr.sin_addr ),
            ntohs( client_addr.sin_port )
        );
    }

    close( sock );
    return NULL;
}