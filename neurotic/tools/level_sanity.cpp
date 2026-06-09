#include "algos/neurotic_algos.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{
constexpr float kSampleRate = 48000.0f;
constexpr int kSeconds = 4;
constexpr int kTotalSamples = static_cast<int>(kSampleRate) * kSeconds;
constexpr int kWarmupSamples = static_cast<int>(kSampleRate);
constexpr float kPi = 3.14159265358979323846f;

const char *kAlgoNames[] = {
    "CrossRes",
    "Braid",
    "TapeHyd",
    "Binaural",
    "Formant",
    "Diffusion",
    "Harmonic",
    "PhaseLoom",
    "Smear",
    "PitchShift",
    "Reverb",
    "EuDelay",
    "WavScram",
    "ModalBank",
    "SympString",
    "FMRes",
    "Bazzer",
};

struct Metrics
{
    double inSumSq = 0.0;
    double outSumSqL = 0.0;
    double outSumSqR = 0.0;
    float inPeak = 0.0f;
    float outPeakL = 0.0f;
    float outPeakR = 0.0f;
    int clipCount = 0;
    int nonFiniteCount = 0;
    int measuredSamples = 0;
};

uint32_t NextRand(uint32_t &state)
{
    state = state * 1664525u + 1013904223u;
    return state;
}

float WhiteNoise(uint32_t &state)
{
    const uint32_t x = NextRand(state);
    const float unit = static_cast<float>((x >> 8) & 0x00FFFFFFu) / static_cast<float>(0x00FFFFFFu);
    return unit * 2.0f - 1.0f;
}

float InputSample(const char *source, int n, uint32_t &noiseState)
{
    if (std::strcmp(source, "sine") == 0)
    {
        return 0.20f * std::sin(2.0f * kPi * 220.0f * static_cast<float>(n) / kSampleRate);
    }
    if (std::strcmp(source, "impulse") == 0)
    {
        return n == kWarmupSamples ? 0.80f : 0.0f;
    }
    return 0.20f * WhiteNoise(noiseState);
}

NeuroticRuntime RuntimeForAlgo(int algoIndex)
{
    NeuroticRuntime rt;
    rt.algoIndex = algoIndex;
    rt.mix = 1.0f;
    rt.outTrim = 1.0f;
    rt.fb = 0.35f;
    rt.c1 = 0.5f;
    rt.c2 = 0.5f;
    rt.c3 = 0.5f;
    rt.c4 = 0.5f;
    rt.c5 = 0.5f;
    rt.lfoDepth = 0.0f;
    rt.lfoRate = 0.2f;

    if (algoIndex == 10)
    {
        rt.c1 = 0.60f; // time
        rt.c2 = 0.55f; // reverb feedback
        rt.c3 = 0.35f; // cross
        rt.c4 = 0.55f; // taps
        rt.c5 = 0.50f; // tilt
    }
    else if (algoIndex == 11)
    {
        rt.fb = 0.45f;
        rt.c1 = 0.50f; // taps
        rt.c2 = 0.00f; // offset, keeps one tap close to full delay
        rt.c3 = 0.50f; // steps
        rt.c4 = 0.50f; // bpm
        rt.c5 = 0.35f; // scale
    }
    else if (algoIndex == 12)
    {
        rt.c1 = 0.45f;
        rt.c2 = 0.35f;
        rt.c3 = 0.65f;
        rt.c4 = 0.45f;
        rt.c5 = 0.50f;
    }
    else if (algoIndex == 13)
    {
        rt.c1 = 0.45f; // root
        rt.c2 = 0.45f; // structure
        rt.c3 = 0.70f; // brightness
        rt.c4 = 0.55f; // damping
        rt.c5 = 0.35f; // position
    }
    else if (algoIndex == 14)
    {
        rt.c1 = 0.42f; // root
        rt.c2 = 0.40f; // chord/spread
        rt.c3 = 0.68f; // damping
        rt.c4 = 0.65f; // brightness
        rt.c5 = 0.50f; // detune
    }
    else if (algoIndex == 15)
    {
        rt.c1 = 0.45f; // carrier
        rt.c2 = 0.45f; // ratio
        rt.c3 = 0.62f; // brightness
        rt.c4 = 0.55f; // feedback
        rt.c5 = 0.50f; // damping
    }
    else if (algoIndex == 16)
    {
        rt.c1 = 0.55f; // bass
        rt.c2 = 0.55f; // high
        rt.c3 = 0.45f; // crossover
        rt.c4 = 0.50f; // mid
        rt.c5 = 0.35f; // drive
    }

    return rt;
}

Metrics RunCase(NeuroticAlgoBank &bank, int algoIndex, const char *source)
{
    bank.Reset(algoIndex);
    NeuroticRuntime rt = RuntimeForAlgo(algoIndex);
    Metrics metrics;
    uint32_t noiseState = 0x12345678u + static_cast<uint32_t>(algoIndex) * 7919u;

    for (int n = 0; n < kTotalSamples; ++n)
    {
        const float in = InputSample(source, n, noiseState);
        float outL = 0.0f;
        float outR = 0.0f;
        bank.Process(algoIndex, in, in, rt, outL, outR);

        if (n < kWarmupSamples)
        {
            continue;
        }

        metrics.measuredSamples++;
        metrics.inSumSq += static_cast<double>(in) * static_cast<double>(in);
        metrics.outSumSqL += static_cast<double>(outL) * static_cast<double>(outL);
        metrics.outSumSqR += static_cast<double>(outR) * static_cast<double>(outR);
        metrics.inPeak = std::max(metrics.inPeak, std::fabs(in));
        metrics.outPeakL = std::max(metrics.outPeakL, std::fabs(outL));
        metrics.outPeakR = std::max(metrics.outPeakR, std::fabs(outR));
        if (!std::isfinite(outL) || !std::isfinite(outR))
        {
            metrics.nonFiniteCount++;
        }
        if (std::fabs(outL) >= 0.999f || std::fabs(outR) >= 0.999f)
        {
            metrics.clipCount++;
        }
    }

    return metrics;
}

void PrintCase(int algoIndex, const char *source, const Metrics &m, bool csv)
{
    const double inRms = std::sqrt(m.inSumSq / std::max(1, m.measuredSamples));
    const double outRmsL = std::sqrt(m.outSumSqL / std::max(1, m.measuredSamples));
    const double outRmsR = std::sqrt(m.outSumSqR / std::max(1, m.measuredSamples));
    const double outRms = 0.5 * (outRmsL + outRmsR);
    const double ratio = inRms > 1.0e-12 ? outRms / inRms : outRms;
    const double db = ratio > 1.0e-12 ? 20.0 * std::log10(ratio) : -120.0;
    const float peak = std::max(m.outPeakL, m.outPeakR);

    const char *status = "ok";
    if (m.nonFiniteCount > 0 || m.clipCount > 0 || peak > 1.2f)
    {
        status = "fail";
    }
    else if (std::strcmp(source, "impulse") != 0 && (db < -12.0 || db > 8.0 || peak > 0.95f))
    {
        status = "warn";
    }

    if (csv)
    {
        std::printf("%d,%s,%s,%.6f,%.6f,%.2f,%.6f,%.6f,%d,%d,%s\n",
                    algoIndex,
                    kAlgoNames[algoIndex],
                    source,
                    inRms,
                    outRms,
                    db,
                    m.inPeak,
                    peak,
                    m.clipCount,
                    m.nonFiniteCount,
                    status);
    }
    else
    {
        std::printf("%2d %-10s %-7s ratio=%6.2f dB=%7.2f peak=%6.3f clips=%4d finite=%s %s\n",
                    algoIndex,
                    kAlgoNames[algoIndex],
                    source,
                    ratio,
                    db,
                    peak,
                    m.clipCount,
                    m.nonFiniteCount == 0 ? "yes" : "no",
                    status);
    }
}
} // namespace

int main(int argc, char **argv)
{
    const bool csv = argc > 1 && std::strcmp(argv[1], "--csv") == 0;
    NeuroticAlgoBank bank;
    bank.Init(kSampleRate);

    if (csv)
    {
        std::puts("algo,name,source,in_rms,out_rms,ratio_db,in_peak,out_peak,clips,nonfinite,status");
    }

    const char *sources[] = {"sine", "noise", "impulse"};
    for (int algo = 0; algo < static_cast<int>(sizeof(kAlgoNames) / sizeof(kAlgoNames[0])); ++algo)
    {
        for (const char *source : sources)
        {
            PrintCase(algo, source, RunCase(bank, algo, source), csv);
        }
    }

    return 0;
}
