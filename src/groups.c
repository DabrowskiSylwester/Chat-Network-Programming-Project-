#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <limits.h>

#include "groups.h"


#define GROUPS_DIR "data/groups/"
#define GROUP_MCAST_BASE "239.0.0."
#define GROUP_MCAST_PORT 7000
#define GROUP_MCAST_START 1

extern pthread_mutex_t groups_mutex;


int group_exists( const char *groupname ) {

    char path[256];
    snprintf(path, sizeof(path), "%s%s", GROUPS_DIR, groupname);

    return access(path, F_OK) == 0;
}

int groups_next_id(void)
{
    DIR *dir = opendir(GROUPS_DIR);
    if (!dir) return 1;

    struct dirent *de;
    int max_id = 0;
    char path[1024];
    char line[256];
    FILE *f;
    int id;

    while ((de = readdir(dir))) {
        if (de->d_name[0] == '.') continue;

        snprintf(path, sizeof(path), "%s%s",GROUPS_DIR, de->d_name);
        f = fopen(path, "r");
        if (!f) continue;

        while (fgets(line, sizeof(line), f)) {
            if (sscanf(line, "id=%d", &id) == 1) {
                if (id > max_id)
                    max_id = id;
                break;
            }
        }
        fclose(f);
    }

    closedir(dir);
    return max_id + 1;
}


int group_create(
    const char *groupname,
    const char *creator_login,
    group_info_t *out
) {
    char path[256];
    FILE *f;
    //printf( "[DEBUG] Group create function: \n");  

    snprintf(path, sizeof(path), "%s%s",GROUPS_DIR, groupname);
 
    /* grupa już istnieje */
    if (access(path, F_OK) == 0) {
        return -1;
    }
    

    int id = groups_next_id();

    snprintf(out->name, sizeof(out->name), "%s", groupname);
    snprintf(out->mcast_ip, sizeof(out->mcast_ip),
             GROUP_MCAST_BASE "%d",
             GROUP_MCAST_START + id);
    out->mcast_port = GROUP_MCAST_PORT+id;
    out->id = id;

    f = fopen(path, "w");
    if (!f) {
        return -1;
    }

    fprintf(f,
        "id=%u\n"
        "mcast=%s\n"
        "port=%u\n"
        "%s\n",
        out->id,
        out->mcast_ip,
        out->mcast_port,
        creator_login
    );
    

    fclose(f);
    
    return 0;
}

int group_has_user(const char *groupname, const char *login) {
    char path[256];
    snprintf(path, sizeof(path), "%s%s", GROUPS_DIR, groupname);

    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[256];
    int line_no = 0;

    while (fgets(line, sizeof(line), f)) {
        line_no++;

        /* metadatas */
        if (line_no <= 3)
            continue;

        line[strcspn(line, "\n")] = '\0';

        if (strcmp(line, login) == 0) {
            fclose(f);
            return 1;
        }
    }

    fclose(f);
    return 0;
}


int group_get_info( const char *groupname, group_info_t *out ) {
    char path[256];
    snprintf(path, sizeof(path), "%s%s", GROUPS_DIR, groupname);

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char line[128];

    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "id=%u", &out->id) == 1) 
            continue;
        if (sscanf(line, "mcast=%15s", out->mcast_ip) == 1)
            continue;
        if (sscanf(line, "port=%hu", &out->mcast_port) == 1)
            continue;
}
    strncpy( out->name, groupname, sizeof(out->name)-1 );

    fclose( f );
    return 0;
}

int group_add_user( const char *groupname, const char *login ) {
    
    char path[256];
    snprintf( path, sizeof(path), "%s%s", GROUPS_DIR, groupname );

    if (group_has_user(groupname, login)) {
        //pthread_mutex_unlock(&groups_mutex);
        return 1;   // Already in group
    }

    FILE *f = fopen(path, "a");
    if (!f) {
        //pthread_mutex_unlock( &groups_mutex );
        return -1;
    }

    fprintf(f, "%s\n", login);
    fclose(f);

    
    return 0;
}

int group_list(char *out) {
    DIR *dir = opendir(GROUPS_DIR);
    if (!dir) return -1;

    struct dirent *ent;
    size_t used = 0;

    while ((ent = readdir(dir))) {
        if (ent->d_name[0] == '.')
            continue;

        size_t len = strlen(ent->d_name);
        if (used + len + 1 >= MAX_MESSAGE_LEN)
            break;

        memcpy(out + used, ent->d_name, len);
        used += len;
        out[used++] = '\n';
    }

    closedir(dir);
    return used;
}


int group_send_user_groups(int client_fd, const char *login)
{
    DIR *dir = opendir(GROUPS_DIR);
    if (!dir) return -1;

    struct dirent *de;
    group_info_t g;

    //pthread_mutex_lock(&groups_mutex);

    while ((de = readdir(dir))) {
        if (de->d_name[0] == '.') continue;

        if (group_has_user(de->d_name, login)) {
            if (group_get_info(de->d_name, &g) == 0) {
                send_tlv(
                    client_fd,
                    TLV_GROUP_INFO,
                    &g,
                    sizeof(g)
                );
            }
        }
    }

    //pthread_mutex_unlock(&groups_mutex);
    closedir(dir);
    return 0;
}

