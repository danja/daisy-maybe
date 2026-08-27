/* res_sample.h — SD card WAV browsing and loading into SDRAM.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Files live in 0:/resonators (the card root is used as a fallback). Samples
 * are excitation material for the resonator bodies: short hits, mallet
 * strikes, noise bursts. Loading is blocking and MUST run from the main loop,
 * never the audio callback.
 *
 * Two hardware constraints shape this:
 *   • SDMMC1's DMA can only reach AXI SRAM, so f_read lands in a small staging
 *     buffer here and is converted/copied into the SDRAM store.
 *   • The audio callback runs throughout, so the player is detached before the
 *     store is touched and only reattached once the load completes. */

#pragma once

#include <cstdint>

namespace res
{

/* Descriptor for one loaded sample. A null `data` means "nothing loaded" and
 * every reader treats that as silence, which is what makes detach-before-load
 * safe against a torn read in the audio callback. */
struct Sample
{
    const float *data = nullptr;
    int frames = 0;
    /* file rate / engine rate, folded into the playback increment so a 44.1 kHz
     * file plays at the right speed with no resampler. */
    float rateRatio = 1.0f;
};

class SampleStore
{
  public:
    static constexpr int kMaxFiles = 32;
    /* FatFS is built with LFN up to 255 chars. A name longer than this is
     * skipped by Scan rather than truncated: a truncated name lists fine and
     * then fails to open, which is a far worse failure than not appearing. */
    static constexpr int kNameLen = 64;

    /* store/capacity: caller-owned SDRAM buffer, in floats. */
    void Init(float *store, uint32_t capacity);

    /* Mounts the card. Safe to call again to retry after a card swap. */
    bool Mount();
    bool Mounted() const { return mounted_; }

    /* Rescans the directory. Returns the file count. */
    int Scan();
    int Count() const { return count_; }
    const char *Name(int i) const;

    /* Blocking load. On success `out` describes the loaded sample; on failure
     * `out` is empty and Status() explains why. */
    bool Load(int index, float engineRate, Sample &out);

    const char *Status() const { return status_; }
    bool Truncated() const { return truncated_; }
    uint32_t LoadedFrames() const { return frames_; }

  private:
    float *store_ = nullptr;
    uint32_t capacity_ = 0;
    uint32_t frames_ = 0;
    int count_ = 0;
    bool mounted_ = false;
    bool truncated_ = false;
    bool skipped_ = false;
    const char *status_ = "no card";
    char names_[kMaxFiles][kNameLen];
    char dir_[16] = "";
};

} // namespace res
