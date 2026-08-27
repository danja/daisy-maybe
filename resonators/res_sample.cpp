/* res_sample.cpp — SD card WAV browsing and loading into SDRAM.
 * SPDX-License-Identifier: Apache-2.0 */

#include "res_sample.h"

#include "res_wav.h"

#include "daisy_seed.h"
#include "ff.h"
#include "sys/fatfs.h"

namespace res
{

namespace
{
/* FatFS objects and the read staging buffer must live in AXI SRAM (SDMMC1's
 * DMA cannot reach DTCMRAM, and the SDRAM store is not DMA-addressable for
 * this peripheral either) — plain .bss is AXI SRAM on this build. */
daisy::FatFSInterface gFsi;
FATFS *gFs = nullptr;
FIL gFil;
constexpr uint32_t kStageBytes = 4096;
uint8_t gStage[kStageBytes];

bool EndsWithWav(const char *s)
{
    int n = 0;
    while(s[n])
        n++;
    if(n < 5)
        return false;
    auto lower = [](char c) { return (c >= 'A' && c <= 'Z') ? char(c + 32) : c; };
    return s[n - 4] == '.' && lower(s[n - 3]) == 'w' && lower(s[n - 2]) == 'a'
           && lower(s[n - 1]) == 'v';
}

/* Returns false if `src` did not fit, so callers can reject rather than
 * silently store an unusable prefix. */
bool CopyStr(char *dst, const char *src, int len)
{
    int i = 0;
    for(; i < len - 1 && src[i]; ++i)
        dst[i] = src[i];
    dst[i] = '\0';
    return src[i] == '\0';
}
} // namespace

/* WavReader over an open FatFS file. Non-static so the vtable is emitted once. */
struct FilReader : WavReader
{
    FIL *f;
    explicit FilReader(FIL *fp) : f(fp) {}
    uint32_t Read(void *dst, uint32_t bytes) override
    {
        UINT br = 0;
        if(f_read(f, dst, bytes, &br) != FR_OK)
            return 0;
        return uint32_t(br);
    }
    bool Seek(uint32_t pos) override { return f_lseek(f, pos) == FR_OK; }
    uint32_t Size() const override { return uint32_t(f_size(f)); }
};

void SampleStore::Init(float *store, uint32_t capacity)
{
    store_ = store;
    capacity_ = capacity;
    count_ = 0;
    frames_ = 0;
    for(int i = 0; i < kMaxFiles; ++i)
        names_[i][0] = '\0';
}

bool SampleStore::Mount()
{
    if(gFsi.Init(daisy::FatFSInterface::Config::MEDIA_SD)
       != daisy::FatFSInterface::Result::OK)
    {
        mounted_ = false;
        status_ = "fs init";
        return false;
    }
    gFs = &gFsi.GetSDFileSystem();
    if(f_mount(gFs, gFsi.GetSDPath(), 1) != FR_OK)
    {
        mounted_ = false;
        status_ = "no card";
        return false;
    }
    mounted_ = true;
    status_ = "ready";
    return true;
}

const char *SampleStore::Name(int i) const
{
    if(i < 0 || i >= count_)
        return "-";
    return names_[i];
}

int SampleStore::Scan()
{
    count_ = 0;
    skipped_ = false;
    if(!mounted_ && !Mount())
        return 0;

    /* Prefer 0:/resonators, fall back to the card root. */
    DIR dir;
    const char *sd = gFsi.GetSDPath();
    char path[24];
    int n = 0;
    for(; sd[n] && n < 8; ++n)
        path[n] = sd[n];
    CopyStr(path + n, "resonators", int(sizeof(path)) - n);

    if(f_opendir(&dir, path) == FR_OK)
    {
        CopyStr(dir_, path, sizeof(dir_));
    }
    else if(f_opendir(&dir, sd) == FR_OK)
    {
        CopyStr(dir_, sd, sizeof(dir_));
    }
    else
    {
        status_ = "no dir";
        return 0;
    }

    FILINFO fno;
    while(count_ < kMaxFiles)
    {
        if(f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0)
            break;
        if(fno.fattrib & (AM_DIR | AM_HID | AM_SYS))
            continue;
        if(fno.fname[0] == '.')
            continue; // macOS resource forks
        if(!EndsWithWav(fno.fname))
            continue;
        /* Skip rather than truncate — see kNameLen. */
        if(!CopyStr(names_[count_], fno.fname, kNameLen))
        {
            skipped_ = true;
            continue;
        }
        count_++;
    }
    f_closedir(&dir);
    status_ = count_ ? "ready" : (skipped_ ? "name long" : "no wavs");
    return count_;
}

bool SampleStore::Load(int index, float engineRate, Sample &out)
{
    out = Sample{};
    truncated_ = false;
    frames_ = 0;

    if(index < 0 || index >= count_ || store_ == nullptr)
    {
        status_ = "no file";
        return false;
    }

    char path[sizeof(dir_) + 1 + kNameLen];
    int n = 0;
    for(; dir_[n] && n < int(sizeof(dir_)) - 1; ++n)
        path[n] = dir_[n];
    if(n && path[n - 1] != '/' && path[n - 1] != ':')
        path[n++] = '/';
    CopyStr(path + n, names_[index], int(sizeof(path)) - n);

    if(f_open(&gFil, path, FA_READ) != FR_OK)
    {
        status_ = "open err";
        return false;
    }

    FilReader reader(&gFil);
    WavInfo info;
    const WavResult wr = ParseWav(reader, info);
    if(wr != WavResult::Ok)
    {
        f_close(&gFil);
        status_ = WavResultName(wr);
        return false;
    }

    const uint32_t frameBytes = uint32_t(info.channels) * (info.bits / 8u);
    uint32_t want = info.frames;
    if(want > capacity_)
    {
        want = capacity_;
        truncated_ = true;
    }

    if(!reader.Seek(info.dataOffset))
    {
        f_close(&gFil);
        status_ = "seek err";
        return false;
    }

    /* Read whole frames at a time so a chunk boundary never splits a frame. */
    const uint32_t framesPerChunk = kStageBytes / frameBytes;
    uint32_t done = 0;
    while(done < want)
    {
        uint32_t chunk = want - done;
        if(chunk > framesPerChunk)
            chunk = framesPerChunk;
        const uint32_t got = reader.Read(gStage, chunk * frameBytes);
        if(got < frameBytes)
            break; // EOF or read error: keep what we have
        const uint32_t haveFrames = got / frameBytes;
        WavDecodeMono(gStage, haveFrames, info, store_ + done);
        done += haveFrames;
        if(haveFrames < chunk)
            break;
    }
    f_close(&gFil);

    if(done == 0)
    {
        status_ = "empty";
        return false;
    }

    frames_ = done;
    out.data = store_;
    out.frames = int(done);
    out.rateRatio = float(info.sampleRate) / engineRate;
    status_ = truncated_ ? "trunc" : "loaded";
    return true;
}

} // namespace res
