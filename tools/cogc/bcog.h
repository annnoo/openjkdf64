#ifndef COGC_BCOG_H
#define COGC_BCOG_H
#include <stdint.h>

#define BCOG_MAGIC    "BCOG"
#define BCOG_VERSION  1

// 28-byte file header
typedef struct __attribute__((packed)) BcogHeader {
    char     magic[4];      // "BCOG"
    uint32_t version;       // 1
    uint32_t flags;         // sithCogScript.flags
    uint32_t num_symbols;
    uint32_t num_triggers;
    uint32_t num_idk;
    uint32_t code_size;     // int32 word count
} BcogHeader;

// 56 bytes per symbol
typedef struct __attribute__((packed)) BcogSymbol {
    int32_t  symbol_id;
    uint32_t val_type;      // COG_VARTYPE_*
    int32_t  val_data[3];   // bitcast: int/float/xyz
    int32_t  field_14;
    char     name[32];
} BcogSymbol;

// 12 bytes per trigger
typedef struct __attribute__((packed)) BcogTrigger {
    uint32_t trigId;
    uint32_t trigPc;
    uint32_t field_8;
} BcogTrigger;

// 104 bytes per idk reference
typedef struct __attribute__((packed)) BcogIdk {
    int32_t type;
    int32_t flags;
    int32_t linkid;
    int32_t mask;
    int32_t hash;
    char    value[32];
    char    desc[48];   // heap-allocated in live code; 48 bytes covers real usage
} BcogIdk;

#endif // COGC_BCOG_H
