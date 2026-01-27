//#define USER_DIR "data/users/"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include "protocol.h"
#include "user_account.h"

/* GLOBAL STATE (serwer) */
static active_user_t * active_users = NULL;

/* mutex - global in tcp_server.c */
extern pthread_mutex_t server_mutex;

int user_exists( const char * login ) {
    char path[ 256 ];

    if ( !login )
        return 0;

    snprintf( path, sizeof( path ), "%s%s", USER_DIR, login );
    return access( path, F_OK ) == 0;
}

int user_authenticate(
    const char * login,
    const char * password,
    user_t * out_user
) {
    char path[ 256 ];
    char line[ 128 ];
    FILE * f;

    char stored_pass[ MAX_PASSWORD_LEN ] = "";
    char stored_username[ MAX_USERNAME_LEN ] = "";

    if ( !login || !password || !out_user )
        return -1;

    snprintf( path, sizeof( path ), "%s%s", USER_DIR, login );

    f = fopen( path, "r" );
    if ( !f )
        return -1;

    while ( fgets( line, sizeof( line ), f ) ) {

        if ( sscanf( line, "password=%31s", stored_pass ) == 1 )
            continue;

        if ( sscanf( line, "username=%31[^\n]", stored_username ) == 1 )
            continue;
    }

    fclose( f );

    if ( stored_pass[0] == '\0' || stored_username[0] == '\0' )
        return -1;

    if ( strcmp( password, stored_pass ) != 0 )
        return -1;

    strncpy( out_user->login, login, MAX_USERNAME_LEN - 1 );
    out_user->login[ MAX_USERNAME_LEN - 1 ] = '\0';
    

    strncpy( out_user->username, stored_username, MAX_USERNAME_LEN - 1 );
    out_user->username[ MAX_USERNAME_LEN - 1 ] = '\0';

    return 0;
}

int user_create(
    const char * login,
    const char * password,
    const char * username
) {
    char path[ 256 ];
    FILE * f;

    /* basic validation */
    if ( !login || !password || !username ){
        return -1;
    }

    if ( strlen( login ) == 0 ||
         strlen( password ) == 0 ||
         strlen( username ) == 0 ){
        return -1;
    }

    if ( strlen( login ) >= MAX_USERNAME_LEN ||
         strlen( password ) >= MAX_PASSWORD_LEN ||
         strlen( username ) >= MAX_USERNAME_LEN ){
        return -1;
    }

    /* build file path */
    snprintf( path, sizeof( path ), "%s%s", USER_DIR, login );

    /* do not overwrite existing user */
    if ( access( path, F_OK ) == 0 ){
        return -1;
    }

    /* create file */
    f = fopen( path, "w" );
    if ( !f ){
        perror("user_create fopen");
        return -1;
    }
    /* Structure of file: 
        name of file is login (unchabgeble and unique)
        fistr line: password
        second line: User name (name displayed in chat)
    */
    fprintf( f, "password=%s\n", password );
    fprintf( f, "username=%s\n", username );

    fclose( f );
    return 0;
}

int user_change_password(
    const char * login,
    const char * new_password
) {
    char path[ 256 ];
    char line[ 256 ];
    char stored_username[ MAX_USERNAME_LEN ] = "";
    FILE * f;

    if ( !login || !new_password )
        return -1;

    if ( strlen( new_password ) == 0 ||
         strlen( new_password ) >= MAX_PASSWORD_LEN )
        return -1;

    snprintf( path, sizeof( path ), "%s%s", USER_DIR, login );

    /* open existing user file */
    f = fopen( path, "r" );
    if ( !f )
        return -1;

    /* read current username */
    while ( fgets( line, sizeof( line ), f ) ) {
        sscanf( line, "username=%31[^\n]", stored_username );
    }

    fclose( f );

    if ( strlen( stored_username ) == 0 )
        return -1;

    /* rewrite file with new password */
    f = fopen( path, "w" );
    if ( !f )
        return -1;

    fprintf( f, "password=%s\n", new_password );
    fprintf( f, "username=%s\n", stored_username );

    fclose( f );
    return 0;
}

int user_change_username(
    const char * login,
    const char * new_username
) {
    char path[ 256 ];
    char line[ 256 ];
    char stored_password[ MAX_PASSWORD_LEN ] = "";
    FILE * f;

    if ( !login || !new_username )
        return -1;

    if ( strlen( new_username ) == 0 ||
         strlen( new_username ) >= MAX_USERNAME_LEN )
        return -1;

    snprintf( path, sizeof( path ), "%s%s", USER_DIR, login );

    /* open existing user file */
    f = fopen( path, "r" );
    if ( !f )
        return -1;

    /* read current password */
    while ( fgets( line, sizeof( line ), f ) ) {
        sscanf( line, "password=%31s", stored_password );
    }

    fclose( f );

    if ( strlen( stored_password ) == 0 )
        return -1;

    /* rewrite file with new username */
    f = fopen( path, "w" );
    if ( !f )
        return -1;

    fprintf( f, "password=%s\n", stored_password );
    fprintf( f, "username=%s\n", new_username );

    fclose( f );
    return 0;
}



void add_active_user(
    const char * login,
    const char * username,
    int client_fd
) {
    active_user_t * u = malloc( sizeof( *u ) );
    if ( !u )
        return;

    strncpy( u->login, login, MAX_USERNAME_LEN - 1 );
    strncpy( u->username, username, MAX_USERNAME_LEN - 1 );
    u->login[ MAX_USERNAME_LEN - 1 ] = '\0';
    u->username[ MAX_USERNAME_LEN - 1 ] = '\0';

    u->client_fd = client_fd;
    u->next = active_users;
    active_users = u;

    printf(
        "[users] added login='%s' fd=%d\n",
        u->login,
        u->client_fd
    );
}

void remove_active_user_by_fd( int client_fd ) {
    active_user_t ** pp = &active_users;
    active_user_t *  cur;

    while ( ( cur = *pp ) != NULL ) {

        if ( cur->client_fd == client_fd ) {
            *pp = cur->next;

            printf(
                "[users] removed login='%s' fd=%d\n",
                cur->login,
                cur->client_fd
            );

            free( cur );
            return;
        }

        pp = &cur->next;
    }
}

void dump_active_users( void ) {
    active_user_t * u = active_users;

    printf( "[users] active users:\n" );

    while ( u ) {
        printf(
            "  login='%s' username='%s' fd=%d\n",
            u->login,
            u->username,
            u->client_fd
        );
        u = u->next;
    }
}

void send_active_users( int client_fd ) {
    char buf[ MAX_MESSAGE_LEN ];
    size_t off = 0;
    active_user_t * u = active_users;

    buf[0] = '\0';

    while ( u ) {
        int n = snprintf(
            buf + off,
            sizeof( buf ) - off,
            "<%s> %s\n",
            u->login,
            u->username
        );

        if ( n < 0 || off + n >= sizeof( buf ) )
            break;

        off += n;
        u = u->next;
    }

    send_tlv(
        client_fd,
        TLV_ACTIVE_USERS,
        buf,
        off
    );
}

int is_user_logged_in( const char * login ) {
    active_user_t * u = active_users;

    while ( u ) {
        if ( strcmp( u->login, login ) == 0 )
            return 1;
        u = u->next;
    }

    return 0;
}

active_user_t * find_active_user_by_login(
    const char * login
) {
    active_user_t * u = active_users;

    while ( u ) {
        if ( strcmp( u->login, login ) == 0 ){
            return u;
        }
        u = u->next;
    }

    return NULL;    //if not found return NULL pointer
}

active_user_t * find_active_user_by_fd(
    int fd
) {
    active_user_t * u = active_users;

    while ( u ) {
        if ( u->client_fd == fd ){
            return u;
        }
        u = u->next;
    }

    return NULL; 
}

