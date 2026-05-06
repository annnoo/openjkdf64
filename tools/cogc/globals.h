/* cogc stub globals.h -- overrides src/globals.h.cog (a Python cog template, not a real header)
 * This file is found first via -I. (tools/cogc/) in the Makefile.
 *
 * We declare extern all the globals that sithCogParse.c, cog.y, lex.yy.c,
 * and stdConffile.c reference via the generated globals.h in the normal build.
 */
#ifndef _COGC_GLOBALS_H
#define _COGC_GLOBALS_H

#include "engine_config.h"
#include "types.h"

/* ---- HostServices pointers ---- */
extern struct HostServices* pHS;
extern struct HostServices* std_pHS;
extern struct HostServices* pSithHS;
extern struct HostServices* pLowLevelHS;
extern struct HostServices* rdroid_pHS;
extern struct HostServices* jkRes_pHS;

/* ---- COG parser globals (sithCogParse.c / cog.y / lex.yy.c) ---- */
extern sithCogSymboltable* sithCog_pSymbolTable;
extern sithCogSymboltable* sithCogParse_pSymbolTable;

extern sith_cog_parser_node* cogparser_nodes_alloc;
extern sith_cog_parser_node* cogparser_topnode;
extern int32_t*              cogvm_stack;
extern sithCogScript*        parsing_script;

extern int cogparser_num_nodes;
extern int cogparser_current_nodeidx;
extern int cogvm_stackpos;
extern int cog_yacc_loop_depth;
extern int parsing_script_idk;
extern int yacc_linenum;

extern int cog_parser_node_stackpos[SITHCOG_NODE_STACKDEPTH];

/* ---- stdConffile global entry buffer ---- */
extern stdConffileEntry stdConffile_entry;

/* ---- stdConffile internal state (normally mmap'd) ---- */
extern char        aFilenameStack[2560];
extern char*       apBufferStack[20];
extern int         linenumStack[20];
extern char        aEntryStack[0x14*(STDCONF_LINEBUFFER_LEN+4)];
extern stdFile_t   openFileStack[20];
extern int         stdConffile_linenum;
extern int         stdConffile_bOpen;
extern stdFile_t   openFile;
extern stdFile_t   writeFile;
extern uint32_t    stackLevel;
extern char        stdConffile_aWriteFilename[128];
extern char        stdConffile_pFilename[128];
extern char*       stdConffile_aLine;

/* ---- lex/yacc file handle (normally mmap'd; we provide as a real global) ---- */
extern stdFile_t yyin;
extern stdFile_t yyout;

/* ---- std.c / stdConffile.c misc buffers ---- */
extern char std_genBuffer[1024];
extern char printfBuffer[STDCONF_LINEBUFFER_LEN];

/* ---- Mode flags ---- */
extern int Main_bMotsCompat;
extern int Main_bDwCompat;
extern int Main_bEnhancedCogVerbs;

#endif /* _COGC_GLOBALS_H */
