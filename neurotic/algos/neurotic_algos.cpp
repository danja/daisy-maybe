#include "neurotic_algos.h"
#include "neurotic_config.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;
constexpr size_t kFftSize = 1024;
constexpr size_t kHopSize = 256;
constexpr size_t kBins = kFftSize / 2 + 1;

float Clamp01(float v)
{
    return std::clamp(v, 0.0f, 1.0f);
}

float Lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

float SoftClip(float x)
{
    const float absx = std::fabs(x);
    return x / (1.0f + absx);
}

float SoftLimit(float x)
{
    const float absx = std::fabs(x);
    if (absx <= 0.95f)
    {
        return x;
    }
    const float sign = x < 0.0f ? -1.0f : 1.0f;
    return sign * (0.95f + 0.05f * std::tanh((absx - 0.95f) * 20.0f));
}

float MapExpo(float value, float minVal, float maxVal)
{
    value = Clamp01(value);
    return minVal * std::pow(maxVal / minVal, value);
}

float MapPitch(float value, float minMidi, float maxMidi)
{
    const float note = Lerp(minMidi, maxMidi, Clamp01(value));
    return 440.0f * std::pow(2.0f, (note - 69.0f) / 12.0f);
}

float HotEnd(float value)
{
    value = Clamp01(value);
    return value * value * (3.0f - 2.0f * value);
}

float OnePoleProcess(float x, float cutoffHz, float sampleRate, float &state)
{
    const float alpha = std::clamp(cutoffHz / (cutoffHz + sampleRate), 0.0f, 1.0f);
    state += (x - state) * alpha;
    return state;
}

float Allpass(float x, float a, float &x1, float &y1)
{
    const float y = -a * x + x1 + a * y1;
    x1 = x;
    y1 = y;
    return y;
}

float ShortestPhaseDelta(float from, float to)
{
    float delta = to - from;
    while (delta > kPi)
        delta -= kTwoPi;
    while (delta < -kPi)
        delta += kTwoPi;
    return delta;
}

struct VowelAnchor
{
    float x;
    float y;
    float f1;
    float f2;
    float f3;
    float g1;
    float g2;
    float g3;
};

constexpr VowelAnchor kVowels[] = {
    {0.12f, 0.18f, 800.0f, 1150.0f, 2900.0f, 1.00f, 0.86f, 0.50f}, // A
    {0.76f, 0.34f, 500.0f, 1900.0f, 2550.0f, 0.85f, 1.00f, 0.48f}, // E
    {0.92f, 0.88f, 280.0f, 2250.0f, 3000.0f, 0.70f, 1.00f, 0.60f}, // I
    {0.24f, 0.72f, 560.0f, 880.0f, 2400.0f, 1.00f, 0.78f, 0.46f},  // O
    {0.48f, 0.94f, 320.0f, 820.0f, 2200.0f, 0.78f, 0.82f, 0.56f},  // U
};

void BlendVowel(float x, float y, float *freqs, float *gains)
{
    x = Clamp01(x);
    y = Clamp01(y);

    float sum = 0.0f;
    freqs[0] = freqs[1] = freqs[2] = 0.0f;
    gains[0] = gains[1] = gains[2] = 0.0f;
    for (const VowelAnchor &v : kVowels)
    {
        const float dx = x - v.x;
        const float dy = y - v.y;
        const float dist2 = dx * dx + dy * dy;
        const float w = 1.0f / ((dist2 + 0.018f) * (dist2 + 0.018f));
        sum += w;
        freqs[0] += v.f1 * w;
        freqs[1] += v.f2 * w;
        freqs[2] += v.f3 * w;
        gains[0] += v.g1 * w;
        gains[1] += v.g2 * w;
        gains[2] += v.g3 * w;
    }

    const float inv = (sum > 0.0f) ? (1.0f / sum) : 1.0f;
    for (int i = 0; i < 3; ++i)
    {
        freqs[i] *= inv;
        gains[i] *= inv;
    }
}

struct SimpleDelay
{
    static constexpr size_t kMax = 8192;
    float buffer[kMax];
    size_t write;

    void Reset()
    {
        std::fill(&buffer[0], &buffer[kMax], 0.0f);
        write = 0;
    }

    float Read(float delaySamples) const
    {
        const float d = std::clamp(delaySamples, 1.0f, static_cast<float>(kMax - 2));
        float read = static_cast<float>(write) - d;
        while (read < 0.0f)
            read += static_cast<float>(kMax);
        const size_t i0 = static_cast<size_t>(read);
        const size_t i1 = (i0 + 1) % kMax;
        const float frac = read - static_cast<float>(i0);
        return buffer[i0] + (buffer[i1] - buffer[i0]) * frac;
    }

    void Write(float v)
    {
        buffer[write] = v;
        write = (write + 1) % kMax;
    }
};

struct SpectralFft
{
    float cosTable[kFftSize / 2];
    float sinTable[kFftSize / 2];
    uint16_t bitRev[kFftSize];

    void Init()
    {
        for (size_t i = 0; i < kFftSize / 2; ++i)
        {
            const float phase = kTwoPi * static_cast<float>(i) / static_cast<float>(kFftSize);
            cosTable[i] = std::cos(phase);
            sinTable[i] = std::sin(phase);
        }

        size_t bits = 0;
        for (size_t n = kFftSize; n > 1; n >>= 1)
            ++bits;

        for (size_t i = 0; i < kFftSize; ++i)
        {
            size_t x = i;
            size_t y = 0;
            for (size_t b = 0; b < bits; ++b)
            {
                y = (y << 1) | (x & 1u);
                x >>= 1;
            }
            bitRev[i] = static_cast<uint16_t>(y);
        }
    }

    void Execute(float *re, float *im, bool inverse)
    {
        for (size_t i = 0; i < kFftSize; ++i)
        {
            const size_t j = bitRev[i];
            if (j > i)
            {
                std::swap(re[i], re[j]);
                std::swap(im[i], im[j]);
            }
        }

        for (size_t size = 2; size <= kFftSize; size <<= 1)
        {
            const size_t half = size >> 1;
            const size_t step = kFftSize / size;
            for (size_t start = 0; start < kFftSize; start += size)
            {
                for (size_t k = 0; k < half; ++k)
                {
                    const size_t idx = k * step;
                    const float cosVal = cosTable[idx];
                    const float sinVal = inverse ? sinTable[idx] : -sinTable[idx];
                    const size_t even = start + k;
                    const size_t odd = even + half;
                    const float tre = cosVal * re[odd] - sinVal * im[odd];
                    const float tim = sinVal * re[odd] + cosVal * im[odd];
                    const float ure = re[even];
                    const float uim = im[even];
                    re[even] = ure + tre;
                    im[even] = uim + tim;
                    re[odd] = ure - tre;
                    im[odd] = uim - tim;
                }
            }
        }

        if (!inverse)
        {
            const float scale = 1.0f / static_cast<float>(kFftSize);
            for (size_t i = 0; i < kFftSize; ++i)
            {
                re[i] *= scale;
                im[i] *= scale;
            }
        }
    }
};

struct SpectralStereo
{
    float input[2][kFftSize];
    float fftRe[2][kFftSize];
    float fftIm[2][kFftSize];
    float re[2][kBins];
    float im[2][kBins];
    float window[kFftSize];
    float overlapInv[kHopSize];
    float output[2][4096];
    size_t inputWrite;
    size_t hopCounter;
    size_t outRead;
    size_t outWrite;
    bool primed;
    SpectralFft fft;

    void Init()
    {
        fft.Init();
        for (size_t i = 0; i < kFftSize; ++i)
        {
            const float phase = static_cast<float>(i) / static_cast<float>(kFftSize);
            window[i] = 0.5f - 0.5f * std::cos(kTwoPi * phase);
        }
        const size_t overlap = kFftSize / kHopSize;
        for (size_t i = 0; i < kHopSize; ++i)
        {
            float sum = 0.0f;
            for (size_t m = 0; m < overlap; ++m)
            {
                const size_t idx = i + m * kHopSize;
                sum += window[idx] * window[idx];
            }
            overlapInv[i] = (sum > 1.0e-9f) ? (1.0f / sum) : 1.0f;
        }
        Reset();
    }

    void Reset()
    {
        std::fill(&input[0][0], &input[0][kFftSize], 0.0f);
        std::fill(&input[1][0], &input[1][kFftSize], 0.0f);
        std::fill(&output[0][0], &output[0][4096], 0.0f);
        std::fill(&output[1][0], &output[1][4096], 0.0f);
        inputWrite = 0;
        hopCounter = 0;
        outRead = 0;
        outWrite = 0;
        primed = false;
    }

    void ProcessSample(float inL, float inR, float &outL, float &outR)
    {
        input[0][inputWrite] = inL;
        input[1][inputWrite] = inR;
        inputWrite = (inputWrite + 1) % kFftSize;

        outL = 0.0f;
        outR = 0.0f;
        if (primed)
        {
            outL = output[0][outRead];
            outR = output[1][outRead];
            output[0][outRead] = 0.0f;
            output[1][outRead] = 0.0f;
            outRead = (outRead + 1) % 4096;
        }

        hopCounter++;
        if (hopCounter >= kHopSize)
        {
            hopCounter = 0;
        }
    }

    bool ReadyForFrame() const { return hopCounter == 0; }

    void BuildSpectrum()
    {
        size_t src = inputWrite;
        for (int ch = 0; ch < 2; ++ch)
        {
            size_t idx = src;
            for (size_t i = 0; i < kFftSize; ++i)
            {
                fftRe[ch][i] = window[i] * input[ch][idx];
                fftIm[ch][i] = 0.0f;
                idx = (idx + 1) % kFftSize;
            }
            fft.Execute(fftRe[ch], fftIm[ch], false);
            re[ch][0] = fftRe[ch][0];
            im[ch][0] = 0.0f;
            re[ch][kBins - 1] = fftRe[ch][kFftSize / 2];
            im[ch][kBins - 1] = 0.0f;
            for (size_t k = 1; k < kBins - 1; ++k)
            {
                re[ch][k] = fftRe[ch][k];
                im[ch][k] = fftIm[ch][k];
            }
        }
    }

    void InverseToOutput()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            fftRe[ch][0] = re[ch][0];
            fftIm[ch][0] = 0.0f;
            fftRe[ch][kFftSize / 2] = re[ch][kBins - 1];
            fftIm[ch][kFftSize / 2] = 0.0f;
            for (size_t k = 1; k < kBins - 1; ++k)
            {
                fftRe[ch][k] = re[ch][k];
                fftIm[ch][k] = im[ch][k];
                const size_t mirror = kFftSize - k;
                fftRe[ch][mirror] = re[ch][k];
                fftIm[ch][mirror] = -im[ch][k];
            }

            fft.Execute(fftRe[ch], fftIm[ch], true);

            const size_t frameStart = outWrite;
            size_t dst = frameStart;
            for (size_t i = 0; i < kFftSize; ++i)
            {
                const float norm = overlapInv[(frameStart + i) % kHopSize];
                output[ch][dst] += fftRe[ch][i] * window[i] * norm * 0.9f;
                dst = (dst + 1) % 4096;
            }
        }

        outWrite = (outWrite + kHopSize) % 4096;
        if (!primed)
        {
            outRead = outWrite;
            primed = true;
        }
    }
};

SpectralStereo s_spectral;
SimpleDelay s_delayA;
SimpleDelay s_delayB;

#if NEUROTIC_ENABLE_EUDELAY
constexpr size_t kEuDelayMax = 4096;
alignas(4) float s_eudelayBufferL[kEuDelayMax];
alignas(4) float s_eudelayBufferR[kEuDelayMax];
#endif
}

class AlgoNcr
{
public:
    void Init(float sampleRate)
    {
        sampleRate_ = sampleRate;
        for (int i = 0; i < 2; ++i)
        {
            svfL_[i].Init(sampleRate_);
            svfR_[i].Init(sampleRate_);
        }
    }

    void Reset()
    {
        for (int i = 0; i < 2; ++i)
        {
            svfL_[i].Init(sampleRate_);
            svfR_[i].Init(sampleRate_);
        }
    }

    void Process(float inL, float inR, const NeuroticRuntime &rt, float &outL, float &outR)
    {
        const float tension = rt.c1;
        const float mass = rt.c2;
        const float damping = rt.c3;
        const float asym = rt.c4;
        const float resonance = rt.c5;

        const float base = MapPitch(tension, 36.0f, 95.0f);
        const float massHot = HotEnd(mass);
        const float resHot = HotEnd(resonance);
        const float spread = 0.75f + mass * 2.6f + massHot * 2.2f;
        const float q = 0.7f + resHot * 16.0f;

        float sumL = 0.0f;
        float sumR = 0.0f;
        for (int i = 0; i < 2; ++i)
        {
            const float ratio = (i == 0) ? (0.62f + massHot * 0.18f) : (1.45f + massHot * 0.95f);
            const float freqL = base * ratio;
            const float freqR = base * ratio * (1.0f + asym * (0.18f + massHot * 0.28f));
            svfL_[i].SetFreq(std::clamp(freqL * spread, 20.0f, sampleRate_ * 0.42f));
            svfR_[i].SetFreq(std::clamp(freqR * spread, 20.0f, sampleRate_ * 0.42f));
            svfL_[i].SetRes(q);
            svfR_[i].SetRes(q);
            svfL_[i].Process(inL);
            svfR_[i].Process(inR);
            sumL += svfL_[i].Band() * (1.0f / (i + 1));
            sumR += svfR_[i].Band() * (1.0f / (i + 1));
        }

        const float dampCut = MapExpo(1.0f - damping, 120.0f, 6000.0f);
        sumL = OnePoleProcess(sumL, dampCut, sampleRate_, lpStateL_);
        sumR = OnePoleProcess(sumR, dampCut, sampleRate_, lpStateR_);

        outL = SoftLimit(sumL * (2.35f + resHot * 0.90f));
        outR = SoftLimit(sumR * (2.35f + resHot * 0.90f));
    }

private:
    float sampleRate_ = 48000.0f;
    daisysp::Svf svfL_[2];
    daisysp::Svf svfR_[2];
    float lpStateL_ = 0.0f;
    float lpStateR_ = 0.0f;
};

class AlgoLsb
{
public:
    void Init(float sampleRate)
    {
        (void)sampleRate;
    }

    void Reset() { }

    void Process(float inL, float inR, const NeuroticRuntime &rt, float &outL, float &outR)
    {
        s_spectral.ProcessSample(inL, inR, outL, outR);
        if (!s_spectral.ReadyForFrame())
            return;

        s_spectral.BuildSpectrum();

        const float depth = std::clamp(rt.c1 * (1.0f + HotEnd(rt.c1) * 0.75f), 0.0f, 1.0f);
        const float formant = Clamp01(rt.c2);
        const float transient = Clamp01(rt.c3);
        const float weave = std::clamp(rt.c4 * (1.0f + HotEnd(rt.c4) * 0.65f), 0.0f, 1.0f);

        const float protect = Lerp(0.04f, 1.0f, transient);
        const size_t formantBin = static_cast<size_t>(formant * formant * (kBins - 1));

        for (size_t k = 0; k < kBins; ++k)
        {
            const float reL = s_spectral.re[0][k];
            const float imL = s_spectral.im[0][k];
            const float reR = s_spectral.re[1][k];
            const float imR = s_spectral.im[1][k];

            const float magL = std::sqrt(reL * reL + imL * imL);
            const float magR = std::sqrt(reR * reR + imR * imR);
            const float phaseL = std::atan2(imL, reL);
            const float phaseR = std::atan2(imR, reR);

            const float mixBase = (k < formantBin) ? std::max(weave, depth * 0.55f) : depth;
            const float hi = static_cast<float>(k) / static_cast<float>(kBins - 1);
            const float mix = std::clamp(mixBase * (1.10f + 0.35f * hi), 0.0f, 1.0f);
            const float spectralBite = 1.0f + mix * (0.18f + 0.42f * std::sin(static_cast<float>(k) * 0.071f));
            const float magLNew = Lerp(magL, magR, mix) * spectralBite;
            const float magRNew = Lerp(magR, magL, mix) * spectralBite;
            const float phaseShift = mix * protect * 7.5f;
            const float phaseLNew = phaseL + ShortestPhaseDelta(phaseL, phaseR) * phaseShift;
            const float phaseRNew = phaseR + ShortestPhaseDelta(phaseR, phaseL) * phaseShift;

            const float targetReL = magLNew * std::cos(phaseLNew);
            const float targetImL = magLNew * std::sin(phaseLNew);
            const float targetReR = magRNew * std::cos(phaseRNew);
            const float targetImR = magRNew * std::sin(phaseRNew);

            s_spectral.re[0][k] = Lerp(reL, targetReL, mix);
            s_spectral.im[0][k] = Lerp(imL, targetImL, mix);
            s_spectral.re[1][k] = Lerp(reR, targetReR, mix);
            s_spectral.im[1][k] = Lerp(imR, targetImR, mix);
        }

        s_spectral.InverseToOutput();
    }

private:
    int unused_ = 0;
};

class AlgoNth
{
public:
    void Init(float sampleRate)
    {
        sampleRate_ = sampleRate;
    }

    void Reset()
    {
        lpStateL_ = 0.0f;
        lpStateR_ = 0.0f;
        phase_ = 0.0f;
    }

    void Process(float inL, float inR, const NeuroticRuntime &rt, float &outL, float &outR)
    {
        const float drive = rt.c1;
        const float flow = rt.c2 + rt.lfoValue * rt.lfoDepth * 0.55f;
        const float headGap = rt.c3;
        const float fb = std::clamp(rt.c4 * 1.35f, 0.0f, 0.985f);

        const float flowClamped = std::clamp(flow, 0.0f, 1.0f);
        const float flowHot = HotEnd(flowClamped);
        const float baseDelay = MapExpo(0.04f + flowClamped * 0.96f, 55.0f, 3100.0f);
        phase_ += (0.07f + flowClamped * 1.6f + flowHot * 3.2f) / sampleRate_;
        if (phase_ > 1.0f)
            phase_ -= 1.0f;
        const float mod = std::sin(phase_ * kTwoPi) * (25.0f + flowClamped * 170.0f + flowHot * 260.0f);
        const float delaySamp = baseDelay + mod;

        const float satL = SoftClip(inL * (1.0f + drive * 4.0f + HotEnd(drive) * 5.0f));
        const float satR = SoftClip(inR * (1.0f + drive * 4.0f + HotEnd(drive) * 5.0f));

        const float dl = s_delayA.Read(delaySamp);
        const float dr = s_delayB.Read(delaySamp * 0.97f);
        const float gapCut = MapExpo(1.0f - HotEnd(headGap), 55.0f, 14000.0f);
        const float fbL = OnePoleProcess(dl, gapCut, sampleRate_, lpStateL_);
        const float fbR = OnePoleProcess(dr, gapCut, sampleRate_, lpStateR_);

        s_delayA.Write(satL + fbL * fb);
        s_delayB.Write(satR + fbR * fb);

        outL = SoftLimit(dl * (0.65f + drive * 0.25f));
        outR = SoftLimit(dr * (0.65f + drive * 0.25f));
    }

private:
    float sampleRate_ = 48000.0f;
    float lpStateL_ = 0.0f;
    float lpStateR_ = 0.0f;
    float phase_ = 0.0f;
};

class AlgoBgm
{
public:
    void Init(float sampleRate)
    {
        sampleRate_ = sampleRate;
    }

    void Reset()
    {
        phase_ = 0.0f;
        lpState_ = 0.0f;
    }

    void Process(float inL, float inR, const NeuroticRuntime &rt, float &outL, float &outR)
    {
        const float az = rt.c1 * 2.0f - 1.0f;
        const float elev = rt.c2;
        const float dist = rt.c3;
        const float spinAmount = rt.c4;
        const float spin = rt.lfoValue * rt.lfoDepth * (0.4f + spinAmount * 2.4f);

        phase_ += (0.05f + rt.lfoRate * 2.2f) / sampleRate_;
        if (phase_ > 1.0f)
            phase_ -= 1.0f;
        const float spinPan = std::sin(phase_ * kTwoPi) * spin * 2.2f;

        const float pan = std::clamp(az + spinPan, -1.0f, 1.0f);
        const float itd = std::fabs(pan) * (18.0f + dist * 120.0f + HotEnd(dist) * 150.0f);
        const float leftGain = std::sqrt(0.5f * (1.0f - pan));
        const float rightGain = std::sqrt(0.5f * (1.0f + pan));

        s_delayA.Write(inL);
        s_delayB.Write(inR);

        float l = inL;
        float r = inR;
        if (pan > 0.0f)
        {
            l = s_delayA.Read(1.0f + itd);
        }
        else if (pan < 0.0f)
        {
            r = s_delayB.Read(1.0f + itd);
        }

        const float cutoff = MapExpo(1.0f - HotEnd(dist), 80.0f, 16000.0f);
        const float mono = 0.5f * (l + r);
        const float distant = OnePoleProcess(mono, cutoff, sampleRate_, lpState_);

        outL = SoftLimit((distant + (l - distant) * elev) * leftGain * 2.6f);
        outR = SoftLimit((distant + (r - distant) * elev) * rightGain * 2.6f);
    }

private:
    float sampleRate_ = 48000.0f;
    float phase_ = 0.0f;
    float lpState_ = 0.0f;
};

class AlgoNff
{
public:
    void Init(float sampleRate)
    {
        sampleRate_ = sampleRate;
        for (int i = 0; i < 3; ++i)
        {
            formL_[i].Init(sampleRate_);
            formR_[i].Init(sampleRate_);
            formL_[i].SetRes(2.0f);
            formR_[i].SetRes(2.0f);
        }
        airStateL_ = 0.0f;
        airStateR_ = 0.0f;
        controlCounter_ = 0;
    }

    void Reset()
    {
        for (int i = 0; i < 3; ++i)
        {
            formL_[i].Init(sampleRate_);
            formR_[i].Init(sampleRate_);
        }
        airStateL_ = 0.0f;
        airStateR_ = 0.0f;
        controlCounter_ = 0;
        noiseState_ = 0xA5366B4Du;
    }

    void Process(float inL, float inR, const NeuroticRuntime &rt, float &outL, float &outR)
    {
        if (controlCounter_ <= 0)
        {
            UpdateControls(rt);
            controlCounter_ = kControlInterval;
        }
        controlCounter_--;

        const float noise = NextNoise() * noiseScale_;
        const float nL = SoftClip((inL + noise) * exciteGain_);
        const float nR = SoftClip((inR + noise) * exciteGain_);

        float sumL = 0.0f;
        float sumR = 0.0f;
        for (int i = 0; i < 3; ++i)
        {
            formL_[i].Process(nL);
            formR_[i].Process(nR);
            sumL += formL_[i].Band() * gains_[i];
            sumR += formR_[i].Band() * gains_[i];
        }

        const float lpL = OnePoleProcess(inL, airCut_, sampleRate_, airStateL_);
        const float lpR = OnePoleProcess(inR, airCut_, sampleRate_, airStateR_);
        const float airL = inL - lpL;
        const float airR = inR - lpR;

        outL = SoftLimit(sumL * 0.95f + airL * breathAirGain_);
        outR = SoftLimit(sumR * 0.95f + airR * breathAirGain_);
    }

private:
    static constexpr int kControlInterval = 16;

    float NextNoise()
    {
        uint32_t x = noiseState_;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        noiseState_ = x;
        const float unit = static_cast<float>((x >> 8) & 0x00FFFFFFu) / 16777215.0f;
        return unit - 0.5f;
    }

    void UpdateControls(const NeuroticRuntime &rt)
    {
        float freqs[3];
        BlendVowel(rt.c1, rt.c2, freqs, gains_);

        const float res = 1.0f + HotEnd(rt.c5) * 10.0f;
        const float breath = Clamp01(rt.c4);
        const float split = (breath - 0.5f) * 0.10f;
        for (int i = 0; i < 3; ++i)
        {
            formL_[i].SetRes(res);
            formR_[i].SetRes(res);
            formL_[i].SetFreq(std::clamp(freqs[i] * (1.0f - split), 40.0f, sampleRate_ * 0.42f));
            formR_[i].SetFreq(std::clamp(freqs[i] * (1.0f + split), 40.0f, sampleRate_ * 0.42f));
        }

        const float artic = Clamp01(rt.c3);
        noiseScale_ = artic * 0.20f;
        exciteGain_ = 1.0f + artic * 1.4f;
        airCut_ = MapExpo(breath, 500.0f, 12000.0f);
        breathAirGain_ = breath * 0.9f;
    }

    float sampleRate_ = 48000.0f;
    daisysp::Svf formL_[3];
    daisysp::Svf formR_[3];
    float airStateL_ = 0.0f;
    float airStateR_ = 0.0f;
    float gains_[3] = {1.0f, 0.8f, 0.5f};
    float airCut_ = 500.0f;
    float breathAirGain_ = 0.0f;
    float noiseScale_ = 0.0f;
    float exciteGain_ = 1.0f;
    int controlCounter_ = 0;
    uint32_t noiseState_ = 0xA5366B4Du;
};

class AlgoNdm
{
public:
    void Init(float sampleRate)
    {
        sampleRate_ = sampleRate;
    }

    void Reset()
    {
        phase_ = 0.0f;
    }

    void Process(float inL, float inR, const NeuroticRuntime &rt, float &outL, float &outR)
    {
        const float spread = rt.c1;
        const float color = rt.c2;
        const float gran = rt.c3;
        const float drift = rt.c4 + rt.lfoValue * rt.lfoDepth * 0.65f;

        const float spreadHot = HotEnd(spread);
        const float driftClamped = std::clamp(drift, 0.0f, 1.0f);
        phase_ += (0.06f + driftClamped * 1.4f + HotEnd(driftClamped) * 2.4f) / sampleRate_;
        if (phase_ > 1.0f)
            phase_ -= 1.0f;
        const float mod = std::sin(phase_ * kTwoPi) * (10.0f + spread * 60.0f + spreadHot * 180.0f);

        const float delayA = 24.0f + spread * 260.0f + spreadHot * 520.0f + mod;
        const float delayB = 54.0f + spread * 330.0f + spreadHot * 680.0f - mod;

        const float dl = s_delayA.Read(delayA);
        const float dr = s_delayB.Read(delayB);
        const float fb = std::clamp(0.22f + gran * 0.55f + HotEnd(gran) * 0.22f, 0.0f, 0.94f);
        s_delayA.Write(SoftClip(inL + dl * fb));
        s_delayB.Write(SoftClip(inR + dr * fb));

        const float tilt = (color - 0.5f) * (0.9f + HotEnd(std::fabs(color - 0.5f) * 2.0f) * 0.8f);
        outL = SoftLimit((dl + inL * tilt) * (1.05f + spreadHot * 0.35f));
        outR = SoftLimit((dr - inR * tilt) * (1.05f + spreadHot * 0.35f));
    }

private:
    float sampleRate_ = 48000.0f;
    float phase_ = 0.0f;
};

class AlgoNhc
{
public:
    void Init(float sampleRate)
    {
        (void)sampleRate;
    }

    void Reset() { }

    void Process(float inL, float inR, const NeuroticRuntime &rt, float &outL, float &outR)
    {
        s_spectral.ProcessSample(inL, inR, outL, outR);
        if (!s_spectral.ReadyForFrame())
            return;

        s_spectral.BuildSpectrum();

        const float stretch = rt.c1;
        const float inh = rt.c2;
        const float sparsity = rt.c3;
        const float mirror = rt.c4;

        const float stretchHot = HotEnd(stretch);
        const float scale = 0.42f + stretch * 1.75f + stretchHot * 1.25f;
        const float inharm = inh * 0.22f + HotEnd(inh) * 0.48f;

        for (size_t k = 0; k < kBins; ++k)
        {
            const float src = static_cast<float>(k) * (scale + std::sin(k * 0.01f) * inharm);
            if (src >= static_cast<float>(kBins - 1))
            {
                s_spectral.re[0][k] = 0.0f;
                s_spectral.im[0][k] = 0.0f;
                s_spectral.re[1][k] = 0.0f;
                s_spectral.im[1][k] = 0.0f;
                continue;
            }
            const size_t i0 = static_cast<size_t>(src);
            const size_t i1 = i0 + 1;
            const float frac = src - static_cast<float>(i0);

            const float reL = s_spectral.re[0][i0] + (s_spectral.re[0][i1] - s_spectral.re[0][i0]) * frac;
            const float imL = s_spectral.im[0][i0] + (s_spectral.im[0][i1] - s_spectral.im[0][i0]) * frac;
            const float reR = s_spectral.re[1][i0] + (s_spectral.re[1][i1] - s_spectral.re[1][i0]) * frac;
            const float imR = s_spectral.im[1][i0] + (s_spectral.im[1][i1] - s_spectral.im[1][i0]) * frac;

            const int period = 2 + static_cast<int>(sparsity * 30.0f);
            const int width = std::max(1, period / (5 + static_cast<int>(HotEnd(sparsity) * 7.0f)));
            const int slot = static_cast<int>(k) % period;
            const float gate = (slot < width) ? 1.0f : Lerp(0.32f, 0.08f, HotEnd(sparsity));
            s_spectral.re[0][k] = reL * gate;
            s_spectral.im[0][k] = imL * gate;
            s_spectral.re[1][k] = reR * gate;
            s_spectral.im[1][k] = imR * gate;
        }

        if (mirror > 0.0f)
        {
            const size_t mid = kBins / 2;
            for (size_t k = mid; k < kBins; ++k)
            {
                const size_t mirrorBin = kBins - 1 - k;
                const float fold = mirror * 0.75f + HotEnd(mirror) * 0.65f;
                s_spectral.re[0][mirrorBin] += s_spectral.re[0][k] * fold;
                s_spectral.im[0][mirrorBin] -= s_spectral.im[0][k] * fold;
                s_spectral.re[1][mirrorBin] += s_spectral.re[1][k] * fold;
                s_spectral.im[1][mirrorBin] -= s_spectral.im[1][k] * fold;
            }
        }

        const float gain = 4.6f + stretch * 2.4f + stretchHot * 2.4f;
        for (size_t k = 0; k < kBins; ++k)
        {
            s_spectral.re[0][k] *= gain;
            s_spectral.im[0][k] *= gain;
            s_spectral.re[1][k] *= gain;
            s_spectral.im[1][k] *= gain;
        }

        s_spectral.InverseToOutput();
    }

private:
    int unused_ = 0;
};

class AlgoNpl
{
public:
    void Init(float sampleRate)
    {
        (void)sampleRate;
    }

    void Reset() { }

    void Process(float inL, float inR, const NeuroticRuntime &rt, float &outL, float &outR)
    {
        s_spectral.ProcessSample(inL, inR, outL, outR);
        if (!s_spectral.ReadyForFrame())
            return;

        s_spectral.BuildSpectrum();

        const float bind = std::clamp(rt.c1 * 2.2f + HotEnd(rt.c1) * 1.4f, 0.0f, 1.0f);
        const float swirl = std::clamp(rt.c2 * 2.6f + HotEnd(rt.c2) * 2.2f + rt.lfoValue * rt.lfoDepth * 2.4f, 0.0f, 1.0f);
        const float tilt = std::clamp(rt.c3 * 2.5f + HotEnd(rt.c3) * 2.0f, 0.0f, 1.0f);
        const float stereo = rt.c4;

        for (size_t k = 1; k < kBins - 1; ++k)
        {
            const float reL = s_spectral.re[0][k];
            const float imL = s_spectral.im[0][k];
            const float reR = s_spectral.re[1][k];
            const float imR = s_spectral.im[1][k];

            const float magL = std::sqrt(reL * reL + imL * imL);
            const float magR = std::sqrt(reR * reR + imR * imR);
            const float phaseL = std::atan2(imL, reL);
            const float phaseR = std::atan2(imR, reR);

            const float norm = static_cast<float>(k) / static_cast<float>(kBins);
            const float warp = norm * norm * tilt * 7.0f;
            const float swirlPhase = std::sin(static_cast<float>(k) * (0.016f + 0.024f * swirl)) * swirl * 13.0f;

            const float phaseLNew = phaseL + swirlPhase + warp;
            const float phaseRNew = phaseR - swirlPhase - warp;

            const float linkL = phaseLNew + ShortestPhaseDelta(phaseLNew, phaseRNew) * bind;
            const float linkR = phaseRNew + ShortestPhaseDelta(phaseRNew, phaseLNew) * bind;

            s_spectral.re[0][k] = magL * std::cos(linkL);
            s_spectral.im[0][k] = magL * std::sin(linkL);
            s_spectral.re[1][k] = magR * std::cos(linkR);
            s_spectral.im[1][k] = magR * std::sin(linkR);
        }

        const float widen = 0.35f + stereo * 2.45f;
        for (size_t k = 1; k < kBins - 1; ++k)
        {
            const float midRe = 0.5f * (s_spectral.re[0][k] + s_spectral.re[1][k]);
            const float sideRe = 0.5f * (s_spectral.re[0][k] - s_spectral.re[1][k]);
            const float midIm = 0.5f * (s_spectral.im[0][k] + s_spectral.im[1][k]);
            const float sideIm = 0.5f * (s_spectral.im[0][k] - s_spectral.im[1][k]);
            s_spectral.re[0][k] = midRe + sideRe * widen;
            s_spectral.re[1][k] = midRe - sideRe * widen;
            s_spectral.im[0][k] = midIm + sideIm * widen;
            s_spectral.im[1][k] = midIm - sideIm * widen;
        }

        s_spectral.InverseToOutput();
    }

private:
    int unused_ = 0;
};

class AlgoNsm
{
public:
    void Init(float sampleRate) { sampleRate_ = sampleRate; }

    void Reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int i = 0; i < kMaxPoles; ++i)
            {
                x1_[ch][i] = 0.0f;
                y1_[ch][i] = 0.0f;
            }
            fbState_[ch] = 0.0f;
        }
    }

    void Process(float inL, float inR, const NeuroticRuntime &rt, float &outL, float &outR)
    {
        const float lfo = rt.lfoValue * rt.lfoDepth * 0.35f;
        const float freqControl = std::clamp(rt.c1 + lfo, 0.0f, 1.0f);
        const float freqHz = MapPitch(freqControl, 28.0f, 122.0f);
        const float g = std::tan(kPi * freqHz / sampleRate_);
        float a = (1.0f - g) / (1.0f + g);
        const float res = 0.2f + rt.c2 * 0.78f;
        a = std::clamp(a * res, -0.98f, 0.98f);

        const int poles = 2 + static_cast<int>(rt.c3 * 126.0f + 0.5f);
        const float fb = std::clamp(rt.c4, 0.0f, 0.95f);

        float in[2] = {inL + fbState_[0] * fb, inR + fbState_[1] * fb};
        float out[2] = {0.0f, 0.0f};

        for (int ch = 0; ch < 2; ++ch)
        {
            float x = in[ch];
            for (int i = 0; i < poles; ++i)
            {
                x = Allpass(x, a, x1_[ch][i], y1_[ch][i]);
            }
            out[ch] = x;
            fbState_[ch] = x;
        }

        outL = SoftClip(out[0]);
        outR = SoftClip(out[1]);
    }

private:
    static constexpr int kMaxPoles = 128;
    float sampleRate_ = 48000.0f;
    float x1_[2][kMaxPoles];
    float y1_[2][kMaxPoles];
    float fbState_[2];
};

class AlgoNps
{
public:
    void Init(float sampleRate)
    {
        sampleRate_ = sampleRate;
        Reset();
    }

    void Reset()
    {
        phase_ = 0.0f;
    }

    void Process(float inL, float inR, const NeuroticRuntime &rt, float &outL, float &outR)
    {
        const float baseOffset = (rt.c1 * 2.0f - 1.0f) * 12.0f;
        const float scale = 0.25f + rt.c2 * 1.75f;
        const float semis = baseOffset * scale;
        const float ratio = std::pow(2.0f, semis / 12.0f);
        const float stereo = (rt.c4 - 0.5f) * 0.15f;

        const float windowSec = 0.02f + rt.c3 * 0.10f;
        const float windowSamples = windowSec * sampleRate_;
        const float inc = (1.0f - ratio) / windowSamples;

        phase_ += inc;
        if (phase_ >= 1.0f)
            phase_ -= 1.0f;
        if (phase_ < 0.0f)
            phase_ += 1.0f;

        const float phaseB = phase_ + 0.5f;
        const float phaseBWrapped = (phaseB >= 1.0f) ? (phaseB - 1.0f) : phaseB;

        const float winA = 1.0f - std::fabs(phase_ * 2.0f - 1.0f);
        const float winB = 1.0f - std::fabs(phaseBWrapped * 2.0f - 1.0f);
        const float winSum = std::max(0.001f, winA + winB);

        s_delayA.Write(inL);
        s_delayB.Write(inR);

        const float delayA = windowSamples * phase_;
        const float delayB = windowSamples * phaseBWrapped;
        const float delayAR = windowSamples * std::clamp(phase_ * (1.0f + stereo), 0.0f, 1.0f);
        const float delayBR = windowSamples * std::clamp(phaseBWrapped * (1.0f - stereo), 0.0f, 1.0f);

        const float aL = s_delayA.Read(delayA);
        const float bL = s_delayA.Read(delayB);
        const float aR = s_delayB.Read(delayAR);
        const float bR = s_delayB.Read(delayBR);

        const float outBaseL = (aL * winA + bL * winB) / winSum;
        const float outBaseR = (aR * winA + bR * winB) / winSum;

        outL = Lerp(outBaseL, inL, 0.1f);
        outR = Lerp(outBaseR, inR, 0.1f);

    }

private:
    float sampleRate_ = 48000.0f;
    float phase_ = 0.0f;
};

class AlgoNrv
{
public:
    void Init(float sampleRate)
    {
        sampleRate_ = sampleRate;
        delayL_.Reset();
        delayR_.Reset();
    }

    void Reset()
    {
        delayL_.Reset();
        delayR_.Reset();
    }

    void Process(float inL, float inR, const NeuroticRuntime &rt, float &outL, float &outR)
    {
        const float timeScale = MapExpo(rt.c1, 0.10f, 6.0f);
        const float baseDelaySamples = std::clamp(sampleRate_ * 0.16f * timeScale, 1.0f, kMaxDelaySamples_);
        const float cross = std::clamp(rt.c3 * (1.0f + HotEnd(rt.c3) * 0.35f), 0.0f, 1.0f);
        const float tilt = Clamp01(rt.c5);
        const float fb = std::clamp(rt.c2 * (1.0f + HotEnd(rt.c2) * 0.18f), 0.0f, 0.988f);
        const int taps = std::clamp(1 + static_cast<int>(rt.c4 * 9.999f), 1, 10);

        static constexpr int kPrimeTaps[10] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29};
        static constexpr float kMaxPrime = 29.0f;
        const float tiltBipolar = tilt * 2.0f - 1.0f;

        float sumL = 0.0f;
        float sumR = 0.0f;
        for (int i = 0; i < taps; ++i)
        {
            const float ratio = static_cast<float>(kPrimeTaps[i]) / kMaxPrime;
            const float delayL = baseDelaySamples * ratio;
            const float delayR = baseDelaySamples * ratio * 1.07f;
            const float tapL = delayL_.Read(delayL);
            const float tapR = delayR_.Read(delayR);
            const float tiltWeight = std::clamp(1.0f + tiltBipolar * (ratio - 0.5f) * 2.1f, 0.15f, 2.05f);
            sumL += tapL * tiltWeight;
            sumR += tapR * tiltWeight;
        }

        const float invTaps = 1.0f / static_cast<float>(taps);
        sumL *= invTaps;
        sumR *= invTaps;

        const float crossL = sumL + cross * sumR;
        const float crossR = sumR + cross * sumL;

        delayL_.Write(SoftClip(inL + crossL * fb));
        delayR_.Write(SoftClip(inR + crossR * fb));

        constexpr float kOutputMakeup = 3.2f;
        outL = SoftLimit(crossL * kOutputMakeup);
        outR = SoftLimit(crossR * kOutputMakeup);
    }

private:
    struct ReverbDelay
    {
        static constexpr size_t kMax = 32000;
        float buffer[kMax];
        size_t write;

        void Reset()
        {
            std::fill(&buffer[0], &buffer[kMax], 0.0f);
            write = 0;
        }

        float Read(float delaySamples) const
        {
            const float d = std::clamp(delaySamples, 1.0f, static_cast<float>(kMax - 2));
            float read = static_cast<float>(write) - d;
            while (read < 0.0f)
                read += static_cast<float>(kMax);
            const size_t i0 = static_cast<size_t>(read);
            const size_t i1 = (i0 + 1) % kMax;
            const float frac = read - static_cast<float>(i0);
            return buffer[i0] + (buffer[i1] - buffer[i0]) * frac;
        }

        void Write(float v)
        {
            buffer[write] = v;
            write = (write + 1) % kMax;
        }
    };

    static constexpr float kMaxDelaySamples_ = static_cast<float>(ReverbDelay::kMax - 2);
    float sampleRate_ = 48000.0f;
    ReverbDelay delayL_;
    ReverbDelay delayR_;
};

#if NEUROTIC_ENABLE_EUDELAY
class AlgoNed
{
public:
    void Init(float sampleRate)
    {
        sampleRate_ = sampleRate;
        Reset();
    }

    void Reset()
    {
        std::fill(&bufferL_[0], &bufferL_[kMaxDelay], 0.0f);
        std::fill(&bufferR_[0], &bufferR_[kMaxDelay], 0.0f);
        writeL_ = 0;
        writeR_ = 0;
        lastSteps_ = -1;
        lastTaps_ = -1;
        lastOffset_ = -1;
        for (int i = 0; i < kMaxSteps; ++i)
        {
            pattern_[i] = false;
        }
    }

    void Process(float inL, float inR, const NeuroticRuntime &rt, float &outL, float &outR)
    {
        const int steps = std::clamp(2 + static_cast<int>(rt.c3 * 14.0f + 0.5f), 2, 16);
        const int bpm = std::clamp(80 + static_cast<int>(rt.c4 * 120.0f + 0.5f), 80, 200);
        const float scale = 0.25f + std::clamp(rt.c5, 0.0f, 1.0f) * 3.75f;

        const float tapControl = std::clamp(rt.c1, 0.0f, 1.0f);
        const float offsetControl = std::clamp(rt.c2, 0.0f, 1.0f);

        accumInL_ += inL;
        accumInR_ += inR;

        if (++downsampleCounter_ >= kDownsample)
        {
            downsampleCounter_ = 0;
            const float inAvgL = accumInL_ * kDownsampleRecip_;
            const float inAvgR = accumInR_ * kDownsampleRecip_;
            accumInL_ = 0.0f;
            accumInR_ = 0.0f;

            const int taps = std::clamp(1 + static_cast<int>(tapControl * static_cast<float>(steps - 1) + 0.5f), 1, steps);
            const int offset = std::clamp(static_cast<int>(offsetControl * static_cast<float>(steps - 1) + 0.5f), 0, steps - 1);

            const float fine = 1.0f;
            const float baseDelaySec = (60.0f / static_cast<float>(bpm));
            const float desiredDelaySec = baseDelaySec * scale * fine;
            const float maxDelaySec = (static_cast<float>(kMaxDelay - 2) * static_cast<float>(kDownsample) * 0.9f) / sampleRate_;
            const float totalDelaySec = maxDelaySec * (1.0f - std::exp(-desiredDelaySec / maxDelaySec));
            const float totalDelaySamples = std::max(1.0f, totalDelaySec * sampleRate_);
            const float cappedDelay = totalDelaySamples / static_cast<float>(kDownsample);
            const float stepSamples = std::max(4.0f, cappedDelay / static_cast<float>(steps));

            if (steps != lastSteps_ || taps != lastTaps_ || offset != lastOffset_)
            {
                BuildPattern(steps, taps);
                lastSteps_ = steps;
                lastTaps_ = taps;
                lastOffset_ = offset;
            }

            float sumL = 0.0f;
            float sumR = 0.0f;
            int activeTaps = 0;

            for (int i = 0; i < steps; ++i)
            {
                if (!pattern_[i])
                    continue;
                const int idx = (i + offset) % steps;
                const float delaySamples = std::max(1.0f, stepSamples * static_cast<float>(idx + 1));
                const float tapL = Read(bufferL_, writeL_, delaySamples);
                const float tapR = Read(bufferR_, writeR_, delaySamples);
                sumL += tapL;
                sumR += tapR;
                activeTaps++;
            }

            const float tapNorm = (activeTaps > 0) ? (1.0f / std::sqrt(static_cast<float>(activeTaps))) : 0.0f;
            sumL *= tapNorm;
            sumR *= tapNorm;

            const float fb = std::clamp(rt.fb, 0.0f, 0.75f);
            const float writeL = std::clamp(inAvgL + sumL * fb, -1.0f, 1.0f);
            const float writeR = std::clamp(inAvgR + sumR * fb, -1.0f, 1.0f);
            Write(bufferL_, writeL_, writeL);
            Write(bufferR_, writeR_, writeR);

            const float outGain = 0.85f;
            lastOutL_ = SoftLimit(sumL * outGain);
            lastOutR_ = SoftLimit(sumR * outGain);
        }

        outL = lastOutL_;
        outR = lastOutR_;
    }

private:
    static constexpr int kMaxSteps = 16;
    static constexpr size_t kMaxDelay = kEuDelayMax;
    static constexpr int kDownsample = 8;
    static constexpr float kDownsampleRecip_ = 1.0f / static_cast<float>(kDownsample);

    void BuildPattern(int steps, int taps)
    {
        for (int i = 0; i < kMaxSteps; ++i)
        {
            pattern_[i] = false;
        }
        if (steps <= 0)
            return;
        for (int i = 0; i < steps; ++i)
        {
            pattern_[i] = ((i * taps) % steps) < taps;
        }
    }

    float Read(const float *buffer, size_t write, float delaySamples) const
    {
        const float d = std::clamp(delaySamples, 1.0f, static_cast<float>(kMaxDelay - 2));
        float read = static_cast<float>(write) - d;
        while (read < 0.0f)
            read += static_cast<float>(kMaxDelay);
        const size_t i0 = static_cast<size_t>(read);
        const size_t i1 = (i0 + 1) % kMaxDelay;
        const float frac = read - static_cast<float>(i0);
        return buffer[i0] + (buffer[i1] - buffer[i0]) * frac;
    }

    void Write(float *buffer, size_t &write, float v)
    {
        buffer[write] = v;
        write = (write + 1) % kMaxDelay;
    }

    float sampleRate_ = 48000.0f;
    int lastSteps_ = -1;
    int lastTaps_ = -1;
    int lastOffset_ = -1;
    int downsampleCounter_ = 0;
    float lastOutL_ = 0.0f;
    float lastOutR_ = 0.0f;
    float accumInL_ = 0.0f;
    float accumInR_ = 0.0f;
    bool pattern_[kMaxSteps];
    size_t writeL_ = 0;
    size_t writeR_ = 0;
    float *bufferL_ = s_eudelayBufferL;
    float *bufferR_ = s_eudelayBufferR;
};
#endif

class AlgoNws
{
public:
    void Init(float sampleRate)
    {
        sampleRate_ = sampleRate;
        Reset();
    }

    void Reset()
    {
        std::fill(&inBlockL_[0], &inBlockL_[kBlock], 0.0f);
        std::fill(&inBlockR_[0], &inBlockR_[kBlock], 0.0f);
        std::fill(&outBlockL_[0], &outBlockL_[kBlock], 0.0f);
        std::fill(&outBlockR_[0], &outBlockR_[kBlock], 0.0f);
        std::fill(&smoothCoeffsL_[0], &smoothCoeffsL_[kBlock], 0.0f);
        std::fill(&smoothCoeffsR_[0], &smoothCoeffsR_[kBlock], 0.0f);
        // Start non-identity so the effect is obvious immediately on selection.
        permute_[0] = 1;
        permute_[1] = 2;
        permute_[2] = 0;
        blockIndex_ = 0;
        blockCounter_ = 0;
        prng_ = 0x9E3779B9u;
    }

    void Process(float inL, float inR, const NeuroticRuntime &rt, float &outL, float &outR)
    {
        inBlockL_[blockIndex_] = inL;
        inBlockR_[blockIndex_] = inR;
        outL = outBlockL_[blockIndex_];
        outR = outBlockR_[blockIndex_];

        blockIndex_++;
        if (blockIndex_ >= kBlock)
        {
            blockIndex_ = 0;
            ProcessBlock(inBlockL_, outBlockL_, smoothCoeffsL_, rt, 0.0f);
            ProcessBlock(inBlockR_, outBlockR_, smoothCoeffsR_, rt, 0.11f + 0.33f * Clamp01(rt.c4));
        }
    }

private:
    static constexpr int kBlock = 32;
    static constexpr float kInvSqrt2 = 0.70710678118f;

    void ForwardHaar(float *x, float *tmp) const
    {
        int len = kBlock;
        for (int level = 0; level < 3; ++level)
        {
            const int half = len / 2;
            for (int i = 0; i < half; ++i)
            {
                const float a = x[2 * i];
                const float b = x[2 * i + 1];
                tmp[i] = (a + b) * kInvSqrt2;
                tmp[half + i] = (a - b) * kInvSqrt2;
            }
            for (int i = 0; i < len; ++i)
            {
                x[i] = tmp[i];
            }
            len = half;
        }
    }

    void InverseHaar(float *x, float *tmp) const
    {
        for (int len = 8; len <= kBlock; len *= 2)
        {
            const int half = len / 2;
            for (int i = 0; i < half; ++i)
            {
                const float a = x[i];
                const float d = x[half + i];
                tmp[2 * i] = (a + d) * kInvSqrt2;
                tmp[2 * i + 1] = (a - d) * kInvSqrt2;
            }
            for (int i = 0; i < len; ++i)
            {
                x[i] = tmp[i];
            }
        }
    }

    uint32_t NextRand()
    {
        uint32_t x = prng_;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        prng_ = x;
        return x;
    }

    void UpdatePermutation(float stereoOffset)
    {
        const int mode = static_cast<int>(NextRand() % 5u);
        switch (mode)
        {
        case 0:
            permute_[0] = 0;
            permute_[1] = 2;
            permute_[2] = 1;
            break;
        case 1:
            permute_[0] = 1;
            permute_[1] = 0;
            permute_[2] = 2;
            break;
        case 2:
            permute_[0] = 1;
            permute_[1] = 2;
            permute_[2] = 0;
            break;
        case 3:
            permute_[0] = 2;
            permute_[1] = 0;
            permute_[2] = 1;
            break;
        default:
            permute_[0] = 2;
            permute_[1] = 1;
            permute_[2] = 0;
            break;
        }

        const int rot = static_cast<int>(stereoOffset * 2.99f + 0.5f) % 3;
        if (rot != 0)
        {
            int p[3] = {permute_[0], permute_[1], permute_[2]};
            for (int i = 0; i < 3; ++i)
            {
                permute_[i] = p[(i + rot) % 3];
            }
        }
    }

    void ProcessBlock(const float *in, float *out, float *smooth, const NeuroticRuntime &rt, float stereoOffset)
    {
        float coeffs[kBlock];
        float tmp[kBlock];
        for (int i = 0; i < kBlock; ++i)
        {
            coeffs[i] = in[i];
        }

        ForwardHaar(coeffs, tmp);

        const float scramble = std::clamp(Clamp01(rt.c1) * (1.15f + HotEnd(rt.c1) * 0.35f), 0.0f, 1.0f);
        const float rate = Clamp01(rt.c2);
        // Bias toward smoother default behavior around center settings.
        const float smoothing = std::pow(Clamp01(rt.c3), 0.75f);
        const float spread = Clamp01(rt.c4);
        const float tilt = Clamp01(rt.c5);

        const int updateEvery = 1 + static_cast<int>((1.0f - rate) * (1.0f - rate) * 17.0f + 0.5f);
        if (++blockCounter_ >= updateEvery)
        {
            blockCounter_ = 0;
            UpdatePermutation(spread + stereoOffset);
        }

        float bands[3][kBlock];
        for (int i = 0; i < kBlock; ++i)
        {
            bands[0][i] = 0.0f;
            bands[1][i] = 0.0f;
            bands[2][i] = 0.0f;
        }
        for (int i = 0; i < 4; ++i)
        {
            bands[0][i] = coeffs[i];
        }
        for (int i = 4; i < 16; ++i)
        {
            bands[1][i] = coeffs[i];
        }
        for (int i = 16; i < 32; ++i)
        {
            bands[2][i] = coeffs[i];
        }

        const float tiltFine = 1.0f + 0.8f * tilt;
        const float tiltCoarse = 1.0f + 0.8f * (1.0f - tilt);
        auto RemapBand = [&](int srcBand, int srcLen, int srcStart, int dstPos, int dstLen) -> float {
            const int srcPos = (dstPos * srcLen) / dstLen;
            return bands[srcBand][srcStart + srcPos];
        };

        for (int i = 0; i < 4; ++i)
        {
            const float src = bands[0][i];
            float dst = src;
            if (permute_[0] == 0)
                dst = bands[0][i];
            else if (permute_[0] == 1)
                dst = RemapBand(1, 12, 4, i, 4);
            else
                dst = RemapBand(2, 16, 16, i, 4);
            coeffs[i] = Lerp(src, dst, scramble) * tiltCoarse;
        }
        for (int i = 4; i < 16; ++i)
        {
            const float src = bands[1][i];
            const int dstPos = i - 4;
            float dst = src;
            if (permute_[1] == 0)
                dst = RemapBand(0, 4, 0, dstPos, 12);
            else if (permute_[1] == 1)
                dst = bands[1][i];
            else
                dst = RemapBand(2, 16, 16, dstPos, 12);
            coeffs[i] = Lerp(src, dst, scramble);
        }
        for (int i = 16; i < 32; ++i)
        {
            const float src = bands[2][i];
            const int dstPos = i - 16;
            float dst = src;
            if (permute_[2] == 0)
                dst = RemapBand(0, 4, 0, dstPos, 16);
            else if (permute_[2] == 1)
                dst = RemapBand(1, 12, 4, dstPos, 16);
            else
                dst = bands[2][i];
            coeffs[i] = Lerp(src, dst, scramble) * tiltFine;
        }

        const float alpha = 0.93f - 0.86f * smoothing;
        for (int i = 0; i < kBlock; ++i)
        {
            smooth[i] += (coeffs[i] - smooth[i]) * alpha;
            coeffs[i] = std::clamp(smooth[i], -2.0f, 2.0f);
        }

        InverseHaar(coeffs, tmp);

        for (int i = 0; i < kBlock; ++i)
        {
            const float dryKeep = 0.12f * (1.0f - scramble);
            out[i] = SoftLimit(coeffs[i] * (1.0f + HotEnd(scramble) * 0.25f) + in[i] * dryKeep);
        }
    }

    float sampleRate_ = 48000.0f;
    int blockIndex_ = 0;
    int blockCounter_ = 0;
    uint32_t prng_ = 0x9E3779B9u;
    int permute_[3] = {0, 1, 2};
    float inBlockL_[kBlock];
    float inBlockR_[kBlock];
    float outBlockL_[kBlock];
    float outBlockR_[kBlock];
    float smoothCoeffsL_[kBlock];
    float smoothCoeffsR_[kBlock];
};

class AlgoNmb
{
public:
    void Init(float sampleRate)
    {
        sampleRate_ = sampleRate;
        for (int i = 0; i < kModes; ++i)
        {
            modeL_[i].Init(sampleRate_);
            modeR_[i].Init(sampleRate_);
        }
        Reset();
    }

    void Reset()
    {
        controlCounter_ = 0;
        for (int i = 0; i < kModes; ++i)
        {
            gain_[i] = 0.0f;
        }
    }

    void Process(float inL, float inR, const NeuroticRuntime &rt, float &outL, float &outR)
    {
        if (controlCounter_ <= 0)
        {
            UpdateControls(rt);
            controlCounter_ = kControlInterval;
        }
        controlCounter_--;

        const float input = 0.5f * (inL + inR);
        float odd = 0.0f;
        float even = 0.0f;
        for (int i = 0; i < kModes; ++i)
        {
            modeL_[i].Process(input);
            modeR_[i].Process(input);
            const float l = modeL_[i].Band() * gain_[i];
            const float r = modeR_[i].Band() * gain_[i];
            if (i & 1)
            {
                even += 0.5f * (l + r);
            }
            else
            {
                odd += 0.5f * (l + r);
            }
        }

        outL = SoftLimit(odd * 2.2f);
        outR = SoftLimit(even * 2.2f);
    }

private:
    static constexpr int kModes = 12;
    static constexpr int kControlInterval = 16;

    void UpdateControls(const NeuroticRuntime &rt)
    {
        const float root = MapPitch(rt.c1, 30.0f, 92.0f);
        const float structure = Clamp01(rt.c2);
        const float brightness = Clamp01(rt.c3);
        const float damping = Clamp01(rt.c4);
        const float position = Clamp01(rt.c5);
        const float stiffness = Lerp(-0.025f, 0.105f, HotEnd(structure));
        const float qBase = 6.0f + HotEnd(damping) * 42.0f;
        const float bright = brightness * brightness;
        float stretch = 1.0f;

        for (int i = 0; i < kModes; ++i)
        {
            const float harmonic = static_cast<float>(i + 1);
            const float freq = std::clamp(root * harmonic * stretch, 30.0f, sampleRate_ * 0.42f);
            const float hi = static_cast<float>(i) / static_cast<float>(kModes - 1);
            const float ampTilt = std::pow(1.0f - hi * 0.82f, 1.0f + (1.0f - bright) * 3.2f);
            const float pickup = 0.5f - 0.5f * std::cos(kPi * harmonic * position);
            gain_[i] = ampTilt * (0.20f + 0.80f * pickup);

            modeL_[i].SetFreq(freq);
            modeR_[i].SetFreq(std::clamp(freq * (1.0f + 0.0025f * static_cast<float>((i & 3) - 1)), 30.0f, sampleRate_ * 0.42f));
            modeL_[i].SetRes(qBase * (1.0f - hi * 0.45f));
            modeR_[i].SetRes(qBase * (1.0f - hi * 0.45f));

            stretch += stiffness;
            if (stiffness > 0.0f)
            {
                stretch += stiffness * hi * 0.25f;
            }
            else
            {
                stretch += stiffness * hi * 0.08f;
            }
        }
    }

    float sampleRate_ = 48000.0f;
    daisysp::Svf modeL_[kModes];
    daisysp::Svf modeR_[kModes];
    float gain_[kModes];
    int controlCounter_ = 0;
};

class AlgoNss
{
public:
    void Init(float sampleRate)
    {
        sampleRate_ = sampleRate;
        Reset();
    }

    void Reset()
    {
        for (int s = 0; s < kStrings; ++s)
        {
            std::fill(&string_[s][0], &string_[s][kDelay], 0.0f);
            write_[s] = 0;
            lp_[s] = 0.0f;
        }
        controlCounter_ = 0;
    }

    void Process(float inL, float inR, const NeuroticRuntime &rt, float &outL, float &outR)
    {
        if (controlCounter_ <= 0)
        {
            UpdateControls(rt);
            controlCounter_ = kControlInterval;
        }
        controlCounter_--;

        const float input = 0.5f * (inL + inR);
        float sumL = 0.0f;
        float sumR = 0.0f;
        for (int s = 0; s < kStrings; ++s)
        {
            const float delayed = Read(s, delay_[s]);
            lp_[s] += (delayed - lp_[s]) * dampCoef_[s];
            const float injected = input * excite_[s];
            const float next = SoftClip(injected + lp_[s] * feedback_[s]);
            string_[s][write_[s]] = next;
            write_[s] = (write_[s] + 1) % kDelay;

            const float voice = delayed * level_[s];
            sumL += voice * panL_[s];
            sumR += voice * panR_[s];
        }

        outL = SoftLimit(sumL * 2.4f);
        outR = SoftLimit(sumR * 2.4f);
    }

private:
    static constexpr int kStrings = 3;
    static constexpr size_t kDelay = 1024;
    static constexpr int kControlInterval = 16;

    float Read(int s, float delaySamples) const
    {
        const float d = std::clamp(delaySamples, 2.0f, static_cast<float>(kDelay - 2));
        float read = static_cast<float>(write_[s]) - d;
        while (read < 0.0f)
            read += static_cast<float>(kDelay);
        const size_t i0 = static_cast<size_t>(read);
        const size_t i1 = (i0 + 1) % kDelay;
        const float frac = read - static_cast<float>(i0);
        return string_[s][i0] + (string_[s][i1] - string_[s][i0]) * frac;
    }

    void UpdateControls(const NeuroticRuntime &rt)
    {
        static constexpr float kIntervals[3][kStrings] = {
            {0.0f, 7.01955f, 12.0f},
            {0.0f, 3.0f, 10.0f},
            {0.0f, 12.0f, 24.0f},
        };
        const float root = MapPitch(rt.c1, 30.0f, 82.0f);
        const float spread = Clamp01(rt.c2);
        const float damping = Clamp01(rt.c3);
        const float brightness = Clamp01(rt.c4);
        const float detune = (Clamp01(rt.c5) - 0.5f) * 2.0f;
        const int chord = std::clamp(static_cast<int>(spread * 2.999f), 0, 2);
        const float chordBlend = spread * 2.999f - static_cast<float>(chord);

        for (int s = 0; s < kStrings; ++s)
        {
            const int nextChord = std::min(chord + 1, 2);
            const float semi = Lerp(kIntervals[chord][s], kIntervals[nextChord][s], chordBlend);
            const float fine = detune * static_cast<float>((s * 5 + 3) % 7 - 3) * 0.018f;
            const float freq = std::clamp(root * std::pow(2.0f, semi / 12.0f) * (1.0f + fine), 48.0f, sampleRate_ * 0.20f);
            delay_[s] = sampleRate_ / freq;
            feedback_[s] = std::clamp(0.72f + damping * 0.24f - static_cast<float>(s) * 0.018f, 0.45f, 0.975f);
            dampCoef_[s] = 0.04f + brightness * brightness * 0.42f;
            excite_[s] = (s == 0) ? 0.42f : (0.16f + brightness * 0.18f);
            level_[s] = 1.0f / std::sqrt(static_cast<float>(s + 1));
            const float pan = static_cast<float>(s) / static_cast<float>(kStrings - 1);
            panL_[s] = std::sqrt(1.0f - pan);
            panR_[s] = std::sqrt(pan);
        }
    }

    float sampleRate_ = 48000.0f;
    float string_[kStrings][kDelay];
    size_t write_[kStrings];
    float lp_[kStrings];
    float delay_[kStrings];
    float feedback_[kStrings];
    float dampCoef_[kStrings];
    float excite_[kStrings];
    float level_[kStrings];
    float panL_[kStrings];
    float panR_[kStrings];
    int controlCounter_ = 0;
};

class AlgoNfm
{
public:
    void Init(float sampleRate)
    {
        sampleRate_ = sampleRate;
        Reset();
    }

    void Reset()
    {
        carrierPhase_ = 0.0f;
        modPhase_ = 0.0f;
        envL_ = 0.0f;
        envR_ = 0.0f;
        prev_ = 0.0f;
        lpL_ = 0.0f;
        lpR_ = 0.0f;
    }

    void Process(float inL, float inR, const NeuroticRuntime &rt, float &outL, float &outR)
    {
        const float freq = MapPitch(rt.c1, 30.0f, 96.0f);
        const float ratio = std::pow(2.0f, Lerp(-12.0f, 24.0f, Clamp01(rt.c2)) / 12.0f);
        const float brightness = Clamp01(rt.c3);
        const float feedback = (Clamp01(rt.c4) - 0.5f) * 2.0f;
        const float damping = Clamp01(rt.c5);
        const float inputEnv = std::min(1.0f, std::fabs(0.5f * (inL + inR)) * 4.0f);
        const float attack = 0.01f + brightness * 0.08f;
        const float release = 0.0004f + damping * damping * 0.02f;
        const float target = std::max(inputEnv, 0.04f + damping * 0.16f);
        envL_ += (target - envL_) * (target > envL_ ? attack : release);
        envR_ += (target - envR_) * (target > envR_ ? attack : release);

        const float modFreq = std::min(freq * ratio, sampleRate_ * 0.45f);
        modPhase_ += modFreq / sampleRate_;
        carrierPhase_ += freq / sampleRate_;
        if (modPhase_ >= 1.0f)
            modPhase_ -= 1.0f;
        if (carrierPhase_ >= 1.0f)
            carrierPhase_ -= 1.0f;

        const float fbPhase = feedback < 0.0f ? feedback * feedback * prev_ * -0.55f : 0.0f;
        const float mod = std::sin(kTwoPi * (modPhase_ + fbPhase));
        const float index = (0.25f + brightness * 7.0f) * (0.35f + envL_ * 1.65f);
        const float carrier = std::sin(kTwoPi * carrierPhase_ + mod * index + std::max(0.0f, feedback) * prev_ * 1.8f);
        prev_ += (carrier - prev_) * 0.18f;

        const float wet = (carrier + mod * 0.35f) * envL_;
        const float cutoff = MapExpo(1.0f - damping, 180.0f, 16000.0f);
        const float left = wet + inL * 0.20f;
        const float right = (carrier - mod * 0.35f) * envR_ + inR * 0.20f;
        outL = SoftLimit(OnePoleProcess(left, cutoff, sampleRate_, lpL_) * 1.45f);
        outR = SoftLimit(OnePoleProcess(right, cutoff * 1.07f, sampleRate_, lpR_) * 1.45f);
    }

private:
    float sampleRate_ = 48000.0f;
    float carrierPhase_ = 0.0f;
    float modPhase_ = 0.0f;
    float envL_ = 0.0f;
    float envR_ = 0.0f;
    float prev_ = 0.0f;
    float lpL_ = 0.0f;
    float lpR_ = 0.0f;
};

class AlgoNbz
{
public:
    void Init(float sampleRate)
    {
        sampleRate_ = sampleRate;
        midBpL_.Init(sampleRate_);
        midBpR_.Init(sampleRate_);
        Reset();
    }

    void Reset()
    {
        lpStateL_ = 0.0f;
        lpStateR_ = 0.0f;
        phaseL_ = 0.0f;
        phaseR_ = 0.5f;
        highApX1L_ = 0.0f;
        highApY1L_ = 0.0f;
        highApX2L_ = 0.0f;
        highApY2L_ = 0.0f;
        highApX1R_ = 0.0f;
        highApY1R_ = 0.0f;
        highApX2R_ = 0.0f;
        highApY2R_ = 0.0f;
        s_delayA.Reset();
        s_delayB.Reset();
        midBpL_.SetRes(0.45f);
        midBpR_.SetRes(0.45f);
    }

    void Process(float inL, float inR, const NeuroticRuntime &rt, float &outL, float &outR)
    {
        const float crossoverHz = MapExpo(rt.c3, 50.0f, 250.0f);
        // Use a proper one-pole coefficient; previous linear form made cutoff effectively far too low.
        const float alpha = std::clamp(1.0f - std::exp(-kTwoPi * crossoverHz / sampleRate_), 0.0f, 1.0f);

        lpStateL_ += (inL - lpStateL_) * alpha;
        lpStateR_ += (inR - lpStateR_) * alpha;
        const float lowL = lpStateL_;
        const float lowR = lpStateR_;
        const float highL = inL - lowL;
        const float highR = inR - lowR;

        const float bassMix = Clamp01(rt.c1);
        const float highMix = Clamp01(rt.c2);
        const float midHz = MapExpo(rt.c4, 220.0f, 4200.0f);
        const float drive = 1.0f + Clamp01(rt.c5) * 9.0f;
        const float distMix = Clamp01(rt.c5);
        const float lfo = std::clamp(rt.lfoValue * Clamp01(rt.lfoDepth), -1.0f, 1.0f);

        midBpL_.SetFreq(midHz);
        midBpR_.SetFreq(midHz);
        midBpL_.Process(inL);
        midBpR_.Process(inR);
        const float midL = midBpL_.Band();
        const float midR = midBpR_.Band();
        const float midDistL = std::tanh(midL * drive) * distMix;
        const float midDistR = std::tanh(midR * drive) * distMix;

        // Drive the octave path from mostly-low content, with a little full-band injection
        // so sub generation is still obvious on thinner sources.
        const float lowSubL = PitchDownOctave(lowL + inL * 0.35f, phaseL_, s_delayA);
        const float lowSubR = PitchDownOctave(lowR + inR * 0.35f, phaseR_, s_delayB);

        const float apA1L = std::clamp(0.25f + 0.45f * (0.5f + 0.5f * lfo), 0.05f, 0.85f);
        const float apA2L = std::clamp(0.20f + 0.40f * (0.5f + 0.5f * lfo), 0.05f, 0.85f);
        const float apA1R = std::clamp(0.25f + 0.45f * (0.5f - 0.5f * lfo), 0.05f, 0.85f);
        const float apA2R = std::clamp(0.20f + 0.40f * (0.5f - 0.5f * lfo), 0.05f, 0.85f);
        const float highShiftL = Allpass(Allpass(highL, apA1L, highApX1L_, highApY1L_), apA2L, highApX2L_, highApY2L_);
        const float highShiftR = Allpass(Allpass(highR, apA1R, highApX1R_, highApY1R_), apA2R, highApX2R_, highApY2R_);

        // At full control, pivot from original low band toward strong octave-down low band.
        const float subBoostL = SoftClip(lowSubL * 6.4f);
        const float subBoostR = SoftClip(lowSubR * 6.4f);
        const float bassEffectL = (subBoostL * 6.0f) - (lowL * 0.45f);
        const float bassEffectR = (subBoostR * 6.0f) - (lowR * 0.45f);

        // Replace low/high bands instead of quietly layering on top of dry,
        // so C1/C2 can push to a clearly transformed sound.
        auto BlendWithHardMode = [](float dryBand, float wetBand, float control) -> float {
            const float c = Clamp01(control);
            if (c > 0.75f)
            {
                return wetBand;
            }
            return Lerp(dryBand, wetBand, c / 0.75f);
        };

        const float lowOutL = BlendWithHardMode(lowL, bassEffectL * 4.6f, bassMix);
        const float highOutL = BlendWithHardMode(highL, highShiftL * 2.2f, highMix);

        // Keep sub amount tied to C1 on both channels so C1=0 fully removes sub.
        const float lowOutR = BlendWithHardMode(lowR, bassEffectR * 4.6f, bassMix);
        const float highOutR = BlendWithHardMode(highR, highShiftR * 2.2f, highMix);

        const float dryMidL = inL - lowL - highL;
        const float dryMidR = inR - lowR - highR;
        const float coreL = dryMidL + lowOutL + highOutL;
        const float coreR = dryMidR + lowOutR + highOutR;

        outL = SoftClip((coreL + 1.15f * midDistL) * 0.08f);
        outR = SoftClip((coreR + 1.15f * midDistR) * 0.08f);
    }

private:
    float PitchDownOctave(float in, float &phase, SimpleDelay &delay)
    {
        constexpr float ratio = 0.5f;
        const float windowSec = 0.035f;
        const float windowSamples = windowSec * sampleRate_;
        const float inc = (1.0f - ratio) / windowSamples;

        phase += inc;
        if (phase >= 1.0f)
            phase -= 1.0f;
        if (phase < 0.0f)
            phase += 1.0f;

        const float phaseB = (phase >= 0.5f) ? (phase - 0.5f) : (phase + 0.5f);
        const float winA = 1.0f - std::fabs(phase * 2.0f - 1.0f);
        const float winB = 1.0f - std::fabs(phaseB * 2.0f - 1.0f);
        const float winSum = std::max(0.001f, winA + winB);

        delay.Write(in);
        const float a = delay.Read(windowSamples * phase);
        const float b = delay.Read(windowSamples * phaseB);
        return (a * winA + b * winB) / winSum;
    }

    float sampleRate_ = 48000.0f;
    float lpStateL_ = 0.0f;
    float lpStateR_ = 0.0f;
    float phaseL_ = 0.0f;
    float phaseR_ = 0.5f;
    float highApX1L_ = 0.0f;
    float highApY1L_ = 0.0f;
    float highApX2L_ = 0.0f;
    float highApY2L_ = 0.0f;
    float highApX1R_ = 0.0f;
    float highApY1R_ = 0.0f;
    float highApX2R_ = 0.0f;
    float highApY2R_ = 0.0f;
    daisysp::Svf midBpL_;
    daisysp::Svf midBpR_;
};

static AlgoNcr s_algoNcr;
static AlgoLsb s_algoLsb;
static AlgoNth s_algoNth;
static AlgoBgm s_algoBgm;
static AlgoNff s_algoNff;
static AlgoNdm s_algoNdm;
static AlgoNhc s_algoNhc;
static AlgoNpl s_algoNpl;
static AlgoNsm s_algoNsm;
static AlgoNps s_algoNps;
static AlgoNrv s_algoNrv;
#if NEUROTIC_ENABLE_EUDELAY
static AlgoNed s_algoNed;
#endif
static AlgoNws s_algoNws;
static AlgoNmb s_algoNmb;
static AlgoNss s_algoNss;
static AlgoNfm s_algoNfm;
static AlgoNbz s_algoNbz;

void NeuroticAlgoBank::Init(float sampleRate)
{
    sampleRate_ = sampleRate;
    s_spectral.Init();
    s_delayA.Reset();
    s_delayB.Reset();
#if NEUROTIC_ENABLE_EUDELAY
    std::fill(&s_eudelayBufferL[0], &s_eudelayBufferL[kEuDelayMax], 0.0f);
    std::fill(&s_eudelayBufferR[0], &s_eudelayBufferR[kEuDelayMax], 0.0f);
#endif
    ncr_ = &s_algoNcr;
    lsb_ = &s_algoLsb;
    nth_ = &s_algoNth;
    bgm_ = &s_algoBgm;
    nff_ = &s_algoNff;
    ndm_ = &s_algoNdm;
    nhc_ = &s_algoNhc;
    npl_ = &s_algoNpl;
    nsm_ = &s_algoNsm;
    nps_ = &s_algoNps;
    nrv_ = &s_algoNrv;
#if NEUROTIC_ENABLE_EUDELAY
    ned_ = &s_algoNed;
#endif
    nws_ = &s_algoNws;
    nmb_ = &s_algoNmb;
    nss_ = &s_algoNss;
    nfm_ = &s_algoNfm;
    nbz_ = &s_algoNbz;

    ncr_->Init(sampleRate_);
    lsb_->Init(sampleRate_);
    nth_->Init(sampleRate_);
    bgm_->Init(sampleRate_);
    nff_->Init(sampleRate_);
    ndm_->Init(sampleRate_);
    nhc_->Init(sampleRate_);
    npl_->Init(sampleRate_);
    nsm_->Init(sampleRate_);
    nps_->Init(sampleRate_);
    nrv_->Init(sampleRate_);
#if NEUROTIC_ENABLE_EUDELAY
    ned_->Init(sampleRate_);
#endif
    nws_->Init(sampleRate_);
    nmb_->Init(sampleRate_);
    nss_->Init(sampleRate_);
    nfm_->Init(sampleRate_);
    nbz_->Init(sampleRate_);
}

void NeuroticAlgoBank::Reset(int algoIndex)
{
    s_spectral.Reset();
    s_delayA.Reset();
    s_delayB.Reset();
    switch (algoIndex)
    {
    case 0:
        ncr_->Reset();
        break;
    case 1:
        lsb_->Reset();
        break;
    case 2:
        nth_->Reset();
        break;
    case 3:
        bgm_->Reset();
        break;
    case 4:
        nff_->Reset();
        break;
    case 5:
        ndm_->Reset();
        break;
    case 6:
        nhc_->Reset();
        break;
    case 7:
        npl_->Reset();
        break;
    case 8:
        nsm_->Reset();
        break;
    case 9:
        nps_->Reset();
        break;
    case 10:
        nrv_->Reset();
        break;
#if NEUROTIC_ENABLE_EUDELAY
    case 11:
        ned_->Reset();
        break;
#endif
    case 12:
        nws_->Reset();
        break;
    case 13:
        nmb_->Reset();
        break;
    case 14:
        nss_->Reset();
        break;
    case 15:
        nfm_->Reset();
        break;
    case 16:
        nbz_->Reset();
        break;
    default:
        break;
    }
}

void NeuroticAlgoBank::Process(int algoIndex, float inL, float inR, const NeuroticRuntime &rt, float &outL, float &outR)
{
    switch (algoIndex)
    {
    case 0:
        ncr_->Process(inL, inR, rt, outL, outR);
        break;
    case 1:
        lsb_->Process(inL, inR, rt, outL, outR);
        break;
    case 2:
        nth_->Process(inL, inR, rt, outL, outR);
        break;
    case 3:
        bgm_->Process(inL, inR, rt, outL, outR);
        break;
    case 4:
        nff_->Process(inL, inR, rt, outL, outR);
        break;
    case 5:
        ndm_->Process(inL, inR, rt, outL, outR);
        break;
    case 6:
        nhc_->Process(inL, inR, rt, outL, outR);
        break;
    case 7:
        npl_->Process(inL, inR, rt, outL, outR);
        break;
    case 8:
        nsm_->Process(inL, inR, rt, outL, outR);
        break;
    case 9:
        nps_->Process(inL, inR, rt, outL, outR);
        break;
    case 10:
        nrv_->Process(inL, inR, rt, outL, outR);
        break;
#if NEUROTIC_ENABLE_EUDELAY
    case 11:
        ned_->Process(inL, inR, rt, outL, outR);
        break;
#endif
    case 12:
        nws_->Process(inL, inR, rt, outL, outR);
        break;
    case 13:
        nmb_->Process(inL, inR, rt, outL, outR);
        break;
    case 14:
        nss_->Process(inL, inR, rt, outL, outR);
        break;
    case 15:
        nfm_->Process(inL, inR, rt, outL, outR);
        break;
    case 16:
        nbz_->Process(inL, inR, rt, outL, outR);
        break;
    default:
        outL = inL;
        outR = inR;
        break;
    }
}
