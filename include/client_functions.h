#ifndef CLIENT_FUNCTIONS_H
#define CLIENT_FUNCTIONS_H

#include <stdint.h>
#include <netinet/in.h>

/**
 * @brief Discovers the chat server on the local network using Multicast.
 *
 * @details This function broadcasts a discovery packet (e.g., TLV_DISCOVER) 
 * to the specified multicast group. It then listens for a unicast response 
 * from the server containing the TCP connection details.
 * * * WARNING: This is a blocking operation. It is recommended to implement 
 * a socket timeout (SO_RCVTIMEO) to prevent the client from hanging indefinitely 
 * if no server is active.
 *
 * @param mcast_addr The IPv4 multicast group address (e.g., "239.0.0.1").
 * @param mcast_port The UDP port of the multicast group.
 * @param out_addr   Pointer to a `sockaddr_in` structure. On success, this structure 
 * is populated with the server's IP address and TCP port.
 * The caller must allocate memory for this structure.
 *
 * @return int Returns 0 on success (server found), -1 on failure (socket error or timeout).
 */
int discover_server(
    const char * mcast_addr,
    uint16_t     mcast_port,
    struct sockaddr_in * out_addr
);

/**
 * @brief Establishes a TCP connection to the discovered server.
 *
 * @details Creates a standard TCP stream socket and attempts to connect 
 * to the address provided. This connection will be used for all subsequent 
 * unicast communication (login, commands, private messages).
 *
 * @param server_addr Pointer to the server's address structure (usually obtained 
 * via `discover_server`).
 *
 * @return int Returns the new socket file descriptor on success.
 * Returns -1 if the socket creation or connection attempt fails.
 */
int client_connect_tcp(
    const struct sockaddr_in * server_addr
);


int client_login(
    int sock,
    const char * login,
    const char * password
);

int client_create_account(
    int sock,
    const char * login,
    const char * password,
    const char * username
);

int client_change_password( 
    int sock, 
    const char * old_password,
    const char * new_password 
);

int client_change_username( 
    int sock, 
    const char * new_username 
);


int client_get_active_users( int sock );

#endif
