/*
 * cogc/main.c -- COG script compiler
 *
 * Reads a .cog file and writes a binary .bcog file loadable by the N64
 * runtime without a flex/yacc parser on-device.
 *
 * Usage: cogc <input.cog> [output.bcog]
 *   If output path is omitted, replaces .cog extension with .bcog.
 */

#include "jk.h"
#include "bcog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#include "Win95/std.h"
#include "stdPlatform.h"
#include "Cog/sithCogParse.h"
#include "engine_config.h"

/* -----------------------------------------------------------------------
 * Globals required by the engine subsystems we pull in.
 * ----------------------------------------------------------------------- */

/* HostServices pointers */
HostServices *pHS        = NULL;
HostServices *std_pHS    = NULL;
HostServices *pSithHS    = NULL;
HostServices *pLowLevelHS= NULL;
HostServices *rdroid_pHS = NULL;
HostServices *jkRes_pHS  = NULL;

/* COG parser globals -- used by sithCogParse.c, cog.y, lex.yy.c */
sithCogSymboltable *sithCog_pSymbolTable      = NULL;
sithCogSymboltable *sithCogParse_pSymbolTable = NULL;

sith_cog_parser_node *cogparser_nodes_alloc   = NULL;
sith_cog_parser_node *cogparser_topnode       = NULL;
int32_t              *cogvm_stack             = NULL;
sithCogScript        *parsing_script          = NULL;

int cogparser_num_nodes        = 0;
int cogparser_current_nodeidx  = 0;
int cogvm_stackpos             = 0;
int cog_yacc_loop_depth        = 1;
int parsing_script_idk         = 1;
int yacc_linenum               = 0;

int cog_parser_node_stackpos[SITHCOG_NODE_STACKDEPTH];

/* stdConffile global entry buffer -- used by stdConffile.c and sithCogParse.c */
stdConffileEntry stdConffile_entry;

/* stdConffile internal state (normally mmap'd) */
char        aFilenameStack[2560];
char*       apBufferStack[20];
int         linenumStack[20];
char        aEntryStack[0x14*(STDCONF_LINEBUFFER_LEN+4)];
stdFile_t   openFileStack[20];
int         stdConffile_linenum = 0;
int         stdConffile_bOpen   = 0;
stdFile_t   openFile  = 0;
stdFile_t   writeFile = 0;
uint32_t    stackLevel = 0;
char        stdConffile_aWriteFilename[128];
char        stdConffile_pFilename[128];
char*       stdConffile_aLine = NULL;

/* lex/yacc file handle (normally mmap'd; we provide as a real global) */
stdFile_t yyin  = 0;
stdFile_t yyout = 0;

/* std.c / stdConffile.c misc buffers */
char std_genBuffer[1024];
char printfBuffer[STDCONF_LINEBUFFER_LEN];

/* Mode flags -- cogc always targets JK (not MOTS, not DW). */
int Main_bMotsCompat       = 0;
int Main_bDwCompat         = 0;
int Main_bEnhancedCogVerbs = 1;

/* -----------------------------------------------------------------------
 * Forward declaration from cogc_verbs.c
 * ----------------------------------------------------------------------- */
void cogc_RegisterAll(sithCogSymboltable *table);

/* -----------------------------------------------------------------------
 * Helpers
 * ----------------------------------------------------------------------- */

/* Replace the last occurrence of .cog with .bcog, or append .bcog. */
static void make_output_path(const char *in, char *out, size_t outsz)
{
    strncpy(out, in, outsz - 1);
    out[outsz - 1] = '\0';

    /* Find last dot */
    char *dot = strrchr(out, '.');
    if (dot && strcasecmp(dot, ".cog") == 0) {
        strcpy(dot, ".bcog");
    } else {
        size_t len = strlen(out);
        if (len + 5 < outsz)
            strcat(out, ".bcog");
    }
}

static void write_u32(FILE *f, uint32_t v)
{
    fwrite(&v, 4, 1, f);
}

static void write_i32(FILE *f, int32_t v)
{
    fwrite(&v, 4, 1, f);
}

/* -----------------------------------------------------------------------
 * main
 * ----------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input.cog> [output.bcog]\n", argv[0]);
        return 1;
    }

    /* Use C locale so floating-point parses with '.' not ',' */
    setlocale(LC_ALL, "C");

    /* --- HostServices init (pattern from src/Tests/rle_test.c:230-240) --- */
    static HostServices hs;
    stdInitServices(&hs);
    hs.debugPrint   = stdConsolePrintf;
    hs.messagePrint = stdConsolePrintf;
    hs.errorPrint   = stdConsolePrintf;
    pHS         = &hs;
    std_pHS     = &hs;
    pSithHS     = &hs;
    pLowLevelHS = &hs;
    rdroid_pHS  = &hs;
    jkRes_pHS   = &hs;
    stdStartup(&hs);

    /* --- Build global symbol table (verbs, messages, globals) --- */
    sithCog_pSymbolTable = sithCogParse_NewSymboltable(SITHCOG_SYMBOL_LIMIT);
    if (!sithCog_pSymbolTable) {
        fprintf(stderr, "cogc: failed to allocate global symbol table\n");
        return 1;
    }
    /* Verb IDs start at 0x100 */
    sithCog_pSymbolTable->bucket_idx = 0x100;

    cogc_RegisterAll(sithCog_pSymbolTable);

    /* --- Parse the COG source file --- */
    sithCogScript script;
    memset(&script, 0, sizeof(script));

    int result = sithCogParse_Load(argv[1], &script, 0);
    if (result == 0) {
        fprintf(stderr, "cogc: failed to parse '%s'\n", argv[1]);
        return 1;
    }

    /* --- Determine output path --- */
    char outpath[512];
    if (argc >= 3) {
        strncpy(outpath, argv[2], sizeof(outpath) - 1);
        outpath[sizeof(outpath) - 1] = '\0';
    } else {
        make_output_path(argv[1], outpath, sizeof(outpath));
    }

    /* --- Open output file --- */
    FILE *fp = fopen(outpath, "wb");
    if (!fp) {
        fprintf(stderr, "cogc: cannot open output '%s'\n", outpath);
        return 1;
    }

    /* --- Write BcogHeader --- */
    {
        BcogHeader hdr;
        memset(&hdr, 0, sizeof(hdr));
        memcpy(hdr.magic, BCOG_MAGIC, 4);
        hdr.version      = BCOG_VERSION;
        hdr.flags        = (uint32_t)script.flags;
        hdr.num_symbols  = script.pSymbolTable ? script.pSymbolTable->entry_cnt : 0;
        hdr.num_triggers = script.num_triggers;
        hdr.num_idk      = script.numIdk;
        hdr.code_size    = script.codeSize;
        fwrite(&hdr, sizeof(hdr), 1, fp);
    }

    /* --- Write symbols --- */
    if (script.pSymbolTable) {
        uint32_t n = script.pSymbolTable->entry_cnt;
        for (uint32_t i = 0; i < n; i++) {
            sithCogSymbol *sym = &script.pSymbolTable->buckets[i];
            BcogSymbol bs;
            memset(&bs, 0, sizeof(bs));
            bs.symbol_id  = sym->symbol_id;
            bs.val_type   = sym->val.type;
            bs.val_data[0] = sym->val.data[0];
            bs.val_data[1] = sym->val.data[1];
            bs.val_data[2] = sym->val.data[2];
            bs.field_14   = sym->field_14;
            if (sym->pName)
                strncpy(bs.name, sym->pName, sizeof(bs.name) - 1);
            fwrite(&bs, sizeof(bs), 1, fp);
        }
    }

    /* --- Write triggers --- */
    for (uint32_t i = 0; i < script.num_triggers; i++) {
        BcogTrigger bt;
        bt.trigId  = script.triggers[i].trigId;
        bt.trigPc  = script.triggers[i].trigPc;
        bt.field_8 = script.triggers[i].field_8;
        fwrite(&bt, sizeof(bt), 1, fp);
    }

    /* --- Write IDK references --- */
    for (uint32_t i = 0; i < script.numIdk; i++) {
        sithCogReference *ref = &script.aIdk[i];
        BcogIdk bi;
        memset(&bi, 0, sizeof(bi));
        bi.type   = ref->type;
        bi.flags  = ref->flags;
        bi.linkid = ref->linkid;
        bi.mask   = ref->mask;
        bi.hash   = ref->hash;
        /* ref->desc is never set by the parser -- leave bi.desc as zeros */
        strncpy(bi.value, ref->value, sizeof(bi.value) - 1);
        fwrite(&bi, sizeof(bi), 1, fp);
    }

    /* --- Write bytecode --- */
    if (script.codeSize > 0 && script.script_program) {
        fwrite(script.script_program, sizeof(int32_t), script.codeSize, fp);
    }

    fclose(fp);

    printf("cogc: wrote '%s' (%u symbols, %u triggers, %u refs, %u words)\n",
           outpath,
           script.pSymbolTable ? script.pSymbolTable->entry_cnt : 0,
           script.num_triggers,
           script.numIdk,
           script.codeSize);

    return 0;
}
