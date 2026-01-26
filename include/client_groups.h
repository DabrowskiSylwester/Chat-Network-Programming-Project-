#ifndef CLIENT_GROUPS_H
#define CLIENT_GROUPS_H

#include <stdint.h>
#include <stddef.h>

#include "protocol.h"
#include "group_types.h"
//#include "client_ui.h"


// typedef struct {
//     char name[MAX_GROUP_NAME_LEN];
//     char mcast_ip[16];
//     uint16_t mcast_port;
//     uint32_t id;
// } group_info_t;

// typedef struct {
//     int sock;
//     char groupname[MAX_GROUP_NAME_LEN];
//     char mcast_ip[16];
//     uint16_t mcast_port;
//     int running;
//     pthread_t recv_tid;
    
// } group_ctx_t;
/* forward declaration */
typedef struct client_ctx client_ctx_t;

int client_group_create(int sock,  const char *groupname);

int client_group_list(int sock);

int client_group_join(int sock, const char *name);

int join_multicast(
    group_ctx_t *ctx,
    const char *groupname,
    const char *mcast_ip,
    uint16_t port
);

group_ctx_t *find_group_ctx(client_ctx_t *ctx, const char *name) ;

int leave_multicast(group_ctx_t *g);
void leave_all_groups(client_ctx_t *ctx);

#endif //GROUPS_H