#include "rdKeyframe.h"

#include "Engine/rdroid.h"
#include "General/stdConffile.h"
#include "General/stdString.h"
#include "General/crc32.h"
#include "stdPlatform.h"
#include "Win95/std.h"
#include "jk.h"

keyframeLoader_t rdKeyframe_RegisterLoader(keyframeLoader_t loader)
{
    keyframeLoader_t result = pKeyframeLoader;
    pKeyframeLoader = loader;
    return result;
}

keyframeUnloader_t rdKeyframe_RegisterUnloader(keyframeUnloader_t loader)
{
    keyframeUnloader_t result = pKeyframeUnloader;
    pKeyframeUnloader = loader;
    return result;
}

void rdKeyframe_NewEntry(rdKeyframe *keyframe)
{
    _memset(keyframe, 0, sizeof(rdKeyframe));
#ifdef SITH_DEBUG_STRUCT_NAMES
    stdString_SafeStrCopy(keyframe->name, "UNKNOWN", 32);
#endif
#ifdef STDHASHTABLE_CRC32_KEYS
    keyframe->namecrc = stdCrc32("UNKNOWN", strlen("UNKNOWN"));
#endif
}

rdKeyframe* rdKeyframe_Load(char *fname)
{
    rdKeyframe *keyframe;

    if (pKeyframeLoader)
        return (rdKeyframe*)pKeyframeLoader(fname);

    keyframe = (rdKeyframe*)rdroid_pHS->alloc(sizeof(rdKeyframe));
    if (!keyframe)
        return NULL;

    if (rdKeyframe_LoadEntry(fname, keyframe))
      return keyframe;

    // This was inlined
    rdKeyframe_FreeEntry(keyframe);
    
    return NULL;
}

int rdKeyframe_LoadEntry(char *key_fpath, rdKeyframe *keyframe)
{
    char *key_fname_only;
    rdJoint *paJoints;
    rdKeyframe *num_joints;
    unsigned int num_markers_read;
#ifndef TARGET_N64
    rdMarkers *markers;
#endif
    rdJoint *joint;
    rdAnimEntry *anim_entry;
    int anim_entry_read;
    unsigned int num_markers;
    int node_idx;
    int anim_entry_cnt;
    unsigned int num_nodes;
    flex32_t posx, posy, posz;
    flex32_t orientationx, orientationy, orientationz;
    flex32_t velx, vely, velz;
    flex32_t angVelx, angVely, angVelz;
    int entry_num;
    char mesh_name[32];
    unsigned int nodes_read;
    flex32_t ftmp;

    int prevId = keyframe->id;
#ifdef TARGET_N64
    char prevFpath[64];
    stdString_SafeStrCopy(prevFpath, keyframe->fpath, sizeof(prevFpath));
#endif

    rdKeyframe_NewEntry(keyframe);

    // Restore preserved fields
    keyframe->id = prevId;
#ifdef TARGET_N64
    stdString_SafeStrCopy(keyframe->fpath, prevFpath, sizeof(keyframe->fpath));
#endif

    key_fname_only = stdFileFromPath(key_fpath);
#ifdef SITH_DEBUG_STRUCT_NAMES
    stdString_SafeStrCopy(keyframe->name, key_fname_only, 32);
#endif
#ifdef STDHASHTABLE_CRC32_KEYS
    keyframe->namecrc = stdCrc32(key_fname_only, strlen(key_fname_only));
#endif
#ifdef TARGET_N64
    stdString_SafeStrCopy(keyframe->fpath, key_fpath, sizeof(keyframe->fpath));
#endif

    if (!stdConffile_OpenRead(key_fpath)) {
        stdPlatform_Printf("rdKeyframe_LoadEntry: Failed to open keyframe file `%s`\n", key_fpath);
        goto open_fail;
    }

    if (!stdConffile_ReadLine()) {
      stdPlatform_Printf("rdKeyframe_LoadEntry: ReadLine failed at start of `%s`\n", key_fpath);
      goto read_fail;
    }

    if (_sscanf(stdConffile_aLine, " section: %s", std_genBuffer) != 1) {
      stdPlatform_Printf("rdKeyframe_LoadEntry: Section header missing in `%s`: '%s'\n", key_fpath, stdConffile_aLine);
      goto read_fail;
    }

    if (!__strcmpi(std_genBuffer, "header"))
    {
        if (!stdConffile_ReadLine())
            goto read_fail;

        if (_sscanf(stdConffile_aLine, " flags %d", &keyframe->flags) != 1) {
            stdPlatform_Printf("rdKeyframe_LoadEntry: Failed to parse 'flags' in `%s`\n", key_fpath);
            goto read_fail;
        }

        if (!stdConffile_ReadLine())
            goto read_fail;

        if (_sscanf(stdConffile_aLine, " type %x", &keyframe->type) != 1) {
            stdPlatform_Printf("rdKeyframe_LoadEntry: Failed to parse 'type' in `%s`\n", key_fpath);
            goto read_fail;
        }

        if (!stdConffile_ReadLine())
            goto read_fail;

        if (_sscanf(stdConffile_aLine, " frames %d", &keyframe->numFrames) != 1)
            goto read_fail;

        if (!stdConffile_ReadLine())
            goto read_fail;

        if (_sscanf(stdConffile_aLine, " fps %f", &keyframe->fps) != 1) // FLEXTODO
            goto read_fail;

        if (!stdConffile_ReadLine())
            goto read_fail;

        if (_sscanf(stdConffile_aLine, " joints %d", &keyframe->numJoints) != 1)
            goto read_fail;

        if (!stdConffile_ReadLine())
            goto read_fail;

        if (_sscanf(stdConffile_aLine, " section: %s", std_genBuffer) != 1)
            goto read_fail;
    }

#ifndef TARGET_N64
    if (!__strcmpi(std_genBuffer, "markers"))
    {
        if (!stdConffile_ReadLine())
            goto read_fail;

        if (_sscanf(stdConffile_aLine, " markers %d", &keyframe->numMarkers) != 1)
            goto read_fail;

        for (num_markers_read = 0; num_markers_read < keyframe->numMarkers; num_markers_read++)
        {
            if (!stdConffile_ReadLine())
                goto read_fail;

            if (_sscanf(stdConffile_aLine, " %f %d", &ftmp, &keyframe->markers.marker_int[num_markers_read]) != 2) // FLEXTODO
                goto read_fail;
            
            keyframe->markers.marker_float[num_markers_read] = ftmp;
        }

        if (!stdConffile_ReadLine())
            goto read_fail;

        if (_sscanf(stdConffile_aLine, " section: %s", std_genBuffer) != 1)
            goto read_fail;
    }
#else
    if (!__strcmpi(std_genBuffer, "markers"))
    {
        if (!stdConffile_ReadLine())
            goto read_fail;

        if (_sscanf(stdConffile_aLine, " markers %d", &anim_entry_cnt) != 1)
            goto read_fail;

        for (num_markers_read = 0; num_markers_read < anim_entry_cnt; num_markers_read++)
        {
            if (!stdConffile_ReadLine())
                goto read_fail;
        }

        if (!stdConffile_ReadLine())
            goto read_fail;

        if (_sscanf(stdConffile_aLine, " section: %s", std_genBuffer) != 1)
            goto read_fail;
    }
#endif

#ifdef TARGET_N64
    // Short circuit for lazy loading on N64
    // We only load if we are NOT in the initial level load.
    // If we ARE in level load, we just store the metadata.
    if (sithWorld_pLoading) {
        keyframe->bLoaded = 0;
        keyframe->paJoints = NULL;
        stdConffile_Close();
        return 1;
    }
#endif

    if (__strcmpi(std_genBuffer, "keyframes"))
        goto read_fail;

    keyframe->numJoints2 = keyframe->numJoints;
    keyframe->paJoints = (rdJoint*)rdroid_pHS->alloc(sizeof(rdJoint) * keyframe->numJoints);
    if (!keyframe->paJoints)
        goto read_fail;

    _memset(keyframe->paJoints, 0, sizeof(rdJoint) * keyframe->numJoints);
    for (nodes_read = 0; nodes_read < keyframe->numJoints2; nodes_read++)
    {
        joint = &keyframe->paJoints[nodes_read];
        if (!stdConffile_ReadLine())
            goto read_fail;

        if (_sscanf(stdConffile_aLine, " node %d", &node_idx) != 1)
            goto read_fail;

#ifdef SITH_DEBUG_STRUCT_NAMES
        if (!stdConffile_ReadLine())
            goto read_fail;

        if (_sscanf(stdConffile_aLine, " mesh %s", mesh_name) != 1)
            goto read_fail;

        stdString_SafeStrCopy(joint->mesh_name, mesh_name, 32);
#endif
        
        if (!stdConffile_ReadLine())
            goto read_fail;

        if (_sscanf(stdConffile_aLine, " entries %d", &anim_entry_cnt) != 1)
            goto read_fail;

        joint->nodeIdx = node_idx;
        joint->numAnimEntries = anim_entry_cnt;
        joint->paAnimEntries = (rdAnimEntry*)rdroid_pHS->alloc(sizeof(rdAnimEntry) * anim_entry_cnt + 2); // Added: prevent some oob accesses in rdPuppet
        if (!joint->paAnimEntries)
          goto read_fail;

        anim_entry = joint->paAnimEntries;
        for (anim_entry_read = 0; anim_entry_read < joint->numAnimEntries; anim_entry_read++)
        {
            if (!stdConffile_ReadLine()) {
                goto read_fail;
            }
            
            if (_sscanf(
                   stdConffile_aLine,
                   " %d: %f %x %f %f %f %f %f %f",
                   &entry_num,
                   &ftmp,
                   &anim_entry->flags,
                   &posx,
                   &posy,
                   &posz,
                   &orientationx,
                   &orientationy,
                   &orientationz) != 9) {
              goto read_fail;
            }
            
            anim_entry->frameNum = ftmp; // FLEXTODO
            anim_entry->pos.x = posx; // FLEXTODO
            anim_entry->pos.y = posy; // FLEXTODO
            anim_entry->pos.z = posz; // FLEXTODO
            anim_entry->orientation.x = orientationx; // FLEXTODO
            anim_entry->orientation.y = orientationy; // FLEXTODO
            anim_entry->orientation.z = orientationz; // FLEXTODO
            
            if (!stdConffile_ReadLine()
              || _sscanf(stdConffile_aLine, " %f %f %f %f %f %f", &velx, &vely, &velz, &angVelx, &angVely, &angVelz) != 6)
            {
              goto read_fail;
            }

#ifndef TARGET_N64
            anim_entry->vel.x = velx; // FLEXTODO
            anim_entry->vel.y = vely; // FLEXTODO
            anim_entry->vel.z = velz; // FLEXTODO
            anim_entry->angVel.x = angVelx; // FLEXTODO
            anim_entry->angVel.y = angVely; // FLEXTODO
            anim_entry->angVel.z = angVelz; // FLEXTODO
#endif
            anim_entry++;
        }
    }
    
#ifdef TARGET_N64
    keyframe->bLoaded = 1;
#endif
    stdConffile_Close();
    return 1;
  
read_fail:
    stdConffile_Close();
open_fail:
    return 0;
}

int rdKeyframe_Write(char *out_fpath, rdKeyframe *keyframe, char *creation_method)
{
    int fd;
    int totalAnimEntries;
    rdJoint *joint_iter;
    rdAnimEntry *animEntry_iter;
    unsigned int i;
    unsigned int j;

    fd = rdroid_pHS->fileOpen(out_fpath, "wt+");
    if (!fd)
        return 0;

    rdroid_pHS->filePrintf(fd, "# KEYFRAME '%s' created from '%s'\n\n", keyframe, creation_method);
    rdroid_pHS->filePrintf(fd, "###############\n");
    rdroid_pHS->filePrintf(fd, "SECTION: HEADER\n\n");
    rdroid_pHS->filePrintf(fd, "FLAGS  0x%04x\n", keyframe->flags);
    rdroid_pHS->filePrintf(fd, "TYPE   0x%X\n", keyframe->type);
    rdroid_pHS->filePrintf(fd, "FRAMES %d\n", keyframe->numFrames);
    rdroid_pHS->filePrintf(fd, "FPS    %.3f\n", keyframe->fps);
    rdroid_pHS->filePrintf(fd, "JOINTS %d\n", keyframe->numJoints);
#ifndef TARGET_N64
    if (keyframe->numMarkers)
    {
        rdroid_pHS->filePrintf(fd, "\n\n");
        rdroid_pHS->filePrintf(fd, "###############\n");
        rdroid_pHS->filePrintf(fd, "SECTION: MARKERS\n\n");
        rdroid_pHS->filePrintf(fd, "MARKERS %d\n\n", keyframe->numMarkers);
        for (i = 0; i < keyframe->numMarkers; i++)
        {
            rdroid_pHS->filePrintf(fd, "%f %d\n", keyframe->markers.marker_float[i], keyframe->markers.marker_int[i]);
        }
    }
#endif
    rdroid_pHS->filePrintf(fd, "\n\n");
    rdroid_pHS->filePrintf(fd, "###############\n");
    rdroid_pHS->filePrintf(fd, "SECTION: KEYFRAMES\n\n");
    joint_iter = keyframe->paJoints;
    for (i = 0; i < keyframe->numJoints; i++)
    {
        rdroid_pHS->filePrintf(fd, "NODE %d\n", joint_iter->nodeIdx);
#ifdef SITH_DEBUG_STRUCT_NAMES
        rdroid_pHS->filePrintf(fd, "MESH %s\n", joint_iter->mesh_name);
#endif
        rdroid_pHS->filePrintf(fd, "ENTRIES %d\n", joint_iter->numAnimEntries);
        animEntry_iter = joint_iter->paAnimEntries;
        for (j = 0; j < joint_iter->numAnimEntries; j++)
        {
            rdroid_pHS->filePrintf(
                fd,
                "%d: %f 0x%x %f %f %f %f %f %f\n",
                j,
                animEntry_iter->frameNum,
                animEntry_iter->flags,
                animEntry_iter->pos.x,
                animEntry_iter->pos.y,
                animEntry_iter->pos.z,
                animEntry_iter->orientation.x,
                animEntry_iter->orientation.y,
                animEntry_iter->orientation.z);
#ifndef TARGET_N64
            rdroid_pHS->filePrintf(
                fd,
                "%f %f %f %f %f %f\n",
                animEntry_iter->vel.x,
                animEntry_iter->vel.y,
                animEntry_iter->vel.z,
                animEntry_iter->angVel.x,
                animEntry_iter->angVel.y,
                animEntry_iter->angVel.z);
#else
            rdroid_pHS->filePrintf(fd, "0.000000 0.000000 0.000000 0.000000 0.000000 0.000000\n");
#endif
            animEntry_iter++;
        }
        joint_iter++;
    }
    rdroid_pHS->fileClose(fd);
    return 1;
}

void rdKeyframe_FreeJoints(rdKeyframe *keyframe)
{
    rdJoint *joint_iter;
    unsigned int i;

    if (!keyframe)
        return;

    if (keyframe->paJoints)
    {
        joint_iter = keyframe->paJoints;
        for (i = 0; i < keyframe->numJoints; i++)
        {
            if (joint_iter->paAnimEntries)
            {
                rdroid_pHS->free(joint_iter->paAnimEntries);
                joint_iter->paAnimEntries = NULL;
            }
            joint_iter++;
        }
        rdroid_pHS->free(keyframe->paJoints);
        keyframe->paJoints = NULL;
    }
}

void rdKeyframe_FreeEntry(rdKeyframe *keyframe)
{
    if (!keyframe)
        return;

    if (pKeyframeUnloader)
    {
        pKeyframeUnloader(keyframe);
        return;
    }

    rdKeyframe_FreeJoints(keyframe);
}

#ifdef TARGET_N64
int rdKeyframe_EnsureLoaded(rdKeyframe *keyframe)
{
    if (!keyframe || keyframe->bLoaded) return 1;
    
    // stdPlatform_Printf("Lazy loading keyframe: %s\n", keyframe->fpath);
    
    // Save current loading world temporarily to avoid short-circuit
    sithWorld* prevLoading = sithWorld_pLoading;
    sithWorld_pLoading = NULL;
    
    int ret = rdKeyframe_LoadEntry(keyframe->fpath, keyframe);
    
    sithWorld_pLoading = prevLoading;
    return ret;
}
#endif
