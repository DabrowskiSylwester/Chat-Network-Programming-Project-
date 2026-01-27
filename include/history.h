#ifndef HISTORY_H
#define HISTORY_H

#include <stdint.h>
#include <stddef.h>
#include "protocol.h"

//#define HISTORY_DIR "data/history/"


void make_history_filename(
    char * out,
    size_t out_size,
    const char * login1,
    const char * login2
);

int history_append_message(
    const char * login_src,
    const char * username_src,
    const char * login_dst,
    const char * message
);

#endif //HISTORY_H