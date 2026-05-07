// N64 stub: fcaseopen — DFS is case-sensitive; lowercase everything after "rom:/"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

int casepath(char const *path, char *r) {
    if (r) strcpy(r, path);
    return 1;
}

FILE *fcaseopen(char const *path, char const *mode) {
    static char prefixed[512];
    int start;
    if (path[0] != 'r' || path[1] != 'o' || path[2] != 'm' || path[3] != ':') {
        snprintf(prefixed, sizeof(prefixed), "rom:/%s", path);
        start = 5; // skip "rom:/"
    } else {
        snprintf(prefixed, sizeof(prefixed), "%s", path);
        start = 5;
    }
    // Lowercase everything after "rom:/" — DFS stores files as lowercase
    for (int i = start; prefixed[i]; i++)
        prefixed[i] = tolower((unsigned char)prefixed[i]);
    return fopen(prefixed, mode);
}

void casechdir(char const *path) {}
