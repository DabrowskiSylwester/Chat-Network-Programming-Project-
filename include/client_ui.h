#ifndef CLIENT_UI_H
#define CLIENT_UI_H

#include <stdint.h>
#include "protocol.h"
#include "client_groups.h"
#include "group_types.h"

#define ANSI_COLOR_RED     "\033[31m"  // Błędy, offline
#define ANSI_COLOR_GREEN   "\033[32m"  // Sukces, online, login
#define ANSI_COLOR_YELLOW  "\033[33m"  // Ostrzeżenia, komunikaty systemowe
#define ANSI_COLOR_BLUE    "\033[34m"  // Rzadko używany na czarnym tle (słabo widać)
#define ANSI_COLOR_MAGENTA "\033[35m"  // Nagłówki, wyróżnienia
#define ANSI_COLOR_CYAN    "\033[36m"  // Nicki użytkowników, komendy
#define ANSI_COLOR_WHITE   "\033[37m"  // Zwykły tekst
#define ANSI_COLOR_RESET   "\033[0m"

extern pthread_mutex_t print_mutex;

#define MAX_GROUPS 16

typedef enum {
    PENDING_NONE = 0,
    PENDING_CHANGE_PASSWORD,
    PENDING_GROUP_CREATE,
    PENDING_GROUP_JOIN,
    PENDING_CHANGE_USERNAME
} pending_action_t;

typedef struct client_ctx {
    int sock;
    int running;

    pending_action_t pending;
    //pthread_mutex_t print_mutex;

    /* chat state */
    int in_chat;
    char chat_user[ MAX_USERNAME_LEN ];

    /* group */
    group_info_t last_group;
    group_ctx_t groups[MAX_GROUPS];
    int group_count; 

    int in_group_chat;
    char chat_group[MAX_GROUP_NAME_LEN];
    
} client_ctx_t;

/**
 * @brief Displays the startup menu and gets the user's choice.
 *
 * @details This function prints the initial options (Login, Create Account, Exit)
 * to the standard output. It reads an integer from the user.
 * * * IMPORTANT: This function consumes the leftover newline character ('\n') 
 * from the input buffer using getchar(). This prevents subsequent calls 
 * to fgets() or read_line() from reading an empty line immediately.
 *
 * @return int Returns the user's choice (e.g., 1, 2, 0). 
 * Returns -1 if the input was invalid (non-numeric).
 */
int menu_start( void );

/**
 * @brief Reads a line of text from standard input (stdin) safely.
 *
 * @details This helper function prints a prompt to the user and uses fgets() 
 * to read input, ensuring no buffer overflow occurs.
 * * * Unlike standard fgets(), this function automatically removes the trailing 
 * newline character ('\n') from the end of the string, making the data 
 * ready for processing or network transmission.
 *
 * @param prompt   The message to display to the user before waiting for input.
 * @param buf      Pointer to the buffer where the input string will be stored.
 * @param buf_size The maximum size of the buffer (including the null terminator).
 */
void read_line(
    const char * prompt,
    char * buf,
    size_t buf_size
);

/**
 * @brief Converts a server status code into a human-readable string.
 *
 * @details This helper function takes a protocol status enumeration (e.g., 
 * STATUS_OK, STATUS_FAIL, STATUS_LOGIN_TAKEN) and maps it to a clear 
 * text description suitable for printing to the console.
 *
 * @param status The status code received from the server (TLV_STATUS).
 * @return const char* A pointer to a static string literal describing the status.
 * @note The returned string is statically allocated; do NOT attempt to free it.
 */
const char * status_to_string( status_t status );

/**
 * @brief Reads a command line from standard input safely.
 *
 * @details This function wraps the input reading logic (typically using fgets).
 * It ensures that:
 * 1. The input does not exceed the buffer size (preventing overflows).
 * 2. The trailing newline character ('\n') is removed, leaving a clean string.
 *
 * @param buf  Pointer to the buffer where the command string will be stored.
 * @param size The maximum size of the buffer (in bytes).
 */
void read_command( char * buf, size_t size );


void * client_recv_thread( void * arg );



#endif
