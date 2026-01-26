#ifndef GROUPS_H
#define GROUPS_H

#include <stdint.h>
#include <stddef.h>

#include "protocol.h"


typedef struct {
    char name[MAX_GROUP_NAME_LEN];
    char mcast_ip[16];
    uint16_t mcast_port;
    uint32_t id;
} group_info_t;


int group_exists( const char *groupname );

int groups_next_id(void);

int group_has_user(const char *groupname, const char *login);

int group_create(
    const char *groupname,
    const char *creator_login,
    group_info_t *out
);

int group_get_info(
    const char *groupname,
    group_info_t *out
);

int group_add_user(
    const char *groupname,
    const char *login
);

int group_list(char *out);

int group_send_user_groups(int client_fd, const char *login);

#endif //GROUPS_H