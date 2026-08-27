/* sample_voice.h — polyphonic one-shot player for SD samples.
 * SPDX-License-Identifier: Apache-2.0
 *
 * The resonator bodies are passive: they only ring when something hits them.
 * This is the hammer. Each voice plays a loaded sample once from the top at a
 * rate set by the triggering MIDI note, with velocity as gain, and retires
 * when it runs off the end.
 *
 * Trigger() is called from the main loop and Process() from the audio
 * callback. Both only ever touch a voice through its `active` flag, which the
 * main loop sets last and the callback clears — a torn read costs at most one
 * block, and there is no lock affordable here. */

#pragma once

#include <cmath>

#include "res_sample.h"

namespace res
{

class SampleVoices
{
  public:
    static constexpr int kVoices = 4;
    /* Middle C plays the file at its stored pitch. */
    static constexpr int kRootNote = 60;

    void Init(float sampleRate)
    {
        sampleRate_ = sampleRate;
        AllOff();
    }

    void AllOff()
    {
        for(int v = 0; v < kVoices; ++v)
        {
            voice_[v].active = false;
            voice_[v].pos = 0.0f;
        }
        next_ = 0;
    }

    /* Detach before the store is rewritten; every voice reads `sample_.data`,
     * so clearing the descriptor is what makes a load safe mid-playback. */
    void SetSample(const Sample &s)
    {
        AllOff();
        sample_ = s;
    }
    bool HasSample() const { return sample_.data != nullptr && sample_.frames > 0; }
    int Frames() const { return sample_.frames; }

    /* semitones: offset from kRootNote. gain: 0..1, normally MIDI velocity. */
    void Trigger(float semitones, float gain)
    {
        if(!HasSample())
            return;

        /* Round-robin, but steal a finished voice first so a held chord does
         * not cut its own tail off. */
        int slot = -1;
        for(int v = 0; v < kVoices; ++v)
        {
            if(!voice_[v].active)
            {
                slot = v;
                break;
            }
        }
        if(slot < 0)
        {
            slot = next_;
            next_ = (next_ + 1) % kVoices;
        }

        Voice &vo = voice_[slot];
        vo.pos = 0.0f;
        vo.inc = sample_.rateRatio * std::pow(2.0f, semitones / 12.0f);
        vo.gain = gain;
        vo.active = true;
    }

    /* One mono sample of the summed voices. */
    float Process()
    {
        const float *data = sample_.data;
        const int frames = sample_.frames;
        if(data == nullptr || frames <= 0)
            return 0.0f;

        float sum = 0.0f;
        for(int v = 0; v < kVoices; ++v)
        {
            Voice &vo = voice_[v];
            if(!vo.active)
                continue;

            const int i0 = static_cast<int>(vo.pos);
            if(i0 >= frames - 1)
            {
                vo.active = false;
                continue;
            }
            const float frac = vo.pos - static_cast<float>(i0);
            sum += (data[i0] + (data[i0 + 1] - data[i0]) * frac) * vo.gain;
            vo.pos += vo.inc;
        }
        return sum;
    }

    int ActiveCount() const
    {
        int n = 0;
        for(int v = 0; v < kVoices; ++v)
            if(voice_[v].active)
                n++;
        return n;
    }

  private:
    struct Voice
    {
        float pos = 0.0f;
        float inc = 1.0f;
        float gain = 0.0f;
        bool active = false;
    };

    float sampleRate_ = 48000.0f;
    Sample sample_{};
    Voice voice_[kVoices];
    int next_ = 0;
};

} // namespace res
