/* dirac_wav.cpp — minimal WAV header parser + mono decoder
 * SPDX-License-Identifier: Apache-2.0 */

#include "dirac_wav.h"

#include <cstring>

namespace dirac
{

namespace
{
inline uint32_t Rd32(const uint8_t *p)
{
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16)
           | (uint32_t(p[3]) << 24);
}
inline uint16_t Rd16(const uint8_t *p)
{
    return uint16_t(uint16_t(p[0]) | (uint16_t(p[1]) << 8));
}
inline bool Tag(const uint8_t *p, const char *s)
{
    return p[0] == uint8_t(s[0]) && p[1] == uint8_t(s[1]) && p[2] == uint8_t(s[2])
           && p[3] == uint8_t(s[3]);
}
} // namespace

const char *WavResultName(WavResult r)
{
    switch(r)
    {
        case WavResult::Ok: return "ok";
        case WavResult::NotRiff: return "not-riff";
        case WavResult::NoFmt: return "no-fmt";
        case WavResult::NoData: return "no-data";
        case WavResult::Unsupported: return "unsupported";
        case WavResult::Truncated: return "truncated";
    }
    return "?";
}

WavResult ParseWav(WavReader &r, WavInfo &info)
{
    info = WavInfo{};

    uint8_t hdr[12];
    if(!r.Seek(0) || r.Read(hdr, 12) != 12)
        return WavResult::NotRiff;
    if(!Tag(hdr, "RIFF") || !Tag(hdr + 8, "WAVE"))
        return WavResult::NotRiff;

    const uint32_t fileSize = r.Size();
    uint32_t pos = 12;
    bool haveFmt = false, haveData = false;

    /* Walk the chunk list. Chunks are 8-byte header + payload, word-aligned
     * (an odd size carries a pad byte). Stop once we have both fmt and data —
     * or at EOF, which is how truncated files land on NoData. */
    while(pos + 8 <= fileSize && !(haveFmt && haveData))
    {
        uint8_t ch[8];
        if(!r.Seek(pos) || r.Read(ch, 8) != 8)
            break;
        const uint32_t size = Rd32(ch + 4);
        const uint32_t body = pos + 8;

        if(Tag(ch, "fmt "))
        {
            if(size < 16)
                return WavResult::Unsupported;
            uint8_t fmt[16];
            if(r.Read(fmt, 16) != 16)
                return WavResult::Truncated;
            info.format = Rd16(fmt + 0);
            info.channels = Rd16(fmt + 2);
            info.sampleRate = Rd32(fmt + 4);
            info.bits = Rd16(fmt + 14);
            /* WAVE_FORMAT_EXTENSIBLE carries the real format in the GUID's
             * first two bytes. */
            if(info.format == 0xFFFE && size >= 40)
            {
                uint8_t ext[10];
                if(r.Read(ext, 10) == 10)
                    info.format = Rd16(ext + 8);
            }
            haveFmt = true;
        }
        else if(Tag(ch, "data"))
        {
            info.dataOffset = body;
            /* Clamp: a wrong size field (or a truncated file) must not send the
             * loader reading past EOF. */
            const uint32_t avail = (body < fileSize) ? (fileSize - body) : 0;
            info.dataBytes = (size < avail) ? size : avail;
            haveData = true;
        }

        /* Guard against a zero/overflowing size field spinning the loop. */
        const uint32_t step = 8 + size + (size & 1u);
        if(step < 8 || body + size < body)
            break;
        pos += step;
    }

    if(!haveFmt)
        return WavResult::NoFmt;
    if(!haveData || info.dataBytes == 0)
        return WavResult::NoData;
    if(info.channels == 0 || info.sampleRate == 0)
        return WavResult::Unsupported;

    const bool pcm = (info.format == 1)
                     && (info.bits == 8 || info.bits == 16 || info.bits == 24
                         || info.bits == 32);
    const bool flt = (info.format == 3) && (info.bits == 32);
    if(!pcm && !flt)
        return WavResult::Unsupported;

    const uint32_t frameBytes = uint32_t(info.channels) * (info.bits / 8u);
    info.frames = info.dataBytes / frameBytes;
    if(info.frames == 0)
        return WavResult::NoData;
    return WavResult::Ok;
}

uint32_t WavDecodeMono(const uint8_t *src, uint32_t frames, const WavInfo &info,
                       float *dst)
{
    const uint32_t nc = info.channels;
    const uint32_t bps = info.bits / 8u;
    const float inv = 1.0f / float(nc);

    for(uint32_t f = 0; f < frames; ++f)
    {
        float sum = 0.0f;
        for(uint32_t c = 0; c < nc; ++c)
        {
            const uint8_t *p = src + (size_t(f) * nc + c) * bps;
            float v = 0.0f;
            if(info.format == 3)
            {
                float tmp;
                memcpy(&tmp, p, 4);
                v = tmp;
            }
            else
            {
                switch(info.bits)
                {
                    case 8: // unsigned
                        v = (float(p[0]) - 128.0f) * (1.0f / 128.0f);
                        break;
                    case 16:
                        v = float(int16_t(Rd16(p))) * (1.0f / 32768.0f);
                        break;
                    case 24:
                    {
                        int32_t s = int32_t(uint32_t(p[0]) | (uint32_t(p[1]) << 8)
                                            | (uint32_t(p[2]) << 16));
                        if(s & 0x800000)
                            s |= int32_t(0xFF000000u); // sign extend
                        v = float(s) * (1.0f / 8388608.0f);
                        break;
                    }
                    case 32:
                        v = float(int32_t(Rd32(p))) * (1.0f / 2147483648.0f);
                        break;
                    default: break;
                }
            }
            sum += v;
        }
        dst[f] = sum * inv;
    }
    return frames;
}

} // namespace dirac
