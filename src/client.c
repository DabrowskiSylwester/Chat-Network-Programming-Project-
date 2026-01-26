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
#include <pthread.h>

#include "protocol.h"
#include "client_functions.h"
#include "client_ui.h"
#include "client_groups.h"

#define MCAST_ADDR "239.0.0.1"
#define MCAST_PORT 5000



int main( void ) {

    struct sockaddr_in server_addr;

    if ( discover_server(   //find server addres
            MCAST_ADDR,
            MCAST_PORT,
            &server_addr
        ) < 0 ) {
        fprintf( stderr, "Discovery failed\n" );
        return 1;
    }

    /* Connect through TCP */
    int sock = client_connect_tcp( &server_addr );
    if ( sock < 0 ) {
        fprintf( stderr, "TCP connect failed\n" );
        return 1;
    }

    /* User interface begins - start menu*/
    char input[ 128 ];
    char login[ MAX_USERNAME_LEN ];
    char password[ MAX_PASSWORD_LEN ];
    char username[ MAX_USERNAME_LEN ];
    int logged_in = 0;
    // int in_chat = 0;                     //old version
    // char chat_user[ MAX_USERNAME_LEN ];  //old version
    
    printf(ANSI_COLOR_RED
        "================================\n"
        " Der wunderbare Chat\n"
        "================================\n"
        ANSI_COLOR_RESET ANSI_COLOR_CYAN
        "Login or type a command.\n"
        "Type /help for available commands.\n"
    ANSI_COLOR_RESET);
    
    while ( !logged_in ) {

        read_command( input, sizeof( input ) );

        if ( strcmp( input, "/help" ) == 0 ) {

            printf(ANSI_COLOR_YELLOW
                "Available commands:\n"
                "  <login>    login to server\n"
                "  /create   create new account\n"
                "  /exit     quit client\n"
            ANSI_COLOR_RESET);

        } else if ( strcmp( input, "/exit" ) == 0 ) {

            printf(ANSI_COLOR_RED "Bye.\n" ANSI_COLOR_RESET);
            close( sock );
            return 0;

        } else if ( strcmp( input, "/create" ) == 0 ) {

            read_line( ANSI_COLOR_CYAN "Login: ", login, sizeof( login ) );
            read_line( ANSI_COLOR_CYAN "Password: ", password, sizeof( password ) );
            read_line( ANSI_COLOR_CYAN "Username: " ANSI_COLOR_RESET, username, sizeof( username ) );

            if ( client_create_account(
                    sock,
                    login,
                    password,
                    username
                ) == 0 ) {

                printf(ANSI_COLOR_GREEN "Account created successfully\n" ANSI_COLOR_RESET);
                if ( client_login( sock, login, password ) == 0 ) {

                printf(ANSI_COLOR_GREEN "Login successful\n" ANSI_COLOR_RESET);
                logged_in = 1;

            } else {
                printf(ANSI_COLOR_RED "Login failed\n" ANSI_COLOR_RESET);
            }

            } else {
                printf(ANSI_COLOR_RED "Account creation failed\n" ANSI_COLOR_RESET);
            }

        } else if ( input[0] != '\0' ) {

            /* treat input as login */
            strncpy( login, input, sizeof( login ) - 1 );
            read_line(ANSI_COLOR_CYAN "Password: "ANSI_COLOR_RESET, password, sizeof( password ) );

            if ( client_login( sock, login, password ) == 0 ) {

                printf(ANSI_COLOR_GREEN "Login successful\n" ANSI_COLOR_RESET);
                logged_in = 1;

            } else {
                printf(ANSI_COLOR_RED "Login failed\n" ANSI_COLOR_RESET);
            }
        }
    }

    char cmd[ 128 ];

    printf(ANSI_COLOR_CYAN
        "Logged in as %s\n"
        "Type /help for available commands\n" ANSI_COLOR_RESET,
        login
    );

    client_ctx_t ctx = {
        .sock = sock,
        .running = 1,
        .in_chat = 0
    };

    pthread_mutex_init( &print_mutex, NULL );

    pthread_t recv_tid;
    pthread_create(
        &recv_tid,
        NULL,
        client_recv_thread,
        &ctx
    );

    while ( ctx.running ) {

        read_command( cmd, sizeof( cmd ) );

        /* ================= CHAT MODE ================= */
        if ( ctx.in_chat ) {

            if ( strcmp( cmd, "/exit" ) == 0 ) {
                ctx.in_chat = 0;
                ctx.chat_user[0] = '\0';
                printf(ANSI_COLOR_YELLOW "Leaving chat\n" ANSI_COLOR_RESET);
                continue;
            } else if ( strcmp( cmd, "/history" ) == 0 ) {

                int lines = 0;   // whole history
                client_get_history( sock, ctx.chat_user, lines );
                continue;
            } else if ( strncmp( cmd, "/history", 8 ) == 0 ) {

                int n = 0;   // 0 = cała historia

                /* parse optional N */
                if ( strlen( cmd ) > 8 ) {
                    n = atoi( cmd + 8 );
                    if ( n < 0 ) n = 0;
                }
            
                client_get_history( sock, ctx.chat_user, n );
                continue;
            }


            /* everything else = message */
            client_send_message(
                ctx.sock,
                ctx.chat_user,
                cmd
            );
            continue;
        }

        /* ================= COMMAND MODE ================= */

        if ( strcmp( cmd, "/help" ) == 0 ) {

            printf(ANSI_COLOR_YELLOW
                "Available commands:\n"
                "  /msg <destination LOGIN>\n"
                //"  /history <N>"
                "  /group_join <name>\n"
                "  /users\n"
                "  /groups\n"
                "  /change_password\n"
                "  /change_username\n"
                "  /group_create\n"
                "  /exit\n"
            ANSI_COLOR_RESET);

        } else if ( strcmp( cmd, "/users" ) == 0 ) {

            client_get_active_users( sock );        //corrected

        } else if ( strcmp( cmd, "/change_password" ) == 0 ) {

            char old_pass[ MAX_PASSWORD_LEN ];
            char new_pass[ MAX_PASSWORD_LEN ];

            read_line(ANSI_COLOR_CYAN "Current password: ", old_pass, sizeof( old_pass ) );
            read_line(ANSI_COLOR_CYAN "New password: "ANSI_COLOR_RESET, new_pass, sizeof( new_pass ) );

            ctx.pending = PENDING_CHANGE_PASSWORD;
            client_change_password( sock, old_pass, new_pass ); //corrected

        } else if ( strcmp( cmd, "/change_username" ) == 0 ) {

            char new_name[ MAX_USERNAME_LEN ];
            read_line(ANSI_COLOR_CYAN "New username: "ANSI_COLOR_RESET, new_name, sizeof( new_name ) );

            ctx.pending = PENDING_CHANGE_USERNAME;
            client_change_username( sock, new_name );   //corrected

        } else if (strcmp(cmd, "/group_create") == 0) {

            char groupname[MAX_GROUP_NAME_LEN];
            read_line("Group name: ", groupname, sizeof(groupname));
            
            ctx.pending = PENDING_GROUP_CREATE;
            client_group_create(sock, groupname);

        } else if (strcmp(cmd, "/groups") == 0) {

            client_group_list(sock);

        } else if (strncmp(cmd, "/group_join ", 12) == 0) {
            ctx.pending = PENDING_GROUP_JOIN;
            client_group_join(sock, cmd + 12);

        } else if ( strncmp( cmd, "/msg ", 5 ) == 0 ) {

            ctx.in_chat = 1;
            strncpy(
                ctx.chat_user,
                cmd + 5,
                sizeof( ctx.chat_user ) - 1
            );

            printf(ANSI_COLOR_GREEN
                "Entering chat with %s\n"
                ANSI_COLOR_CYAN
                "Type /history <N> to print history\n"
                "If N = 0 whole history is printed\n"
                "Type /exit to leave chat\n",
                ctx.chat_user
            );

        } else if ( strcmp( cmd, "/exit" ) == 0 ) {

            printf(ANSI_COLOR_MAGENTA "Logging out...\n"ANSI_COLOR_RESET);
            leave_all_groups(&ctx); 
            break;

        } else if ( cmd[0] != '\0' ) {

            printf(ANSI_COLOR_RED "Unknown command. Type /help\n" ANSI_COLOR_RESET);
        }
    }

    /* shutdown */
    ctx.running = 0;
    shutdown( sock, SHUT_RDWR );
    pthread_join( recv_tid, NULL );
    close( sock );
    return 0;
}