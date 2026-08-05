#pragma once

#include <algorithm>
#include <cstddef>
#include <cmath>

#include "daisysp.h"
#include "distortion.h"

enum class ResonatorModel
{
    Delay = 0,
    ModalBank = 1,
    SympString = 2,
    FMRes = 3,
};

struct ModelProcessParams
{
    float freqX = 440.0f;
    float freqY = 440.0f;
    float ratio = 1.0f;
    int size = 1;
    int taps = 1;
};

class ModalBankModel
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

    void Process(float inX, float inY, const ModelProcessParams &p, float &outX, float &outY)
    {
        if (controlCounter_ <= 0)
        {
            UpdateControls(p);
            controlCounter_ = kControlInterval;
        }
        --controlCounter_;

        const float input = 0.5f * (inX + inY);
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

        outX = SoftClipSample(odd * 2.0f);
        outY = SoftClipSample(even * 2.0f);
    }

private:
    static constexpr int kModes = 12;
    static constexpr int kControlInterval = 16;

    static float HotEnd(float v)
    {
        v = std::clamp(v, 0.0f, 1.0f);
        return v * v * (3.0f - 2.0f * v);
    }

    void UpdateControls(const ModelProcessParams &p)
    {
        const float size = std::clamp(static_cast<float>(p.size - 1) / 7.0f, 0.0f, 1.0f);
        const float density = std::clamp(static_cast<float>(p.taps - 1) / 4.0f, 0.0f, 1.0f);
        const float structure = std::clamp((std::log2(std::clamp(p.ratio, 0.25f, 4.0f)) + 2.0f) * 0.25f, 0.0f, 1.0f);
        const float root = std::clamp(p.freqX / (1.0f + size * 3.0f), 20.0f, sampleRate_ * 0.18f);
        const float stiffness = -0.025f + HotEnd(structure) * 0.13f;
        const float qBase = 8.0f + density * 44.0f;
        const float bright = 1.0f - size * 0.55f;
        float stretch = 1.0f;

        for (int i = 0; i < kModes; ++i)
        {
            const float harmonic = static_cast<float>(i + 1);
            const float hi = static_cast<float>(i) / static_cast<float>(kModes - 1);
            const float freq = std::clamp(root * harmonic * stretch, 30.0f, sampleRate_ * 0.42f);
            const float ampTilt = std::pow(1.0f - hi * 0.84f, 1.0f + (1.0f - bright) * 3.0f);
            const float pickup = 0.5f - 0.5f * std::cos(3.14159265358979323846f * harmonic * (0.18f + density * 0.68f));
            gain_[i] = ampTilt * (0.25f + 0.75f * pickup);

            modeL_[i].SetFreq(freq);
            modeR_[i].SetFreq(std::clamp(freq * (1.0f + 0.002f * static_cast<float>((i & 3) - 1)), 30.0f, sampleRate_ * 0.42f));
            modeL_[i].SetRes(qBase * (1.0f - hi * 0.42f));
            modeR_[i].SetRes(qBase * (1.0f - hi * 0.42f));

            stretch += stiffness;
            stretch += stiffness * hi * (stiffness > 0.0f ? 0.25f : 0.08f);
        }
    }

    float sampleRate_ = 48000.0f;
    daisysp::Svf modeL_[kModes];
    daisysp::Svf modeR_[kModes];
    float gain_[kModes];
    int controlCounter_ = 0;
};

class SympStringModel
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

    void Process(float inX, float inY, const ModelProcessParams &p, float &outX, float &outY)
    {
        if (controlCounter_ <= 0)
        {
            UpdateControls(p);
            controlCounter_ = kControlInterval;
        }
        --controlCounter_;

        const float input = 0.5f * (inX + inY);
        float sumX = 0.0f;
        float sumY = 0.0f;
        for (int s = 0; s < kStrings; ++s)
        {
            const float delayed = Read(s, delay_[s]);
            lp_[s] += (delayed - lp_[s]) * dampCoef_[s];
            const float next = SoftClipSample(input * excite_[s] + lp_[s] * feedback_[s]);
            string_[s][write_[s]] = next;
            write_[s] = (write_[s] + 1) % kDelay;

            const float voice = delayed * level_[s];
            sumX += voice * panX_[s];
            sumY += voice * panY_[s];
        }

        outX = SoftClipSample(sumX * 2.3f);
        outY = SoftClipSample(sumY * 2.3f);
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

    void UpdateControls(const ModelProcessParams &p)
    {
        static constexpr float kIntervals[3][kStrings] = {
            {0.0f, 7.01955f, 12.0f},
            {0.0f, 3.0f, 10.0f},
            {0.0f, 12.0f, 24.0f},
        };
        const float spread = std::clamp((std::log2(std::clamp(p.ratio, 0.25f, 4.0f)) + 2.0f) * 0.25f, 0.0f, 1.0f);
        const float size = std::clamp(static_cast<float>(p.size - 1) / 7.0f, 0.0f, 1.0f);
        const float bright = std::clamp(static_cast<float>(p.taps - 1) / 4.0f, 0.0f, 1.0f);
        const int chord = std::clamp(static_cast<int>(spread * 2.999f), 0, 2);
        const float chordBlend = spread * 2.999f - static_cast<float>(chord);
        const float root = std::clamp(p.freqX / (1.0f + size * 2.5f), 48.0f, sampleRate_ * 0.18f);

        for (int s = 0; s < kStrings; ++s)
        {
            const int nextChord = std::min(chord + 1, 2);
            const float semi = kIntervals[chord][s] + (kIntervals[nextChord][s] - kIntervals[chord][s]) * chordBlend;
            const float freq = std::clamp(root * std::pow(2.0f, semi / 12.0f), 48.0f, sampleRate_ * 0.20f);
            delay_[s] = sampleRate_ / freq;
            feedback_[s] = std::clamp(0.74f + size * 0.18f + bright * 0.05f - static_cast<float>(s) * 0.018f, 0.45f, 0.975f);
            dampCoef_[s] = 0.04f + bright * bright * 0.42f;
            excite_[s] = (s == 0) ? 0.42f : (0.14f + bright * 0.20f);
            level_[s] = 1.0f / std::sqrt(static_cast<float>(s + 1));
            const float pan = static_cast<float>(s) / static_cast<float>(kStrings - 1);
            panX_[s] = std::sqrt(1.0f - pan);
            panY_[s] = std::sqrt(pan);
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
    float panX_[kStrings];
    float panY_[kStrings];
    int controlCounter_ = 0;
};

class FMResModel
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
        envX_ = 0.0f;
        envY_ = 0.0f;
        prev_ = 0.0f;
        lpX_ = 0.0f;
        lpY_ = 0.0f;
    }

    void Process(float inX, float inY, const ModelProcessParams &p, float &outX, float &outY)
    {
        const float ratio = std::clamp(p.ratio, 0.25f, 4.0f);
        const float bright = std::clamp(static_cast<float>(p.taps - 1) / 4.0f, 0.0f, 1.0f);
        const float damping = std::clamp(static_cast<float>(p.size - 1) / 7.0f, 0.0f, 1.0f);
        const float inputEnv = std::min(1.0f, std::fabs(0.5f * (inX + inY)) * 4.0f);
        const float target = std::max(inputEnv, 0.04f + damping * 0.16f);
        const float attack = 0.01f + bright * 0.08f;
        const float release = 0.0004f + damping * damping * 0.02f;
        envX_ += (target - envX_) * (target > envX_ ? attack : release);
        envY_ += (target - envY_) * (target > envY_ ? attack : release);

        const float carrierFreq = std::clamp(p.freqX, 20.0f, sampleRate_ * 0.35f);
        const float modFreq = std::min(carrierFreq * ratio, sampleRate_ * 0.45f);
        modPhase_ += modFreq / sampleRate_;
        carrierPhase_ += carrierFreq / sampleRate_;
        if (modPhase_ >= 1.0f)
            modPhase_ -= 1.0f;
        if (carrierPhase_ >= 1.0f)
            carrierPhase_ -= 1.0f;

        const float feedback = (ratio - 1.0f) * 0.35f;
        const float mod = std::sin(kTwoPi * (modPhase_ + std::max(0.0f, -feedback) * prev_ * 0.25f));
        const float index = (0.25f + bright * 7.0f) * (0.35f + envX_ * 1.65f);
        const float carrier = std::sin(kTwoPi * carrierPhase_ + mod * index + std::max(0.0f, feedback) * prev_ * 1.6f);
        prev_ += (carrier - prev_) * 0.18f;

        const float cutoff = 180.0f + (1.0f - damping) * (16000.0f - 180.0f);
        const float left = (carrier + mod * 0.35f) * envX_ + inX * 0.20f;
        const float right = (carrier - mod * 0.35f) * envY_ + inY * 0.20f;
        outX = SoftClipSample(OnePole(left, cutoff, lpX_) * 1.35f);
        outY = SoftClipSample(OnePole(right, cutoff * 1.07f, lpY_) * 1.35f);
    }

private:
    static constexpr float kTwoPi = 6.28318530717958647692f;

    float OnePole(float x, float cutoffHz, float &state) const
    {
        const float alpha = std::clamp(cutoffHz / (cutoffHz + sampleRate_), 0.0f, 1.0f);
        state += (x - state) * alpha;
        return state;
    }

    float sampleRate_ = 48000.0f;
    float carrierPhase_ = 0.0f;
    float modPhase_ = 0.0f;
    float envX_ = 0.0f;
    float envY_ = 0.0f;
    float prev_ = 0.0f;
    float lpX_ = 0.0f;
    float lpY_ = 0.0f;
};

class RingsStyleModels
{
public:
    void Init(float sampleRate)
    {
        modal_.Init(sampleRate);
        strings_.Init(sampleRate);
        fm_.Init(sampleRate);
        lastModel_ = ResonatorModel::Delay;
    }

    void Process(ResonatorModel model, float inX, float inY, const ModelProcessParams &params, float &outX, float &outY)
    {
        if (model != lastModel_)
        {
            Reset(model);
            lastModel_ = model;
        }

        switch (model)
        {
        case ResonatorModel::ModalBank:
            modal_.Process(inX, inY, params, outX, outY);
            break;
        case ResonatorModel::SympString:
            strings_.Process(inX, inY, params, outX, outY);
            break;
        case ResonatorModel::FMRes:
            fm_.Process(inX, inY, params, outX, outY);
            break;
        case ResonatorModel::Delay:
        default:
            outX = inX;
            outY = inY;
            break;
        }
    }

private:
    void Reset(ResonatorModel model)
    {
        switch (model)
        {
        case ResonatorModel::ModalBank:
            modal_.Reset();
            break;
        case ResonatorModel::SympString:
            strings_.Reset();
            break;
        case ResonatorModel::FMRes:
            fm_.Reset();
            break;
        case ResonatorModel::Delay:
        default:
            break;
        }
    }

    ModalBankModel modal_;
    SympStringModel strings_;
    FMResModel fm_;
    ResonatorModel lastModel_ = ResonatorModel::Delay;
};
