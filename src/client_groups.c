#define _GNU_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>   
#include <arpa/inet.h>
#include <pthread.h>


#include "client_groups.h"
#include "client_ui.h"

int client_group_create(int sock,  const char *groupname)
{
    command_t cmd = CMD_CREATE_GROUP;
   
    send_tlv(sock, TLV_COMMAND, &cmd, sizeof(cmd));
    send_tlv(sock, TLV_GROUPNAME, groupname, strlen(groupname));

    return 0;   
}

int client_group_list(int sock) {
    command_t cmd = CMD_LIST_GROUPS;
    send_tlv(sock, TLV_COMMAND, &cmd, sizeof(cmd));
    return 0;   
}

int client_group_join(int sock, const char *name) {
    command_t cmd = CMD_JOIN_GROUP;
    send_tlv(sock, TLV_COMMAND, &cmd, sizeof(cmd));
    send_tlv(sock, TLV_GROUPNAME, name, strlen(name));
    return 0;
}

static void * group_recv_thread(void *arg) {
    group_ctx_t *ctx = arg;
    char buf[MAX_MESSAGE_LEN+1];

    while (ctx->running) {
        ssize_t n = recvfrom(
            ctx->sock,
            buf,
            sizeof(buf) - 1,
            0,
            NULL,
            NULL
        );

        if (n <= 0)
            continue;

        buf[n] = '\0';


        pthread_mutex_lock(&print_mutex);
        printf(
            ANSI_COLOR_CYAN"\n[group:%s]\033[0m %s\n> "ANSI_COLOR_RESET,
            ctx->groupname,
            buf
        );
        fflush(stdout);
        pthread_mutex_unlock(&print_mutex);
    }
    return NULL;
}

int join_multicast(
    group_ctx_t *ctx,
    const char *groupname,
    const char *mcast_ip,
    uint16_t port
) {
    struct sockaddr_in addr;
    struct ip_mreq mreq;
    

    memset(ctx, 0, sizeof(*ctx));
    ctx->running = 1; 

    ctx->sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (ctx->sock < 0) {
        perror("socket");
        return -1;
    }

    int reuse = 1;
    setsockopt(ctx->sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(ctx->sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(ctx->sock);
        return -1;
    }

    inet_pton(AF_INET, mcast_ip, &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if (setsockopt(
            ctx->sock,
            IPPROTO_IP,
            IP_ADD_MEMBERSHIP,
            &mreq,
            sizeof(mreq)
        ) < 0) {
        perror("IP_ADD_MEMBERSHIP");
        close(ctx->sock);
        return -1;
    }

    strncpy(ctx->groupname, groupname, sizeof(ctx->groupname)-1);
    strncpy(ctx->mcast_ip, mcast_ip, sizeof(ctx->mcast_ip)-1);
    ctx->mcast_port = port;
    ctx->running = 1;

    pthread_create(
        &ctx->recv_tid,
        NULL,
        group_recv_thread,
        ctx
    );

    printf(ANSI_COLOR_GREEN"\n[group] joined multicast %s (%s:%u)\n"
            ANSI_COLOR_RESET">" ,
           groupname, mcast_ip, port);

    return 0;
}

group_ctx_t *find_group_ctx(client_ctx_t *ctx, const char *name) {
    for (int i = 0; i < ctx->group_count; i++) {
        if (strcmp(ctx->groups[i].groupname, name) == 0)
            return &ctx->groups[i];
    }
    return NULL;
}

int leave_multicast(group_ctx_t *g)
{
    if (!g->running)
        return 0;

    g->running = 0;

    printf(
        ANSI_COLOR_YELLOW"\n[group] leaving %s (%s:%u)\n"
        ANSI_COLOR_RESET"> ",
        g->groupname,
        g->mcast_ip,
        g->mcast_port
    );
    fflush(stdout);

    struct ip_mreq mreq;
    inet_pton(AF_INET, g->mcast_ip, &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    setsockopt(
        g->sock,
        IPPROTO_IP,
        IP_DROP_MEMBERSHIP,
        &mreq,
        sizeof(mreq)
    );

    
    close(g->sock);

   
    pthread_join(g->recv_tid, NULL);

    memset(g, 0, sizeof(*g));
    return 0;
}

void leave_all_groups(client_ctx_t *ctx)
{
    for (int i = 0; i < ctx->group_count; i++) {
        leave_multicast(&ctx->groups[i]);
    }
    ctx->group_count = 0;
}
