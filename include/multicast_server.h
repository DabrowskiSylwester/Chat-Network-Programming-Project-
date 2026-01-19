#ifndef MULTICAST_SERVER_H
#define MULTICAST_SERVER_H

#include <stddef.h> // Required for size_t
#include <stdlib.h>
#include <stdint.h>

/**
 * @brief Context structure for the multicast handling thread.
 *
 * @details This structure allows passing multiple configuration parameters 
 * to the multicast thread via the single `void *` argument allowed by pthread_create.
 * It contains all necessary information to set up the multicast listener
 * and advertise the TCP server's location.
 */

typedef struct {
    const char * mcast_addr;
    uint16_t     mcast_port;
    uint16_t     tcp_port;
} multicast_ctx_t;

/**
 * @brief Thread entry point for the multicast service discovery mechanism.
 *
 * @details This function is designed to be run in a separate thread. Its main responsibilities are:
 * 1. Creating and binding a UDP socket.
 * 2. Joining the specified multicast group (`mcast_addr`).
 * 3. Listening for broadcast/multicast queries from clients.
 * 4. Responding to clients with the server's TCP port (`tcp_port`).
 *
 * @param arg Pointer to a `multicast_ctx_t` structure containing configuration.
 * NOTE: The thread is expected to cast this to `multicast_ctx_t*`.
 * Memory management of `arg` depends on implementation (usually malloc'd in main).
 * * @return void* Returns NULL upon thread termination (usually runs indefinitely).
 */
void * multicast_thread( void * arg );

/**
 * @brief Retrieves the local IP address of the active network interface.
 *
 * @details This function determines the local IP address by creating a UDP socket
 * and "connecting" it to a public DNS server (8.8.8.8). 
 * * NOTE: This does NOT actually send any packets over the network. 
 * "Connecting" a UDP socket simply tells the kernel to check the routing table 
 * and decide which local interface (and IP) would be used to reach that external address.
 * This is a standard trick to get the real IP instead of 127.0.0.1.
 *
 * @param ip_buf  Buffer where the IP string (e.g., "192.168.1.5") will be stored.
 * @param buf_len The size of the provided buffer (INET_ADDRSTRLEN is recommended).
 * @return int    Returns 0 on success, -1 on failure (e.g., no network interface up).
 */
int get_local_ip( char * ip_buf, size_t buf_len );

/** 
 * @brief Starts multicast discovery service.
 * Blocks in receive loop.
 *
 * @param mcast_addr   multicast IPv4 address (string)
 * @param mcast_port   multicast UDP port
 * @param tcp_port     TCP port advertised to clients
 *
 * @return UDP socket fd on success, -1 on error
 */
// int multicast_server_run(
//     const char * mcast_addr,
//     uint16_t     mcast_port,
//     uint16_t     tcp_port
// );


#endif