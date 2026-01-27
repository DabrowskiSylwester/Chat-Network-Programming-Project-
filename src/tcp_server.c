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
#include <syslog.h> 
#include "protocol.h"
#include "tcp_server.h"
#include "user_account.h"
#include "history.h"
#include "groups.h"

pthread_mutex_t server_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t history_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t groups_mutex = PTHREAD_MUTEX_INITIALIZER;

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

    syslog( LOG_INFO, "TCP server listening on port %u\n", port );
    return sock;
}



void * client_thread( void * arg ) {

    client_ctx_t * ctx = ( client_ctx_t * ) arg;
    int client_fd = ctx->client_fd;

    uint16_t type;
    uint16_t len;
    void * data = NULL;

    char login[ MAX_USERNAME_LEN ] = {0};
    char password[ MAX_PASSWORD_LEN ] = {0};
    char username[ MAX_USERNAME_LEN ] = {0};

    syslog( LOG_INFO,"[tcp] client connected (fd=%d)\n", client_fd );

    while (1) {

        /* ---- receive TLV ---- */
        if ( recv_tlv( client_fd, &type, &data, &len ) < 0 ) {
            syslog( LOG_INFO, "[tcp] client disconnected (fd=%d)\n", client_fd );
            break;
        }
        syslog( LOG_INFO, "[TLV] received TLV type=%u len=%u\n", type, len);
        /* ---- handle TLV ---- */
        switch ( type ) {

        case TLV_COMMAND: {
            syslog( LOG_INFO, "[TLV] TLV_COMMAND:\n");
            
            command_t cmd;

            if ( len != sizeof( command_t ) ) {
                free( data );
                break;
            }

            memcpy( &cmd, data, sizeof( cmd ) );
             free( data );
            data = NULL;
            syslog( LOG_INFO, "[CMD] received command=%u\n", cmd);
            switch ( cmd ) {

            case CMD_LOGIN: {       // to login client has to send a sequence of messages
                syslog( LOG_INFO, "[CMD] CMD_LOGIN:\n");
                
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

                size_t n = len < MAX_USERNAME_LEN - 1 ? len : MAX_USERNAME_LEN - 1;
                memcpy( login, data, n );
                login[n] = '\0';

                free( data );

                if ( recv_tlv( client_fd, &type, &data, &len ) < 0 ){
                    goto cleanup;
                }

                if ( type != TLV_PASSWORD ) {
                    free( data );
                    goto cleanup;;
                }

                // strncpy( password, data, MAX_PASSWORD_LEN - 1 );

                n = len < MAX_PASSWORD_LEN - 1 ? len : MAX_PASSWORD_LEN - 1;
                memcpy( password, data, n );
                password[n] = '\0';

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

                pthread_mutex_lock(&groups_mutex);
                if (status == STATUS_OK) {
                    group_send_user_groups(client_fd, login);
                }
                pthread_mutex_unlock(&groups_mutex);

                break;
            }

            case CMD_CREATE_ACCOUNT: {
                status_t status;
                syslog( LOG_INFO, "[CMD] CMD_CREATE_ACCOUNT\n");

                /* LOGIN */
                if ( recv_tlv( client_fd, &type, &data, &len ) < 0 || type != TLV_LOGIN )
                    goto create_fail;

                // strncpy( login, data, MAX_USERNAME_LEN - 1 );
                
                size_t n = len < MAX_USERNAME_LEN - 1 ? len : MAX_USERNAME_LEN - 1;
                memcpy( login, data, n );
                login[n] = '\0';
                
                free( data );

                /* PASSWORD */
                if ( recv_tlv( client_fd, &type, &data, &len ) < 0 || type != TLV_PASSWORD )
                    goto create_fail;

                // strncpy( password, data, MAX_PASSWORD_LEN - 1 );
                
                n = len < MAX_PASSWORD_LEN - 1 ? len : MAX_PASSWORD_LEN - 1;
                memcpy( password, data, n );
                password[n] = '\0';

                free( data );

                /* USERNAME */
                if ( recv_tlv( client_fd, &type, &data, &len ) < 0 || type != TLV_USERNAME )
                    goto create_fail;

                // strncpy( username, data, MAX_USERNAME_LEN - 1 );

                n = len < MAX_USERNAME_LEN - 1 ? len : MAX_USERNAME_LEN - 1;
                memcpy( username, data, n );
                username[n] = '\0';

                free( data );

                

                pthread_mutex_lock( &server_mutex );
                syslog( LOG_INFO,
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
                syslog( LOG_INFO, "[CMD] CMD_CHANGE_PASSWORD:\n");

                status_t status;
                char old_pass[ MAX_PASSWORD_LEN ] = {0};
                char new_pass[ MAX_PASSWORD_LEN ] = {0};
                user_t user;
                        
                /* old password */
                if ( recv_tlv( client_fd, &type, &data, &len ) < 0 || type != TLV_PASSWORD )
                    goto cleanup;
                        
                // strncpy( old_pass, data, MAX_PASSWORD_LEN - 1 );

                size_t n = len < MAX_PASSWORD_LEN - 1 ? len : MAX_PASSWORD_LEN - 1;
                memcpy( old_pass, data, n );
                old_pass[n] = '\0';

                free( data );
                        
                /* new password */
                if ( recv_tlv( client_fd, &type, &data, &len ) < 0 || type != TLV_PASSWORD )
                    goto cleanup;
                        
                // strncpy( new_pass, data, MAX_PASSWORD_LEN - 1 );

                n = len < MAX_PASSWORD_LEN - 1 ? len : MAX_PASSWORD_LEN - 1;
                memcpy( new_pass, data, n );
                new_pass[n] = '\0';

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
                syslog( LOG_INFO, "[CMD] CMD_CHANGE_USERNAME:\n");

                status_t status;
                        
                /* expect new username */
                if ( recv_tlv( client_fd, &type, &data, &len ) < 0 ||
                     type != TLV_USERNAME ) {
                    status = STATUS_ERROR;
                    send_tlv( client_fd, TLV_STATUS, &status, sizeof( status ) );
                    break;
                }
            
                char new_username[ MAX_USERNAME_LEN ] = {0};

                //strncpy( new_username, data, MAX_USERNAME_LEN - 1 );

                size_t n = len < MAX_USERNAME_LEN - 1 ? len : MAX_USERNAME_LEN - 1;
                memcpy( new_username, data, n );
                new_username[n] = '\0';

                free( data );
                data = NULL;
            
                pthread_mutex_lock( &server_mutex );
            
                if ( user_change_username( login, new_username ) == 0 )
                    status = STATUS_OK;
                else
                    status = STATUS_ERROR;
            
                pthread_mutex_unlock( &server_mutex );

                active_user_t * this = find_active_user_by_fd( client_fd );
                strcpy( this->username, new_username );
                            
                send_tlv(
                    client_fd,
                    TLV_STATUS,
                    &status,
                    sizeof( status )
                );
                break;
            }
            

            case CMD_GET_ACTIVE_USERS: {
                syslog( LOG_INFO, "[CMD] CMD_GET_ACTIVE_USERS:\n");

                pthread_mutex_lock( &server_mutex );
                send_active_users( client_fd );
                pthread_mutex_unlock( &server_mutex );
                        
                break;
            }

            case CMD_SEND_TO_USER: {
                syslog( LOG_INFO, "[CMD] CMD_SEND_TO_USER:\n");
                char target[ MAX_USERNAME_LEN ] = {0};
                char message[ MAX_MESSAGE_LEN ] = {0};
                active_user_t * dst = NULL;
                active_user_t * src = NULL;

                /* recipient */
                if ( recv_tlv( client_fd, &type, &data, &len ) < 0 || type != TLV_LOGIN )
                    goto cleanup;

                // strncpy( target, data, MAX_USERNAME_LEN - 1 );

                size_t n = len < MAX_USERNAME_LEN - 1 ? len : MAX_USERNAME_LEN - 1;
                memcpy( target, data, n );
                target[n] = '\0';

                free( data );

                /* message */
                if ( recv_tlv( client_fd, &type, &data, &len ) < 0 || type != TLV_MESSAGE )
                    goto cleanup;

                n = len < MAX_MESSAGE_LEN - 1 ? len : MAX_MESSAGE_LEN - 1;
                memcpy( message, data, n );
                message[n] = '\0';

                free( data );

                pthread_mutex_lock( &server_mutex );

                dst = find_active_user_by_login( target );

                if ( !dst ) {
                    status_t st = STATUS_USER_NOT_FOUND;
                    send_tlv( client_fd, TLV_STATUS, &st, sizeof( st ) );
                    pthread_mutex_unlock( &server_mutex );
                    break;
                }
            
                src = find_active_user_by_fd( client_fd );

                if ( !src ) {
                    pthread_mutex_unlock( &server_mutex );
                    break;
                }

                /* send message to recipient */
                send_tlv(
                    dst->client_fd,
                    TLV_LOGIN,
                    src->login,                   // sender
                    strlen( src->login )
                );
                send_tlv(
                    dst->client_fd,
                    TLV_USERNAME,
                    src->username,
                    strlen( src->username )
                );
                send_tlv(
                    dst->client_fd,
                    TLV_MESSAGE,
                    message,
                    strlen( message )
                );
            
                status_t st = STATUS_OK;
                send_tlv( client_fd, TLV_STATUS, &st, sizeof( st ) );

                pthread_mutex_lock(&history_mutex);

                history_append_message(
                    src->login,
                    src->username,
                    dst->login,
                    message
                );
                
                pthread_mutex_unlock(&history_mutex);
            
                pthread_mutex_unlock( &server_mutex );
                break;
            }
            case CMD_GET_HISTORY: {
                syslog( LOG_INFO, "[CMD] CMD_GET_HISTORY:\n");

                char target[ MAX_USERNAME_LEN ] = {0};
                int max_lines = 0;

                if ( recv_tlv( client_fd, &type, &data, &len ) < 0 || type != TLV_LOGIN ){
                    if ( data ) {
                        free ( data );
                    }
                    break;
                }

                // strncpy( target, data, MAX_USERNAME_LEN - 1 );

                size_t n = len < MAX_USERNAME_LEN - 1 ? len : MAX_USERNAME_LEN - 1;
                memcpy( target, data, n );
                target[n] = '\0';

                free( data );
                
                if ( recv_tlv( client_fd, &type, &data, &len ) < 0 ||
                     type != TLV_UINT16 ||
                     len != sizeof( uint16_t ) ) {
                    if ( data ) {
                        free( data );
                    }
                    break;
                }

                uint16_t tmp;
                memcpy( &tmp, data, sizeof( tmp ) );
                max_lines = ntohs( tmp );
                free( data );

                active_user_t * src = find_active_user_by_fd( client_fd );
                if ( !src ) {
                    break;
                }

                
                char path[ 512 ];

                pthread_mutex_lock(&history_mutex);

                /* ======= HISTORIA GRUPOWA ======= */
                if (group_exists(target)) {
                    syslog( LOG_INFO, "Group history read.");
                    snprintf(
                        path,
                        sizeof(path),
                        HISTORY_DIR "%s",
                        target
                    );
                
                /* ======= HISTORIA 1vs1 ======= */
                } else {
                    syslog( LOG_INFO, "Hisotry read.");
                    char filename[256];
                    make_history_filename(
                        filename,
                        sizeof(filename),
                        src->login,
                        target
                    );
                
                    snprintf(
                        path,
                        sizeof(path),
                        HISTORY_DIR "%s",
                        filename
                    );
                }

                FILE *f = fopen(path, "r");
                if (!f) {
                    pthread_mutex_unlock(&history_mutex);
                    status_t st = STATUS_ERROR;
                    send_tlv(client_fd, TLV_STATUS, &st, sizeof(st));
                    break;
                }
            
                /* read all lines */
                char * lines[1024];
                int count = 0;
                char buf[1024];

                while ( fgets( buf, sizeof(buf), f ) && count < 1024 ) {
                    lines[count++] = strdup( buf );
                }
                fclose( f );
            
                pthread_mutex_unlock( &history_mutex );
            
                int start = 0;
                if ( max_lines > 0 && count > max_lines ){
                    start = count - max_lines;
                } 
            
                /* build output */
                char out[ HISTORY_OUT_MAX ];
                size_t out_len = 0;
                out[0] = '\0';

                for ( int i = start; i < count; ++i ) {
                    size_t l = strlen( lines[i] );
                    if ( out_len + l >= HISTORY_OUT_MAX - 1 ){
                        break;
                    }
                
                    memcpy( out + out_len, lines[i], l );
                    out_len += l;
                    out[out_len] = '\0';
                    free( lines[i] );
                }
            
                send_tlv( client_fd, TLV_HISTORY, out, out_len );
                break;
            }
        
            case CMD_CREATE_GROUP: {

                syslog( LOG_INFO, "[CMD] CMD_CREATE_GROUP:\n");
                char groupname[MAX_GROUP_NAME_LEN] = {0};
                group_info_t g;
                status_t st;
                        
                if (recv_tlv(client_fd, &type, &data, &len) < 0 || type != TLV_GROUPNAME){
                    syslog( LOG_INFO, "[CMD] Unexpected TLV or nothing at all:\n");
                    break;
                }
                syslog( LOG_INFO, "[CMD] Received TLV_GROUPNAME:\n");  

                size_t n = len < sizeof(groupname)-1 ? len : sizeof(groupname)-1;
                memcpy(groupname, data, n);
                groupname[n] = '\0';
                free(data);

                syslog( LOG_INFO, "Creating group: %s:\n",groupname);   
                
                pthread_mutex_lock( &groups_mutex );
                
                if (group_create(groupname, login, &g) == 0) {
                    st = STATUS_OK;
                } else {
                    st = STATUS_ERROR;
                }
                
                pthread_mutex_unlock( &groups_mutex );

                send_tlv(client_fd, TLV_STATUS, &st, sizeof(st));
            
                if (st == STATUS_OK) {
                    send_tlv(client_fd, TLV_GROUP_INFO, &g, sizeof(g));
                    syslog( LOG_INFO, "[group] Group created\n");  
                }
                

                break;
            }

            case CMD_LIST_GROUPS: {
                char buffer[MAX_MESSAGE_LEN] = {0};

                pthread_mutex_lock(&groups_mutex);
                int n = group_list(buffer);
                pthread_mutex_unlock(&groups_mutex);

                if (n < 0) {
                    status_t st = STATUS_ERROR;
                    send_tlv(client_fd, TLV_STATUS, &st, sizeof(st));
                    break;
                }
            
                send_tlv(client_fd, TLV_GROUP_LIST, buffer, n);
                break;
            }

            case CMD_JOIN_GROUP: {
                char groupname[MAX_GROUP_NAME_LEN] = {0};
                status_t st;
                group_info_t g;

                syslog( LOG_INFO, "[CMD] CMD_JOIN_GROUP" );
                if (recv_tlv(client_fd, &type, &data, &len) < 0 || type != TLV_GROUPNAME)
                    break;

                size_t n = len < sizeof(groupname)-1 ? len : sizeof(groupname)-1;
                memcpy(groupname, data, n);
                groupname[n] = '\0';
                free(data);

                pthread_mutex_lock(&groups_mutex);

                 if (!group_exists(groupname)) {
                    st = STATUS_GROUP_NOT_FOUND;
                } else if (group_add_user(groupname, login) == 1) {
                    st = STATUS_ALREADY_IN_GROUP;
                } else if (group_get_info(groupname, &g) == 0) {
                    st = STATUS_OK;
                } else {
                    st = STATUS_ERROR;
                }
            
                pthread_mutex_unlock(&groups_mutex);
            
                send_tlv(client_fd, TLV_STATUS, &st, sizeof(st));
                if (st == STATUS_OK) {
                    send_tlv(client_fd, TLV_GROUP_INFO, &g, sizeof(g)); //multicast infos
                }
                syslog( LOG_INFO, "[group] Group joined" );

                break;
            }

            case CMD_GROUP_MSG: {

                char groupname[MAX_GROUP_NAME_LEN] = {0};
                char message[MAX_MESSAGE_LEN] = {0};
                status_t st;
                syslog( LOG_INFO, "[CMD] CMD_GROUP_MSG" );

                active_user_t * src = NULL;
                src = find_active_user_by_fd( client_fd );
                /* group name */
                if (recv_tlv(client_fd, &type, &data, &len) < 0 ||
                    type != TLV_GROUPNAME)
                    break;

                memcpy(groupname, data, len);
                free(data);

                /* message */
                if (recv_tlv(client_fd, &type, &data, &len) < 0 ||
                    type != TLV_MESSAGE)
                    break;

                memcpy(message, data, len);
                free(data);

                pthread_mutex_lock(&groups_mutex);

                if (!group_exists(groupname) ||
                    !group_has_user(groupname, src->login)) {
                    st = STATUS_ERROR;
                    pthread_mutex_unlock(&groups_mutex);
                    send_tlv(client_fd, TLV_STATUS, &st, sizeof(st));
                    break;
                }
            
                group_info_t g;
                group_get_info(groupname, &g);
            
                pthread_mutex_unlock(&groups_mutex);
            
                /* --- MULTICAST SEND --- */
                group_multicast_send(&g, src->login, src->username, message);
            
                /* --- HISTORY --- */
                group_history_append(groupname, src->login, src->username, message);
            
                st = STATUS_OK;
                send_tlv(client_fd, TLV_STATUS, &st, sizeof(st));
                break;
            }



            

            default:
                /* unsupported command */
                syslog( LOG_INFO, "Unsupported COMMAND" );
                break;
            }

            break;
        }

        default:
            /* unexpected TLV */
            syslog( LOG_INFO,  "Unexpected TLV");
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