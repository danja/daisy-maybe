#include "neurotic_algos.h"

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

float MapExpo(float value, float minVal, float maxVal)
{
    value = Clamp01(value);
    return minVal * std::pow(maxVal / minVal, value);
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
        const float mass = rt.c1;
        const float tension = rt.c2;
        const float damping = rt.c3;
        const float asym = rt.c4;

        const float base = MapExpo(tension, 60.0f, 2400.0f);
        const float spread = 1.0f + mass * 2.5f;
        const float q = 0.8f + (1.0f - damping) * 8.0f;

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

        const float depth = std::clamp(rt.c1 * 1.6f, 0.0f, 1.0f);
        const float formant = std::clamp(rt.c2 * 1.4f, 0.0f, 1.0f);
        const float transient = rt.c3;
        const float weave = std::clamp(rt.c4 * 1.6f, 0.0f, 1.0f);

        const float protect = std::clamp(transient * 0.6f + 0.2f, 0.0f, 1.0f);
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

            const float braid = depth;
            const float formMix = (k < formantBin) ? weave : braid;
            const float magLNew = magL * (1.0f - formMix) + magR * formMix;
            const float magRNew = magR * (1.0f - formMix) + magL * formMix;
            const float phaseLNew = phaseL + ShortestPhaseDelta(phaseL, phaseR) * braid * protect * 1.6f;
            const float phaseRNew = phaseR + ShortestPhaseDelta(phaseR, phaseL) * braid * protect * 1.6f;

            s_spectral.re[0][k] = magLNew * std::cos(phaseLNew);
            s_spectral.im[0][k] = magLNew * std::sin(phaseLNew);
            s_spectral.re[1][k] = magRNew * std::cos(phaseRNew);
            s_spectral.im[1][k] = magRNew * std::sin(phaseRNew);
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
        const float gapCut = MapExpo(1.0f - headGap, 200.0f, 9000.0f);
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
        const float spin = rt.c4 + rt.lfoValue * rt.lfoDepth * 0.5f;

        phase_ += (0.2f + spin * 2.0f) / sampleRate_;
        if (phase_ > 1.0f)
            phase_ -= 1.0f;
        const float spinPan = std::sin(phase_ * kTwoPi) * spin;

        const float pan = std::clamp(az + spinPan, -1.0f, 1.0f);
        const float itd = std::fabs(pan) * (10.0f + dist * 20.0f);
        const float leftGain = 0.5f * (1.0f - pan);
        const float rightGain = 0.5f * (1.0f + pan);

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

        const float cutoff = MapExpo(1.0f - dist, 300.0f, 12000.0f);
        const float mono = 0.5f * (l + r);
        const float distant = OnePoleProcess(mono, cutoff, sampleRate_, lpState_);

        outL = (distant + (l - distant) * elev) * leftGain * 2.0f;
        outR = (distant + (r - distant) * elev) * rightGain * 2.0f;
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
    }

    void Reset()
    {
        for (int i = 0; i < 3; ++i)
        {
            formL_[i].Init(sampleRate_);
            formR_[i].Init(sampleRate_);
        }
    }

    void Process(float inL, float inR, const NeuroticRuntime &rt, float &outL, float &outR)
    {
        const float vowel = rt.c1;
        const float art = rt.c2;
        const float breath = rt.c3;
        const float split = rt.c4;

        const float base = MapExpo(vowel, 200.0f, 1000.0f);
        const float spread = 1.4f + art * 1.5f;
        const float f1 = base;
        const float f2 = base * spread;
        const float f3 = base * (spread + 0.8f);

        formL_[0].SetFreq(f1);
        formL_[1].SetFreq(f2);
        formL_[2].SetFreq(f3);

        const float splitMul = 1.0f + split * 0.3f;
        formR_[0].SetFreq(f1 * splitMul);
        formR_[1].SetFreq(f2 * splitMul);
        formR_[2].SetFreq(f3 * splitMul);

        const float noise = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * breath * 0.1f;
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

        outL = sumL * 0.6f;
        outR = sumR * 0.6f;
    }

private:
    float sampleRate_ = 48000.0f;
    daisysp::Svf formL_[3];
    daisysp::Svf formR_[3];
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
    void Init(float sampleRate) { sampleRate_ = sampleRate; }
    void Reset() { envL_ = 0.0f; envR_ = 0.0f; }

    void Process(float inL, float inR, const NeuroticRuntime &rt, float &outL, float &outR)
    {
        const float punch = std::clamp(rt.c1 * 1.5f, 0.0f, 1.0f);
        const float glue = std::clamp(rt.c2 * 1.3f, 0.0f, 1.0f);
        const float lift = std::clamp(rt.c3 * 1.4f, 0.0f, 1.0f);
        const float bias = rt.c4;

        const float attack = 0.002f + (1.0f - punch) * 0.02f;
        const float release = 0.01f + (1.0f - punch) * 0.12f;

        envL_ += (std::fabs(inL) - envL_) * attack;
        envR_ += (std::fabs(inR) - envR_) * attack;
        envL_ += (std::fabs(inL) - envL_) * release;
        envR_ += (std::fabs(inR) - envR_) * release;

        const float env = Lerp(envL_, envR_, glue);
        const float comp = 1.0f / (1.0f + env * (1.0f + punch * 10.0f));
        const float liftGain = 1.0f + lift * 1.0f;

        const float mixL = Lerp(inL, inR, bias);
        const float mixR = Lerp(inR, inL, bias);
        outL = mixL * comp * liftGain;
        outR = mixR * comp * liftGain;
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

        const float scale = 0.5f + stretch * 1.5f;
        const float inharm = inh * 0.4f;

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
            const int width = std::max(1, period / 8);
            const int slot = static_cast<int>(k) % period;
            const float gate = (slot < width) ? 1.0f : 0.05f;
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
                s_spectral.re[0][mirrorBin] += s_spectral.re[0][k] * mirror;
                s_spectral.im[0][mirrorBin] -= s_spectral.im[0][k] * mirror;
                s_spectral.re[1][mirrorBin] += s_spectral.re[1][k] * mirror;
                s_spectral.im[1][mirrorBin] -= s_spectral.im[1][k] * mirror;
            }
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

        const float bind = rt.c1;
        const float swirl = std::clamp(rt.c2 * 1.6f + rt.lfoValue * rt.lfoDepth * 0.4f, 0.0f, 1.0f);
        const float tilt = std::clamp(rt.c3 * 1.6f, 0.0f, 1.0f);
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

            const float warp = (static_cast<float>(k) / static_cast<float>(kBins)) * tilt;
        const float swirlPhase = std::sin(k * 0.03f) * swirl * 1.2f;

            const float phaseLNew = phaseL + swirlPhase + warp;
            const float phaseRNew = phaseR - swirlPhase - warp;

            const float linkL = phaseLNew + ShortestPhaseDelta(phaseLNew, phaseRNew) * bind;
            const float linkR = phaseRNew + ShortestPhaseDelta(phaseRNew, phaseLNew) * bind;

            s_spectral.re[0][k] = magL * std::cos(linkL);
            s_spectral.im[0][k] = magL * std::sin(linkL);
            s_spectral.re[1][k] = magR * std::cos(linkR);
            s_spectral.im[1][k] = magR * std::sin(linkR);
        }

        const float widen = 1.0f + stereo * 0.5f;
        s_spectral.re[0][0] *= widen;
        s_spectral.re[1][0] *= 1.0f - stereo * 0.3f;

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
            holdSampleL_ = s_delayA.Read(10.0f + scatter * 80.0f);
            holdSampleR_ = s_delayB.Read(10.0f + (1.0f - scatter) * 80.0f);
            holdWindow_ = 0.0f;
        }
        hold_--;

        s_delayA.Write(inL);
        s_delayB.Write(inR);

        holdWindow_ += 1.0f / std::max(1, grainSize);
        const float win = 0.5f - 0.5f * std::cos(kTwoPi * std::clamp(holdWindow_, 0.0f, 1.0f));

        const float driftAmt = (static_cast<float>(rand()) / RAND_MAX - 0.5f) * drift * 0.2f;
        const float grainL = holdSampleL_ * win + driftAmt;
        const float grainR = holdSampleR_ * win - driftAmt;

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

void NeuroticAlgoBank::Init(float sampleRate)
{
    sampleRate_ = sampleRate;
    s_spectral.Init();
    s_delayA.Reset();
    s_delayB.Reset();
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
    default:
        outL = inL;
        outR = inR;
        break;
    }
}
