#pragma once

#include <algorithm>
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
        for(size_t i = 0; i < max_size; ++i)
        {
            line_[i] = 0.0f;
        }
        write_ptr_ = 0;
    }

    void SetDelay(float delay)
    {
        delay_ = std::clamp(delay, 1.0f, static_cast<float>(max_size - 2));
    }

    float Read() const
    {
        const int32_t delay_integral = static_cast<int32_t>(delay_);
        const float   delay_fractional = delay_ - static_cast<float>(delay_integral);
        const float a = line_[(write_ptr_ + delay_integral) % max_size];
        const float b = line_[(write_ptr_ + delay_integral + 1) % max_size];
        return a + (b - a) * delay_fractional;
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
    float  line_[max_size];
    size_t write_ptr_ = 0;
    float  delay_ = 1.0f;
};

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

    void Write1(float v) { d1.Write(v); }
    void Write2(float v) { d2.Write(v); }

    void AddAt1(float delay, float v) { d1.AddAt(delay, v); }
    void AddAt2(float delay, float v) { d2.AddAt(delay, v); }
};
