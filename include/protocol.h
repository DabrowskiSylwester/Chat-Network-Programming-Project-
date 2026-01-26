#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

/* Maximum buffer sizes (subject to change) */
#define MAX_USERNAME_LEN    32     
#define MAX_PASSWORD_LEN    32
#define MAX_MESSAGE_LEN     1024
#define MAX_GROUP_NAME_LEN  32  
#define TLV_HEADER_LENGTH   4


/* -------------------------------------------------------------------------- */
/* TLV Types                                  */
/* -------------------------------------------------------------------------- */

/*
 * TLV_LOGIN       -> Contains the user's login.
 * TLV_PASSWORD    -> Contains the user's password.
 * TLV_COMMAND     -> Contains a command identifier (see command_t).
 * TLV_MESSAGE     -> Contains a text message.
 * TLV_USERNAME    -> Contains a display name (can differ from login).
 * TLV_GROUPNAME   -> Contains a group name.
 * TLV_HISTORY     -> Contains chat history data.
 * TLV_ACTIVE_USERS -> Contains user lists.
 * TLV_STATUS      -> Contains a status code (see status_t).
 */
typedef enum {
    TLV_LOGIN       = 1,
    TLV_PASSWORD,    
    TLV_COMMAND,     
    TLV_MESSAGE,
    TLV_USERNAME,
    TLV_GROUPNAME,
    TLV_GROUP_INFO,
    TLV_GROUP_LIST,
    TLV_HISTORY,
    TLV_ACTIVE_USERS,
    TLV_STATUS,
    TLV_UINT16
} tlv_type_t;

typedef enum {
    TLV_DISCOVER = 100,
    TLV_SERVER_INFO = 101
} tlv_discovery_t;

/* -------------------------------------------------------------------------- */
/*                                Commands                                    */
/* -------------------------------------------------------------------------- */

/* Commands used within the TLV_COMMAND type */
typedef enum {
    CMD_LOGIN       = 1,
    CMD_LOGOUT,
    CMD_CREATE_ACCOUNT,
    CMD_CHANGE_USERNAME,
    CMD_CHANGE_PASSWORD, 
    CMD_GET_ACTIVE_USERS,        /* Request list of active users, groups, etc. */
    CMD_SEND_TO_USER,
    CMD_SEND_TO_GROUP,
    CMD_CREATE_GROUP,
    CMD_LIST_GROUPS,
    CMD_JOIN_GROUP,
    CMD_GET_HISTORY
} command_t;

/* -------------------------------------------------------------------------- */
/*                           Status and Error Codes                           */
/* -------------------------------------------------------------------------- */

/* Status codes sent by the server to indicate success or failure */
typedef enum {
    STATUS_OK = 0,                /* Operation successful */
    STATUS_ERROR,                 /* General error */
    STATUS_AUTHENTICATION_ERROR,  /* Incorrect login or password */
    STATUS_ALREADY_LOGGED_IN,
    STATUS_USER_NOT_FOUND,
    STATUS_ALREADY_IN_GROUP,
    STATUS_GROUP_NOT_FOUND
} status_t;

/* -------------------------------------------------------------------------- */
/*                             TLV Structures                                 */
/* -------------------------------------------------------------------------- */

/* * Defines the binary format of the data:
 * - 2 bytes for Type
 * - 2 bytes for Length
 * - Variable bytes for Value (determined by Length)
 * * Padding is disabled via pragma to ensure correct network transmission.
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t type;
    uint16_t length;
} tlv_header_t;
#pragma pack(pop)

#pragma pack(push, 1)   
typedef struct {
    tlv_header_t header;
    uint8_t  value[];
} tlv_t;
#pragma pack(pop)

/* * Answer on TLV_DISCOVERY message.
 *  Contains ip address and port for client
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t ip;     // IPv4, network byte order
    uint16_t port;   // TCP port, network byte order
} server_info_t;
#pragma pack(pop)

/* -------------------------------------------------------------------------- */
/*                         Communication Functions                            */
/* -------------------------------------------------------------------------- */

/* * Sends a TLV packet.
 * Returns 0 on success, -1 on failure.
 */
int send_tlv(int fd, uint16_t type, const void *data, uint16_t len);

/* * Receives a TLV packet.
 *!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * NOTE: This function allocates memory for *data. The caller is responsible 
 * for freeing this memory using free(*data).
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * Returns 0 on success, -1 on failure.
 */
int recv_tlv(int fd, uint16_t *type, void **data, uint16_t *len);

#endif /* PROTOCOL_H */