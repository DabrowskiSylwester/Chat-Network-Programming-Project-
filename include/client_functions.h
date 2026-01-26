#ifndef CLIENT_FUNCTIONS_H
#define CLIENT_FUNCTIONS_H

#include <stdint.h>
#include <netinet/in.h>

/**
 * @brief Performs server discovery via UDP Multicast.
 *
 * @details This function executes the client-side discovery protocol:
 * 1. Creates a UDP socket and binds it to an ephemeral local port.
 * 2. Sends a `TLV_DISCOVER` packet to the specified multicast group.
 * 3. Blocks waiting for a unicast response (`TLV_SERVER_INFO`) from the server.
 * 4. Parses the response to extract the server's TCP IP address and port.
 *
 * @warning This function is **blocking**. It will hang indefinitely in `recvfrom`
 * if no server responds, unless a socket timeout is configured (which is not
 * shown in this implementation).
 *
 * @param mcast_addr The IPv4 multicast group address (e.g., "239.0.0.1").
 * @param mcast_port The UDP port number of the multicast group.
 * @param out_addr   [OUT] Pointer to a `struct sockaddr_in` where the discovered
 * server's TCP connection details will be stored.
 * The caller must allocate this memory.
 *
 * @return int Returns 0 on success (server found and `out_addr` populated).
 * Returns -1 on failure (socket error, partial read, or unexpected TLV type).
 */
int discover_server(
    const char * mcast_addr,
    uint16_t     mcast_port,
    struct sockaddr_in * out_addr
);

/**
 * @brief Establishes a standard TCP connection to a specific server.
 *
 * @details This function performs the following steps:
 * 1. Creates a new IPv4 TCP socket (SOCK_STREAM).
 * 2. Initiates the TCP 3-way handshake using the `connect()` system call.
 * 3. Blocks execution until the connection is established or an error occurs.
 *
 * @param server_addr Pointer to a `struct sockaddr_in` containing the
 * destination IP address and TCP port (typically obtained via discovery).
 *
 * @return int Returns the active socket file descriptor on success.
 * Returns -1 if the socket could not be created or the connection was refused.
 */
int client_connect_tcp(
    const struct sockaddr_in * server_addr
);


/**
 * @brief Authenticates the user with the server using the TLV protocol.
 *
 * @details This function performs a sequential 4-step handshake:
 * 1. Sends a `TLV_COMMAND` packet with `CMD_LOGIN` to initiate the process.
 * 2. Sends a `TLV_LOGIN` packet containing the username string.
 * 3. Sends a `TLV_PASSWORD` packet containing the password string.
 * 4. Blocks and waits for a `TLV_STATUS` response from the server.
 *
 * @note This function handles the dynamic memory allocated by `recv_tlv` 
 * internally (frees the response payload before returning).
 *
 * @param sock     The open TCP socket descriptor connected to the server.
 * @param login    The username string (null-terminated).
 * @param password The password string (null-terminated).
 *
 * @return int Returns 0 if the server replied with `STATUS_OK`.
 * Returns -1 if network errors occurred, the packet type was wrong, 
 * or the server returned an error status (e.g., bad password).
 */
int client_login(
    int sock,
    const char * login,
    const char * password
);

/**
 * @brief Registers a new user account on the server using the TLV protocol.
 *
 * @details This function executes a multi-stage registration handshake:
 * 1. Sends `TLV_COMMAND` with `CMD_CREATE_ACCOUNT`.
 * 2. Sends `TLV_LOGIN` (the unique account ID).
 * 3. Sends `TLV_PASSWORD` (the raw password).
 * 4. Sends `TLV_USERNAME` (the friendly display name).
 * 5. Blocks and waits for a `TLV_STATUS` confirmation from the server.
 *
 * @note The server typically returns a non-OK status if the login is already taken
 * or if the password violates policy.
 *
 * @param sock     The open TCP socket descriptor.
 * @param login    The unique login string (e.g., "jdoe").
 * @param password The password string.
 * @param username The display name (e.g., "John Doe").
 *
 * @return int Returns 0 if the account was successfully created (`STATUS_OK`).
 * Returns -1 on network error, protocol mismatch, or if the server rejected
 * the registration (e.g., login taken).
 */
int client_create_account(
    int sock,
    const char * login,
    const char * password,
    const char * username
);

/**
 * @brief Requests a password change for the currently logged-in user.
 *
 * @details This function implements a secure password update sequence:
 * 1. Sends `TLV_COMMAND` with `CMD_CHANGE_PASSWORD`.
 * 2. Sends `TLV_PASSWORD` containing the **old** password (for verification).
 * 3. Sends `TLV_PASSWORD` containing the **new** password.
 * 4. Blocks and waits for the server's `TLV_STATUS` response.
 *
 * @param sock         The open TCP socket descriptor.
 * @param old_password The current password (must match what is in the database).
 * @param new_password The desired new password.
 *
 * @return int Returns 0 on success (`STATUS_OK`).
 * Returns -1 if the old password was incorrect, or if a network error occurred.
 */
int client_change_password( 
    int sock, 
    const char * old_password,
    const char * new_password 
);

/**
 * @brief Requests a change of the user's display name.
 *
 * @details Sends a `CMD_CHANGE_USERNAME` command followed by the new username string.
 * This updates the name other users see in the chat room active list.
 *
 * @param sock         The open TCP socket descriptor.
 * @param new_username The new display name string.
 *
 * @return int Returns 0 on success.
 * Returns -1 on failure (e.g., invalid characters or network error).
 */
int client_change_username( 
    int sock, 
    const char * new_username 
);

/**
 * @brief Retrieves and displays the list of currently connected users.
 *
 * @details This function implements a synchronous request-response cycle:
 * 1. Sends a `TLV_COMMAND` with the value `CMD_GET_ACTIVE_USERS` to the server.
 * 2. Blocks and waits for a response packet.
 * 3. Verifies that the response type is `TLV_ACTIVE_USERS`.
 * 4. Writes the payload directly to standard output (stdout).
 *
 * @note This function expects the server to format the user list as a 
 * text block (string) within the payload.
 *
 * @param sock The open TCP socket descriptor connected to the server.
 * @return int Returns 0 on success (list received and printed).
 * Returns -1 on network error or if the server response was unexpected.
 */
int client_get_active_users( int sock );

int client_send_message(
    int sock,
    const char * target,
    const char * message
);

int client_get_history(
    int sock,
    const char * with_user,
    int lines
);



#endif
