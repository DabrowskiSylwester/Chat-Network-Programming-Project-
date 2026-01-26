#ifndef GROUP_TYPES_H
#define GROUP_TYPES_H

#include <stdint.h>
#include <pthread.h>

#include "protocol.h"


#define MAX_GROUPS 16

typedef struct {
    char name[MAX_GROUP_NAME_LEN];
    char mcast_ip[16];
    uint16_t mcast_port;
    uint32_t id;
} group_info_t;

typedef struct {
    int sock;
    char groupname[MAX_GROUP_NAME_LEN];
    char mcast_ip[16];
    uint16_t mcast_port;
    int running;
    pthread_t recv_tid;
} group_ctx_t;

#endif
