#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

constexpr size_t kMaxDelaySamples = 48000;

template <size_t max_size>
class DelayBuffer
{
public:
    void Init()
    {
        Reset();
    }

    void Reset()
    {
        for (size_t i = 0; i < max_size; ++i)
        {
            line_[i] = 0.0f;
        }
        write_ptr_ = 0;
    }

    void SetDelay(float delay)
    {
        delay_ = std::clamp(delay, 1.0f, static_cast<float>(max_size - 2));
    }

    float ReadAt(float delay) const
    {
        const float clamped = std::clamp(delay, 1.0f, static_cast<float>(max_size - 2));
        const int32_t delay_integral = static_cast<int32_t>(clamped);
        const float delay_fractional = clamped - static_cast<float>(delay_integral);
        const float a = line_[(write_ptr_ + delay_integral) % max_size];
        const float b = line_[(write_ptr_ + delay_integral + 1) % max_size];
        return a + (b - a) * delay_fractional;
    }

    float Read() const
    {
        return ReadAt(delay_);
    }

    void Write(float sample)
    {
        line_[write_ptr_] = sample;
        write_ptr_ = (write_ptr_ - 1 + max_size) % max_size;
    }

    void AddAt(float delay, float sample)
    {
        const float clamped = std::clamp(delay, 0.0f, static_cast<float>(max_size - 2));
        const int32_t delay_integral = static_cast<int32_t>(clamped);
        const float delay_fractional = clamped - static_cast<float>(delay_integral);
        const size_t idx = (write_ptr_ + delay_integral) % max_size;
        const size_t idx2 = (idx + 1) % max_size;
        line_[idx] += sample * (1.0f - delay_fractional);
        line_[idx2] += sample * delay_fractional;
    }

private:
    float line_[max_size];
    size_t write_ptr_ = 0;
    float delay_ = 1.0f;
};

/* Multi-tap read at incommensurate fractions of the base delay, normalised by
 * the tap gains so the count does not change the level. Lives here rather than
 * in main.cpp so host_dsp/loop_fixture.cpp measures the same reader the
 * firmware runs — the Delay makeup gain is derived from it. */
inline float ReadPrimeSpacedTaps(const DelayBuffer<kMaxDelaySamples> &delay,
                                 float baseDelay,
                                 int tapCount)
{
    constexpr float tapRatios[] = {1.0f, 0.733f, 0.569f, 0.421f, 0.317f};
    constexpr float tapGains[] = {1.0f, 0.82f, 0.68f, 0.56f, 0.46f};

    const int count = std::clamp(
        tapCount, 1, static_cast<int>(sizeof(tapRatios) / sizeof(tapRatios[0])));
    float sum = 0.0f;
    float gainSq = 0.0f;
    for (int tap = 0; tap < count; ++tap)
    {
        const float gain = tapGains[tap];
        sum += delay.ReadAt(baseDelay * tapRatios[tap]) * gain;
        gainSq += gain * gain;
    }

    /* Power normalisation, not amplitude. The taps sit at incommensurate
     * fractions of the delay so what they return is uncorrelated; dividing by
     * the sum of gains treats it as if it were coherent and buries the level —
     * five taps measured 0.08x the input that way. Dividing by the root of the
     * summed squares is the right normaliser for uncorrelated sources and
     * holds the level roughly constant as Taps changes. */
    return gainSq > 0.0f ? sum / std::sqrt(gainSq) : 0.0f;
}

struct DelayLinePair
{
    DelayBuffer<kMaxDelaySamples> d1;
    DelayBuffer<kMaxDelaySamples> d2;

    void Init()
    {
        d1.Init();
        d2.Init();
    }

    void SetDelayTimes(float delay1, float delay2)
    {
        d1.SetDelay(delay1);
        d2.SetDelay(delay2);
    }

    float Read1() const { return d1.Read(); }
    float Read2() const { return d2.Read(); }
    float ReadAt1(float delay) const { return d1.ReadAt(delay); }
    float ReadAt2(float delay) const { return d2.ReadAt(delay); }

    void Write1(float v) { d1.Write(v); }
    void Write2(float v) { d2.Write(v); }

    void AddAt1(float delay, float v) { d1.AddAt(delay, v); }
    void AddAt2(float delay, float v) { d2.AddAt(delay, v); }
};
