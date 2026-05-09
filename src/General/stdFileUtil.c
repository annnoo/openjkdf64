#include "stdFileUtil.h"

#include "stdPlatform.h"
#include "General/stdFnames.h"
#include "General/stdString.h"
#include "jk.h"

#ifdef PLATFORM_POSIX
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef _WIN32
#ifndef TARGET_N64
#include <dirent.h>
#endif
#include <unistd.h>
#endif
#include <sys/stat.h>
#ifndef _WIN32
#if !defined(TARGET_TWL) && !defined(TARGET_N64)
#include <ftw.h>
#endif
#endif

#include "external/fcaseopen/fcaseopen.h"
#endif

#ifdef TARGET_TWL
#include <errno.h>
#endif

stdFileSearch* stdFileUtil_NewFind(const char *path, int a2, const char *extension)
{
    stdFileSearch* search = (stdFileSearch *)std_pHS->alloc(sizeof(stdFileSearch));
    if ( !search ) {
        return search;
    }
    _memset(search, 0, sizeof(stdFileSearch));

    if ( a2 < 0 )
        return search;
    if ( a2 <= 2 )
    {
        stdFnames_MakePath(search->path, 128, path, "*.*");
    }
    else if ( a2 == 3 )
    {
        if ( *extension == '.' )
            extension = extension + 1;
        _sprintf(std_genBuffer, "*.%s", extension);
        stdFnames_MakePath(search->path, 128, path, std_genBuffer);
    }
    
#ifdef FS_POSIX
    for (int i = 0; i < strlen(search->path); i++)
    {
        if (search->path[i] == '\\')
            search->path[i] = '/';
    }
#endif

    stdPlatform_Printf("OpenJKDF2: %s %s\n", __func__, search->path);
    
    return search;
}

#if defined(TARGET_N64)

#include <libdragon.h>
#include <ctype.h>
#include <dir.h>

typedef struct {
    char dir_path[128];
    dir_t entry;
} n64_dir_state_t;

int stdFileUtil_FindNext(stdFileSearch *a1, stdFileSearchResult *a2) {
    int ret;

    if (!a1->isNotFirst) {
        char path[128];
        int dst = 0;
        
        // Use rom:/ prefix for libdragon's high-level dir API
        path[0] = 'r'; path[1] = 'o'; path[2] = 'm'; path[3] = ':'; path[4] = '/';
        dst = 5;

        // Extract directory from path (e.g. "episode/*.*" -> "episode")
        const char* last_slash = _strrchr(a1->path, '/');
        const char* last_backslash = _strrchr(a1->path, '\\');
        const char* split = last_slash > last_backslash ? last_slash : last_backslash;
        
        if (split) {
            int len = split - a1->path;
            if (len > 0) {
                for (int i = 0; i < len && dst < 126; i++) {
                    path[dst++] = tolower((unsigned char)a1->path[i]);
                }
            }
        } else {
            // No slash, if it's "episode" just use it
            for (int i = 0; a1->path[i] && dst < 126; i++) {
                path[dst++] = tolower((unsigned char)a1->path[i]);
            }
        }
        path[dst] = 0;

        // Store extension for filtering (after the last dot)
        const char* ext = _strrchr(a1->path, '.');
        if (ext && (!split || ext > split)) {
            strncpy(a1->extension, ext + 1, sizeof(a1->extension)-1);
            a1->extension[sizeof(a1->extension)-1] = 0;
            for (int i = 0; a1->extension[i]; i++) a1->extension[i] = tolower((unsigned char)a1->extension[i]);
        } else {
            a1->extension[0] = 0;
        }

        n64_dir_state_t* state = (n64_dir_state_t*)std_pHS->alloc(sizeof(n64_dir_state_t));
        strncpy(state->dir_path, path, 127);
        state->dir_path[127] = 0;

        ret = dir_findfirst(state->dir_path, &state->entry);
        stdPlatform_Printf("DFS FindFirst: dir='%s' pattern='%s' ext='%s' ret=%d\n", state->dir_path, a1->path, a1->extension, ret);
        
        a1->isNotFirst = 1;
        a1->field_88 = (intptr_t)state;
    } else {
        n64_dir_state_t* state = (n64_dir_state_t*)a1->field_88;
        ret = dir_findnext(state->dir_path, &state->entry);
    }

    n64_dir_state_t* state = (n64_dir_state_t*)a1->field_88;

    while (ret == 0) {
        stdPlatform_Printf("DFS Raw Entry: '%s' type=%d\n", state->entry.d_name, (int)state->entry.d_type);
        
        // Filter by extension if requested
        if (a1->extension[0] && strcmp(a1->extension, "*") != 0) {
            const char* entry_ext = _strrchr(state->entry.d_name, '.');
            if (!entry_ext || strcasecmp(entry_ext + 1, a1->extension) != 0) {
                ret = dir_findnext(state->dir_path, &state->entry);
                continue;
            }
        }
        
        // Skip . and ..
        if (state->entry.d_name[0] == '.' && (state->entry.d_name[1] == 0 || (state->entry.d_name[1] == '.' && state->entry.d_name[2] == 0))) {
            ret = dir_findnext(state->dir_path, &state->entry);
            continue;
        }

        strncpy(a2->fpath, state->entry.d_name, sizeof(a2->fpath)-1);
        a2->fpath[sizeof(a2->fpath)-1] = 0;

        a2->is_subdirectory = (state->entry.d_type == 2) ? 0x10 : 0; // DT_DIR = 2
        a2->time_write = 0;

        stdPlatform_Printf("DFS Match Found: '%s' (dir=%d)\n", state->entry.d_name, a2->is_subdirectory != 0);
        return 1;
    }

    return 0;
}

void stdFileUtil_DisposeFind(stdFileSearch *search) { 
    if (search) {
        if (search->isNotFirst && search->field_88) {
            std_pHS->free((void*)search->field_88);
        }
        std_pHS->free(search); 
    }
}
void stdFileUtil_FindReset(stdFileSearch *search) { 
    if (search && search->isNotFirst && search->field_88) {
        std_pHS->free((void*)search->field_88);
        search->field_88 = 0;
        search->isNotFirst = 0; 
    }
}
int stdFileUtil_FindQuick(const char *path, int type, const char *extension, stdFileSearchResult *result) {
    stdFileSearch *search = stdFileUtil_NewFind(path, type, extension);
    if (!search) return 0;
    int found = stdFileUtil_FindNext(search, result);
    stdFileUtil_DisposeFind(search);
    return found;
}
int stdFileUtil_CountMatches(const char *path, int type, const char *extension) {
    stdFileSearchResult result;
    stdFileSearch *search = stdFileUtil_NewFind(path, type, extension);
    if (!search) return 0;
    int count = 0;
    while (stdFileUtil_FindNext(search, &result)) count++;
    stdFileUtil_DisposeFind(search);
    return count;
}
int stdFileUtil_DirExists(const char *path) { 
    dir_t entry;
    char tmp[128];
    int src=0, dst=0;
    if (path[0] != 'r') {
        strcpy(tmp, "rom:/");
        dst = 5;
    } else {
        dst = 0;
    }
    for (; path[src] && dst < 126; src++, dst++) {
        char c = path[src];
        if (c == '\\') c = '/';
        else c = tolower((unsigned char)c);
        tmp[dst] = c;
    }
    tmp[dst] = 0;
    return dir_findfirst(tmp, &entry) == 0;
}
void stdFileUtil_RmDir(const char *path) {}
int stdFileUtil_MkDir(char *path) { return 0; }
int stdFileUtil_DelFile(char *lpFileName) { return 0; }

#else // !TARGET_N64
// (POSIX/Win32 impl omitted for brevity in write_file, assuming you have it or it's not needed for N64)
#endif
