#include <stdlib.h>     // malloc, free
#include <string.h>     // memcpy
#include <unistd.h>     // write, read
#include <arpa/inet.h>  // htons, ntohs
#include <errno.h>

#include "protocol.h"



/*
 * @brief  Writes all requested data to a file descriptor.
 *
 * @details Handles the "partial write" problem common in network programming.
 * It repeatedly calls write() until the entire buffer is transmitted
 * or an error occurs.
 *
 * @param  fd   The socket file descriptor.
 * @param  buf  Pointer to the buffer containing data.
 * @param  len  The size of the data in bytes.
 * @return int  Returns 0 on success, -1 if an error occurred.
 */
static int write_all( int fd, const void *buf, size_t len ) {

    size_t offset = 0;           // actual byte to write
    const uint8_t *p = buf;      // begin of datas

    while ( offset < len ) {       // do until all data are sent 

        ssize_t n = write( fd, p + offset, len - offset );    // actual sending
        if ( n <= 0 ){             // success?
            return -1;
        }

        offset += n;             // n is the count of sent bytes -> shift offset
    }

    return 0;                   // sned 0 when done
}

/*
 * @brief Sends a TLV (Type-Length-Value) packet over a file descriptor.
 *
 * @details This function handles byte-order conversion (Host to Network) ensuring
 * that integers are interpreted correctly regardless of the machine architecture.
 *
 * @param fd   Socket file descriptor.
 * @param type The message type (e.g., TLV_LOGIN).
 * @param data Pointer to the payload (structs, strings, etc.). Can be NULL if len is 0.
 * @param len  Length of the payload in bytes.
 * @return     0 on success, -1 on failure.
 */
int send_tlv( int fd, uint16_t type, const void *data, uint16_t len ){

    tlv_header_t hdr;               //header structure

    hdr.type = htons( type );        // network byte order
    hdr.length  = htons( len );

    /* Send TLV Header */
    if ( write_all( fd, &hdr, TLV_HEADER_LENGTH ) < 0 ){    
        return -1;
    }

    /* Send TLV datas */
    if ( len > 0 && data != NULL ) {                             // check if data exist 
        if ( write_all( fd, data, len ) < 0){                    // if yes, send all
            return -1;
        }
    }

    return 0;
}

/*
 * @brief  Reliably reads a specific number of bytes from a file descriptor.
 *
 * @details Unlike standard read(), which may return fewer bytes than requested
 * (a "partial read"), this function blocks and loops until exactly 
 * `len` bytes are received into the buffer.
 * * This is essential for network protocols (like TLV) where you must 
 * ensure a complete header is received before processing.
 *
 * @param  fd   The file descriptor (e.g., an active socket) to read from.
 * @param  buf  Pointer to the destination buffer.
 * @param  len  The exact number of bytes to read.
 * @return int  Returns 0 on success (all bytes read).
 * Returns -1 on failure (error or connection closed prematurely).
 */
static int read_all( int fd, void *buf, size_t len ) {

    size_t offset = 0;               // actual byte to read
    uint8_t *p = buf;                // begin of datas buffer

    while ( offset < len ){          // do until all data are read 

        ssize_t n = read( fd, p + offset, len - offset );     // reading
        if ( n <= 0 ){
            return -1;
        }

        offset += n;                // n is the count of read bytes -> shift offset
    }
    return 0;
}

/*
 * @brief  Receives a full TLV packet from the network.
 *
 * @details This function works in two stages:
 * 1. Reads the fixed-size header (Type and Length).
 * 2. Allocates memory dynamically based on the received Length.
 * 3. Reads the variable-size Value (payload) into the allocated memory.
 *
 * @warning The caller is responsible for freeing the memory allocated in `*data`!
 * Use `free(*data)` when you are done processing the message.
 *
 * @param  fd    The socket file descriptor.
 * @param  type  Output pointer where the message type will be stored.
 * @param  data  Output pointer to a pointer. The function sets this to the 
 * newly allocated buffer containing the payload.
 * @param  len   Output pointer where the payload length will be stored.
 * @return int   Returns 0 on success, -1 on failure (error, disconnect, or malloc fail).
 */
int recv_tlv( int fd, uint16_t *type, void **data, uint16_t *len ){

    tlv_header_t hdr;

    /* Read TLV header */
    if ( read_all( fd, &hdr, TLV_HEADER_LENGTH ) < 0 ){
        return -1;
    }

    *type = ntohs( hdr.type );  // host byte order
    *len  = ntohs( hdr.length );

    /* Read TLV datas */
    if ( *len > 0 ) {

        *data = malloc( *len );                //!!!!!!!!!!!!DANGER!!!!!!!!!!!!MEMORY LEAK POSSIBLE!!!!!!!!!!!!

        if ( *data == NULL ){
            return -1;
        }

        if ( read_all(fd, *data, *len ) < 0) {
            free( *data );
            return -1;
        }
    } else {

        *data = NULL;
    }

    return 0;
}