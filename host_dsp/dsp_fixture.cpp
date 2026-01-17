#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <vector>

#include "spectral_processor.h"

namespace
{
constexpr float kSampleRate = 48000.0f;
constexpr float kFrequency = 1000.0f;
constexpr float kInputAmp = 0.2f;
constexpr float kTimeRatio = 1.0f;
constexpr float kVibe = 0.5f;
constexpr float kPi = 3.14159265358979323846f;
constexpr size_t kSeconds = 2;
constexpr size_t kWarmupFrames = 4;

const char *ProcessName(SpectralProcess process)
{
    switch (process)
    {
    case SpectralProcess::Thru:
        return "Thru";
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

struct Goertzel
{
    float coeff = 0.0f;
    float q1 = 0.0f;
    float q2 = 0.0f;

    void Init(float targetHz, size_t n, float sampleRate)
    {
        const float k = std::round(static_cast<float>(n) * targetHz / sampleRate);
        const float w = (2.0f * kPi * k) / static_cast<float>(n);
        coeff = 2.0f * std::cos(w);
        q1 = 0.0f;
        q2 = 0.0f;
    }

    void Process(float sample)
    {
        const float q0 = coeff * q1 - q2 + sample;
        q2 = q1;
        q1 = q0;
    }

    float MagnitudeSquared() const
    {
        return q1 * q1 + q2 * q2 - coeff * q1 * q2;
    }
};

struct ThdResult
{
    float rms = 0.0f;
    float fundRms = 0.0f;
    float thdn = 0.0f;
    float harmonics[5]{};
};

ThdResult MeasureThd(const std::vector<float> &samples, float sampleRate, float freq)
{
    const size_t n = samples.size();
    double sumSq = 0.0;
    for (float s : samples)
        sumSq += static_cast<double>(s) * static_cast<double>(s);

    Goertzel goertzel[5];
    for (int i = 0; i < 5; ++i)
        goertzel[i].Init(freq * static_cast<float>(i + 1), n, sampleRate);

    for (float s : samples)
    {
        for (auto &g : goertzel)
            g.Process(s);
    }

    ThdResult result{};
    result.rms = std::sqrt(static_cast<float>(sumSq / static_cast<double>(n)));
    const float fundMag = std::sqrt(goertzel[0].MagnitudeSquared());
    result.fundRms = (std::sqrt(2.0f) * fundMag) / static_cast<float>(n);
    for (int i = 0; i < 5; ++i)
    {
        const float mag = std::sqrt(goertzel[i].MagnitudeSquared());
        result.harmonics[i] = (std::sqrt(2.0f) * mag) / static_cast<float>(n);
    }
    const float totalSq = result.rms * result.rms;
    const float fundSq = result.fundRms * result.fundRms;
    const float noiseSq = std::max(0.0f, totalSq - fundSq);
    result.thdn = (result.fundRms > 0.0f) ? std::sqrt(noiseSq) / result.fundRms : 0.0f;
    return result;
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
} // namespace

int main()
{
    std::vector<float> window;
    BuildWindow(window);

    const size_t totalSamples = static_cast<size_t>(kSampleRate) * kSeconds;
    const size_t warmup = SpectralChannel::kFftSize * kWarmupFrames;
    const size_t capture = static_cast<size_t>(kSampleRate);

    std::printf("process, rms, fund, thdn, h2, h3, h4, h5\n");

    for (int p = 0; p < static_cast<int>(SpectralProcess::Count); ++p)
    {
        const auto process = static_cast<SpectralProcess>(p);
        SpectralChannel channel;
        channel.Init(kSampleRate, window.data());

        std::vector<float> output;
        output.reserve(capture);

        for (size_t i = 0; i < totalSamples; ++i)
        {
            const float phase = 2.0f * kPi * kFrequency * static_cast<float>(i) / kSampleRate;
            const float input = kInputAmp * std::sin(phase);
            const float wet = channel.ProcessSample(input,
                                                    process,
                                                    kTimeRatio,
                                                    kVibe,
                                                    0.0f,
                                                    1.0f,
                                                    1.0f,
                                                    1.0f,
                                                    true,
                                                    true,
                                                    true);
            if (i >= warmup && output.size() < capture)
            {
                output.push_back(wet);
            }
        }

        const ThdResult result = MeasureThd(output, kSampleRate, kFrequency);
        std::printf("%s, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f\n",
                    ProcessName(process),
                    result.rms,
                    result.fundRms,
                    result.thdn,
                    result.harmonics[1],
                    result.harmonics[2],
                    result.harmonics[3],
                    result.harmonics[4]);
    }
    return 0;
}
