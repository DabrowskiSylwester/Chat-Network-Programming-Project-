#include <stdio.h>      // printf, perror
#include <stdlib.h>     // exit
#include <string.h>     // memset, memcpy
#include <unistd.h>     // close
#include <stdint.h>     // uint16_t


#include "client_ui.h"


int menu_start( void ) {
    int choice;

    printf("\n");
    printf("================================\n");
    printf("  Simple Chat Client\n");
    printf("================================\n");
    printf("1) Login\n");
    printf("2) Create account\n");
    printf("0) Exit\n");
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