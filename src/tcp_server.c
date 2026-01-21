#define _GNU_SOURCE

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h> 
#include <stdio.h>      /* For NULL */
#include <string.h>     /* For memset */
#include <unistd.h>     /* For close */
#include <sys/types.h>  /* For socklen_t */
#include <sys/socket.h> /* For socket, connect, getsockname */
#include <netinet/in.h> /* For struct sockaddr_in, htons */
#include <arpa/inet.h>  /* For inet_pton, inet_ntop */

#include "protocol.h"
#include "tcp_server.h"
#include "user_account.h"

pthread_mutex_t server_mutex = PTHREAD_MUTEX_INITIALIZER;


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



// void handle_tcp_client( int client_fd ) {

//     uint16_t type;
//     uint16_t len;
//     void * data = NULL;

//     printf( "Client connected (fd=%d)\n", client_fd );

//     while (1) {

//         if ( recv_tlv( client_fd, &type, &data, &len ) < 0 ) {  //!!!!!REMEMBER TO CLEAN
//             printf( "Client disconnected (fd=%d)\n", client_fd );
//             break;
//         }

//         printf(
//             "Received TLV: type=%u len=%u\n",
//             type,
//             len
//         );

//         //echo
//         send_tlv( client_fd, type, data, len );

//         free( data );           //NO MEMORY LEAK
//     }

//     close( client_fd );
// }

void * client_thread( void * arg ) {

    client_ctx_t * ctx = ( client_ctx_t * ) arg;
    int client_fd = ctx->client_fd;

    uint16_t type;
    uint16_t len;
    void * data = NULL;

    char login[ MAX_USERNAME_LEN ] = {0};
    char password[ MAX_PASSWORD_LEN ] = {0};
    char username[ MAX_USERNAME_LEN ] = {0};

    printf( "[tcp] client connected (fd=%d)\n", client_fd );

    while (1) {

        /* ---- receive TLV ---- */
        if ( recv_tlv( client_fd, &type, &data, &len ) < 0 ) {
            printf( "[tcp] client disconnected (fd=%d)\n", client_fd );
            break;
        }

        /* ---- handle TLV ---- */
        switch ( type ) {

        case TLV_COMMAND: {
            command_t cmd;

            if ( len != sizeof( command_t ) ) {
                free( data );
                break;
            }

            memcpy( &cmd, data, sizeof( cmd ) );
             free( data );
            data = NULL;

            switch ( cmd ) {

            case CMD_LOGIN: {       // to login client has to send a sequence of messages
                user_t user;
                status_t status;

                /* expect LOGIN and PASSWORD */
                if ( recv_tlv( client_fd, &type, &data, &len ) < 0 ){
                    goto cleanup;
                }

                if ( type != TLV_LOGIN ) {
                    free( data );
                    goto cleanup;;
                }

                strncpy( login, data, MAX_USERNAME_LEN - 1 );
                free( data );

                if ( recv_tlv( client_fd, &type, &data, &len ) < 0 ){
                    goto cleanup;
                }

                if ( type != TLV_PASSWORD ) {
                    free( data );
                    goto cleanup;;
                }

                strncpy( password, data, MAX_PASSWORD_LEN - 1 );
                free( data );

                /* ---- authentication ---- */
                pthread_mutex_lock( &server_mutex );
                
                if ( is_user_logged_in( login ) ) {

                    status = STATUS_ALREADY_LOGGED_IN;   // already logged in -> it blocks login in from two computers in the same time

                } else if ( user_authenticate( login, password, &user ) == 0 ) {
                
                    add_active_user(
                        user.login,
                        user.username,
                        client_fd
                    );
                    status = STATUS_OK;
                
                } else {
                
                    status = STATUS_AUTHENTICATION_ERROR;
                }
                pthread_mutex_unlock( &server_mutex );

                send_tlv(
                    client_fd,
                    TLV_STATUS,
                    &status,
                    sizeof( status )
                );
                break;
            }

            case CMD_CREATE_ACCOUNT: {
                status_t status;
                printf("[tcp] CMD_CREATE_ACCOUNT\n");

                /* LOGIN */
                if ( recv_tlv( client_fd, &type, &data, &len ) < 0 || type != TLV_LOGIN )
                    goto create_fail;

                strncpy( login, data, MAX_USERNAME_LEN - 1 );
                free( data );

                /* PASSWORD */
                if ( recv_tlv( client_fd, &type, &data, &len ) < 0 || type != TLV_PASSWORD )
                    goto create_fail;

                strncpy( password, data, MAX_PASSWORD_LEN - 1 );
                free( data );

                /* USERNAME */
                if ( recv_tlv( client_fd, &type, &data, &len ) < 0 || type != TLV_USERNAME )
                    goto create_fail;

                strncpy( username, data, MAX_USERNAME_LEN - 1 );
                free( data );

                printf(
                    "[tcp] create_account login='%s' password='%s' username='%s'\n",
                    login, password, username
                );

                pthread_mutex_lock( &server_mutex );
                printf( //debug
                    "[tcp] create_account login='%s' password='%s' username='%s'\n",
                    login,
                    password,
                    username
                );
                if ( user_create( login, password, username ) == 0 )
                    status = STATUS_OK;
                else
                    status = STATUS_ERROR;

                pthread_mutex_unlock( &server_mutex );

                send_tlv(
                    client_fd,
                    TLV_STATUS,
                    &status,
                    sizeof( status )
                );
                break;
                
            create_fail:
                status = STATUS_ERROR;
                send_tlv( client_fd, TLV_STATUS, &status, sizeof( status ) );
                break;
            }

            case CMD_CHANGE_PASSWORD: {
                status_t status;
                char old_pass[ MAX_PASSWORD_LEN ] = {0};
                char new_pass[ MAX_PASSWORD_LEN ] = {0};
                user_t user;
                        
                /* old password */
                if ( recv_tlv( client_fd, &type, &data, &len ) < 0 || type != TLV_PASSWORD )
                    goto cleanup;
                        
                strncpy( old_pass, data, MAX_PASSWORD_LEN - 1 );
                free( data );
                        
                /* new password */
                if ( recv_tlv( client_fd, &type, &data, &len ) < 0 || type != TLV_PASSWORD )
                    goto cleanup;
                        
                strncpy( new_pass, data, MAX_PASSWORD_LEN - 1 );
                free( data );
                        
                pthread_mutex_lock( &server_mutex );
                        
                if ( user_authenticate( login, old_pass, &user ) != 0 ) {
                    status = STATUS_AUTHENTICATION_ERROR;
                } else if ( user_change_password( login, new_pass ) == 0 ) {
                    status = STATUS_OK;
                } else {
                    status = STATUS_ERROR;
                }
            
                pthread_mutex_unlock( &server_mutex );
            
                send_tlv( client_fd, TLV_STATUS, &status, sizeof( status ) );
                break;
            }


            case CMD_CHANGE_USERNAME: {
                status_t status;
                        
                /* expect new username */
                if ( recv_tlv( client_fd, &type, &data, &len ) < 0 ||
                     type != TLV_USERNAME ) {
                    status = STATUS_ERROR;
                    send_tlv( client_fd, TLV_STATUS, &status, sizeof( status ) );
                    break;
                }
            
                char new_username[ MAX_USERNAME_LEN ] = {0};
                strncpy( new_username, data, MAX_USERNAME_LEN - 1 );
                free( data );
                data = NULL;
            
                pthread_mutex_lock( &server_mutex );
            
                if ( user_change_username( login, new_username ) == 0 )
                    status = STATUS_OK;
                else
                    status = STATUS_ERROR;
            
                pthread_mutex_unlock( &server_mutex );
            
                send_tlv(
                    client_fd,
                    TLV_STATUS,
                    &status,
                    sizeof( status )
                );
                break;
            }
            

            case CMD_GET_ACTIVE_USERS: {

                pthread_mutex_lock( &server_mutex );
                send_active_users( client_fd );
                pthread_mutex_unlock( &server_mutex );
                        
                break;
            }
            

            default:
                /* unsupported command */
                break;
            }

            break;
        }

        default:
            /* unexpected TLV */
            free( data );
            break;
        }

        data = NULL;
    }

cleanup:
    pthread_mutex_lock( &server_mutex );
    remove_active_user_by_fd( client_fd );
    dump_active_users();      /* DEBUG – na razie */
    pthread_mutex_unlock( &server_mutex );
    close( client_fd );
    free( ctx );
    return NULL;
}