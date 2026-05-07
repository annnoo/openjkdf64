// stdSound_N64.c — libdragon mixer-backed audio for OpenJKDF2 N64 port
//
// Architecture:
//   The engine drives all file I/O: it opens WAVs from the GOB filesystem,
//   parses them via stdSound_ParseWav, allocates a buffer, feeds in raw PCM
//   via stdSound_BufferCopyData / stdSound_BufferSetData+Unlock, then calls
//   stdSound_BufferPlay.  We wrap that PCM in a libdragon waveform_t and hand
//   it to the RSP mixer.
//
//   Channel layout (32 mixer channels):
//     0-3  : reserved for music / ambient streams
//     4-31 : SFX round-robin pool

#include "Win95/stdSound.h"

#include "Gui/jkGUISound.h"
#include "Main/Main.h"
#include "stdPlatform.h"
#include "Platform/wuRegistry.h"
#include "General/stdMath.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "jk.h"

#include <libdragon.h>   // pulls in audio.h, mixer.h, samplebuffer.h

// ── N64-extended sound buffer ─────────────────────────────────────────────────
// Starts with the same fields as stdNullSoundBuffer so it is cast-compatible
// with stdSound_buffer_t.  Extra N64-specific state follows.

typedef struct {
    // ── stdNullSoundBuffer fields (MUST be first, MUST match layout) ──────────
    void*    data;
    int      format;
    int      bStereo;
    int      bitsPerSample;
    uint32_t nSamplesPerSec;
    int      bufferBytes;
    int      bufferLen;      // number of samples
    int      refcnt;
    flex_t   vol;
    int      bIsCopy;
    rdVector3 pos;
    rdVector3 vel;
    // ── N64 extension ─────────────────────────────────────────────────────────
    waveform_t wave;
    wav64_t    wav64;
    int        is_wav64;
    int        mixer_ch;     // -1 when not playing
    int        looping;
} n64SoundBuf_t;

// ── Global state ──────────────────────────────────────────────────────────────
flex_t stdSound_fMenuVolume = 1.0f;

static bool g_audio_inited = false;

#define SFX_CH_FIRST 4
#define SFX_CH_LAST  31
static int g_next_ch = SFX_CH_FIRST;

// ── Channel allocator ─────────────────────────────────────────────────────────
static int alloc_sfx_ch(void)
{
    int start = g_next_ch;
    do {
        int ch = g_next_ch;
        g_next_ch = (g_next_ch >= SFX_CH_LAST) ? SFX_CH_FIRST : g_next_ch + 1;
        if (!mixer_ch_playing(ch))
            return ch;
    } while (g_next_ch != start);
    // All busy — steal next anyway
    int ch = g_next_ch;
    g_next_ch = (g_next_ch >= SFX_CH_LAST) ? SFX_CH_FIRST : g_next_ch + 1;
    return ch;
}

// ── Waveform read callback ────────────────────────────────────────────────────
// The mixer calls this whenever it needs more PCM samples.
// JK SFX are 8-bit unsigned; the N64 mixer expects 8-bit *signed*, so we
// flip the sign bit (XOR with 0x80) while copying.
static void n64_wave_read(void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking)
{
    n64SoundBuf_t *nb = (n64SoundBuf_t*)ctx;

    if (nb->wave.bits == 8) {
        uint8_t *src = (uint8_t*)nb->data + wpos;
        int8_t  *dst = (int8_t*)samplebuffer_append(sbuf, wlen);
        for (int i = 0; i < wlen; i++)
            dst[i] = (int8_t)((int)src[i] ^ 0x80);
    } else {
        // 16-bit signed — copy straight through
        int16_t *src = (int16_t*)nb->data + wpos;
        void    *dst = samplebuffer_append(sbuf, wlen);
        memcpy(dst, src, wlen * sizeof(int16_t));
    }
}

// ── stdSound API ──────────────────────────────────────────────────────────────

uint32_t stdSound_ParseWav(stdFile_t sound_file,
                           uint32_t *nSamplesPerSec, int32_t *bitsPerSample,
                           int32_t *bStereo, int32_t *seekOffset)
{
    unsigned int result;
    char v9[4];
    stdWaveFormat v10;
    uint32_t seekPos;

    // Check for wav64 first
    std_pHS->fseek(sound_file, 0, 0);
    std_pHS->fileRead(sound_file, v9, 4);
    if (!_memcmp(v9, "WAV6", 4)) {
        // This is a wav64 file. Metadata is at known offsets.
        // We'll use a temporary wav64_t to parse it if we could, 
        // but for now let's just parse the bits we need.
        uint32_t channels, frequency;
        std_pHS->fseek(sound_file, 8, 0);
        std_pHS->fileRead(sound_file, &channels, 4);
        std_pHS->fileRead(sound_file, &frequency, 4);
        
        *nSamplesPerSec = frequency;
        *bitsPerSample = 16; // Most wav64 are 16-bit
        *bStereo = (channels == 2);
        *seekOffset = 0xFFFFFFFF; // Special flag for wav64
        
        // Return total size of file as a rough estimate for bufferBytes
        std_pHS->fseek(sound_file, 0, SEEK_END);
        result = std_pHS->ftell(sound_file);
        return result;
    }

    std_pHS->fseek(sound_file, 8, 0);
    std_pHS->fileRead(sound_file, v9, 4);
    result = 0;
    if (!_memcmp(v9, "WAVE", 4))
    {
        std_pHS->fseek(sound_file, 4, SEEK_CUR);
        std_pHS->fileRead(sound_file, &seekPos, 4);
        std_pHS->fileRead(sound_file, &v10, sizeof(stdWaveFormat));
        *nSamplesPerSec = v10.nSamplesPerSec;
        *bitsPerSample  = 8 * (v10.nBlockAlign / (int)v10.nChannels);
        *bStereo        = v10.nChannels == 2;

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

int stdSound_Startup(void)
{
    if (!g_audio_inited) {
        audio_init(22050, 4);
        mixer_init(32);
        g_audio_inited = true;
        debugf("[N64] stdSound_Startup: audio+mixer ready (22050 Hz, 32 ch)\n");
    }
    return 1;
}

void stdSound_Shutdown(void)
{
    if (g_audio_inited) {
        mixer_close();
        audio_close();
        g_audio_inited = false;
    }
}

void stdSound_SetMenuVolume(flex_t vol)
{
    stdSound_fMenuVolume = vol;
    // Apply master volume to all active SFX channels
    if (g_audio_inited)
        mixer_set_vol((float)vol);
}

// ── Buffer lifecycle ──────────────────────────────────────────────────────────

stdSound_buffer_t* stdSound_BufferCreate(int bStereo, uint32_t nSamplesPerSec,
                                         uint16_t bitsPerSample, int bufferLen)
{
    n64SoundBuf_t *nb = (n64SoundBuf_t*)malloc(sizeof(n64SoundBuf_t));
    if (!nb) return NULL;
    memset(nb, 0, sizeof(*nb));

    int bytesPerSample = (bitsPerSample / 8) * (bStereo ? 2 : 1);
    int totalBytes     = bufferLen * bytesPerSample;

    // +MIXER_LOOP_OVERREAD bytes tail for safe RSP over-read
    nb->data           = malloc(totalBytes + MIXER_LOOP_OVERREAD);
    if (!nb->data) { free(nb); return NULL; }
    memset((uint8_t*)nb->data + totalBytes, 0, MIXER_LOOP_OVERREAD);

    nb->bStereo        = bStereo;
    nb->nSamplesPerSec = nSamplesPerSec;
    nb->bitsPerSample  = bitsPerSample;
    nb->bufferLen      = bufferLen;
    nb->bufferBytes    = totalBytes;
    nb->vol            = 1.0f;
    nb->mixer_ch       = -1;

    // Waveform descriptor
    nb->wave.name      = "sfx";
    nb->wave.bits      = (bitsPerSample <= 8) ? 8 : 16;
    nb->wave.channels  = bStereo ? 2 : 1;
    nb->wave.frequency = (float)nSamplesPerSec;
    nb->wave.len       = bufferLen;
    nb->wave.loop_len  = 0;
    nb->wave.read      = n64_wave_read;
    nb->wave.ctx       = nb;

    return (stdSound_buffer_t*)nb;
}

stdSound_buffer_t* stdSound_BufferCreateFromHandle(stdFile_t fhand)
{
    const char* path = N64_GetPathForHandle(fhand);
    if (!path) return NULL;

    n64SoundBuf_t *nb = (n64SoundBuf_t*)malloc(sizeof(n64SoundBuf_t));
    if (!nb) return NULL;
    memset(nb, 0, sizeof(*nb));

    // wav64_open wants a path like "rom:/sound/fire.wav64"
    char romPath[256];
    if (path[0] == '/')
        snprintf(romPath, sizeof(romPath), "rom:%s", path);
    else
        snprintf(romPath, sizeof(romPath), "rom:/%s", path);

    wav64_open(&nb->wav64, romPath);
    nb->is_wav64 = 1;
    nb->mixer_ch = -1;
    nb->vol      = 1.0f;
    
    // Fill in standard fields for common code
    nb->bStereo        = (nb->wav64.wave.channels == 2);
    nb->nSamplesPerSec = (uint32_t)nb->wav64.wave.frequency;
    nb->bitsPerSample  = 16; 
    nb->bufferLen      = nb->wav64.wave.len;
    
    debugf("[N64] stdSound_BufferCreateFromHandle: opened %s (%d Hz, %d ch)\n", 
           romPath, (int)nb->nSamplesPerSec, nb->bStereo ? 2 : 1);

    return (stdSound_buffer_t*)nb;
}

void stdSound_BufferRelease(stdSound_buffer_t *buf)
{
    if (!buf) return;
    n64SoundBuf_t *nb = (n64SoundBuf_t*)buf;
    stdSound_BufferStop(buf);
    if (nb->is_wav64) {
        wav64_close(&nb->wav64);
    } else {
        free(nb->data);
    }
    free(nb);
}

// ── Data loading ──────────────────────────────────────────────────────────────

void* stdSound_BufferSetData(stdSound_buffer_t *sound, int bufferBytes, int32_t *bufferMaxSize)
{
    n64SoundBuf_t *nb = (n64SoundBuf_t*)sound;
    if (bufferMaxSize) *bufferMaxSize = nb->bufferBytes;
    return nb->data;
}

int stdSound_BufferUnlock(stdSound_buffer_t *sound, void *buffer, int bufferReadLen)
{
    // Data written directly into nb->data — nothing to do.
    (void)sound; (void)buffer; (void)bufferReadLen;
    return 1;
}

void stdSound_BufferCopyData(stdSound_buffer_t *buf, void *data, int size,
                              int bitsPerSample, int bStereo, uint32_t sampleRate)
{
    if (!buf || !data || size <= 0) return;
    n64SoundBuf_t *nb = (n64SoundBuf_t*)buf;

    // Reallocate if needed
    if (size > nb->bufferBytes) {
        free(nb->data);
        nb->data       = malloc(size + MIXER_LOOP_OVERREAD);
        nb->bufferBytes = size;
    }
    if (!nb->data) return;

    memcpy(nb->data, data, size);
    memset((uint8_t*)nb->data + size, 0, MIXER_LOOP_OVERREAD);

    int bytesPerSample  = (bitsPerSample / 8) * (bStereo ? 2 : 1);
    nb->bitsPerSample   = bitsPerSample;
    nb->bStereo         = bStereo;
    nb->nSamplesPerSec  = sampleRate;
    nb->bufferLen       = (bytesPerSample > 0) ? (size / bytesPerSample) : 0;

    nb->wave.bits       = (bitsPerSample <= 8) ? 8 : 16;
    nb->wave.channels   = bStereo ? 2 : 1;
    nb->wave.frequency  = (float)sampleRate;
    nb->wave.len        = nb->bufferLen;
}

// ── Playback ──────────────────────────────────────────────────────────────────

int stdSound_BufferPlay(stdSound_buffer_t *buf, int loop)
{
    if (!buf || !g_audio_inited) return 0;
    n64SoundBuf_t *nb = (n64SoundBuf_t*)buf;

    int ch = alloc_sfx_ch();
    nb->mixer_ch = ch;

    float vol = (float)nb->vol;
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;

    mixer_ch_set_vol_pan(ch, vol, 0.5f);
    
    if (nb->is_wav64) {
        wav64_set_loop(&nb->wav64, loop);
        wav64_play(&nb->wav64, ch);
    } else {
        if (!nb->data || nb->bufferLen <= 0) return 0;
        nb->looping       = loop;
        nb->wave.loop_len = loop ? nb->wave.len : 0;
        mixer_ch_play(ch, &nb->wave);
    }
    return 1;
}

int stdSound_BufferStop(stdSound_buffer_t *buf)
{
    if (!buf) return 0;
    n64SoundBuf_t *nb = (n64SoundBuf_t*)buf;
    if (nb->mixer_ch >= 0) {
        mixer_ch_stop(nb->mixer_ch);
        nb->mixer_ch = -1;
    }
    return 1;
}

int stdSound_BufferReset(stdSound_buffer_t *buf)
{
    return stdSound_BufferStop(buf);
}

int stdSound_IsPlaying(stdSound_buffer_t *buf, rdVector3 *pos)
{
    if (!buf || !g_audio_inited) return 0;
    n64SoundBuf_t *nb = (n64SoundBuf_t*)buf;
    if (nb->mixer_ch < 0) return 0;
    return mixer_ch_playing(nb->mixer_ch) ? 1 : 0;
}

// ── Volume / pan / frequency ──────────────────────────────────────────────────

void stdSound_BufferSetVolume(stdSound_buffer_t *buf, flex_t vol)
{
    if (!buf) return;
    n64SoundBuf_t *nb = (n64SoundBuf_t*)buf;
    nb->vol = vol;
    if (nb->mixer_ch >= 0 && mixer_ch_playing(nb->mixer_ch)) {
        float v = (float)vol;
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        mixer_ch_set_vol_pan(nb->mixer_ch, v, 0.5f);
    }
}

void stdSound_BufferSetPan(stdSound_buffer_t *buf, flex_t pan)
{
    if (!buf) return;
    n64SoundBuf_t *nb = (n64SoundBuf_t*)buf;
    if (nb->mixer_ch >= 0 && mixer_ch_playing(nb->mixer_ch)) {
        float v = (float)nb->vol;
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        // pan: JK passes raw DirectSound pan in [-10000, 10000] millibels.
        // Normalise to [0..1] for libdragon (0=full left, 1=full right, 0.5=centre).
        float p = (float)pan;
        // If value looks like millibels range, normalise; else assume [-1,1]
        if (p < -1.1f || p > 1.1f)
            p = (p / 10000.0f + 1.0f) * 0.5f;
        else
            p = (p + 1.0f) * 0.5f;
        if (p < 0.0f) p = 0.0f;
        if (p > 1.0f) p = 1.0f;
        mixer_ch_set_vol_pan(nb->mixer_ch, v, p);
    }
}

void stdSound_BufferSetFrequency(stdSound_buffer_t *buf, int freq)
{
    if (!buf || freq <= 0) return;
    n64SoundBuf_t *nb = (n64SoundBuf_t*)buf;
    nb->nSamplesPerSec = freq;
    if (nb->is_wav64)
        nb->wav64.wave.frequency = (float)freq;
    else
        nb->wave.frequency = (float)freq;

    if (nb->mixer_ch >= 0 && mixer_ch_playing(nb->mixer_ch))
        mixer_ch_set_freq(nb->mixer_ch, (float)freq);
}

// ── Buffer duplication ────────────────────────────────────────────────────────

stdSound_buffer_t* stdSound_BufferDuplicate(stdSound_buffer_t *src)
{
    if (!src) return NULL;
    n64SoundBuf_t *nb = (n64SoundBuf_t*)src;
    stdSound_buffer_t *dup = stdSound_BufferCreate(
        nb->bStereo, nb->nSamplesPerSec, nb->bitsPerSample, nb->bufferLen);
    if (!dup) return NULL;
    n64SoundBuf_t *nd = (n64SoundBuf_t*)dup;
    if (nb->data && nd->data)
        memcpy(nd->data, nb->data, nb->bufferBytes);
    nd->bIsCopy = 1;
    nd->vol     = nb->vol;
    return dup;
}

// ── 3D audio stubs (N64 has no hardware 3D audio) ────────────────────────────

stdSound_3dBuffer_t* stdSound_BufferQueryInterface(stdSound_buffer_t *buf)
{
    return (stdSound_3dBuffer_t*)buf;
}

void stdSound_3DBufferRelease(stdSound_3dBuffer_t *buf)
{
    stdSound_BufferRelease((stdSound_buffer_t*)buf);
}

int stdSound_3DSetMode(stdSound_buffer_t *buf, int mode)
{
    (void)buf; (void)mode;
    return 1;
}

void stdSound_SetPosition(stdSound_buffer_t *buf, rdVector3 *pos)
{
    if (buf && pos) ((n64SoundBuf_t*)buf)->pos = *pos;
}

void stdSound_SetVelocity(stdSound_buffer_t *buf, rdVector3 *vel)
{
    if (buf && vel) ((n64SoundBuf_t*)buf)->vel = *vel;
}

void stdSound_SetPositionOrientation(rdVector3 *pos, rdVector3 *lvec, rdVector3 *uvec)
{
    (void)pos; (void)lvec; (void)uvec;
}

void stdSound_CommitDeferredSettings(void) {}

void stdSound_SetMenuSoundFormat(int a1) { (void)a1; }
