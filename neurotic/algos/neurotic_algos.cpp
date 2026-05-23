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
        const float spread = 1.0f + mass * 2.5f;
        const float q = 0.6f + resonance * 10.0f;

        float sumL = 0.0f;
        float sumR = 0.0f;
        for (int i = 0; i < 2; ++i)
        {
            const float ratio = 0.8f + static_cast<float>(i) * 0.9f;
            const float freqL = base * ratio;
            const float freqR = base * ratio * (1.0f + asym * 0.2f);
            svfL_[i].SetFreq(freqL * spread);
            svfR_[i].SetFreq(freqR * spread);
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

        outL = sumL;
        outR = sumR;
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

        const float depth = Clamp01(rt.c1);
        const float formant = Clamp01(rt.c2);
        const float transient = Clamp01(rt.c3);
        const float weave = Clamp01(rt.c4);

        const float protect = Lerp(0.1f, 1.0f, transient);
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

            const float mixBase = (k < formantBin) ? weave : depth;
            const float hi = static_cast<float>(k) / static_cast<float>(kBins - 1);
            const float mix = mixBase * (1.0f - 0.4f * hi);
            const float magLNew = Lerp(magL, magR, mix);
            const float magRNew = Lerp(magR, magL, mix);
            const float phaseShift = mix * protect * 4.0f;
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
        const float flow = rt.c2 + rt.lfoValue * rt.lfoDepth * 0.4f;
        const float headGap = rt.c3;
        const float fb = std::clamp(rt.c4 * 1.2f, 0.0f, 0.98f);

        const float baseDelay = MapExpo(0.1f + std::clamp(flow, 0.0f, 1.0f) * 0.9f, 120.0f, 2000.0f);
        phase_ += (0.1f + flow * 2.0f) / sampleRate_;
        if (phase_ > 1.0f)
            phase_ -= 1.0f;
        const float mod = std::sin(phase_ * kTwoPi) * (20.0f + flow * 140.0f);
        const float delaySamp = baseDelay + mod;

        const float satL = SoftClip(inL * (1.0f + drive * 4.0f));
        const float satR = SoftClip(inR * (1.0f + drive * 4.0f));

        const float dl = s_delayA.Read(delaySamp);
        const float dr = s_delayB.Read(delaySamp * 0.97f);
        const float gapCut = MapExpo(1.0f - headGap, 80.0f, 12000.0f);
        const float fbL = OnePoleProcess(dl, gapCut, sampleRate_, lpStateL_);
        const float fbR = OnePoleProcess(dr, gapCut, sampleRate_, lpStateR_);

        s_delayA.Write(satL + fbL * fb);
        s_delayB.Write(satR + fbR * fb);

        outL = dl;
        outR = dr;
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
        const float spin = rt.lfoValue * rt.lfoDepth * (0.3f + spinAmount * 1.4f);

        phase_ += (0.05f + rt.lfoRate * 2.2f) / sampleRate_;
        if (phase_ > 1.0f)
            phase_ -= 1.0f;
        const float spinPan = std::sin(phase_ * kTwoPi) * spin * 1.6f;

        const float pan = std::clamp(az + spinPan, -1.0f, 1.0f);
        const float itd = std::fabs(pan) * (20.0f + dist * 100.0f);
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

        const float cutoff = MapExpo(1.0f - dist, 200.0f, 14000.0f);
        const float mono = 0.5f * (l + r);
        const float distant = OnePoleProcess(mono, cutoff, sampleRate_, lpState_);

        outL = (distant + (l - distant) * elev) * leftGain * 2.2f;
        outR = (distant + (r - distant) * elev) * rightGain * 2.2f;
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
    }

    void Process(float inL, float inR, const NeuroticRuntime &rt, float &outL, float &outR)
    {
        const float vowel = rt.c1;
        const float art = rt.c2;
        const float artic = rt.c3;
        const float breath = rt.c4;

        const float base = MapPitch(vowel, 43.0f, 83.0f);
        const float spread = 1.4f + art * 1.5f;
        const float f1 = base;
        const float f2 = base * spread;
        const float f3 = base * (spread + 0.8f);

        const float res = 0.8f + rt.c5 * 6.0f;
        for (int i = 0; i < 3; ++i)
        {
            formL_[i].SetRes(res);
            formR_[i].SetRes(res);
        }

        formL_[0].SetFreq(f1);
        formL_[1].SetFreq(f2);
        formL_[2].SetFreq(f3);

        const float splitMul = 1.0f + breath * 0.9f;
        formR_[0].SetFreq(f1 * splitMul);
        formR_[1].SetFreq(f2 * splitMul);
        formR_[2].SetFreq(f3 * splitMul);

        const float noise = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * artic * 0.12f;
        const float nL = inL + noise;
        const float nR = inR + noise;

        float sumL = 0.0f;
        float sumR = 0.0f;
        for (int i = 0; i < 3; ++i)
        {
            formL_[i].Process(nL);
            formR_[i].Process(nR);
            sumL += formL_[i].Band();
            sumR += formR_[i].Band();
        }

        const float airCut = MapExpo(std::clamp(breath, 0.0f, 1.0f), 500.0f, 12000.0f);
        const float lpL = OnePoleProcess(inL, airCut, sampleRate_, airStateL_);
        const float lpR = OnePoleProcess(inR, airCut, sampleRate_, airStateR_);
        const float airL = inL - lpL;
        const float airR = inR - lpR;

        outL = sumL * 0.6f + airL * (breath * 0.8f);
        outR = sumR * 0.6f + airR * (breath * 0.8f);
    }

private:
    float sampleRate_ = 48000.0f;
    daisysp::Svf formL_[3];
    daisysp::Svf formR_[3];
    float airStateL_ = 0.0f;
    float airStateR_ = 0.0f;
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
        const float drift = rt.c4 + rt.lfoValue * rt.lfoDepth * 0.4f;

        phase_ += (0.1f + drift * 1.5f) / sampleRate_;
        if (phase_ > 1.0f)
            phase_ -= 1.0f;
        const float mod = std::sin(phase_ * kTwoPi) * (8.0f + spread * 40.0f);

        const float delayA = 40.0f + spread * 220.0f + mod;
        const float delayB = 70.0f + spread * 300.0f - mod;

        const float dl = s_delayA.Read(delayA);
        const float dr = s_delayB.Read(delayB);
        s_delayA.Write(inL + dl * (0.25f + gran * 0.6f));
        s_delayB.Write(inR + dr * (0.25f + gran * 0.6f));

        const float tilt = (color - 0.5f) * 0.8f;
        outL = dl + inL * tilt;
        outR = dr - inR * tilt;
    }

private:
    float sampleRate_ = 48000.0f;
    float phase_ = 0.0f;
};

class AlgoNes
{
public:
    void Init(float sampleRate)
    {
        sampleRate_ = sampleRate;
        Reset();
    }
    void Reset() { envL_ = 0.0f; envR_ = 0.0f; }

    void Process(float inL, float inR, const NeuroticRuntime &rt, float &outL, float &outR)
    {
        const float punch = std::clamp(rt.c1 * 2.0f, 0.0f, 1.0f);
        const float glue = std::clamp(rt.c2 * 1.6f, 0.0f, 1.0f);
        const float lift = std::clamp(rt.c3 * 2.0f, 0.0f, 1.0f);
        const float bias = rt.c4;

        const float attack = 0.002f + (1.0f - punch) * 0.02f;
        const float release = 0.01f + (1.0f - punch) * 0.12f;

        envL_ += (std::fabs(inL) - envL_) * attack;
        envR_ += (std::fabs(inR) - envR_) * attack;
        envL_ += (std::fabs(inL) - envL_) * release;
        envR_ += (std::fabs(inR) - envR_) * release;

        const float env = Lerp(envL_, envR_, glue);
        const float comp = 1.0f / (1.0f + env * (1.0f + punch * 18.0f));
        const float liftGain = 1.0f + lift * 1.6f;

        const float mixL = Lerp(inL, inR, bias);
        const float mixR = Lerp(inR, inL, bias);
        const float expand = 1.0f + lift * 0.6f;
        outL = SoftClip(mixL * comp * liftGain * expand);
        outR = SoftClip(mixR * comp * liftGain * expand);
    }

private:
    float sampleRate_ = 48000.0f;
    float envL_ = 0.0f;
    float envR_ = 0.0f;
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

        const float scale = 0.6f + stretch * 1.8f;
        const float inharm = inh * 0.18f;

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

            const int period = 2 + static_cast<int>(sparsity * 24.0f);
            const int width = std::max(1, period / 5);
            const int slot = static_cast<int>(k) % period;
            const float gate = (slot < width) ? 1.0f : 0.35f;
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
                const float fold = mirror * 0.6f;
                s_spectral.re[0][mirrorBin] += s_spectral.re[0][k] * fold;
                s_spectral.im[0][mirrorBin] -= s_spectral.im[0][k] * fold;
                s_spectral.re[1][mirrorBin] += s_spectral.re[1][k] * fold;
                s_spectral.im[1][mirrorBin] -= s_spectral.im[1][k] * fold;
            }
        }

        const float gain = 1.6f + stretch * 1.0f;
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

        const float bind = std::clamp(rt.c1 * 3.0f, 0.0f, 1.0f);
        const float swirl = std::clamp(rt.c2 * 4.0f + rt.lfoValue * rt.lfoDepth * 1.6f, 0.0f, 1.0f);
        const float tilt = std::clamp(rt.c3 * 4.0f, 0.0f, 1.0f);
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

            const float warp = (static_cast<float>(k) / static_cast<float>(kBins)) * tilt * 3.0f;
            const float swirlPhase = std::sin(k * 0.02f) * swirl * 8.0f;

            const float phaseLNew = phaseL + swirlPhase + warp;
            const float phaseRNew = phaseR - swirlPhase - warp;

            const float linkL = phaseLNew + ShortestPhaseDelta(phaseLNew, phaseRNew) * bind;
            const float linkR = phaseRNew + ShortestPhaseDelta(phaseRNew, phaseLNew) * bind;

            s_spectral.re[0][k] = magL * std::cos(linkL);
            s_spectral.im[0][k] = magL * std::sin(linkL);
            s_spectral.re[1][k] = magR * std::cos(linkR);
            s_spectral.im[1][k] = magR * std::sin(linkR);
        }

        const float widen = 1.0f + stereo * 1.1f;
        s_spectral.re[0][0] *= widen;
        s_spectral.re[1][0] *= 1.0f - stereo * 0.6f;

        s_spectral.InverseToOutput();
    }

private:
    int unused_ = 0;
};

class AlgoNmg
{
public:
    void Init(float sampleRate)
    {
        sampleRate_ = sampleRate;
    }

    void Reset()
    {
        hold_ = 0;
        holdSampleL_ = 0.0f;
        holdSampleR_ = 0.0f;
    }

    void Process(float inL, float inR, const NeuroticRuntime &rt, float &outL, float &outR)
    {
        const float size = rt.c1;
        const float drift = rt.c2 + rt.lfoValue * rt.lfoDepth * 0.4f;
        const float blend = rt.c3;
        const float scatter = rt.c4;

        const int grainSize = 20 + static_cast<int>(size * 300.0f);
        if (hold_ <= 0)
        {
            hold_ = grainSize;
            const float driftAmt = std::clamp(drift, -1.0f, 1.0f);
            const float jitter = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * driftAmt * 40.0f;
            holdSampleL_ = s_delayA.Read(10.0f + scatter * 80.0f + jitter);
            holdSampleR_ = s_delayB.Read(10.0f + (1.0f - scatter) * 80.0f - jitter);
            holdWindow_ = 0.0f;
        }
        hold_--;

        s_delayA.Write(inL);
        s_delayB.Write(inR);

        holdWindow_ += 1.0f / std::max(1, grainSize);
        const float win = 0.5f - 0.5f * std::cos(kTwoPi * std::clamp(holdWindow_, 0.0f, 1.0f));

        const float grainL = holdSampleL_ * win;
        const float grainR = holdSampleR_ * win;

        outL = Lerp(inL, grainL, blend);
        outR = Lerp(inR, grainR, blend);
    }

private:
    float sampleRate_ = 48000.0f;
    int hold_ = 0;
    float holdSampleL_ = 0.0f;
    float holdSampleR_ = 0.0f;
    float holdWindow_ = 0.0f;
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

class AlgoNce
{
public:
    void Init(float sampleRate)
    {
        sampleRate_ = sampleRate;
        Reset();
    }

    void Reset()
    {
        envL_ = 0.0f;
        envR_ = 0.0f;
    }

    void Process(float inL, float inR, const NeuroticRuntime &rt, float &outL, float &outR)
    {
        const float amount = std::clamp(rt.c1 * 2.0f - 1.0f, -1.0f, 1.0f);
        const float timeScale = MapExpo(rt.c2, 0.05f, 2.0f);
        const float attackSec = (0.001f + rt.c3 * 0.05f) * timeScale;
        const float decaySec = (0.01f + rt.c4 * 0.3f) * timeScale;

        const float atkCoef = std::exp(-1.0f / (attackSec * sampleRate_));
        const float decCoef = std::exp(-1.0f / (decaySec * sampleRate_));

        const float absL = std::fabs(inL);
        const float absR = std::fabs(inR);
        envL_ = (absL > envL_) ? (atkCoef * envL_ + (1.0f - atkCoef) * absL)
                               : (decCoef * envL_ + (1.0f - decCoef) * absL);
        envR_ = (absR > envR_) ? (atkCoef * envR_ + (1.0f - atkCoef) * absR)
                               : (decCoef * envR_ + (1.0f - decCoef) * absR);

        const float env = 0.5f * (envL_ + envR_);
        float gain = 1.0f;
        if (amount >= 0.0f)
        {
            gain = 1.0f / (1.0f + amount * env * 8.0f);
        }
        else
        {
            const float expand = -amount;
            gain = 1.0f + expand * std::clamp(0.5f - env, 0.0f, 0.5f) * 4.0f;
        }

        gain = std::clamp(gain, 0.2f, 2.0f);
        outL = SoftClip(inL * gain);
        outR = SoftClip(inR * gain);
    }

private:
    float sampleRate_ = 48000.0f;
    float envL_ = 0.0f;
    float envR_ = 0.0f;
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
        const float timeScale = MapExpo(rt.c1, 0.125f, 4.0f);
        const float baseDelaySamples = std::clamp(sampleRate_ * 0.16f * timeScale, 1.0f, kMaxDelaySamples_);
        const float cross = Clamp01(rt.c3);
        const float tilt = Clamp01(rt.c5);
        const float fb = std::clamp(rt.c2, 0.0f, 0.98f);
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
            const float tiltWeight = std::clamp(1.0f + tiltBipolar * (ratio - 0.5f) * 1.6f, 0.25f, 1.75f);
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

        constexpr float kOutputMakeup = 2.8f;
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

class AlgoNwl
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
        std::fill(&fbCoeffsL_[0], &fbCoeffsL_[kBlock], 0.0f);
        std::fill(&fbCoeffsR_[0], &fbCoeffsR_[kBlock], 0.0f);
        blockIndex_ = 0;
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
            ProcessBlock(inBlockL_, outBlockL_, fbCoeffsL_, rt, 1.0f);
            ProcessBlock(inBlockR_, outBlockR_, fbCoeffsR_, rt, -1.0f);
        }
    }

private:
    static constexpr int kBlock = 32;
    static constexpr float kInvSqrt2 = 0.70710678118f;
    static constexpr float kLeak = 0.95f;

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

    void ProcessBlock(const float *in, float *out, float *fb, const NeuroticRuntime &rt, float stereoPolarity)
    {
        float coeffs[kBlock];
        float tmp[kBlock];
        for (int i = 0; i < kBlock; ++i)
        {
            coeffs[i] = in[i];
        }

        ForwardHaar(coeffs, tmp);

        float *a3 = &coeffs[0];
        float *d3 = &coeffs[4];
        float *d2 = &coeffs[8];
        float *d1 = &coeffs[16];

        const float amount = Clamp01(rt.c1);
        const float feedback = std::clamp(rt.c2, 0.0f, 0.8f);
        const float damping = Clamp01(rt.c3);
        const float diffusion = Clamp01(rt.c4);
        const float tilt = Clamp01(rt.c5);

        const float up1 = amount * (0.15f + 0.20f * tilt);
        const float up2 = amount * (0.10f + 0.30f * tilt);
        const float up3 = amount * (0.05f + 0.40f * tilt);

        for (int i = 0; i < 4; ++i)
        {
            d3[i] += a3[i] * up1;
        }
        for (int i = 0; i < 8; ++i)
        {
            d2[i] += d3[i >> 1] * up2;
        }
        for (int i = 0; i < 16; ++i)
        {
            d1[i] += d2[i >> 1] * up3;
        }

        const float dampD3 = 1.0f - 0.25f * damping;
        const float dampD2 = 1.0f - 0.40f * damping;
        const float dampD1 = 1.0f - 0.60f * damping;
        for (int i = 0; i < 4; ++i)
        {
            d3[i] *= dampD3;
        }
        for (int i = 0; i < 8; ++i)
        {
            d2[i] *= dampD2;
        }
        for (int i = 0; i < 16; ++i)
        {
            d1[i] *= dampD1;
        }

        for (int i = 0; i < kBlock; ++i)
        {
            float v = coeffs[i] + fb[i] * feedback;
            if (i >= 16 && (i & 1))
            {
                const float decor = -v * stereoPolarity;
                v = Lerp(v, decor, diffusion * 0.35f);
            }
            coeffs[i] = std::clamp(v, -1.5f, 1.5f);
            fb[i] = coeffs[i] * kLeak;
        }

        InverseHaar(coeffs, tmp);

        for (int i = 0; i < kBlock; ++i)
        {
            out[i] = SoftLimit(coeffs[i] * 0.95f);
        }
    }

    float sampleRate_ = 48000.0f;
    int blockIndex_ = 0;
    float inBlockL_[kBlock];
    float inBlockR_[kBlock];
    float outBlockL_[kBlock];
    float outBlockR_[kBlock];
    float fbCoeffsL_[kBlock];
    float fbCoeffsR_[kBlock];
};

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

        const float scramble = 0.35f + 0.65f * Clamp01(rt.c1);
        const float rate = Clamp01(rt.c2);
        // Bias toward smoother default behavior around center settings.
        const float smoothing = std::pow(Clamp01(rt.c3), 0.75f);
        const float spread = Clamp01(rt.c4);
        const float tilt = Clamp01(rt.c5);

        const int updateEvery = 1 + static_cast<int>((1.0f - rate) * (1.0f - rate) * 23.0f + 0.5f);
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
            coeffs[i] = std::clamp(smooth[i], -1.5f, 1.5f);
        }

        InverseHaar(coeffs, tmp);

        for (int i = 0; i < kBlock; ++i)
        {
            const float dryKeep = 0.20f * (1.0f - scramble);
            out[i] = SoftLimit(coeffs[i] * 0.98f + in[i] * dryKeep);
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

class AlgoNwg
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
        gateGain_[0] = 1.0f;
        gateGain_[1] = 1.0f;
        gateGain_[2] = 1.0f;
        step_ = 0;
        blockIndex_ = 0;
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
            ProcessBlock(inBlockL_, inBlockR_, outBlockL_, outBlockR_, rt);
        }
    }

private:
    static constexpr int kBlock = 32;
    static constexpr int kSteps = 16;
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

    bool EuclidHit(int step, int fills, int total, int rotation) const
    {
        if (fills <= 0)
            return false;
        const int idx = (step + rotation) % total;
        return ((idx * fills) % total) < fills;
    }

    void ProcessBlock(const float *inL, const float *inR, float *outL, float *outR, const NeuroticRuntime &rt)
    {
        float coeffL[kBlock];
        float coeffR[kBlock];
        float tmp[kBlock];
        for (int i = 0; i < kBlock; ++i)
        {
            coeffL[i] = inL[i];
            coeffR[i] = inR[i];
        }

        ForwardHaar(coeffL, tmp);
        ForwardHaar(coeffR, tmp);

        float energy[3] = {0.0f, 0.0f, 0.0f};
        for (int i = 0; i < 4; ++i)
        {
            energy[0] += std::fabs(coeffL[i]) + std::fabs(coeffR[i]);
        }
        for (int i = 4; i < 16; ++i)
        {
            energy[1] += std::fabs(coeffL[i]) + std::fabs(coeffR[i]);
        }
        for (int i = 16; i < 32; ++i)
        {
            energy[2] += std::fabs(coeffL[i]) + std::fabs(coeffR[i]);
        }
        energy[0] /= 8.0f;
        energy[1] /= 24.0f;
        energy[2] /= 32.0f;

        const float threshold = 0.002f + 0.06f * Clamp01(rt.c1);
        const int fills = std::clamp(1 + static_cast<int>(Clamp01(rt.c2) * 15.0f + 0.5f), 1, 16);
        // Bias toward snappier gate releases around center settings.
        const float release = std::pow(Clamp01(rt.c3), 1.35f);
        const float weighting = Clamp01(rt.c4) * 2.0f - 1.0f;
        const float tilt = Clamp01(rt.c5);

        const float tCoarse = std::clamp(threshold * (1.1f + 0.8f * weighting), 0.002f, 0.9f);
        const float tMid = std::clamp(threshold, 0.002f, 0.9f);
        const float tFine = std::clamp(threshold * (1.1f - 0.8f * weighting), 0.002f, 0.9f);

        const float attackCoef = 0.65f;
        const float releaseCoef = 0.28f - 0.24f * release;

        const int fineRotate = 2 + static_cast<int>(tilt * 5.0f + 0.5f);
        const int midRotate = 1 + static_cast<int>(tilt * 2.0f + 0.5f);
        const int coarseRotate = static_cast<int>(tilt * 3.0f + 0.5f);

        const bool gateCoarse = EuclidHit(step_, fills, kSteps, coarseRotate) && energy[0] > tCoarse;
        const bool gateMid = EuclidHit(step_, fills, kSteps, midRotate) && energy[1] > tMid;
        const bool gateFine = EuclidHit(step_, fills, kSteps, fineRotate) && energy[2] > tFine;
        step_ = (step_ + 1) % kSteps;

        const float floor = 0.14f + 0.24f * release;
        const float target[3] = {
            gateCoarse ? 1.0f : floor,
            gateMid ? 1.0f : floor,
            gateFine ? 1.0f : floor,
        };

        for (int b = 0; b < 3; ++b)
        {
            const float coef = (target[b] > gateGain_[b]) ? attackCoef : releaseCoef;
            gateGain_[b] += (target[b] - gateGain_[b]) * coef;
        }

        for (int i = 0; i < 4; ++i)
        {
            coeffL[i] *= gateGain_[0];
            coeffR[i] *= gateGain_[0];
        }
        for (int i = 4; i < 16; ++i)
        {
            coeffL[i] *= gateGain_[1];
            coeffR[i] *= gateGain_[1];
        }
        for (int i = 16; i < 32; ++i)
        {
            coeffL[i] *= gateGain_[2];
            coeffR[i] *= gateGain_[2];
        }

        InverseHaar(coeffL, tmp);
        InverseHaar(coeffR, tmp);

        const float makeup = 1.35f + 0.45f * (1.0f - Clamp01(rt.c2));
        for (int i = 0; i < kBlock; ++i)
        {
            outL[i] = SoftLimit(coeffL[i] * makeup);
            outR[i] = SoftLimit(coeffR[i] * makeup);
        }
    }

    float sampleRate_ = 48000.0f;
    int blockIndex_ = 0;
    int step_ = 0;
    float gateGain_[3] = {1.0f, 1.0f, 1.0f};
    float inBlockL_[kBlock];
    float inBlockR_[kBlock];
    float outBlockL_[kBlock];
    float outBlockR_[kBlock];
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
static AlgoNes s_algoNes;
static AlgoNhc s_algoNhc;
static AlgoNpl s_algoNpl;
static AlgoNmg s_algoNmg;
static AlgoNsm s_algoNsm;
static AlgoNce s_algoNce;
static AlgoNps s_algoNps;
static AlgoNrv s_algoNrv;
#if NEUROTIC_ENABLE_EUDELAY
static AlgoNed s_algoNed;
#endif
static AlgoNwl s_algoNwl;
static AlgoNws s_algoNws;
static AlgoNwg s_algoNwg;
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
    nes_ = &s_algoNes;
    nhc_ = &s_algoNhc;
    npl_ = &s_algoNpl;
    nmg_ = &s_algoNmg;
    nsm_ = &s_algoNsm;
    nce_ = &s_algoNce;
    nps_ = &s_algoNps;
    nrv_ = &s_algoNrv;
#if NEUROTIC_ENABLE_EUDELAY
    ned_ = &s_algoNed;
#endif
    nwl_ = &s_algoNwl;
    nws_ = &s_algoNws;
    nwg_ = &s_algoNwg;
    nbz_ = &s_algoNbz;

    ncr_->Init(sampleRate_);
    lsb_->Init(sampleRate_);
    nth_->Init(sampleRate_);
    bgm_->Init(sampleRate_);
    nff_->Init(sampleRate_);
    ndm_->Init(sampleRate_);
    nes_->Init(sampleRate_);
    nhc_->Init(sampleRate_);
    npl_->Init(sampleRate_);
    nmg_->Init(sampleRate_);
    nsm_->Init(sampleRate_);
    nce_->Init(sampleRate_);
    nps_->Init(sampleRate_);
    nrv_->Init(sampleRate_);
#if NEUROTIC_ENABLE_EUDELAY
    ned_->Init(sampleRate_);
#endif
    nwl_->Init(sampleRate_);
    nws_->Init(sampleRate_);
    nwg_->Init(sampleRate_);
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
        nes_->Reset();
        break;
    case 7:
        nhc_->Reset();
        break;
    case 8:
        npl_->Reset();
        break;
    case 9:
        nmg_->Reset();
        break;
    case 10:
        nsm_->Reset();
        break;
    case 11:
        nce_->Reset();
        break;
    case 12:
        nps_->Reset();
        break;
    case 13:
        nrv_->Reset();
        break;
#if NEUROTIC_ENABLE_EUDELAY
    case 14:
        ned_->Reset();
        break;
#endif
    case 15:
        nwl_->Reset();
        break;
    case 16:
        nws_->Reset();
        break;
    case 17:
        nwg_->Reset();
        break;
    case 18:
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
        nes_->Process(inL, inR, rt, outL, outR);
        break;
    case 7:
        nhc_->Process(inL, inR, rt, outL, outR);
        break;
    case 8:
        npl_->Process(inL, inR, rt, outL, outR);
        break;
    case 9:
        nmg_->Process(inL, inR, rt, outL, outR);
        break;
    case 10:
        nsm_->Process(inL, inR, rt, outL, outR);
        break;
    case 11:
        nce_->Process(inL, inR, rt, outL, outR);
        break;
    case 12:
        nps_->Process(inL, inR, rt, outL, outR);
        break;
    case 13:
        nrv_->Process(inL, inR, rt, outL, outR);
        break;
#if NEUROTIC_ENABLE_EUDELAY
    case 14:
        ned_->Process(inL, inR, rt, outL, outR);
        break;
#endif
    case 15:
        nwl_->Process(inL, inR, rt, outL, outR);
        break;
    case 16:
        nws_->Process(inL, inR, rt, outL, outR);
        break;
    case 17:
        nwg_->Process(inL, inR, rt, outL, outR);
        break;
    case 18:
        nbz_->Process(inL, inR, rt, outL, outR);
        break;
    default:
        outL = inL;
        outR = inR;
        break;
    }
}
