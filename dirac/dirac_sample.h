/* dirac_sample.h — SD card WAV browsing and loading into SDRAM.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Files live in 0:/dirac (any .wav; the card root is used as a fallback). Loading is
 * blocking and MUST run in the main loop, never the audio callback.
 *
 * Two hardware constraints shape this:
 *   • SDMMC1's DMA can only reach AXI SRAM, so f_read lands in a small staging
 *     buffer here and is converted/copied into the SDRAM store.
 *   • The audio callback runs throughout, so the engine is detached before the
 *     store is touched and only reattached once the load completes. */

#pragma once

#include <cstdint>

#include "dirac_engine.h"

namespace dirac
{

class SampleStore
{
  public:
    static constexpr int kMaxFiles = 64;
    static constexpr int kNameLen = 24;

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
     * `out` is empty (live mode) and Status() explains why. */
    bool Load(int index, float engineRate, DiracSample &out);

    const char *Status() const { return status_; }
    bool Truncated() const { return truncated_; }
    uint32_t LoadedFrames() const { return frames_; }
    uint32_t CapacityFrames() const { return capacity_; }
    /* 0..100 while a load is running (polled from the display). */
    int Progress() const { return progress_; }

  private:
    float *store_ = nullptr;
    uint32_t capacity_ = 0;
    uint32_t frames_ = 0;
    int count_ = 0;
    bool mounted_ = false;
    bool truncated_ = false;
    int progress_ = 0;
    const char *status_ = "no card";
    char names_[kMaxFiles][kNameLen];
    char dir_[16] = "";
};

} // namespace dirac
