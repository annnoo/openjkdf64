#include "Win95/stdSound.h"

#include "Gui/jkGUISound.h"
#include "Main/Main.h"
#include "stdPlatform.h"
#include "Platform/wuRegistry.h"
#include "General/stdMath.h"

#include <stdio.h>

#include "jk.h"

uint32_t stdSound_ParseWav(stdFile_t sound_file, uint32_t *nSamplesPerSec, int32_t *bitsPerSample, int32_t *bStereo, int32_t *seekOffset)
{
    unsigned int result;
    char v9[4];
    stdWaveFormat v10;
    uint32_t seekPos;

    std_pHS->fseek(sound_file, 8, 0);
    std_pHS->fileRead(sound_file, v9, 4);
    result = 0;
    if (!_memcmp(v9, "WAVE", 4))
    {
        std_pHS->fseek(sound_file, 4, SEEK_CUR);
        std_pHS->fileRead(sound_file, &seekPos, 4);
        std_pHS->fileRead(sound_file, &v10, sizeof(stdWaveFormat));
        *nSamplesPerSec = v10.nSamplesPerSec;
        *bitsPerSample = 8 * (v10.nBlockAlign / (int)v10.nChannels);
        *bStereo = v10.nChannels == 2;

        if (seekPos > 0x10)
            std_pHS->fseek(sound_file, seekPos - 16, 1);

        while (!std_pHS->fileEof(sound_file))
        {
            std_pHS->fileRead(sound_file, v9, 4);
            std_pHS->fileRead(sound_file, &seekPos, 4);
            if (!_memcmp(v9, "data", 4)) break;
            std_pHS->fseek(sound_file, seekPos, SEEK_CUR);
        }
        *seekOffset = std_pHS->ftell(sound_file);
        result = seekPos;
    }
    return result;
}

int stdSound_Startup() { return 1; }
void stdSound_Shutdown() {}
void stdSound_SetMenuVolume(flex_t vol) {}

stdSound_buffer_t* stdSound_BufferCreate(int bStereo, uint32_t nSamplesPerSec, uint16_t bitsPerSample, int bufferLen) { return NULL; }
int stdSound_BufferPlay(stdSound_buffer_t *a1, int a2) { return 0; }
int stdSound_BufferStop(stdSound_buffer_t *a1) { return 0; }
void stdSound_BufferRelease(stdSound_buffer_t *a1) {}
int stdSound_IsPlaying(stdSound_buffer_t *a1, rdVector3 *pos) { return 0; }
void stdSound_BufferSetVolume(stdSound_buffer_t *a1, flex_t a2) {}
void stdSound_BufferSetPan(stdSound_buffer_t *a1, flex_t a2) {}
void stdSound_BufferSetFrequency(stdSound_buffer_t *a1, int a2) {}
int stdSound_BufferReset(stdSound_buffer_t *a1) { return 0; }
void stdSound_BufferCopyData(stdSound_buffer_t *a1, void *data, int size, int bitsPerSample, int bStereo, uint32_t sampleRate) {}
void stdSound_SetPosition(stdSound_buffer_t *pBuf, rdVector3 *pos) {}
void stdSound_SetVelocity(stdSound_buffer_t *pBuf, rdVector3 *vel) {}
void stdSound_SetPositionOrientation(rdVector3 *pos, rdVector3 *lvec, rdVector3 *uvec) {}
void stdSound_CommitDeferredSettings() {}
int stdSound_3DSetMode(stdSound_buffer_t *a1, int a2) { return 0; }
void stdSound_3DBufferRelease(stdSound_3dBuffer_t *a1) {}
void* stdSound_BufferSetData(stdSound_buffer_t *a1, int bufferBytes, int32_t *bufferMaxSize) { return NULL; }
int stdSound_BufferUnlock(stdSound_buffer_t *a1, void* buffer, int bufferReadLen) { return 0; }
stdSound_buffer_t* stdSound_BufferDuplicate(stdSound_buffer_t* buf) { return NULL; }
stdSound_3dBuffer_t* stdSound_BufferQueryInterface(stdSound_buffer_t* a1) { return NULL; }
void stdSound_SetMenuSoundFormat(int a1) {}
