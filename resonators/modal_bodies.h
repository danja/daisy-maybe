/* modal_bodies.h — table-driven modal resonators for struck/stretched bodies.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Beam, marimba, drumhead, membrane and plate differ in three things and
 * nothing else: where their partials sit, how fast each partial dies, and how
 * hard a strike excites the upper ones. So they share one engine and differ
 * only by a BodySpec table.
 *
 * Each partial is a two-pole resonator
 *     y[n] = a0*x[n] + 2*r*cos(w)*y[n-1] - r^2*y[n-2]
 * whose impulse response is r^n*sin(w*n): `w` places the partial and `r` sets
 * its decay directly in seconds, which is exactly the axis these bodies differ
 * on. (The older ModalBankModel uses Svf/Q instead, where decay is a derived
 * quantity and much harder to tune per body.)
 *
 * Stereo comes from two pickup positions weighting the *same* mode outputs,
 * so the filter cost does not double. */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace res
{

/* Partial ratios are relative to the fundamental. Sources: Fletcher & Rossing,
 * "The Physics of Musical Instruments". */
namespace bodies
{
/* Free-free bar, Euler-Bernoulli: f_n proportional to (2n+1)^2. Very
 * inharmonic and very ringy — the classic struck-metal-bar tone. */
constexpr float kBeam[] = {1.0f,   2.7565f, 5.4039f, 8.9330f, 13.3443f, 18.6379f,
                           24.8156f, 31.8771f, 39.8225f, 48.6519f, 58.3652f, 68.9624f};

/* Bar with the arch undercut so partial 2 tunes to 4x and partial 3 to 10x —
 * that deliberate tuning is what makes a marimba pitched where a plain bar is
 * not. Above the tuned partials it returns to the bar law. */
constexpr float kMarimba[] = {1.0f,  4.0f,  10.0f, 19.6f, 32.4f, 48.4f,
                              67.6f, 90.0f, 115.6f, 144.4f, 176.4f, 211.6f};

/* Timpani: the kettle's air load pulls the membrane modes into a near-harmonic
 * 1:1.5:2:2.44:2.9 series, which is why a drumhead has definite pitch. */
constexpr float kDrumhead[] = {1.0f, 1.50f, 2.00f, 2.44f, 2.90f, 3.60f,
                               4.05f, 4.35f, 4.85f, 5.40f, 5.85f, 6.30f};

/* Ideal circular membrane: Bessel zeros over j01. No air load, so the partials
 * stay where the maths puts them and the result reads as unpitched — tom and
 * snare territory. */
constexpr float kMembrane[] = {1.0f,    1.5933f, 2.1355f, 2.2954f, 2.6531f, 2.9173f,
                               3.1555f, 3.5001f, 3.5985f, 3.6534f, 4.0590f, 4.1313f};

/* Simply-supported square plate: f proportional to (m^2 + n^2), normalised by
 * the (1,1) mode. Dense and slow to decay. Several (m,n) pairs are degenerate
 * — the detune below splits them, which is where the shimmer comes from. */
constexpr float kPlate[] = {1.0f, 2.5f,  4.0f,  5.0f,  6.5f,  8.5f,
                            9.0f, 10.0f, 12.5f, 13.0f, 14.5f, 16.25f};
} // namespace bodies

struct BodySpec
{
    const float *ratios;
    /* T60-ish decay of the fundamental, in seconds, at Size 1 and Decay 0.5. */
    float decaySec;
    /* Upper partials decay as decaySec * n^-decayTilt. Large values are what
     * make a marimba a thock and a plate a wash. */
    float decayTilt;
    /* Strike brightness: mode n is excited at n^-exciteTilt. */
    float exciteTilt;
    /* Per-mode random-ish detune depth, for bodies with degenerate modes. */
    float detune;
    /* How much of Pot 2's feedback this body can take. Measured at the worst
     * case on every axis — Ring 1.0, which quadruples mode Q, and Taps 5 — then
     * backed off by about a third for margin. See the headroom sweep in
     * host_dsp/loop_fixture.cpp. These were all a flat zero when feedback was a
     * stored setting that defaulted high; with a centre-off knob you have to
     * ask for it, and the bodies turn out to take a lot more than that. */
    float feedMax;
    /* Root frequency divisor at Size 8 — how much bigger the big body is. */
    float sizeSpan;
};

/* Body order must match ResonatorModel's Beam..Plate run. */
enum class BodyType
{
    Beam = 0,
    Marimba,
    Drumhead,
    Membrane,
    Plate,
    Count,
};

/*                          ratios            decaySec tilt excite detune sizeSpan */
constexpr BodySpec kBodies[] = {
    /*             ratios          decaySec tilt  excite detune feedMax size */
    {bodies::kBeam,      3.0f, 0.55f, 0.70f, 0.000f, 0.45f, 6.0f}, // safe to 0.70
    {bodies::kMarimba,   0.45f, 1.30f, 1.60f, 0.000f, 0.30f, 4.0f}, // safe to 0.48
    {bodies::kDrumhead,  1.40f, 0.80f, 0.90f, 0.001f, 0.28f, 5.0f}, // safe to 0.42
    {bodies::kMembrane,  0.40f, 0.65f, 0.45f, 0.002f, 0.15f, 5.0f}, // safe to 0.24
    {bodies::kPlate,     7.0f, 0.30f, 0.30f, 0.004f, 0.35f, 8.0f}, // safe to 0.54
};

static_assert(sizeof(kBodies) / sizeof(kBodies[0])
                  == static_cast<size_t>(BodyType::Count),
              "BodySpec table must cover every BodyType");

class ModalBodyVoice
{
  public:
    static constexpr int kModes = 12;

    void Init(float sampleRate)
    {
        sampleRate_ = sampleRate;
        Reset();
    }

    void Reset()
    {
        for(int i = 0; i < kModes; ++i)
        {
            bx_.y1[i] = 0.0f;
            bx_.y2[i] = 0.0f;
            by_.y1[i] = 0.0f;
            by_.y2[i] = 0.0f;
        }
        controlCounter_ = 0;
        lastSpec_ = nullptr;
    }

    /* freqX/freqY: the two channels' fundamentals in Hz — the same body struck
     * at two pitches, so they are two instruments rather than two pickups on
     * one. size 1..8, taps 1..5, ratio 0.25..4 (partial stretch), decay 0..1. */
    void SetParams(const BodySpec &spec, float freqX, float freqY, int size, int taps,
                   float ratio, float decay)
    {
        spec_ = &spec;
        freqX_ = freqX;
        freqY_ = freqY;
        size_ = size;
        taps_ = taps;
        ratio_ = ratio;
        decay_ = decay;
    }

    /* Two bodies, one per channel. They share coefficients — same spec, same
     * pitch — but not state, so a signal on IN 2 cannot come out of OUT 1.
     * Feeding one body from a mono sum of both inputs and taking two pickup
     * positions off it was cheaper, but it made the two channels one voice.
     * The pickup weights are kept, one per body, so the two still differ. */
    void Process(float inX, float inY, float &outX, float &outY)
    {
        if(spec_ == nullptr)
        {
            outX = 0.0f;
            outY = 0.0f;
            return;
        }
        if(controlCounter_ <= 0)
        {
            UpdateCoefficients();
            controlCounter_ = kControlInterval;
        }
        --controlCounter_;

        float sumX = 0.0f;
        float sumY = 0.0f;
        for(int i = 0; i < kModes; ++i)
        {
            const float x = bx_.a0[i] * inX + bx_.b1[i] * bx_.y1[i] + bx_.b2[i] * bx_.y2[i];
            bx_.y2[i] = bx_.y1[i];
            bx_.y1[i] = x;
            sumX += x * bx_.pick[i];

            const float y = by_.a0[i] * inY + by_.b1[i] * by_.y1[i] + by_.b2[i] * by_.y2[i];
            by_.y2[i] = by_.y1[i];
            by_.y1[i] = y;
            sumY += y * by_.pick[i];
        }

        outX = sumX;
        outY = sumY;
    }

  private:
    /* 64 samples is ~1.3 ms: fast enough that a CV pitch sweep stays smooth,
     * slow enough that the 24 transcendentals below cost nothing measurable. */
    static constexpr int kControlInterval = 64;
    static constexpr float kPi = 3.14159265358979323846f;

    /* One complete body: its own coefficients, its own filter state, its own
     * pickup. Two of these means IN 2 can never come out of OUT 1, and the two
     * can be tuned apart with Ofst. Coefficients are rebuilt once per
     * kControlInterval, so carrying two sets costs 24 transcendentals every
     * 64 samples rather than 12 — still nothing measurable. */
    struct Bank
    {
        float a0[kModes] = {0.0f};
        float b1[kModes] = {0.0f};
        float b2[kModes] = {0.0f};
        float pick[kModes] = {0.0f};
        float y1[kModes] = {0.0f};
        float y2[kModes] = {0.0f};
    };

    void UpdateCoefficients()
    {
        const BodySpec &spec = *spec_;
        /* Changing body mid-ring would reinterpret the filter state as
         * belonging to the new partials, which rings as a click. */
        if(spec_ != lastSpec_)
        {
            for(int i = 0; i < kModes; ++i)
            {
                bx_.y1[i] = 0.0f;
                bx_.y2[i] = 0.0f;
                by_.y1[i] = 0.0f;
                by_.y2[i] = 0.0f;
            }
            lastSpec_ = spec_;
        }

        const float sizeNorm = std::clamp(static_cast<float>(size_ - 1) / 7.0f, 0.0f, 1.0f);
        const float bright = std::clamp(static_cast<float>(taps_ - 1) / 4.0f, 0.0f, 1.0f);
        /* Ratio stretches or compresses the partial series about the
         * fundamental: 1.0 leaves the body physically true, away from it the
         * partials bend and the body stops sounding like itself. */
        const float stretch
            = std::clamp(1.0f + 0.125f * std::log2(std::clamp(ratio_, 0.25f, 4.0f)),
                         0.75f, 1.25f);
        /* A bigger body is lower and rings longer. */
        const float sizeDiv = 1.0f + sizeNorm * (spec.sizeSpan - 1.0f);
        const float decayScale = (0.25f + std::clamp(decay_, 0.0f, 1.0f) * 3.75f)
                                 * (1.0f + sizeNorm * 1.5f);

        /* Two pickup positions along the body, one per channel. Kept from when
         * both were taken off a single body: the cosine comb is the standard
         * modal pickup, and a node for one partial is an antinode for another,
         * so the channels differ in timbre and not just in pitch. */
        BuildBank(bx_, std::clamp(freqX_ / sizeDiv, 20.0f, sampleRate_ * 0.45f),
                  stretch, decayScale, bright, 0.12f + bright * 0.18f);
        BuildBank(by_, std::clamp(freqY_ / sizeDiv, 20.0f, sampleRate_ * 0.45f),
                  stretch, decayScale, bright, 0.31f + bright * 0.22f);
    }

    void BuildBank(Bank &bank, float root, float stretch, float decayScale, float bright,
                   float pickPos)
    {
        const BodySpec &spec = *spec_;
        const float nyquist = sampleRate_ * 0.48f;

        float gainSum = 0.0f;
        for(int i = 0; i < kModes; ++i)
        {
            const float n = static_cast<float>(i + 1);
            const float ratio = std::pow(spec.ratios[i], stretch);
            /* Split degenerate plate modes; the alternating sign keeps the
             * pair straddling its true frequency rather than drifting sharp. */
            const float detune = 1.0f + spec.detune * ((i & 1) ? 1.0f : -1.0f);
            const float freq = root * ratio * detune;

            /* A partial above Nyquist would alias, so it is silenced rather
             * than folded down — real bodies just run out of audible modes. */
            if(freq >= nyquist || freq < 5.0f)
            {
                bank.a0[i] = 0.0f;
                bank.b1[i] = 0.0f;
                bank.b2[i] = 0.0f;
                bank.pick[i] = 0.0f;
                continue;
            }

            /* decaySec is a T60; the resonator wants the 1/e time, and 60 dB
             * is 6.91 time constants. Feeding T60 in directly rings roughly
             * seven times too long — a marimba that sustains for four seconds. */
            const float t60 = spec.decaySec * decayScale * std::pow(n, -spec.decayTilt);
            const float tau = std::clamp(t60 / 6.91f, 0.0004f, 20.0f);
            const float r = std::exp(-1.0f / (tau * sampleRate_));
            const float w = 2.0f * kPi * freq / sampleRate_;
            const float sinw = std::sin(w);

            bank.b1[i] = 2.0f * r * std::cos(w);
            bank.b2[i] = -r * r;
            /* sin(w) normalises the *impulse* response to unity peak, which is
             * the right choice for a struck body: a strike then sounds the same
             * whether the mode rings for 0.4 s or 28 s. Normalising the
             * steady-state gain instead (a0 = (1-r^2)sin w) makes a long decay
             * inaudible, since the same energy is spread over a longer tail.
             *
             * The cost is the usual one for a high-Q resonator: sustained input
             * sitting on a mode sees gain Q/w, which is enormous. That is why
             * the body models are excited open-loop (see feedScale in main.cpp)
             * and their output is soft-clipped. */
            bank.a0[i] = sinw;

            /* Brightness lifts the upper partials by flattening the strike
             * tilt — a hard mallet against a soft one. */
            const float tilt = spec.exciteTilt * (1.0f - bright * 0.75f);
            const float amp = std::pow(n, -tilt);
            bank.pick[i] = amp * std::sin(kPi * n * pickPos);
            gainSum += amp;
        }

        /* Normalise so adding modes adds density, not level. The 0.30 is set
         * so a full-scale 1 ms strike peaks near 0.8 rather than into the
         * output clipper — a strike is many impulses' worth of energy, not
         * one, and velocity needs the headroom above it to be audible. */
        const float norm = (gainSum > 0.0f) ? (0.30f / gainSum) : 0.0f;
        for(int i = 0; i < kModes; ++i)
        {
            bank.pick[i] *= norm;
        }
    }

    float sampleRate_ = 48000.0f;
    const BodySpec *spec_ = nullptr;
    const BodySpec *lastSpec_ = nullptr;

    float freqX_ = 440.0f;
    float freqY_ = 440.0f;
    int size_ = 1;
    int taps_ = 1;
    float ratio_ = 1.0f;
    float decay_ = 0.5f;

    Bank bx_;
    Bank by_;
    int controlCounter_ = 0;
};

} // namespace res
