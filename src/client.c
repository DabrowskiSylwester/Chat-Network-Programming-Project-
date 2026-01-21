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
#include "client_functions.h"
#include "client_ui.h"

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
    
    printf(
        "================================\n"
        " Simple Chat Client\n"
        "================================\n"
        "Login or type a command.\n"
        "Type /help for available commands.\n"
    );
    
    while ( !logged_in ) {

        read_command( input, sizeof( input ) );

        if ( strcmp( input, "/help" ) == 0 ) {

            printf(
                "Available commands:\n"
                "  <login>    login to server\n"
                "  /create   create new account\n"
                "  /exit     quit client\n"
            );

        } else if ( strcmp( input, "/exit" ) == 0 ) {

            printf( "Bye.\n" );
            close( sock );
            return 0;

        } else if ( strcmp( input, "/create" ) == 0 ) {

            read_line( "Login: ", login, sizeof( login ) );
            read_line( "Password: ", password, sizeof( password ) );
            read_line( "Username: ", username, sizeof( username ) );

            if ( client_create_account(
                    sock,
                    login,
                    password,
                    username
                ) == 0 ) {

                printf( "Account created successfully\n" );
                if ( client_login( sock, login, password ) == 0 ) {

                printf( "Login successful\n" );
                logged_in = 1;

            } else {
                printf( "Login failed\n" );
            }

            } else {
                printf( "Account creation failed\n" );
            }

        } else if ( input[0] != '\0' ) {

            /* treat input as login */
            strncpy( login, input, sizeof( login ) - 1 );
            read_line( "Password: ", password, sizeof( password ) );

            if ( client_login( sock, login, password ) == 0 ) {

                printf( "Login successful\n" );
                logged_in = 1;

            } else {
                printf( "Login failed\n" );
            }
        }
    }


    char cmd[ 128 ];

    printf(
        "Logged in as %s\n"
        "Type /help for available commands\n",
        login
    );

    while ( logged_in ) {

        read_command( cmd, sizeof( cmd ) );

        if ( strcmp( cmd, "/help" ) == 0 ) {

            printf(
                "Available commands:\n"
                "  /users\n"
                "  /change_password\n"
                "  /change_username\n"
                "  /exit\n"
            );

        } else if ( strcmp( cmd, "/users" ) == 0 ) {

            client_get_active_users( sock );

        } else if ( strcmp( cmd, "/change_password" ) == 0 ) {

            char old_pass[ MAX_PASSWORD_LEN ];
            char new_pass[ MAX_PASSWORD_LEN ];

            read_line( "Current password: ", old_pass, sizeof( old_pass ) );
            read_line( "New password: ", new_pass, sizeof( new_pass ) );

            client_change_password( sock, old_pass, new_pass );


        } else if ( strcmp( cmd, "/change_username" ) == 0 ) {

            char new_name[ MAX_USERNAME_LEN ];
            read_line( "New username: ", new_name, sizeof( new_name ) );
            client_change_username( sock, new_name );

        } else if ( strcmp( cmd, "/exit" ) == 0 ) {

            printf( "Logging out...\n" );
            break;

        } else if ( cmd[0] != '\0' ) {

            printf( "Unknown command. Type /help\n" );
        }
    }

    close( sock );
    return 0;
}
