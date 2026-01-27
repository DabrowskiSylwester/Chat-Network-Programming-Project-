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

#include "protocol.h"
#include "client_functions.h"
#include "client_ui.h"

#define BUF_SIZE 256

int discover_server(
    const char * mcast_addr,
    uint16_t     mcast_port,
    struct sockaddr_in * out_addr
) {
    int sock;
    struct sockaddr_in local_addr;
    struct sockaddr_in mcast_sockaddr;
    uint8_t buf[ BUF_SIZE ];

    /* 1. Create UDP socket */
    sock = socket( AF_INET, SOCK_DGRAM, 0 );
    if ( sock < 0 ) {
        perror( "socket" );
        return -1;
    }

    /* 2. Bind local port (for unicast reply) */
    memset( &local_addr, 0, sizeof( local_addr ) );
    local_addr.sin_family = AF_INET;
    local_addr.sin_port   = htons( 0 );
    local_addr.sin_addr.s_addr = htonl( INADDR_ANY );

    if ( bind( sock,
               ( struct sockaddr * ) &local_addr,
               sizeof( local_addr ) ) < 0 ) {
        perror( "bind" );
        close( sock );
        return -1;
    }

    /* 3. Multicast destination */
    memset( &mcast_sockaddr, 0, sizeof( mcast_sockaddr ) );
    mcast_sockaddr.sin_family = AF_INET;
    mcast_sockaddr.sin_port   = htons( mcast_port );
    if ( inet_pton(
            AF_INET,
            mcast_addr,
            &mcast_sockaddr.sin_addr
        ) != 1 ) {
        perror( "inet_pton multicast" );
        close( sock );
        return -1;
    }

    /* 4. Send TLV_DISCOVER */
    tlv_header_t hdr;
    hdr.type   = htons( TLV_DISCOVER );
    hdr.length = htons( 0 );

    if ( sendto(
            sock,
            &hdr,
            sizeof( hdr ),
            0,
            ( struct sockaddr * ) &mcast_sockaddr,
            sizeof( mcast_sockaddr )
        ) < 0 ) {
        perror( "sendto TLV_DISCOVER" );
        close( sock );
        return -1;
    }

    /* 5. Receive TLV_SERVER_INFO */
    ssize_t n = recvfrom( sock, buf, sizeof( buf ), 0, NULL, NULL );
    if ( n < ( ssize_t ) sizeof( tlv_header_t ) ) {
        fprintf( stderr, "discover_server: packet too small\n" );
        close( sock );
        return -1;
    }

    tlv_header_t * rhdr = ( tlv_header_t * ) buf;
    uint16_t type = ntohs( rhdr->type );
    uint16_t len  = ntohs( rhdr->length );

    if ( type != TLV_SERVER_INFO || len != sizeof( server_info_t ) ) {
        fprintf( stderr, "discover_server: unexpected TLV\n" );
        close( sock );
        return -1;
    }

    server_info_t info;
    memcpy(
        &info,
        buf + sizeof( tlv_header_t ),
        sizeof( info )
    );

    /* 6. Fill sockaddr_in for TCP connect */
    memset( out_addr, 0, sizeof( *out_addr ) );
    out_addr->sin_family = AF_INET;
    out_addr->sin_addr.s_addr = info.ip;
    out_addr->sin_port = info.port;

    close( sock );
    return 0;
}



int client_connect_tcp(
    const struct sockaddr_in * server_addr
) {
    int sock;

    sock = socket( AF_INET, SOCK_STREAM, 0 );
    if ( sock < 0 ) {
        perror( "socket TCP" );
        return -1;
    }

    if ( connect(
            sock,
            ( const struct sockaddr * ) server_addr,
            sizeof( *server_addr )
        ) < 0 ) {
        perror( "connect" );
        close( sock );
        return -1;
    }

    return sock;
}

int client_login(
    int sock,
    const char * login,
    const char * password
) {
    uint16_t type;
    uint16_t len;
    void * data = NULL;

    command_t cmd = CMD_LOGIN;

    /* 1. send command */
    if ( send_tlv(
            sock,
            TLV_COMMAND,
            &cmd,
            sizeof( cmd )
        ) < 0 ) {
        perror( "send_tlv COMMAND" );
        return -1;
    }

    /* 2. send login */
    if ( send_tlv(
            sock,
            TLV_LOGIN,
            login,
            strlen( login )
        ) < 0 ) {
        perror( "send_tlv LOGIN" );
        return -1;
    }

    /* 3. send password */
    if ( send_tlv(
            sock,
            TLV_PASSWORD,
            password,
            strlen( password )
        ) < 0 ) {
        perror( "send_tlv PASSWORD" );
        return -1;
    }

    /* 4. receive status */
    if ( recv_tlv(
            sock,
            &type,
            &data,
            &len
        ) < 0 ) {
        perror( "recv_tlv STATUS" );
        return -1;
    }

    if ( type != TLV_STATUS || len != sizeof( status_t ) ) {
        fprintf( stderr, "client_login: unexpected TLV\n" );
        free( data );
        return -1;
    }

    status_t status;

    memcpy( &status, data, sizeof( status ) );
    free( data );

    if ( status != STATUS_OK ) {
        printf( "Login failed: %s\n", status_to_string( status ) );
        return -1;
    }

    return 0;
    
}

int client_create_account(
    int sock,
    const char * login,
    const char * password,
    const char * username
) {
    uint16_t type;
    uint16_t len;
    void * data = NULL;

    command_t cmd = CMD_CREATE_ACCOUNT;

    /* 1. send command */
    if ( send_tlv(
            sock,
            TLV_COMMAND,
            &cmd,
            sizeof( cmd )
        ) < 0 ) {
        perror( "send_tlv COMMAND" );
        return -1;
    }

    /* 2. send login */
    if ( send_tlv(
            sock,
            TLV_LOGIN,
            login,
            strlen( login )
        ) < 0 ) {
        perror( "send_tlv LOGIN" );
        return -1;
    }

    /* 3. send password */
    if ( send_tlv(
            sock,
            TLV_PASSWORD,
            password,
            strlen( password )
        ) < 0 ) {
        perror( "send_tlv PASSWORD" );
        return -1;
    }

    /* 4. send username */
    if ( send_tlv(
            sock,
            TLV_USERNAME,
            username,
            strlen( username )
        ) < 0 ) {
        perror( "send_tlv USERNAME" );
        return -1;
    }

    /* 5. receive status */
    if ( recv_tlv(
            sock,
            &type,
            &data,
            &len
        ) < 0 ) {
        perror( "recv_tlv STATUS" );
        return -1;
    }

    if ( type != TLV_STATUS || len != sizeof( status_t ) ) {
        fprintf( stderr, "client_create_account: unexpected TLV\n" );
        free( data );
        return -1;
    }

    status_t status;
    memcpy( &status, data, sizeof( status ) );
    free( data );

    return ( status == STATUS_OK ) ? 0 : -1;
}

int client_change_password(
    int sock,
    const char * old_password,
    const char * new_password
) {
    
    command_t cmd = CMD_CHANGE_PASSWORD;

    send_tlv( sock, TLV_COMMAND, &cmd, sizeof( cmd ) );
    send_tlv( sock, TLV_PASSWORD, old_password, strlen( old_password ) );
    send_tlv( sock, TLV_PASSWORD, new_password, strlen( new_password ) );

    
   
    return 0;
}

int client_change_username(
    int sock,
    const char * new_username
) {
    
    command_t cmd = CMD_CHANGE_USERNAME;

    send_tlv( sock, TLV_COMMAND, &cmd, sizeof( cmd ) );
    send_tlv( sock, TLV_USERNAME, new_username, strlen( new_username ) );

    

    // printf( "Username changed successfully\n" );
    return 0;
}


int client_get_active_users( int sock ) {
    // uint16_t type, len;  //old
    // void * data = NULL;
    command_t cmd = CMD_GET_ACTIVE_USERS;

    /* send command */
    if ( send_tlv(
            sock,
            TLV_COMMAND,
            &cmd,
            sizeof( cmd )
        ) < 0 ) {
        perror( "send_tlv COMMAND" );
        return -1;
    }

   
     return 0;
}



int client_send_message(
    int sock,
    const char * target,
    const char * message
) {
    command_t cmd = CMD_SEND_TO_USER;
    // uint16_t type, len;  old
    // void * data = NULL;

    send_tlv( sock, TLV_COMMAND, &cmd, sizeof( cmd ) );
    send_tlv( sock, TLV_LOGIN, target, strlen( target ) );
    send_tlv( sock, TLV_MESSAGE, message, strlen( message ) );

    /* wait for status in receiving thread */
    

    return 0;
}

int client_get_history(
    int sock,
    const char * with_user,
    int lines
) {
    command_t cmd = CMD_GET_HISTORY;

    send_tlv( sock, TLV_COMMAND, &cmd, sizeof( cmd ) );
    send_tlv( sock, TLV_LOGIN, with_user, strlen( with_user ) );

    
    uint16_t n = htons( lines );
    send_tlv( sock, TLV_UINT16, &n, sizeof( n ) );
    

    return 0;
}

