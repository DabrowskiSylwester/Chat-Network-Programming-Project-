#ifndef USER_ACCOUNT_H
#define USER_ACCOUNT_H

#include <stdint.h>
#include "protocol.h"

/* -------------------------------------------------------------------------- */
/* Data Structures                                                            */
/* -------------------------------------------------------------------------- */

/**
 * @brief Represents a user's static profile data loaded into memory.
 * * @details This structure holds user information (excluding the password)
 * after a successful authentication or lookup.
 */
typedef struct {
    char login[ MAX_USERNAME_LEN ];    /**< The unique login ID used for authentication. */
    char username[ MAX_USERNAME_LEN ]; /**< The display name visible to other users. */
} user_t;

/**
 * @brief Linked list node representing a currently connected client.
 * * @details This structure is used to maintain the list of online users in RAM.
 * It maps a network socket (client_fd) to a user profile.
 */
typedef struct active_user {
    char login[ MAX_USERNAME_LEN ];    /**< Login of the connected user. */
    char username[ MAX_USERNAME_LEN ]; /**< Display name of the connected user. */
    int  client_fd;                    /**< The socket file descriptor associated with this session. */
    struct active_user * next;         /**< Pointer to the next active user in the list (or NULL). */
} active_user_t;

/* -------------------------------------------------------------------------- */
/* Account Management (Persistent Storage / File I/O)                         */
/* -------------------------------------------------------------------------- */

/**
 * @brief Checks if a user account exists in the persistent storage.
 *
 * @details This function constructs the full file path for the given login
 * (e.g., "users/jdoe") and checks for the file's existence using the 
 * system call `access()`.
 *
 * @param login The login string to check.
 * @return int  Returns 1 (true) if the user file exists.
 * Returns 0 (false) if the file is missing or `login` is NULL.
 */
int user_exists( const char * login );

/**
 * @brief Registers a new user account.
 * * @details Appends the new user's credentials to the persistent storage file.
 * It typically checks if the login is already taken before writing.
 * * @param login    The unique login identifier.
 * @param password The raw password (should ideally be hashed before storage).
 * @param username The display name.
 * * @return int Returns 0 on success.
 * Returns -1 if the file could not be opened or written.
 * Returns 1 if the user already exists.
 */
int user_create(
    const char * login,
    const char * password,
    const char * username
);

/**
 * @brief Creates a new user account in the persistent storage.
 *
 * @details This function performs the registration process:
 * 1. Validates input lengths to prevent buffer overflows.
 * 2. Checks if the user already exists (using the login as the filename).
 * 3. Creates a new file `USER_DIR/<login>` and writes the credentials.
 *
 * The file format is text-based:
 * - Line 1: `password=<value>`
 * - Line 2: `username=<value>` (Display Name)
 *
 * @param login    The unique login identifier (used as the filename). 
 * Must not be empty or longer than MAX_USERNAME_LEN.
 * @param password The user's password.
 * Must not be empty or longer than MAX_PASSWORD_LEN.
 * @param username The display name visible to others in the chat.
 * Must not be empty or longer than MAX_USERNAME_LEN.
 *
 * @return int Returns 0 on success.
 * Returns -1 on failure (invalid input, user already exists, or file write error).
 */
int user_authenticate(
    const char * login,
    const char * password,
    user_t * out_user
);

/**
 * @brief Updates the password for a specific user in persistent storage.
 *
 * @details This function performs a safe update using a Read-Modify-Write strategy:
 * 1. Opens the user's file in read mode to retrieve the current `username` (display name).
 * 2. Validates that the current data is intact.
 * 3. Re-opens the file in write mode (truncating it).
 * 4. Writes the **new** password and the **preserved** username back to the file.
 *
 * @param login        The unique login ID (filename).
 * @param new_password The new password string. Must be within length limits.
 *
 * @return int Returns 0 on success.
 * Returns -1 if the file cannot be opened, the new password is invalid, 
 * or the existing username could not be retrieved (preventing data loss).
 */
int user_change_password(
    const char * login,
    const char * new_password
);

/**
 * @brief Updates the display username for a specific user.
 *
 * @details Similar to password change, this function preserves data integrity:
 * 1. Reads the existing `password` from the file.
 * 2. Overwrites the file with the **preserved** password and the **new** username.
 *
 * @note This changes the visible name in the chat, not the login ID used for authentication.
 *
 * @param login        The unique login ID (filename).
 * @param new_username The new display name to set.
 *
 * @return int Returns 0 on success.
 * Returns -1 on failure (e.g., file not found, invalid input, or read error).
 */
int user_change_username(
    const char * login,
    const char * new_username
);

/* -------------------------------------------------------------------------- */
/* Active Session API (In-Memory Linked List)                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief Adds a newly connected user to the global list of active sessions.
 *
 * @details Allocates memory for a new `active_user_t` node and inserts it
 * at the head of the `active_users` linked list.
 *
 * @warning **Thread Safety:** This function modifies a global shared list.
 * It must be called within a critical section (protected by `server_mutex`)
 * to prevent race conditions.
 *
 * @param login     The user's unique login ID.
 * @param username  The user's display name.
 * @param client_fd The socket file descriptor associated with this connection.
 */
void add_active_user(
    const char * login,
    const char * username,
    int client_fd
);

/**
 * @brief Removes a user from the active list based on their socket descriptor.
 *
 * @details Traverses the singly linked list to find the node with the matching
 * `client_fd`. Once found, it unlinks the node from the list and frees the memory.
 * This is typically called when a client disconnects.
 *
 * @note Uses a double-pointer strategy (`**pp`) to handle removal elegantly,
 * whether the node is at the head, middle, or tail.
 *
 * @warning **Thread Safety:** Access to the global list must be protected
 * by `server_mutex`.
 *
 * @param client_fd The socket descriptor of the user to remove.
 */
void remove_active_user_by_fd( int client_fd );

/**
 * @brief Checks if a specific user login is currently online.
 *
 * @details Performs a linear search through the `active_users` linked list
 * to see if any session matches the provided `login` ID. This is typically used
 * to prevent the same user from logging in twice simultaneously.
 *
 * @warning **Thread Safety:** Must be called within a critical section (mutex locked).
 *
 * @param login The login ID string to search for.
 * @return int Returns 1 (true) if the user is found in the list.
 * Returns 0 (false) if the user is not currently logged in.
 */
int is_user_logged_in( const char * login );

/**
 * @brief Serializes the active user list and sends it to a client.
 *
 * @details This function iterates through the `active_users` list and creates
 * a single text buffer containing the **display usernames** of all online users,
 * separated by newlines (e.g., "Alice\nBob\nCharlie\n").
 *
 * This buffer is then encapsulated in a `TLV_ACTIVE_USERS` packet and sent
 * to the requesting client.
 *
 * @warning **Thread Safety:** Must be called within a critical section (mutex locked).
 * @note If the list is too long to fit in `MAX_MESSAGE_LEN`, it will be truncated.
 *
 * @param client_fd The socket descriptor of the client requesting the list.
 */
void send_active_users( int client_fd );

/**
 * @brief Prints the list of currently active users to the server's console.
 *
 * @details This is a server-side debugging utility that iterates through the
 * `active_users` linked list and prints details (login, display name, socket FD)
 * to standard output.
 *
 * @warning **Thread Safety:** This function accesses a shared global list.
 * Ensure `server_mutex` is locked before calling to prevent race conditions.
 */
void dump_active_users( void );

#endif /* USER_ACCOUNT_H */