#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <vector>

#include "spectral_processor.h"

namespace
{
constexpr float kSampleRate = 48000.0f;
constexpr float kDurationSec = 3.0f;
constexpr float kInputAmp = 0.25f;
constexpr float kFrequency = 440.0f;
constexpr float kPeakLimit = 0.95f;
constexpr float kMinPeak = 0.001f;
constexpr float kMinRms = 0.0005f;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kVibeValues[] = {0.0f, 0.8f};
constexpr float kMixValues[] = {0.0f, 0.5f, 1.0f};
constexpr float kInputGain = 1.4f;
constexpr float kOutputGain = 0.9f;
constexpr float kWetTrim = 0.8f;

float SoftClipInput(float sample)
{
    const float absSample = std::fabs(sample);
    return sample / (1.0f + absSample);
}

float SoftClip(float sample)
{
    return std::clamp(sample, -1.0f, 1.0f);
}

void BuildWindow(std::vector<float> &window)
{
    window.resize(SpectralChannel::kFftSize);
    for (size_t i = 0; i < window.size(); ++i)
    {
        const float phase = static_cast<float>(i) / static_cast<float>(window.size());
        const float hann = 0.5f - 0.5f * std::cos(2.0f * kPi * phase);
        window[i] = std::sqrt(std::max(hann, 0.0f));
    }
}

const char *ProcessName(SpectralProcess process)
{
    switch (process)
    {
    case SpectralProcess::Smear:
        return "Smear";
    case SpectralProcess::Shift:
        return "Shift";
    case SpectralProcess::Comb:
        return "Comb";
    case SpectralProcess::Freeze:
        return "Freeze";
    case SpectralProcess::Gate:
        return "Gate";
    case SpectralProcess::Tilt:
        return "Tilt";
    case SpectralProcess::Fold:
        return "Fold";
    case SpectralProcess::Phase:
        return "Phase";
    default:
        return "Unknown";
    }
}
} // namespace

int main()
{
    std::vector<float> window;
    BuildWindow(window);

    const size_t totalSamples = static_cast<size_t>(kSampleRate * kDurationSec);
    const size_t warmup = SpectralChannel::kFftSize * 4;
    bool ok = true;

    for (int p = 0; p < static_cast<int>(SpectralProcess::Count); ++p)
    {
        const auto process = static_cast<SpectralProcess>(p);
        for (float mix : kMixValues)
        {
            for (float vibe : kVibeValues)
            {
                SpectralChannel channel1;
                SpectralChannel channel2;
                channel1.Init(kSampleRate, window.data());
                channel2.Init(kSampleRate, window.data());

                float peak = 0.0f;
                double sumSq = 0.0;
                size_t count = 0;

                const float wetMix = mix;
                const float dryMix = 1.0f - mix;

                for (size_t i = 0; i < totalSamples; ++i)
                {
                    const float phase = 2.0f * kPi * kFrequency * static_cast<float>(i) / kSampleRate;
                    const float input = kInputAmp * std::sin(phase);
                    const float in1 = SoftClipInput(input * kInputGain);
                    const float in2 = SoftClipInput(input * kInputGain);
                    const float wet1 = SoftClipInput(channel1.ProcessSample(in1, process, 1.0f, vibe)) * kWetTrim;
                    const float wet2 = SoftClipInput(channel2.ProcessSample(in2, process, 1.0f, vibe)) * kWetTrim;
                    const float mix1 = (dryMix * in1 + wetMix * wet1) * kOutputGain;
                    const float mix2 = (dryMix * in2 + wetMix * wet2) * kOutputGain;
                    const float output = SoftClip(0.5f * (mix1 + mix2));

                    if (i >= warmup)
                    {
                        const float absOut = std::fabs(output);
                        peak = std::max(peak, absOut);
                        sumSq += static_cast<double>(output) * static_cast<double>(output);
                        count++;
                        if (!std::isfinite(output))
                        {
                            ok = false;
                        }
                    }
                }

                const float rms = count > 0 ? std::sqrt(static_cast<float>(sumSq / count)) : 0.0f;
                std::printf("%s mix=%.2f vibe=%.2f peak=%.4f rms=%.4f\n",
                            ProcessName(process),
                            static_cast<double>(mix),
                            static_cast<double>(vibe),
                            static_cast<double>(peak),
                            static_cast<double>(rms));

                float minPeak = kMinPeak;
                float minRms = kMinRms;
                if (process == SpectralProcess::Fold && vibe < 0.1f)
                {
                    minPeak = 0.0f;
                    minRms = 0.0f;
                }
                if (mix < 0.01f)
                {
                    minPeak = kMinPeak * 0.5f;
                    minRms = kMinRms * 0.5f;
                }

                if (peak > kPeakLimit || peak < minPeak || rms < minRms)
                {
                    ok = false;
                }
            }
        }
    }

    if (!ok)
    {
        std::fprintf(stderr, "Amplitude sanity check failed (peak > %.2f or non-finite output).\n",
                     static_cast<double>(kPeakLimit));
        return 1;
    }

    std::printf("Amplitude sanity check passed.\n");
    return 0;
}
