#include "sithCogBinary.h"

#include <stdio.h>
#include <string.h>

#include "jk.h"
#include "stdPlatform.h"
#include "Cog/sithCogParse.h"
#include "General/stdHashTable.h"
#include "General/crc32.h"

/*
 * sithCogBinary_Load -- load a pre-compiled .bcog file into *script.
 *
 * The caller is responsible for passing a zeroed sithCogScript.
 * Returns 1 on success, 0 on failure (hard -- no fallback).
 */
int sithCogBinary_Load(const char *bcog_fpath, sithCogScript *script)
{
    stdFile_t fp;
    BcogHeader hdr;
    uint32_t i;

    fp = pSithHS->fileOpen(bcog_fpath, "rb");
    if (!fp)
    {
        stdPrintf(pSithHS->errorPrint, ".\\Cog\\sithCogBinary.c", __LINE__,
                  "COG_USE_PRECOMPILED: could not open '%s'\n", bcog_fpath);
        return 0;
    }

    /* --- header --- */
    if (pSithHS->fileRead(fp, &hdr, sizeof(hdr)) != sizeof(hdr))
        goto fail;

    if (memcmp(hdr.magic, BCOG_MAGIC, 4) != 0 || hdr.version != BCOG_VERSION)
    {
        stdPrintf(pSithHS->errorPrint, ".\\Cog\\sithCogBinary.c", __LINE__,
                  "COG_USE_PRECOMPILED: bad magic/version in '%s'\n", bcog_fpath);
        goto fail;
    }

    _memset(script, 0, sizeof(*script));
    script->flags = (sithCogFlags_t)hdr.flags;

    /* --- symbol table --- */
    {
        sithCogSymboltable *table = sithCogParse_NewSymboltable((int)hdr.num_symbols);
        if (!table)
            goto fail;
        script->pSymbolTable = table;
        table->entry_cnt = (int)hdr.num_symbols;

        for (i = 0; i < hdr.num_symbols; i++)
        {
            BcogSymbol bs;
            sithCogSymbol *sym;
            char *nameBuf;

            if (pSithHS->fileRead(fp, &bs, sizeof(bs)) != sizeof(bs))
                goto fail;
// ... (rest of function)

            sym = &table->buckets[i];
            sym->symbol_id    = bs.symbol_id;
            sym->val.type     = bs.val_type;
            sym->val.data[0]  = bs.val_data[0];
#ifndef COG_COMPRESS_VAR_SIZE
            sym->val.data[1]  = bs.val_data[1];
            sym->val.data[2]  = bs.val_data[2];
#endif
            sym->field_14     = bs.field_14;

            /* copy name into heap-allocated buffer */
            nameBuf = (char *)pSithHS->alloc(_strlen(bs.name) + 1);
            if (!nameBuf)
                goto fail;
            _strcpy(nameBuf, bs.name);
#ifndef COG_CRC32_SYMBOL_NAMES
            sym->pName = nameBuf;
            stdHashTable_SetKeyVal(table->hashtable, sym->pName, sym);
#else
            sym->nameCrc = stdCrc32(nameBuf, strlen(nameBuf));
            stdHashTable_SetKeyVal(table->hashtable, nameBuf, sym);
            pSithHS->free(nameBuf);
#endif
        }
    }

    /* --- triggers --- */
    {
#ifdef COG_DYNAMIC_TRIGGERS
        if (hdr.num_triggers > 0)
        {
            script->triggers = (sithCogTrigger *)pSithHS->alloc(
                sizeof(sithCogTrigger) * hdr.num_triggers);
            if (!script->triggers)
                goto fail;
        }
#endif
        for (i = 0; i < hdr.num_triggers; i++)
        {
            BcogTrigger bt;
            if (pSithHS->fileRead(fp, &bt, sizeof(bt)) != sizeof(bt))
                goto fail;

#ifdef COG_DYNAMIC_TRIGGERS
            script->triggers[i].trigId  = bt.trigId;
            script->triggers[i].trigPc  = bt.trigPc;
            script->triggers[i].field_8 = bt.field_8;
#else
            if (i < 32)
            {
                script->triggers[i].trigId  = bt.trigId;
                script->triggers[i].trigPc  = bt.trigPc;
                script->triggers[i].field_8 = bt.field_8;
            }
#endif
        }
        script->num_triggers = hdr.num_triggers;
    }

    /* --- idk / references --- */
    {
#ifdef COG_DYNAMIC_IDK
        if (hdr.num_idk > 0)
        {
            script->aIdk = (sithCogReference *)pSithHS->alloc(
                sizeof(sithCogReference) * hdr.num_idk);
            if (!script->aIdk)
                goto fail;
            _memset(script->aIdk, 0, sizeof(sithCogReference) * hdr.num_idk);
        }
#endif
        for (i = 0; i < hdr.num_idk; i++)
        {
            BcogIdk bi;
            sithCogReference *ref;

            if (pSithHS->fileRead(fp, &bi, sizeof(bi)) != sizeof(bi))
                goto fail;

#ifdef COG_DYNAMIC_IDK
            ref = &script->aIdk[i];
#else
            if (i >= 128)
                continue;   /* static array limit */
            ref = &script->aIdk[i];
#endif
            ref->type   = bi.type;
            ref->flags  = bi.flags;
            ref->linkid = bi.linkid;
            ref->mask   = bi.mask;
            ref->hash   = bi.hash;
            _memcpy(ref->value, bi.value, 32);
            ref->value[31] = '\0';

            if (bi.desc[0] != '\0')
            {
                char *descBuf = (char *)pSithHS->alloc(_strlen(bi.desc) + 1);
                if (descBuf)
                    _strcpy(descBuf, bi.desc);
                ref->desc = descBuf;
            }
            else
            {
                ref->desc = NULL;
            }
        }
        script->numIdk = hdr.num_idk;
    }

    /* --- bytecode --- */
    {
        script->script_program = (int32_t *)pSithHS->alloc(sizeof(int32_t) * hdr.code_size);
        if (!script->script_program)
            goto fail;
        if (pSithHS->fileRead(fp, script->script_program, sizeof(int32_t) * hdr.code_size) != sizeof(int32_t) * hdr.code_size)
            goto fail;
        script->codeSize = hdr.code_size;
    }

    pSithHS->fileClose(fp);
    return 1;

fail:
    if (fp) pSithHS->fileClose(fp);
    return 0;
}
