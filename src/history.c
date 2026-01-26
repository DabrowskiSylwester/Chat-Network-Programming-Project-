#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <pthread.h>

#include "history.h"

void make_history_filename(
    char * out,
    size_t out_size,
    const char * login1,
    const char * login2
) {
    if ( strcmp( login1, login2 ) < 0 ) {   //it provides one order -> it defines only one name 
        snprintf( out, out_size, "%s_%s", login1, login2 );
    } else {
        snprintf( out, out_size, "%s_%s", login2, login1 );
    }
}

int history_append_message(
    const char * login_src,
    const char * username_src,
    const char * login_dst,
    const char * message
) {
    char filename[ 256 ];
    char path[ 512 ];

    make_history_filename(
        filename,
        sizeof( filename ),
        login_dst,
        login_src
    );

    snprintf(
        path,
        sizeof( path ),
        HISTORY_DIR "%s",
        filename
    );

    /* ensure directory exists */
    mkdir( HISTORY_DIR, 0755 );

    FILE * f = fopen( path, "a" );
    if ( !f ) {
        perror( "fopen history" );
        return -1;
    }

    /* timestamp */
    time_t now = time( NULL );
    struct tm * tm = localtime( &now );

    char timebuf[ 64 ];
    strftime(
        timebuf,
        sizeof( timebuf ),
        "%Y-%m-%d %H:%M:%S",
        tm
    );

    fprintf(
        f,
        "%s <%s> %s : %s\n",
        timebuf,
        login_src,
        username_src,
        message
    );

    fclose( f );
    return 0;
}

