#pragma once

#include <algorithm>
#include <cstddef>
#include <cmath>

#include "daisysp.h"
#include "distortion.h"
#include "filters.h" // SoftClipSample
#include "modal_bodies.h"

enum class ResonatorModel
{
    Delay = 0,
    ModalBank = 1,
    SympString = 2,
    FMRes = 3,
    /* Struck bodies, all served by ModalBodyVoice. This run must stay
     * contiguous and in the same order as res::kBodies. */
    Beam = 4,
    Marimba = 5,
    Drumhead = 6,
    Membrane = 7,
    Plate = 8,
};

constexpr int kNumResonatorModels = 9;
constexpr int kFirstBodyModel = static_cast<int>(ResonatorModel::Beam);

inline bool IsBodyModel(ResonatorModel m)
{
    const int i = static_cast<int>(m);
    return i >= kFirstBodyModel && i < kNumResonatorModels;
}

/* Makeup applied to the resonator's contribution to the OUTPUT MIX only —
 * never to what returns through the feed matrix. The two have to be separate:
 * a model's output gain sits inside its own feedback loop, so raising it to a
 * usable level lowers the drone threshold by exactly the same factor. SympString
 * measured 17 dB below the other models at the feed that let it ring, and every
 * attempt to fix that in the model itself just moved the oscillation point. */
inline float ModelOutGain(ResonatorModel m)
{
    return (m == ResonatorModel::SympString) ? 7.0f : 1.0f;
}

/* How much of Pot 2's feedback each model can take before it stops resonating
 * and starts oscillating. A plain delay line has no regeneration of its own and
 * needs the full amount; everything else already rings internally, so the same
 * feed that makes Delay sing makes them drone. Measured by sweeping feed
 * against ring time — see host_dsp/loop_fixture.cpp. */
inline float ModelFeedScale(ResonatorModel m)
{
    switch(m)
    {
    case ResonatorModel::Delay:
        return 1.0f; // no internal feedback; the matrix *is* its resonance
    case ResonatorModel::SympString:
        /* Its strings already regenerate, and Taps raises their own feedback
         * too, so the worst case is Taps 5 — where the headroom sweep puts the
         * limit at 0.14. The old 0.22 was measured when feed came from a stored
         * setting that defaulted to 0.80; Pot 2 reaches 0.99 at any moment. */
        return 0.10f;
    case ResonatorModel::FMRes:
        return 0.13f; // sweep says 0.20; self-oscillates just past it
    case ResonatorModel::ModalBank:
        /* Genuinely zero, and re-measured to confirm it: at ringing Q the bank
         * sustains on the smallest loop the sweep can resolve. Pot 2 is not
         * dead here — ModalBankModel reads params.feed as damping instead, so
         * the knob shortens and lengthens the ring rather than regenerating. */
        return 0.0f;
    default:
        /* Struck bodies, per body. These were a flat zero, measured back when
         * feedback was a stored setting that defaulted high; re-measured at the
         * real worst case (Ring 1.0, Taps 5) they take 0.24 to 0.70. */
        return res::kBodies[static_cast<int>(m) - kFirstBodyModel].feedMax;
    }
}

/* Knob taper for the feedback control.
 *
 * Ring time goes as 1/(1-g), so a linear knob is almost entirely dead: measured
 * on the Delay model, three quarters of the travel sat at a 55 ms tail and the
 * whole audible range was crammed into the last few percent. That is why Pot 2
 * read as having no effect at all — it was working, just not anywhere you would
 * find by turning it.
 *
 * 1-exp(-a*k), normalised so full travel still maps to exactly 1.0, spends the
 * knob where the ring actually changes. a = 8 was picked by measurement: the
 * Delay tail then runs 0.055, 0.105, 0.305, 0.655, 0.955, 1.155 s across the
 * sweep — monotonic, roughly doubling, and never running away.
 *
 * Normalising to 1.0 matters for safety as well as feel: every per-model limit
 * below was measured at full knob, and a taper that overshot would invalidate
 * all of them. */
inline float FeedTaper(float knob)
{
    constexpr float kA = 8.0f;
    const float k = std::clamp(std::fabs(knob), 0.0f, 1.0f);
    const float c = (1.0f - std::exp(-kA * k)) / (1.0f - std::exp(-kA));
    return (knob < 0.0f) ? -c : c;
}

struct ModelProcessParams
{
    float freqX = 440.0f;
    float freqY = 440.0f;
    float ratio = 1.0f;
    int size = 1;
    int taps = 1;
    float decay = 0.5f;
    /* Pot 2 + CV 2, signed, BEFORE ModelFeedScale is applied. Models that can
     * carry a feedback loop see it through that scale; Modal cannot carry one
     * at all, so it reads this directly as damping. Every model has to do
     * something with Pot 2 — an inert knob is worse than a subtle one. */
    float feed = 0.0f;
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

        /* Two independent banks, one per channel. They used to share a mono sum
         * of both inputs and then split odd and even modes between the outputs,
         * so whatever arrived on Y came back out of X — a cross-feed with no
         * control to turn it off, and audible as the two channels collapsing
         * into each other. modeR_ is already detuned slightly against modeL_,
         * so separate banks still give two distinguishable voices, and X now
         * only ever hears X. */
        float sumX = 0.0f;
        float sumY = 0.0f;
        for (int i = 0; i < kModes; ++i)
        {
            modeL_[i].Process(inX);
            modeR_[i].Process(inY);
            sumX += modeL_[i].Band() * gain_[i];
            sumY += modeR_[i].Band() * gain_[i];
        }

        /* Half the old gain: each output now carries the whole bank rather than
         * half its modes. */
        outX = SoftClipSample(sumX * 20.0f);
        outY = SoftClipSample(sumY * 20.0f);
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
        const float sizeDiv = 1.0f + size * 3.0f;
        const float rootX = std::clamp(p.freqX / sizeDiv, 20.0f, sampleRate_ * 0.18f);
        const float rootY = std::clamp(p.freqY / sizeDiv, 20.0f, sampleRate_ * 0.18f);
        const float stiffness = -0.025f + HotEnd(structure) * 0.13f;
        /* Svf::SetRes takes a NORMALISED resonance and clamps to [0,1]. This
         * used to pass a Q-like 8..52, so every mode sat pinned at maximum
         * resonance: `density` did nothing, and with any feed at all the bank
         * self-oscillated into a permanent drone. Mapped into range, the modes
         * ring and decay, and `density` finally does something. The ceiling
         * matters: measured on the host, the bank self-oscillates on its own
         * above about 0.985, so Taps at maximum must still land clear of it. */
        const float qBase = 0.930f + density * 0.030f;
        /* Pot 2 as damping. The bank cannot carry a feedback loop — see
         * ModelFeedScale — so the knob moves mode Q instead, which is the same
         * musical gesture: clockwise rings longer, anticlockwise damps. The
         * range is deliberately asymmetric. Upwards there is very little room
         * before the measured 0.985 self-oscillation ceiling; downwards there
         * is all the room in the world, and damping is what makes the bank
         * usable when it is too lively. */
        const float damp = std::clamp(p.feed, -1.0f, 1.0f);
        const float qFeed = qBase + (damp > 0.0f ? damp * 0.045f : damp * 0.230f);
        const float bright = 1.0f - size * 0.55f;
        float stretch = 1.0f;

        for (int i = 0; i < kModes; ++i)
        {
            const float harmonic = static_cast<float>(i + 1);
            const float hi = static_cast<float>(i) / static_cast<float>(kModes - 1);
            const float freqX = std::clamp(rootX * harmonic * stretch, 30.0f, sampleRate_ * 0.42f);
            const float freqY = std::clamp(rootY * harmonic * stretch, 30.0f, sampleRate_ * 0.42f);
            const float ampTilt = std::pow(1.0f - hi * 0.84f, 1.0f + (1.0f - bright) * 3.0f);
            const float pickup = 0.5f - 0.5f * std::cos(3.14159265358979323846f * harmonic * (0.18f + density * 0.68f));
            gain_[i] = ampTilt * (0.25f + 0.75f * pickup);

            /* The two banks are tuned independently — Ofst sets how far apart —
             * so they are two instruments, not one heard from two places. */
            modeL_[i].SetFreq(freqX);
            modeR_[i].SetFreq(freqY);
            /* Upper modes damp faster, as in a real body. Subtractive, not
             * multiplicative: near 1.0 the Svf's damping is fiercely
             * nonlinear, and scaling would kill the top modes outright. */
            const float res = std::clamp(qFeed - hi * 0.05f, 0.30f, 0.982f);
            modeL_[i].SetRes(res);
            modeR_[i].SetRes(res);

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
        for (int c = 0; c < kChannels; ++c)
        {
            for (int s = 0; s < kStrings; ++s)
            {
                std::fill(&string_[c][s][0], &string_[c][s][kDelay], 0.0f);
                write_[c][s] = 0;
                lp_[c][s] = 0.0f;
            }
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

        /* Two string sets, one per channel. They used to be a single set
         * excited by a mono sum of both inputs and then panned across the two
         * outputs, which meant whatever was patched to IN 2 came out of OUT 1.
         * Tuning is shared; only the delay lines and their state are per
         * channel. The pan weights are kept, one set per channel, so the two
         * still voice differently rather than being identical. */
        const float in[kChannels] = {inX, inY};
        const float *pan[kChannels] = {panX_, panY_};
        float sum[kChannels] = {0.0f, 0.0f};

        for (int c = 0; c < kChannels; ++c)
        {
            for (int s = 0; s < kStrings; ++s)
            {
                const float delayed = Read(c, s, delay_[c][s]);
                lp_[c][s] += (delayed - lp_[c][s]) * dampCoef_[s];
                const float next
                    = SoftClipSample(in[c] * excite_[s] + lp_[c][s] * feedback_[s]);
                string_[c][s][write_[c][s]] = next;
                write_[c][s] = (write_[c][s] + 1) % kDelay;

                sum[c] += delayed * level_[s] * pan[c][s];
            }
        }

        outX = SoftClipSample(sum[0] * 2.3f);
        outY = SoftClipSample(sum[1] * 2.3f);
    }

private:
    static constexpr int kStrings = 3;
    static constexpr int kChannels = 2;
    static constexpr size_t kDelay = 1024;
    static constexpr int kControlInterval = 16;

    float Read(int c, int s, float delaySamples) const
    {
        const float d = std::clamp(delaySamples, 2.0f, static_cast<float>(kDelay - 2));
        float read = static_cast<float>(write_[c][s]) - d;
        while (read < 0.0f)
            read += static_cast<float>(kDelay);
        const size_t i0 = static_cast<size_t>(read);
        const size_t i1 = (i0 + 1) % kDelay;
        const float frac = read - static_cast<float>(i0);
        return string_[c][s][i0] + (string_[c][s][i1] - string_[c][s][i0]) * frac;
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
        const float sizeDiv = 1.0f + size * 2.5f;
        /* One root per channel. The chord shape is shared — that is what Ratio
         * selects — but the two sets are tuned apart by Ofst, so they are two
         * instruments playing the same voicing rather than one panned across
         * both outputs. */
        const float root[kChannels]
            = {std::clamp(p.freqX / sizeDiv, 48.0f, sampleRate_ * 0.18f),
               std::clamp(p.freqY / sizeDiv, 48.0f, sampleRate_ * 0.18f)};

        for (int s = 0; s < kStrings; ++s)
        {
            const int nextChord = std::min(chord + 1, 2);
            const float semi = kIntervals[chord][s] + (kIntervals[nextChord][s] - kIntervals[chord][s]) * chordBlend;
            const float interval = std::pow(2.0f, semi / 12.0f);
            for (int c = 0; c < kChannels; ++c)
            {
                const float freq = std::clamp(root[c] * interval, 48.0f, sampleRate_ * 0.20f);
                delay_[c][s] = sampleRate_ / freq;
            }
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
    float string_[kChannels][kStrings][kDelay];
    size_t write_[kChannels][kStrings];
    float lp_[kChannels][kStrings];
    float delay_[kChannels][kStrings];
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
        vx_ = Voice_{};
        vy_ = Voice_{};
        envX_ = 0.0f;
        envY_ = 0.0f;
        lpX_ = 0.0f;
        lpY_ = 0.0f;
    }

    void Process(float inX, float inY, const ModelProcessParams &p, float &outX, float &outY)
    {
        const float ratio = std::clamp(p.ratio, 0.25f, 4.0f);
        const float bright = std::clamp(static_cast<float>(p.taps - 1) / 4.0f, 0.0f, 1.0f);
        const float damping = std::clamp(static_cast<float>(p.size - 1) / 7.0f, 0.0f, 1.0f);
        /* One envelope per channel, each following its own input. They used to
         * share a mono sum of both, so a signal on Y opened X's envelope and
         * the two outputs rose and fell together no matter what was patched.
         *
         * The envelope used to be floored at 0.04..0.20 as well, so the carrier
         * never stopped sounding and the model was a drone rather than
         * something you strike. Tracking the input all the way to zero makes it
         * respond to an input signal or a MIDI-fired mallet and then fall
         * silent. */
        const float targetX = std::min(1.0f, std::fabs(inX) * 4.0f);
        const float targetY = std::min(1.0f, std::fabs(inY) * 4.0f);
        const float attack = 0.01f + bright * 0.08f;
        const float release = 0.0004f + damping * damping * 0.02f;
        envX_ += (targetX - envX_) * (targetX > envX_ ? attack : release);
        envY_ += (targetY - envY_) * (targetY > envY_ ? attack : release);

        /* A carrier/modulator pair per channel, each on its own pitch. There
         * used to be one pair, so both outputs carried the same oscillator and
         * Ofst could not separate them. Ratio still sets the C:M relationship
         * for both — that is the timbre, and it stays shared. */
        const float feedback = (ratio - 1.0f) * 0.35f;
        const float cutoff = 180.0f + (1.0f - damping) * (16000.0f - 180.0f);

        outX = SoftClipSample(
            OnePole(Voice(vx_, p.freqX, ratio, feedback, bright, envX_, inX), cutoff, lpX_)
            * 1.35f);
        outY = SoftClipSample(
            OnePole(Voice(vy_, p.freqY, ratio, feedback, bright, envY_, inY), cutoff * 1.07f,
                    lpY_)
            * 1.35f);
    }

private:
    static constexpr float kTwoPi = 6.28318530717958647692f;

    struct Voice_
    {
        float carrierPhase = 0.0f;
        float modPhase = 0.0f;
        float prev = 0.0f;
    };

    float Voice(Voice_ &v, float pitch, float ratio, float feedback, float bright,
                float env, float in) const
    {
        const float carrierFreq = std::clamp(pitch, 20.0f, sampleRate_ * 0.35f);
        const float modFreq = std::min(carrierFreq * ratio, sampleRate_ * 0.45f);
        v.modPhase += modFreq / sampleRate_;
        v.carrierPhase += carrierFreq / sampleRate_;
        if (v.modPhase >= 1.0f)
            v.modPhase -= 1.0f;
        if (v.carrierPhase >= 1.0f)
            v.carrierPhase -= 1.0f;

        const float mod
            = std::sin(kTwoPi * (v.modPhase + std::max(0.0f, -feedback) * v.prev * 0.25f));
        const float index = (0.25f + bright * 7.0f) * (0.35f + env * 1.65f);
        const float carrier = std::sin(kTwoPi * v.carrierPhase + mod * index
                                       + std::max(0.0f, feedback) * v.prev * 1.6f);
        v.prev += (carrier - v.prev) * 0.18f;
        return (carrier + mod * 0.35f) * env + in * 0.20f;
    }

    float OnePole(float x, float cutoffHz, float &state) const
    {
        const float alpha = std::clamp(cutoffHz / (cutoffHz + sampleRate_), 0.0f, 1.0f);
        state += (x - state) * alpha;
        return state;
    }

    float sampleRate_ = 48000.0f;
    Voice_ vx_;
    Voice_ vy_;
    float envX_ = 0.0f;
    float envY_ = 0.0f;
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
        body_.Init(sampleRate);
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
        case ResonatorModel::Beam:
        case ResonatorModel::Marimba:
        case ResonatorModel::Drumhead:
        case ResonatorModel::Membrane:
        case ResonatorModel::Plate:
        {
            const res::BodySpec &spec
                = res::kBodies[static_cast<int>(model) - kFirstBodyModel];
            body_.SetParams(spec, params.freqX, params.freqY, params.size, params.taps,
                            params.ratio, params.decay);
            body_.Process(inX, inY, outX, outY);
            outX = SoftClipSample(outX);
            outY = SoftClipSample(outY);
            break;
        }
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
        case ResonatorModel::Beam:
        case ResonatorModel::Marimba:
        case ResonatorModel::Drumhead:
        case ResonatorModel::Membrane:
        case ResonatorModel::Plate:
            body_.Reset();
            break;
        case ResonatorModel::Delay:
        default:
            break;
        }
    }

    ModalBankModel modal_;
    SympStringModel strings_;
    FMResModel fm_;
    res::ModalBodyVoice body_;
    ResonatorModel lastModel_ = ResonatorModel::Delay;
};
