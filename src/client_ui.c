#include <stdio.h>      // printf, perror
#include <stdlib.h>     // exit
#include <string.h>     // memset, memcpy
#include <unistd.h>     // close
#include <stdint.h>     // uint16_t
#include <pthread.h>


#include "client_ui.h"
#include "client_groups.h"

pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

int menu_start( void ) {
    int choice;

    printf("\n");
    printf(ANSI_COLOR_BLUE "================================\n");
    printf("  Simple Chat Client\n");
    printf("================================\n");
    printf(ANSI_COLOR_YELLOW "1) Login\n");
    printf("2) Create account\n");
    printf("0) Exit\n" ANSI_COLOR_RESET);
    printf("> ");

    if ( scanf("%d", &choice) != 1 )
        return -1;

    /* consume newline */
    getchar();

    return choice;
}

void read_line(
    const char * prompt,
    char * buf,
    size_t buf_size
) {
    printf("%s", prompt);

    if ( fgets( buf, buf_size, stdin ) == NULL ) {
        buf[0] = '\0';
        return;
    }

    buf[ strcspn( buf, "\n" ) ] = '\0';
}

const char * status_to_string( status_t status ) {
    switch ( status ) {

    case STATUS_OK:
        return "Operation successful";

    case STATUS_AUTHENTICATION_ERROR:
        return "Invalid login or password";

    case STATUS_ALREADY_LOGGED_IN:
        return "User is already logged in";

    case STATUS_USER_NOT_FOUND:
        return "User not found";

    case STATUS_GROUP_NOT_FOUND:
        return "Group not found";
    
    case STATUS_ALREADY_IN_GROUP:
        return "Already in group";

    case STATUS_ERROR:
    default:
        return "Operation failed";
    }
}

void read_command( char * buf, size_t size ) {
    printf("> ");
    if ( fgets( buf, size, stdin ) == NULL ) {
        buf[0] = '\0';
        return;
    }
    buf[ strcspn( buf, "\n" ) ] = '\0';
}

void print_colored_history( const char * buf, size_t len ) {

    char line[1024];
    size_t pos = 0;

    while ( pos < len ) {

        /* extract one line */
        size_t i = 0;
        while ( pos < len && buf[pos] != '\n' && i < sizeof(line)-1 ) {
            line[i++] = buf[pos++];
        }

        if ( pos < len && buf[pos] == '\n' )
            pos++;

        line[i] = '\0';

        if ( i == 0 )
            continue;

        /* expected format:
           YYYY-MM-DD HH:MM:SS login username : message
        */

        char date[11], time[9];
        char login[64], username[64];
        char message[512];

        int parsed = sscanf(
            line,
            "%10s %8s %63s %63[^:] : %511[^\n]",
            date,
            time,
            login,
            username,
            message
        );

        if ( parsed == 5 ) {
            printf(
                ANSI_COLOR_YELLOW "%s %s " ANSI_COLOR_RESET
                ANSI_COLOR_CYAN "<%s>" ANSI_COLOR_RESET " "
                ANSI_COLOR_GREEN "%s" ANSI_COLOR_RESET
                " : %s\n",
                date,
                time,
                login,
                username,
                message
            );
        } else if ( parsed == 4 ) {
            printf(
                ANSI_COLOR_YELLOW "%s %s " ANSI_COLOR_RESET
                ANSI_COLOR_CYAN "<%s>" ANSI_COLOR_RESET " "
                ANSI_COLOR_GREEN "%s" ANSI_COLOR_RESET " :\n",
                date,
                time,
                login,
                username
            );
        } else {
            /* fallback */
            printf( "%s\n", line );
        }
    }
}


// Thread that allows printing messeges.
void * client_recv_thread( void * arg ) {

    client_ctx_t * ctx = arg;
    uint16_t type, len;
    void * data = NULL;

    while ( ctx->running ) {

        if ( recv_tlv( ctx->sock, &type, &data, &len ) < 0 ) {
            pthread_mutex_lock( &print_mutex );
            printf(ANSI_COLOR_RED "\n[disconnected from server]\n" ANSI_COLOR_RESET);
            pthread_mutex_unlock( &print_mutex );
            break;
        }

        if ( type == TLV_LOGIN ) {

            char login[ MAX_USERNAME_LEN ] = {0};
            char username[ MAX_USERNAME_LEN ] = {0};

            if ( data && len > 0 ) {
                size_t n = len < MAX_USERNAME_LEN - 1 ? len : MAX_USERNAME_LEN - 1;
                memcpy( login, data, n );
                login[n] = '\0';
            }

            free( data );
            data = NULL;

            /* expect USERNAME */
            if ( recv_tlv( ctx->sock, &type, &data, &len ) < 0 )
                break;

            if ( type != TLV_USERNAME ) {
                if ( data ) {
                    free( data );
                }
                continue;
            }
        
            if ( data && len > 0 ) {
                strncpy( username, data, MAX_USERNAME_LEN - 1 );
            }
            free( data );
            data = NULL;
        
            /* expect MESSAGE */
            if ( recv_tlv( ctx->sock, &type, &data, &len ) < 0 ){
                break;
            }
        
            if ( type != TLV_MESSAGE ) {
                if ( data ) {
                    free( data );
                }
                continue;
            }
        
            pthread_mutex_lock( &print_mutex );
            printf(
                ANSI_COLOR_CYAN "\n  <%s>"ANSI_COLOR_RESET ANSI_COLOR_GREEN"%s"ANSI_COLOR_RESET": %.*s\n> ",
                login,
                username,
                (int)len,
                (char *)data
            );
            fflush( stdout );
            pthread_mutex_unlock( &print_mutex );
            
        
            free( data );
            data = NULL;

        } else if ( type == TLV_STATUS ) {

            status_t st = STATUS_ERROR;
            if ( len == sizeof( status_t ) ) {
                memcpy( &st, data, sizeof( st ) );
            }
            free( data );

            pthread_mutex_lock( &print_mutex );
            
            if ( st == STATUS_OK ) {

                if ( ctx->pending == PENDING_CHANGE_PASSWORD ) {
                    printf( ANSI_COLOR_GREEN"\n[server] Password changed successfully\n" ANSI_COLOR_RESET"> " );
                }
                else if ( ctx->pending == PENDING_CHANGE_USERNAME ) {
                    printf(ANSI_COLOR_GREEN "\n[server] Username changed successfully\n"ANSI_COLOR_RESET"> " );
                }
                else if ( ctx->pending == PENDING_GROUP_CREATE ) {
                    printf(ANSI_COLOR_GREEN "\n[server] Group created successfully\n"ANSI_COLOR_RESET"> " );
                }
                else if ( ctx->pending == PENDING_GROUP_JOIN ) {
                    printf(ANSI_COLOR_GREEN "\n[server] Group joined successfully\n"ANSI_COLOR_RESET"> " );
                }
                else {
                    //printf(ANSI_COLOR_GREEN "\n[server] Unknown OK\n"ANSI_COLOR_RESET"> " );
                }
            
            } else {
                printf(ANSI_COLOR_YELLOW
                    "\n[server] %s\n> "ANSI_COLOR_RESET,
                    status_to_string( st )
                );
            }
            
                ctx->pending = PENDING_NONE;
            

            fflush( stdout );
            pthread_mutex_unlock( &print_mutex );

        } else if ( type == TLV_ACTIVE_USERS ) {

            pthread_mutex_lock( &print_mutex );

            printf(ANSI_COLOR_MAGENTA "\nActive users:\n" ANSI_COLOR_RESET ANSI_COLOR_CYAN);
            fwrite( data, 1, len, stdout );
            printf( "\n>" ANSI_COLOR_RESET);
            fflush( stdout );

            pthread_mutex_unlock( &print_mutex );

            free( data );
            data = NULL;

        } else if ( type == TLV_HISTORY ) {

            pthread_mutex_lock( &print_mutex );
            printf(ANSI_COLOR_RED "\n  ======================= Chat history =======================\n" ANSI_COLOR_RESET);
            print_colored_history( (char *)data, len );
            printf(ANSI_COLOR_RED "============================================================\n"ANSI_COLOR_RESET"> " );
            fflush( stdout );
            pthread_mutex_unlock( &print_mutex );

            free( data );

        } 
        
        else if (type == TLV_GROUP_LIST) {
            pthread_mutex_lock(&print_mutex);
            printf(ANSI_COLOR_MAGENTA"\nAvailable groups:\n"ANSI_COLOR_RESET ANSI_COLOR_CYAN);
            fwrite(data, 1, len, stdout);
            printf(ANSI_COLOR_RESET"> ");
            fflush(stdout);
            pthread_mutex_unlock(&print_mutex);
            free(data);
        }
        else if (type == TLV_GROUP_INFO) {

             if (len != sizeof(group_info_t)) {
                free(data);
                break;
            }
            
            memcpy(&ctx->last_group, data, sizeof(group_info_t));
            
        
            free(data);
        
            pthread_mutex_lock(&print_mutex);
        
            
            
                if (!find_group_ctx(ctx, ctx->last_group.name)) {

                if (ctx->group_count >= MAX_GROUPS) {
                    printf(ANSI_COLOR_RED"\n[group] too many groups\n> "ANSI_COLOR_RESET);
                } else {
                    group_ctx_t *g = &ctx->groups[ctx->group_count++];
                    // printf( "DEBUG: \n %s, %s, %d\n",
                    //     ctx->last_group.name,
                    //     ctx->last_group.mcast_ip,
                    //     ctx->last_group.mcast_port);
                    join_multicast(
                        g,
                        ctx->last_group.name,
                        ctx->last_group.mcast_ip,
                        ctx->last_group.mcast_port
                    );
                }
            }
                
            
            
            fflush(stdout);
            pthread_mutex_unlock(&print_mutex);
        }

        else {

            pthread_mutex_lock( &print_mutex );
            printf(ANSI_COLOR_RED "Unknown TLV!" ANSI_COLOR_RESET);
            pthread_mutex_unlock( &print_mutex );
            free( data );

        }

        data = NULL;
    }

    ctx->running = 0;
    return NULL;
}
