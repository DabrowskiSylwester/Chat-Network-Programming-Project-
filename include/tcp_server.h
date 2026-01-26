#ifndef SERVER_FUNCTIONS_H
#define SERVER_FUNCTIONS_H

#include <stddef.h> // Required for size_t
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

#define BACKLOG 10      //number of waiting TCP clients
#define HISTORY_OUT_MAX 8192

/* global mutex for shared resources */
extern pthread_mutex_t server_mutex;

/**
 * @brief Structure containing the context for a specific client connection.
 *
 * @details This structure is used to pass arguments to the client handling thread.
 * Since pthread_create accepts only a single argument, we wrap all necessary
 * data (socket descriptor, address, etc.) into this structure.
 * * NOTE: This structure should be dynamically allocated in the main thread
 * and freed inside the client_thread to avoid memory leaks.
 */
typedef struct {
    int client_fd;
} client_ctx_t;


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
// void handle_tcp_client( int client_fd );

/**
 * @brief Thread entry point for handling an individual TCP client.
 *
 * @details This function is executed in a separate thread for each connected client.
 * It is responsible for:
 * 1. Unpacking the `client_ctx_t` argument.
 * 2. Running the main communication loop (receive/process/send).
 * 3. Cleaning up resources (closing socket, freeing memory) upon disconnection.
 *
 * @param arg Pointer to `client_ctx_t` structure (needs casting).
 * @return void* Always returns NULL when the thread finishes.
 */
void * client_thread( void * arg );

#endif /* SERVER_FUNCTIONS_H */