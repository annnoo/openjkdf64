#ifndef SITHCOGBINARY_H
#define SITHCOGBINARY_H

/*
 * sithCogBinary.h -- binary .bcog format shared between cogc (host tool)
 * and the engine runtime loader (sithCogBinary.c).
 *
 * File layout (all little-endian):
 *   BcogHeader                (28 bytes)
 *   BcogSymbol[num_symbols]   (56 bytes each)
 *   BcogTrigger[num_triggers] (12 bytes each)
 *   BcogIdk[num_idk]          (104 bytes each)
 *   int32_t[code_size]        (raw bytecode words)
 */

#include <stdint.h>

#define BCOG_MAGIC   "BCOG"
#define BCOG_VERSION 1

typedef struct __attribute__((packed)) BcogHeader {
    char     magic[4];      // "BCOG"
    uint32_t version;       // 1
    uint32_t flags;         // sithCogScript.flags
    uint32_t num_symbols;
    uint32_t num_triggers;
    uint32_t num_idk;
    uint32_t code_size;     // int32 word count
} BcogHeader;

typedef struct __attribute__((packed)) BcogSymbol {
    int32_t  symbol_id;
    uint32_t val_type;      // COG_VARTYPE_*
    int32_t  val_data[3];   // bitcast: int / float / xyz
    int32_t  field_14;
    char     name[32];      // null-terminated symbol name
} BcogSymbol;

typedef struct __attribute__((packed)) BcogTrigger {
    uint32_t trigId;
    uint32_t trigPc;
    uint32_t field_8;
} BcogTrigger;

typedef struct __attribute__((packed)) BcogIdk {
    int32_t type;
    int32_t flags;
    int32_t linkid;
    int32_t mask;
    int32_t hash;
    char    value[32];  // asset name / literal value string
    char    desc[48];   // description (may be empty)
} BcogIdk;

/* Runtime loader -- returns 1 on success, 0 on failure. */
struct sithCogScript;
int sithCogBinary_Load(const char *bcog_fpath, struct sithCogScript *script);

#endif /* SITHCOGBINARY_H */
