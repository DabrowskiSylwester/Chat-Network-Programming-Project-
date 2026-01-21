#ifndef USER_ACCOUNT_H
#define USER_ACCOUNT_H

#include <stdint.h>
#include "protocol.h"

/* user account structure (in memory) */
typedef struct {
    char login[ MAX_USERNAME_LEN ];
    char username[ MAX_USERNAME_LEN ];
} user_t;

/* account management */
int user_exists( const char * login );

int user_create(
    const char * login,
    const char * password,
    const char * username
);

int user_authenticate(
    const char * login,
    const char * password,
    user_t * out_user
);

int user_create(
    const char * login,
    const char * password,
    const char * username
);

int user_change_password(
    const char * login,
    const char * new_password
);

int user_change_username(
    const char * login,
    const char * new_username
);

/* List structure constainig active users */
typedef struct active_user {
    char login[ MAX_USERNAME_LEN ];
    char username[ MAX_USERNAME_LEN ];
    int  client_fd;
    struct active_user * next;  //singly linked list -> pointer to next element
} active_user_t;

/* API */
void add_active_user(
    const char * login,
    const char * username,
    int client_fd
);

void remove_active_user_by_fd( int client_fd );

/* debug helper */
void dump_active_users( void );

void send_active_users( int client_fd );

int is_user_logged_in( const char * login );


#endif
