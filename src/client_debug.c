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
    int choice;

    choice = menu_start();
    if ( choice < 0 ){
        return 1;
    }

    if ( choice == 0 ) {
        printf("Bye.\n");
        return 0;
    }

    if ( choice == 1 ) {        /* LOGIN */

        char login[ MAX_USERNAME_LEN ];
        char password[ MAX_PASSWORD_LEN ];

        read_line( "Login: ", login, sizeof( login ) );
        read_line( "Password: ", password, sizeof( password ) );

        if ( client_login( sock, login, password ) == 0 ) {
            printf("Login successful\n");
            //getchar();   // debug  
            client_get_active_users( sock );        //debug
            printf("Press Enter to logout...\n");   //debug
            getchar();                              //debug
            //client_change_password( sock, "sekret123" );
            client_change_username( sock, "Nowe miano");
        } else {
            printf("Login failed\n");
        }

        close( sock );
        return 0;
    }

    if ( choice == 2 ) {        /* CREATE ACCOUNT */

    char login[ MAX_USERNAME_LEN ];
    char password[ MAX_PASSWORD_LEN ];
    char username[ MAX_USERNAME_LEN ];

    read_line( "Login: ", login, sizeof( login ) );
    read_line( "Password: ", password, sizeof( password ) );
    read_line( "Username (display name): ", username, sizeof( username ) );

    if ( client_create_account(
            sock,
            login,
            password,
            username
            ) == 0 ) {
            printf( "Account created successfully\n" );
        } else {
            printf( "Account creation failed\n" );
        }
    
        close( sock );
        return 0;
    }


    close( sock );
    return 0;
}
