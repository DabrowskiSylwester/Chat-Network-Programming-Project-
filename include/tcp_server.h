#ifndef SERVER_FUNCTIONS_H
#define SERVER_FUNCTIONS_H

#include <stddef.h> // Required for size_t
#include <stdlib.h>
#include <stdint.h>

#define BACKLOG 10      //number of waiting TCP clients

/**
 * @brief  Initializes and starts a TCP server socket.
 *
 * @details This function performs the standard sequence of operations required
 * to accept incoming TCP connections:
 * 1. Creates a socket (IPv4, TCP).
 * 2. Sets the SO_REUSEADDR option (allows immediate restart of the server).
 * 3. Binds the socket to the specified port on all available network interfaces (INADDR_ANY).
 * 4. Puts the socket in listening mode using the global BACKLOG limit.
 *
 * @param  port The port number to bind the server to (e.g., 8080).
 * @return int  Returns the file descriptor of the listening socket on success.
 * Returns -1 on failure (and prints the error to stderr).
 */
int start_tcp_server( uint16_t port ); 


/**
 * @brief  Handles the communication lifecycle for a single TCP client.
 *
 * @details This function enters an infinite loop to process incoming TLV messages
 * from a connected client. Currently, it implements a simple "Echo Server" logic:
 * it sends back whatever it receives.
 * * * IMPORTANT: This function blocks the execution flow until the client disconnects.
 * In a concurrent server, this should be run in a separate thread or process.
 *
 * * MEMORY MANAGEMENT: It automatically frees the payload buffer allocated 
 * by recv_tlv() after processing each message.
 *
 * @param  client_fd The file descriptor of the connected client socket.
 */
void handle_tcp_client( int client_fd );

#endif /* SERVER_FUNCTIONS_H */