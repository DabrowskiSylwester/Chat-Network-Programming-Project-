#ifndef CLIENT_UI_H
#define CLIENT_UI_H

#include <stdint.h>
#include "protocol.h"


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

#endif
