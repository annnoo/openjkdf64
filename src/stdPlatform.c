#include "stdPlatform.h"

#include "Win95/std.h"
#include "General/stdMemory.h"
#include "Main/jkQuakeConsole.h"

#ifdef TARGET_TWL
#include <nds.h>
#include <sys/stat.h>
#include "Platform/TWL/dlmalloc.h"
#endif

#ifdef TARGET_N64
#include <libdragon.h>
extern void N64_PumpIdle(void);
#endif

#ifdef PLATFORM_POSIX
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <time.h>
#include <math.h>
#include <string.h>

#include "external/fcaseopen/fcaseopen.h"
#endif

#include "SDL2_helper.h"

#ifdef TARGET_N64
#define MAX_DFS_HANDLES 128
typedef struct {
    uint32_t handle;
    char path[128];
} DfsPathMap;
static DfsPathMap g_dfs_paths[MAX_DFS_HANDLES];

typedef struct {
    int dfs_handle;
    uint32_t file_pos;
    uint32_t file_size;
    uint32_t buf_pos;
    uint32_t buf_len;
    uint8_t buffer[4096];
} N64BufferedFile;
static FILE* g_n64_files[128];
static char g_n64_paths[128][128];

static void N64_MapHandleToPath(FILE* f, const char* path) {
    for (int i = 0; i < 128; i++) {
        if (g_n64_files[i] == NULL) {
            g_n64_files[i] = f;
            strncpy(g_n64_paths[i], path, 127);
            g_n64_paths[i][127] = 0;
            return;
        }
    }
    debugf("[N64_WARNING] Exceeded 128 open file handles! Path mapping lost for %s\n", path);
}

static const char* N64_GetPathForHandleInternal(FILE* f) {
    for (int i = 0; i < 128; i++) {
        if (g_n64_files[i] == f) {
            return g_n64_paths[i];
        }
    }
    return NULL;
}

static void N64_UnmapHandle(FILE* f) {
    for (int i = 0; i < 128; i++) {
        if (g_n64_files[i] == f) {
            g_n64_files[i] = NULL;
            g_n64_paths[i][0] = 0;
            return;
        }
    }
}

static stdFile_t N64_stdFileOpen(const char* fpath, const char* mode)
{
    // DFS is read-only
    if (mode[0] != 'r') return 0;

    // Convert backslashes, lowercase, and ensure rom:/ prefix
    char tmp[256];
    char tmp_case[256];
    int src = 0, dst = 0;

    // Skip any "rom:/" prefix the caller might have added
    if (fpath[0]=='r' && fpath[1]=='o' && fpath[2]=='m' && fpath[3]==':' && fpath[4]=='/') {
        src = 5;
    }

    // Ensure leading slash
    strcpy(tmp, "rom:/");
    strcpy(tmp_case, "rom:/");
    dst = 5;

    // Skip leading slash to avoid "rom://"
    if (fpath[src] == '/') {
        src++;
    }
  

    for (; fpath[src] && dst < 254; src++, dst++) {
        char c = fpath[src];
        char c_case = c;
        if (c == '\\') { c = '/'; c_case = '/'; }
        else c = tolower((unsigned char)c);
        tmp[dst] = c;
        tmp_case[dst] = c_case;
    }
    tmp[dst] = 0;
    tmp_case[dst] = 0;

    static int n64_fileopen_log_count = 0;
    if (n64_fileopen_log_count < 50) {
        debugf("[N64_fileOpen] normalized path: raw=\"%s\" -> \"%s\" (case=\"%s\")\n", fpath, tmp, tmp_case);
    }

    FILE* f = fopen(tmp, mode);
    if (!f) {
        // If it was a .wav request, try .wav64
        size_t len = strlen(tmp);
        if (len > 4 && strcmp(&tmp[len-4], ".wav") == 0) {
            strcpy(&tmp[len-4], ".wav64");
            f = fopen(tmp, mode);
            if (f) {
                debugf("[N64_fileOpen] redirected .wav -> .wav64: \"%s\" OK\n", tmp);
            } else {
                // Restore .wav for error message if .wav64 also failed
                strcpy(&tmp[len-4], ".wav");
            }
        }
    }

    if (!f) {
        debugf("[N64_fileOpen] fopen(\"%s\") FAILED\n", tmp);
        return 0;
    }

    if (n64_fileopen_log_count < 50) {
        n64_fileopen_log_count++;
    }

    N64_MapHandleToPath(f, tmp);

    debugf("[N64_fileOpen] fopen(\"%s\") OK\n", tmp);
    return (stdFile_t)f;
}

const char* N64_GetPathForHandle(stdFile_t fhand)
{
    return N64_GetPathForHandleInternal((FILE*)fhand);
}

static int N64_stdFileClose(stdFile_t fhand)
{
    if(!fhand) return -1;
    N64_UnmapHandle((FILE*)fhand);
    return fclose((FILE*)fhand);
}

static size_t N64_stdFileRead(stdFile_t fhand, void* dst, size_t len)
{
    FILE* f = (FILE*)fhand;
    size_t total_read = 0;
    const size_t CHUNK_SIZE = 16 * 1024;

    while (total_read < len)
    {
        size_t to_read = len - total_read;
        if (to_read > CHUNK_SIZE) to_read = CHUNK_SIZE;

        size_t ret = fread((char*)dst + total_read, 1, to_read, f);
        if (ret > 0)
        {
            total_read += ret;
        }

        N64_PumpIdle();

        if (ret < to_read) break; // EOF or error
    }

    if (total_read <= 40) debugf("[N64_fileRead] f=%p len=%u ret=%u\n",
        f, (uint32_t)len, (uint32_t)total_read);
    return total_read;
}

static size_t N64_stdFileWrite(stdFile_t fhand, void* dst, size_t len)
{
    return 0; // DFS is read-only
}

static const char* N64_stdFileGets(stdFile_t fhand, char* dst, size_t len)
{
    if (!fhand || len <= 1) return NULL;
    FILE* f = (FILE*)fhand;
    
    size_t i = 0;
    while (i < len - 1) {
        int c = fgetc(f);
        if (c == EOF) break;
        dst[i++] = (char)c;
        if (c == '\n') break;
    }
    dst[i] = 0;
    return (i == 0) ? NULL : dst;
}

static int N64_stdFseek(stdFile_t fhand, int a, int b)
{
    if (!fhand) return -1;
    return fseek((FILE*)fhand, a, b);
}

static int N64_stdFtell(stdFile_t fhand)
{
    return ftell((FILE*)fhand);
}

static int N64_stdFeof(stdFile_t fhand)
{
    return feof((FILE*)fhand);
}

uint32_t N64_TimeMs()
{
    return (uint32_t)TICKS_TO_MS(timer_ticks());
}
#endif

#ifdef PLATFORM_POSIX
uint32_t Linux_TimeMs()
{
    // TWL has hardware timers we can use for accurate ms timing
#if defined(TARGET_TWL)
    //16756
    //return (uint32_t)(((TIMER1_DATA*(1<<16))+TIMER0_DATA)/32.7285);
    return (uint32_t)(((uint64_t)((TIMER1_DATA*(1<<16))+TIMER0_DATA)<<9)/16757);
#elif defined(TARGET_N64)
    return N64_TimeMs();
#else

    struct timespec _t;

#if defined(_MSC_VER) && !defined(WIN64_MINGW)
    timespec_get(&_t, TIME_UTC);
#else
    clock_gettime(CLOCK_MONOTONIC, &_t);
#endif

    return _t.tv_sec*1000 + lround(_t.tv_nsec/1.0e6);
#endif
}

uint64_t Linux_TimeUs()
{
#if defined(TARGET_TWL)
    //return (uint64_t)((flex64_t)((TIMER1_DATA*(1<<16))+TIMER0_DATA)/0.0327285);
    return (uint64_t)(((uint64_t)((TIMER1_DATA*(1<<16))+TIMER0_DATA)<<19)/17159);
#elif defined(TARGET_N64)
    return (uint64_t)N64_TimeMs() * 1000;
#else
    struct timespec _t;

#if defined(_MSC_VER) && !defined(WIN64_MINGW)
    timespec_get(&_t, TIME_UTC);
#else
    clock_gettime(CLOCK_MONOTONIC, &_t);
#endif

    return _t.tv_sec*1000000 + lround(_t.tv_nsec/1.0e3);
#endif
}

static stdFile_t Linux_stdFileOpen(const char* fpath, const char* mode)
{
    char tmp[512];
    size_t len = strlen(fpath);

    if (len > 512) {
        len = 512;
    }
    _strncpy(tmp, fpath, sizeof(tmp));

#ifndef WIN64_STANDALONE
    for (int i = 0; i < len; i++)
    {
        if (tmp[i] == '\\') {
            tmp[i] = '/';
        }
    }
#endif

#ifdef WIN32
for (int i = 0; i < len; i++)
{
    if (tmp[i] == '/') {
        tmp[i] = '\\';
    }
}
#endif

    //printf("open: %s %s\n", fpath, mode);

    stdFile_t ret;
#ifndef TARGET_TWL
    ret = (stdFile_t)fcaseopen(tmp, mode);
#else
    if (mode[0] != 'w') {
        struct stat statstuff;
        int exists = stat(tmp, &statstuff) >= 0;
        if (exists) {
            ret = (stdFile_t)fopen(tmp, mode);
        }
        else {
            return 0;
        }
    }
    else {
        ret = (stdFile_t)fopen(tmp, mode);
    }
#endif
    //printf("File open `%s`->`%s` mode `%s`, ret %x\n", fpath, tmp, mode, ret);
    
    return ret;
}

static int Linux_stdFileClose(stdFile_t fhand)
{
    int ret = fclose((FILE*)fhand);

#ifdef ARCH_WASM
    EM_ASM(
        FS.syncfs(false, function (err) {
            // Error
        });
    );
#endif // ARCH_WASM

    return ret;
}


static size_t Linux_stdFileRead(stdFile_t fhand, void* dst, size_t len)
{
#ifdef TARGET_TWL
    if (!dst || !len) return 0;
#endif
    size_t val =  fread(dst, 1, len, (FILE*)fhand);

    return val;
}

static size_t Linux_stdFileWrite(stdFile_t fhand, void* dst, size_t len)
{
#ifdef TARGET_TWL
    if (!dst || !len) return 0;
#endif
    return fwrite(dst, 1, len, (FILE*)fhand);
}

static const char* Linux_stdFileGets(stdFile_t fhand, char* dst, size_t len)
{
    // Drops static.jkl animclass parsing from 21.87s to 13.578s due to slow locks on getc
#ifdef TARGET_TWL
    char tmp[128];
    const char* retval = dst;
    if (!dst || !len) return 0;
    while(1) {
        size_t res = fread(tmp, 1, sizeof(tmp), (FILE*)fhand);
        if (!res) break;
        for (size_t i = 0; i < res; i++) {
            char val = tmp[i];
            //if (val == '\r') continue;
            *dst++ = val;
            len--;
            if (val == '\n' || len == 1) {
                fseek((FILE*)fhand, (i+1)-res, SEEK_CUR);
                *dst++ = 0;
                return retval;
            }
            if (!val) {
                fseek((FILE*)fhand, (i+1)-res, SEEK_CUR);
                return retval;
            }
        }
    }
#else
    return fgets(dst, len, (FILE*)fhand);
#endif
}

static const wchar_t* Linux_stdFileGetws(stdFile_t fhand, wchar_t* dst, size_t len)
{
    // Can't use fgetws because -fshort-wchar makes wchar_t 2 bytes
    // but libc fgetws expects native wchar_t (4 bytes on POSIX).
    // Read UTF-16LE characters one at a time instead.
    if (!len) return NULL;
    size_t i = 0;
    while (i < len - 1) {
        wchar_t ch = 0;
        if (fread(&ch, sizeof(wchar_t), 1, (FILE*)fhand) != 1) {
            if (i == 0) return NULL;
            break;
        }
        dst[i++] = ch;
        if (ch == L'\n') break;
    }
    dst[i] = L'\0';
    return dst;
}

static int Linux_stdFseek(stdFile_t fhand, int a, int b)
{
    //printf("fseek? %x %x\n", a, b);
    int ret = fseek((FILE*)fhand, a, b);
    //printf("fseek %x\n", ret);
    return ret;
}

static int Linux_stdFtell(stdFile_t fhand)
{
    return ftell((FILE*)fhand);
}

static void* Linux_alloc(uint32_t len)
{
#ifdef TARGET_N64
    void* ret = memalign(8, len);
#else
    void* ret = malloc(len);
#endif
    if (ret) {
        memset(ret, 0, len);
    }
    return ret;
}

static void Linux_free(void* ptr)
{
    return free(ptr);
}

static void* Linux_realloc(void* ptr, uint32_t len)
{
    return realloc(ptr, len);
}

#ifdef TARGET_TWL

#define ALLOC_ALIGN (0x4)

//#define MEM_CHECKING
#define MEM_CHECKING_ADD (0x10)
#define MEM_CHECKING_ZERO_VAL (0x00)
#define MEM_CHECKING_VAL (0xAA)
#define MEM_CHECKING_VAL_FREE (0x55)

int heapSuggestion = HEAP_ANY;

size_t trackingAllocsA = 0;
size_t trackingAllocsAReal = 0;
size_t trackingAllocsB = 0;
size_t trackingAllocsBReal = 0;
size_t trackingAllocsBLimit = 0;
size_t trackingAllocsC = 0;
size_t trackingAllocsCReal = 0;
size_t activeAllocs = 0;

mspace openjkdf2_mem_main_mspace = NULL;
mspace openjkdf2_mem_alt_mspace = NULL;
mspace openjkdf2_mem_nwram_mspace = NULL;

intptr_t openjkdf2_mem_alt_mspace_start;
intptr_t openjkdf2_mem_alt_mspace_end;
intptr_t openjkdf2_mem_main_mspace_start;
intptr_t openjkdf2_mem_main_mspace_end;

typedef struct MemTrackingHeader {
    uint32_t memtype_size;
} tMemTrackingHeader;

#define HDR_MEMTYPE_RD(p) (((p)->memtype_size >> 24) & 0xFF)
#define HDR_SIZE_RD(p) ((p)->memtype_size & 0xFFFFFF)
#define HDR_SET(p,t,s) ((p)->memtype_size=(((t&0xFF)<<24) | (s & 0xFFFFFF)))

#ifdef __cplusplus
extern "C" {
#endif

extern void *__real_malloc(size_t size);
extern void __real_free(void *ptr);
extern void* __real_realloc(void *ptr, size_t len);
extern void *__real_calloc(size_t num, size_t size);

#ifdef __cplusplus
}
#endif

static int TWL_suggestHeap(int which) {
    int prev = heapSuggestion;
    heapSuggestion = which;
    return prev;
}


static void* TWL_mspace_alloc(mspace m, uint8_t marker, uint32_t len, uint32_t lenAlign, size_t* pTrackingAllocs, size_t* pTrackingAllocsReal) {
    if (!m) return NULL;

    void* ret = mspace_malloc(m, lenAlign);//malloc(len + sizeof(tMemTrackingHeader));
    if (ret) {
        *pTrackingAllocs += len;
        *pTrackingAllocsReal += lenAlign;
        activeAllocs += 1;
        //printf("%p %x\n", ret, len + sizeof(tMemTrackingHeader));
#ifdef MEM_CHECKING
        memset(ret, MEM_CHECKING_ZERO_VAL, len);
        memset((uint8_t*)ret+len, MEM_CHECKING_VAL, lenAlign-len);
#else
        memset(ret, 0, len);
#endif
        HDR_SET((tMemTrackingHeader*)ret, marker, len);
        return (void*)(((intptr_t)ret) + sizeof(tMemTrackingHeader));
    }
    return NULL;
}

static void TWL_mspace_free(mspace m, tMemTrackingHeader* pHdr, uint32_t size, uint32_t sizeAlign, size_t* pTrackingAllocs, size_t* pTrackingAllocsReal) {
    void* ptr = (void*)(pHdr+1);
    *pTrackingAllocs -= size;
    *pTrackingAllocsReal -= sizeAlign;
    activeAllocs -= 1;
    HDR_SET(pHdr, 0xDE, 0);
#ifdef MEM_CHECKING
    memset(ptr, MEM_CHECKING_VAL_FREE, sizeAlign);
#endif
    mspace_free(m, (void*)pHdr);
}

static void* TWL_mspace_realloc(mspace m, uint8_t marker, tMemTrackingHeader* pHdr, uint32_t oldSize, uint32_t sizeAlign, uint32_t len, uint32_t lenAlign, size_t* pTrackingAllocs, size_t* pTrackingAllocsReal) {
    void* ptr = (void*)(pHdr+1);

    // TODO: force fallback if heapSuggest doesn't match what we have

    void* ret = mspace_realloc(m, (void*)pHdr, lenAlign);
    if (ret) {
        pHdr = (tMemTrackingHeader*)ret;
        *pTrackingAllocs -= oldSize;
        *pTrackingAllocs += len;
        *pTrackingAllocsReal -= sizeAlign;
        *pTrackingAllocsReal += lenAlign;
#ifdef MEM_CHECKING
        if (lenAlign > sizeAlign && len > oldSize) {
            memset((uint8_t*)ret + sizeAlign, MEM_CHECKING_VAL, lenAlign - sizeAlign);
        }
#endif
        HDR_SET(pHdr, marker, len);
        return (void*)((intptr_t)ret + sizeof(tMemTrackingHeader));
    }

    printf("realloc Fallback %x??\n", marker);

    // Fallback option
    ret = Linux_alloc(len);
    if (ret) {
        memcpy(ret, ptr, oldSize);
        Linux_free(ptr);
        return ret;
    }

    printf("aaaaaaa realloc fail\n");
    return NULL;
}

static void* TWL_alloc(uint32_t len)
{
    static BOOL bDontTryAgain = 0;
    if (!len) {
        return NULL;
    }
    // Why?
    uint32_t lenAlign = ((len + sizeof(tMemTrackingHeader)) + (ALLOC_ALIGN-1)) & ~(ALLOC_ALIGN-1);
#ifdef MEM_CHECKING
    lenAlign += MEM_CHECKING_ADD;
#endif
    void* ret = NULL;

    // TODO wrap system init or something for this
    if (!trackingAllocsBLimit) {
        trackingAllocsBLimit = (intptr_t)getHeapLimit() - (intptr_t)getHeapEnd();
        void* mainBase = NULL;
        while (!mainBase) {
            mainBase = __real_malloc(trackingAllocsBLimit);
            if (!mainBase) {
                trackingAllocsBLimit -= 0x1000;
            }
        }
        openjkdf2_mem_main_mspace_start = (intptr_t)mainBase;
        openjkdf2_mem_main_mspace_end = (intptr_t)mainBase + trackingAllocsBLimit;
        //printf("mainBase %p\n", mainBase);
        openjkdf2_mem_main_mspace = create_mspace_with_base(mainBase, trackingAllocsBLimit, 0);

        // Also map NWRAM
        if (isDSiMode()) {
            intptr_t nwramStart = 0x03000000;
            intptr_t nwramEnd = 0x03040000 + 0x40000;
            nwramSetBlockMapping(NWRAM_BLOCK_B, 0x03000000, 256 * 1024,
                         NWRAM_BLOCK_IMAGE_SIZE_256K);
            nwramSetBlockMapping(NWRAM_BLOCK_C, 0x03040000, 256 * 1024,
                         NWRAM_BLOCK_IMAGE_SIZE_256K);
            openjkdf2_mem_nwram_mspace = create_mspace_with_base((void*)nwramStart, nwramEnd-nwramStart, 0);
        }
    }

    // Go to NWRAM for fast heap suggestions
    if (heapSuggestion == HEAP_FAST) {
        if (ret = TWL_mspace_alloc(openjkdf2_mem_nwram_mspace, 0xD5, len, lenAlign, &trackingAllocsC, &trackingAllocsCReal)) {
            return ret;
        }
    }

    if(ret = TWL_mspace_alloc(openjkdf2_mem_alt_mspace, 0xF0, len, lenAlign, &trackingAllocsA, &trackingAllocsAReal)) {
        return ret;
    }

    if (trackingAllocsBReal + lenAlign < trackingAllocsBLimit) {
        if (ret = TWL_mspace_alloc(openjkdf2_mem_main_mspace, 0xDA, len, lenAlign, &trackingAllocsB, &trackingAllocsBReal)) {
            return ret;
        }
    }

    if (heapSuggestion != HEAP_AUDIO) {
        if (ret = TWL_mspace_alloc(openjkdf2_mem_nwram_mspace, 0xD5, len, lenAlign, &trackingAllocsC, &trackingAllocsCReal)) {
            return ret;
        }
    }

    //uint32_t freeEst = (intptr_t)getHeapLimit() - (intptr_t)getHeapEnd();

    printf("already out? %zx %zx %zx\n", trackingAllocsA, trackingAllocsB, trackingAllocsC);

    // Emergency freeing measures
    if (!bDontTryAgain) {
        extern int sithSound_FreeUpMemory(uint32_t);
        extern int rdMaterial_PurgeEntireMaterialCache();
        if (!sithSound_FreeUpMemory(len)) {
            if (!rdMaterial_PurgeMaterialCache()) {
                rdMaterial_PurgeEntireMaterialCache();
            }
        }
        
        bDontTryAgain = 1;
        ret = TWL_alloc(len);
        bDontTryAgain = 0;
    }

    if (!ret) {
        printf("Failed to allocate %x bytes...\n", len);
        //while (1) {}
        return NULL;
    }
    
    return ret;
}

static void TWL_free(void* ptr)
{
    if (!ptr) {
        return;
    }

    tMemTrackingHeader* pHdr = (tMemTrackingHeader*)((intptr_t)ptr - sizeof(tMemTrackingHeader));
    uint32_t sizeAlign = ((HDR_SIZE_RD(pHdr) + sizeof(tMemTrackingHeader)) + (ALLOC_ALIGN-1)) & ~(ALLOC_ALIGN-1);
    uint32_t size = HDR_SIZE_RD(pHdr);
    uint8_t memtype = HDR_MEMTYPE_RD(pHdr);

#ifdef MEM_CHECKING
    sizeAlign += MEM_CHECKING_ADD;
#endif

#ifdef MEM_CHECKING
    for (uint32_t i = size; i < sizeAlign - sizeof(tMemTrackingHeader); i++) {
        if (*((uint8_t*)ptr + i) != MEM_CHECKING_VAL) {
            printf("OOB write!! %p %x %x\n", pHdr, size, sizeAlign - sizeof(tMemTrackingHeader));
            while(1);
        }
    }
#endif

    if (memtype == 0xF0 /*|| ((intptr_t)pHdr >= openjkdf2_mem_alt_mspace_start && (intptr_t)pHdr <= openjkdf2_mem_alt_mspace_end)*/) {
        TWL_mspace_free(openjkdf2_mem_alt_mspace, pHdr, size, sizeAlign, &trackingAllocsA, &trackingAllocsAReal);
        return;
    }
    else if (HDR_MEMTYPE_RD(pHdr) == 0xDA /*|| ((intptr_t)pHdr >= openjkdf2_mem_main_mspace_start && (intptr_t)pHdr <= openjkdf2_mem_main_mspace_end)*/) {
        TWL_mspace_free(openjkdf2_mem_main_mspace, pHdr, size, sizeAlign, &trackingAllocsB, &trackingAllocsBReal);
        return;
    }
    else if (HDR_MEMTYPE_RD(pHdr) == 0xD5 /*|| ((intptr_t)pHdr >= openjkdf2_mem_main_mspace_start && (intptr_t)pHdr <= openjkdf2_mem_main_mspace_end)*/) {
        TWL_mspace_free(openjkdf2_mem_nwram_mspace, pHdr, size, sizeAlign, &trackingAllocsC, &trackingAllocsCReal);
        return;
    }
    else if (memtype == 0xDE) {
        printf("Double free? %p\n", ptr);
        while(1);
    }
    else {
        printf("Where does this go?? %p %x\n", ptr, memtype);
        while(1);
    }
    /*else {
        __real_free(ptr);
    }*/
}

static void* TWL_realloc(void* ptr, uint32_t len)
{
    if (!len) { return NULL; }
    if (!ptr) {
        return Linux_alloc(len);
    }
    extern mspace openjkdf2_mem_alt_mspace;
    tMemTrackingHeader* pHdr = (tMemTrackingHeader*)((intptr_t)ptr - sizeof(tMemTrackingHeader));
    uint32_t oldSize = HDR_SIZE_RD(pHdr);
    uint32_t sizeAlign = ((HDR_SIZE_RD(pHdr) + sizeof(tMemTrackingHeader)) + (ALLOC_ALIGN-1)) & ~(ALLOC_ALIGN-1);
    uint32_t lenAlign = ((len + sizeof(tMemTrackingHeader)) + (ALLOC_ALIGN-1)) & ~(ALLOC_ALIGN-1);

#ifdef MEM_CHECKING
    sizeAlign += MEM_CHECKING_ADD;
    lenAlign += MEM_CHECKING_ADD;
#endif

#ifdef MEM_CHECKING
    for (uint32_t i = HDR_SIZE_RD(pHdr); i < sizeAlign - sizeof(tMemTrackingHeader); i++) {
        if (*((uint8_t*)ptr + i) != MEM_CHECKING_VAL) {
            printf("OOB write!! %p %x %x, %x %x\n", pHdr, HDR_SIZE_RD(pHdr), sizeAlign - sizeof(tMemTrackingHeader), len, lenAlign - sizeof(tMemTrackingHeader));
            while(1);
        }
    }
#endif

    void* ret = NULL;
    if (HDR_MEMTYPE_RD(pHdr) == 0xF0) {
        return TWL_mspace_realloc(openjkdf2_mem_alt_mspace, 0xF0, pHdr, oldSize, sizeAlign, len, lenAlign, &trackingAllocsA, &trackingAllocsAReal);
    }
    else if (HDR_MEMTYPE_RD(pHdr) == 0xDA) {
        return TWL_mspace_realloc(openjkdf2_mem_main_mspace, 0xDA, pHdr, oldSize, sizeAlign, len, lenAlign, &trackingAllocsB, &trackingAllocsBReal);
    }
    else if (HDR_MEMTYPE_RD(pHdr) == 0xD5) {
        return TWL_mspace_realloc(openjkdf2_mem_nwram_mspace, 0xD5, pHdr, oldSize, sizeAlign, len, lenAlign, &trackingAllocsC, &trackingAllocsCReal);
    }
    else if (HDR_MEMTYPE_RD(pHdr) == 0xDE) {
        printf("Double free realloc? %p\n", ptr);
        while(1);
    }
    else {
        printf("Where does this go?? %p %d\n", ptr, len);
        while(1);
        //return __real_realloc(ptr, len);
    }
}



#ifdef __cplusplus
extern "C" {
#endif

void *__wrap_malloc(uint32_t size) {
    return TWL_alloc(size);
}

void __wrap_free(void *ptr) {
    TWL_free(ptr);
}

void* __wrap_realloc(void *ptr, uint32_t len) {
    return TWL_realloc(ptr, len);
}

void *__wrap_calloc(size_t num, size_t size) {
    return TWL_alloc(num*size);
}

extern int64_t __real___muldi3(int32_t a, int32_t b);

__attribute__((naked)) int64_t __wrap___muldi3(int32_t a, int32_t b) {
    __asm__ (
        ".align 4\n"
        ".thumb\n"
        "bx pc\n"
        "nop\n"
        ".arm\n"

        "mul r3, r0, r3\n"
        "mla r3, r2, r1, r3\n"
        "umull   r0, r1, r2, r0\n"
        "add r1, r3\n"
    
        "bx lr\n"
    );
}

__attribute__((naked)) int64_t __wrap___aeabi_lmul(int64_t a, int64_t b) {
    __asm__ (
        ".align 4\n"
        ".thumb\n"
        "mul r3, r0, r3\n"
        "bx pc\n"
        ".arm\n"

        
        "mla r3, r2, r1, r3\n"
        "umull   r0, r1, r2, r0\n"
        "add r1, r3\n"
    
        "bx lr\n"
    );
}

__attribute__((naked)) int64_t __smull_helper(int32_t a, int32_t b) {
    __asm__ (
        ".align 4\n"
        ".thumb\n"
        "mov r3, r0\n"
        "mov r2, r1\n"
        "bx pc\n"
        "nop\n"
        ".arm\n"

        "smull   r0, r1, r2, r3\n"
    
        "bx lr\n"
    );
}

#ifdef __cplusplus
}
#endif

#endif

static int Linux_stdFeof(stdFile_t fhand)
{
    return feof((FILE*)fhand);
}

uint32_t stdPlatform_GetTimeMsec()
{
    return Linux_TimeMs();
}
#endif

#ifdef STDPLATFORM_HEAP_SUGGESTIONS
static int Dummy_suggestHeap(int which) { return HEAP_ANY; }
#endif

#ifndef PLATFORM_POSIX
void stdPlatform_Assert(const char *msg, const char *file, int line)
{
    char buf[512];
    int lastSlash = 0;
    for (int i = 0; file[i]; i++)
    {
        if ( file[i] == '\\' )
            lastSlash = i;
    }
    _sprintf(buf, "%s\n(%s, %d)\n", msg, &file[lastSlash ? lastSlash + 1 : 0], line);
    jk_printf("ASSERT: %s", buf);
}

void* stdPlatform_AllocHandle(uint32_t size)
{
    return _malloc(size);
}

void stdPlatform_FreeHandle(void *ptr)
{
    _free(ptr);
}

void* stdPlatform_ReallocHandle(void *ptr, uint32_t size)
{
    return _realloc(ptr, size);
}

void* stdPlatform_LockHandle(void *ptr)
{
    return ptr;
}

void stdPlatform_UnlockHandle(void *ptr)
{
}

void stdPlatform_GetDateTime(char *out, uint32_t outLen)
{
    SYSTEMTIME st;
    char tmp[80];
    GetLocalTime(&st);
    const char *ampm = (st.wHour >= 13) ? "pm" : "am";
    uint16_t hour12 = (st.wHour >= 13) ? st.wHour - 12 : st.wHour;
    _sprintf(tmp, "%d/%d/%02d %d:%02d %s", st.wMonth, st.wDay, st.wYear, hour12, st.wMinute, ampm);
    _strncpy(out, tmp, outLen);
}
#endif // !PLATFORM_POSIX

void stdPlatform_InitServices(HostServices *handlers)
{
    handlers->statusPrint = stdPlatform_Printf;
    handlers->messagePrint = stdPlatform_Printf;
    handlers->warningPrint = stdPlatform_Printf;
    handlers->errorPrint = stdPlatform_Printf;
    handlers->some_float = 1000.0;
    handlers->debugPrint = 0;
#ifndef PLATFORM_POSIX
    handlers->assert = stdPlatform_Assert;
#endif
    handlers->unk_0 = 0;
#ifndef PLATFORM_POSIX
    handlers->alloc = daAlloc;
    handlers->free = daFree;
    handlers->realloc =  daRealloc;
    handlers->getTimerTick = stdPlatform_GetTimeMsec;
    handlers->fileOpen = stdFileOpen;
    handlers->fileClose = stdFileClose;
    handlers->fileRead = stdFileRead;
    handlers->fileGets = stdFileGets;
    handlers->fileWrite = stdFileWrite;
    handlers->fileEof = stdFeof;
    handlers->ftell = stdFtell;
    handlers->fseek = stdFseek;
    handlers->fileSize = stdFileSize;
    handlers->filePrintf = stdFilePrintf;
    handlers->fileGetws = stdFileGetws;
    handlers->allocHandle = stdPlatform_AllocHandle;
    handlers->freeHandle = stdPlatform_FreeHandle;
    handlers->reallocHandle = stdPlatform_ReallocHandle;
    handlers->lockHandle = stdPlatform_LockHandle;
    handlers->unlockHandle = stdPlatform_UnlockHandle;
#endif

#ifdef PLATFORM_POSIX
    handlers->alloc = Linux_alloc;
    handlers->free = Linux_free;
    handlers->realloc = Linux_realloc;
    handlers->fileOpen = Linux_stdFileOpen;
    handlers->fileClose = Linux_stdFileClose;
    handlers->fileRead = Linux_stdFileRead;
    handlers->fileGets = Linux_stdFileGets;
    handlers->fileWrite = Linux_stdFileWrite;
    handlers->fileGetws = Linux_stdFileGetws;
    handlers->fseek = Linux_stdFseek;
    handlers->ftell = Linux_stdFtell;
    handlers->getTimerTick = Linux_TimeMs;
    handlers->fileEof = Linux_stdFeof;
#endif

#ifdef STDPLATFORM_HEAP_SUGGESTIONS
   handlers->suggestHeap = Dummy_suggestHeap;
#endif

#ifdef TARGET_TWL
    handlers->alloc = TWL_alloc;
    handlers->free = TWL_free;
    handlers->realloc = TWL_realloc;
    handlers->suggestHeap = TWL_suggestHeap;
#endif

#ifdef TARGET_N64
    handlers->fileOpen = N64_stdFileOpen;
    handlers->fileClose = N64_stdFileClose;
    handlers->fileRead = N64_stdFileRead;
    handlers->fileGets = N64_stdFileGets;
    handlers->fileWrite = N64_stdFileWrite;
    handlers->fseek = N64_stdFseek;
    handlers->ftell = N64_stdFtell;
    handlers->fileEof = N64_stdFeof;
    handlers->getTimerTick = N64_TimeMs;
#endif
}

int stdPlatform_Startup()
{
    return 1;
}

#ifdef PLATFORM_POSIX
int stdPrintf(int (*a1)(const char *, ...), const char *a2, int line, const char *fmt, ...)
{
    va_list args;
    va_start (args, fmt);
    printf("(%p %s:%d) ", a1, a2, line);
    int ret = vprintf(fmt, args);
    va_end (args);
    return ret;
}

#ifdef SDL2_RENDER
static SDL_mutex* stdPlatform_mtxPrintf = NULL;
#endif

int stdPlatform_Printf(const char *fmt, ...)
{
    char tmp[256];
    va_list args;

#ifdef SDL2_RENDER
    if (!stdPlatform_mtxPrintf)
        stdPlatform_mtxPrintf = SDL_CreateMutex();

    SDL_LockMutex(stdPlatform_mtxPrintf);
#endif
    
    va_start(args, fmt);
#ifdef TARGET_N64
    int ret = vfprintf(stderr, fmt, args);
#else
    int ret = vprintf(fmt, args);
#endif
    va_end(args);

#ifdef QUAKE_CONSOLE
    va_start (args, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, args);
    jkQuakeConsole_PrintLine(tmp);
    va_end(args);
#endif

#ifdef TARGET_ANDROID
    LOGI("%s", tmp);
#endif

#ifdef SDL2_RENDER
    SDL_UnlockMutex(stdPlatform_mtxPrintf);
#endif
    return ret;
}
#endif

#ifdef TARGET_N64
void stdPlatform_PrintHeapStats()
{
    heap_stats_t stats;
    sys_get_heap_stats(&stats);
    stdPlatform_Printf("N64 Heap: used=%d total=%d free=%d\n", 
        stats.used, stats.total, stats.total - stats.used);
}
#endif

#ifdef TARGET_TWL
void stdPlatform_PrintHeapStats()
{
    size_t waste = (trackingAllocsAReal - trackingAllocsA) + (trackingAllocsBReal - trackingAllocsB) + (trackingAllocsCReal - trackingAllocsC);
    stdPlatform_Printf("heap ext=0x%zx mn=0x%zx\nnw=0x%zx wst=0x%zx\nnum=%zd\n", trackingAllocsA, trackingAllocsB, trackingAllocsC, waste, activeAllocs);
}
#endif
