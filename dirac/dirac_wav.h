/* dirac_wav.h — minimal WAV header parser
 * SPDX-License-Identifier: Apache-2.0
 *
 * Platform-free: the reader interface is implemented over FatFS `FIL` on
 * hardware and over a memory buffer in the host harness, so malformed-file
 * handling can be tested without a card.
 *
 * Handles: RIFF/WAVE, PCM (format 1) and IEEE float (format 3), 8/16/24/32-bit,
 * any channel count, chunks in any order, odd chunk sizes (pad byte), and
 * `LIST`/`fact`/junk chunks before `data`. */

#pragma once

#include <cstdint>

namespace dirac
{

struct WavReader
{
    virtual ~WavReader() = default;
    /* Reads `bytes` into dst. Returns the number actually read (< bytes at EOF). */
    virtual uint32_t Read(void *dst, uint32_t bytes) = 0;
    virtual bool Seek(uint32_t pos) = 0;
    virtual uint32_t Size() const = 0;
};

struct WavInfo
{
    uint32_t dataOffset = 0; // byte offset of the first sample frame
    uint32_t dataBytes = 0;  // length of the data chunk, clamped to the file
    uint32_t sampleRate = 0;
    uint16_t channels = 0;
    uint16_t bits = 0;
    uint16_t format = 0; // 1 = PCM, 3 = IEEE float
    uint32_t frames = 0; // dataBytes / (channels * bits/8)
};

enum class WavResult
{
    Ok,
    NotRiff,      // missing RIFF/WAVE magic
    NoFmt,        // no fmt chunk
    NoData,       // no data chunk
    Unsupported,  // format/bit-depth we can't decode
    Truncated,    // header ran past the end of the file
};

/* Parses the header only; leaves the reader positioned anywhere. */
WavResult ParseWav(WavReader &r, WavInfo &info);

/* Decodes `frames` frames of interleaved PCM from `src` into mono float in
 * `dst`, averaging channels. Returns frames written. `src` must hold at least
 * frames * channels * (bits/8) bytes. */
uint32_t WavDecodeMono(const uint8_t *src, uint32_t frames, const WavInfo &info,
                       float *dst);

const char *WavResultName(WavResult r);

} // namespace dirac
