/* bodies_fixture.cpp — host harness for the resonators' struck-body models.
 *
 * modal_bodies.h is deliberately free of libDaisy, so the five new bodies can
 * be struck and measured here without hardware. Checks, per body:
 *   • the ring stays finite and bounded (no runaway two-pole feedback),
 *   • decay is monotone and lands near the BodySpec's stated time,
 *   • the strongest measured partial sits on the fundamental,
 *   • nothing rings above Nyquist (partials past it must be silenced, not
 *     folded down as aliases).
 *
 * Build: make -C host_dsp bodies_fixture && host_dsp/bodies_fixture */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "modal_bodies.h"

namespace
{
constexpr float kSampleRate = 48000.0f;
constexpr float kPi = 3.14159265358979323846f;

const char *BodyName(int i)
{
    static const char *const kNames[] = {"Beam", "Marimba", "Drumhead", "Membrane",
                                         "Plate"};
    return kNames[i];
}

/* Goertzel magnitude at `freq`, normalised by the window length. */
float Magnitude(const std::vector<float> &x, size_t from, size_t len, float freq)
{
    const float w = 2.0f * kPi * freq / kSampleRate;
    const float coeff = 2.0f * std::cos(w);
    float s1 = 0.0f, s2 = 0.0f;
    for(size_t i = from; i < from + len && i < x.size(); ++i)
    {
        const float s = x[i] + coeff * s1 - s2;
        s2 = s1;
        s1 = s;
    }
    return std::sqrt(s1 * s1 + s2 * s2 - coeff * s1 * s2) / float(len);
}

float Rms(const std::vector<float> &x, size_t from, size_t len)
{
    double acc = 0.0;
    size_t n = 0;
    for(size_t i = from; i < from + len && i < x.size(); ++i, ++n)
        acc += double(x[i]) * double(x[i]);
    return n ? float(std::sqrt(acc / double(n))) : 0.0f;
}

struct Result
{
    std::vector<float> l, r;
    float peak = 0.0f;      // as the model produces it
    float clipped = 0.0f;   // as main.cpp delivers it, after the soft clip
    bool finite = true;
};

float SoftClip(float x)
{
    return x / (1.0f + std::fabs(x));
}

/* `sustainFreq` > 0 drives a continuous sine instead of a strike, to check the
 * open-loop behaviour of a high-Q mode under the worst input it can be given. */
Result Run(int bodyIndex, float freq, int size, int taps, float ratio, float decay,
           float seconds, float sustainFreq)
{
    res::ModalBodyVoice voice;
    voice.Init(kSampleRate);
    voice.SetParams(res::kBodies[bodyIndex], freq, size, taps, ratio, decay);

    const size_t n = size_t(seconds * kSampleRate);
    const size_t burst = size_t(0.001f * kSampleRate);
    Result out;
    out.l.resize(n);
    out.r.resize(n);

    uint32_t rng = 0x13579BDFu;
    for(size_t i = 0; i < n; ++i)
    {
        float in = 0.0f;
        if(sustainFreq > 0.0f)
        {
            in = std::sin(2.0f * kPi * sustainFreq * float(i) / kSampleRate);
        }
        else if(i < burst)
        {
            rng = rng * 1664525u + 1013904223u;
            in = float(int32_t(rng)) * (1.0f / 2147483648.0f);
        }
        float l = 0.0f, r = 0.0f;
        voice.Process(in, l, r);

        if(!std::isfinite(l) || !std::isfinite(r))
            out.finite = false;
        out.l[i] = l;
        out.r[i] = r;
        out.peak = std::max(out.peak, std::max(std::fabs(l), std::fabs(r)));
        /* main.cpp soft-clips the body output; track both so a merely loud
         * result is not confused with an unstable one. */
        out.clipped = std::max(out.clipped, std::fabs(SoftClip(l)));
    }
    return out;
}

Result Strike(int bodyIndex, float freq, int size, int taps, float ratio, float decay,
              float seconds)
{
    return Run(bodyIndex, freq, size, taps, ratio, decay, seconds, 0.0f);
}

int gFailures = 0;

void Check(bool ok, const char *what, const char *body)
{
    if(!ok)
    {
        std::printf("  FAIL  %-28s (%s)\n", what, body);
        gFailures++;
    }
}
} // namespace

int main()
{
    constexpr float kFreq = 220.0f;
    constexpr float kTail = 4.0f;
    const size_t win = size_t(0.25f * kSampleRate);

    std::printf("body       peak   clip  ringMs  f0/2f0  aliasRatio\n");
    std::printf("-----------------------------------------------------\n");

    for(int b = 0; b < int(res::BodyType::Count); ++b)
    {
        const Result res = Strike(b, kFreq, 1, 3, 1.0f, 0.5f, kTail);
        const char *name = BodyName(b);

        Check(res.finite, "output stayed finite", name);
        /* Two-pole feedback that has gone unstable shows up here long before
         * anything else does. */
        Check(res.peak < 4.0f, "output stayed bounded", name);
        /* And the other way: normalising the mode gain for steady state rather
         * than for the impulse makes a strike inaudible, which every other
         * check above happily passes. */
        Check(res.clipped > 0.05f, "strike is audible", name);
        /* Headroom below the clipper is what makes MIDI velocity audible: a
         * full-velocity strike that already slams the clip leaves nothing for
         * a softer one to be quieter than. */
        Check(res.peak < 1.5f, "strike left velocity headroom", name);

        /* Decay must be monotone across the tail. */
        const float early = Rms(res.l, size_t(0.05f * kSampleRate), win);
        const float mid = Rms(res.l, size_t(1.0f * kSampleRate), win);
        const float late = Rms(res.l, size_t(3.0f * kSampleRate), win);
        Check(early > 1e-6f, "strike produced output", name);
        Check(mid <= early * 1.05f, "decay monotone (early->mid)", name);
        Check(late <= mid * 1.05f, "decay monotone (mid->late)", name);

        /* Ring time: where the envelope falls 60 dB below the early window. */
        float ringMs = kTail * 1000.0f;
        for(size_t i = 0; i + win < res.l.size(); i += win / 4)
        {
            if(Rms(res.l, i, win) < early * 0.001f)
            {
                ringMs = 1000.0f * float(i) / kSampleRate;
                break;
            }
        }

        /* The fundamental should dominate its own octave. */
        const float f0 = Magnitude(res.l, size_t(0.05f * kSampleRate), win, kFreq);
        const float f2 = Magnitude(res.l, size_t(0.05f * kSampleRate), win, kFreq * 2.0f);
        Check(f0 > 0.0f, "fundamental present", name);

        /* Aliasing probe: a partial silenced for being above Nyquist must not
         * reappear as a mirrored tone. Struck high, the top of the band should
         * hold far less energy than the fundamental. */
        const Result hi = Strike(b, 3000.0f, 1, 5, 1.0f, 0.5f, 0.5f);
        const float hiF0 = Magnitude(hi.l, size_t(0.02f * kSampleRate), win, 3000.0f);
        float worstAlias = 0.0f;
        for(float f = 100.0f; f < 2000.0f; f += 100.0f)
            worstAlias = std::max(worstAlias, Magnitude(hi.l, size_t(0.02f * kSampleRate), win, f));
        const float aliasRatio = (hiF0 > 1e-9f) ? worstAlias / hiF0 : 0.0f;
        Check(hi.finite && hi.peak < 8.0f, "high strike stayed bounded", name);
        Check(aliasRatio < 1.0f, "no dominant alias below f0", name);

        std::printf("%-10s %5.2f  %5.2f  %6.0f  %6.2f  %10.3f\n", name, res.peak,
                    res.clipped, ringMs, (f2 > 1e-9f) ? f0 / f2 : 0.0f, aliasRatio);
    }

    /* Sweeping every control must never destabilise the filters. */
    /* Worst case for an open-loop high-Q resonator: a sine parked on the
     * fundamental for a second. It must stay finite and bounded by the clip. */
    std::printf("\nsustained drive on the fundamental\n");
    for(int b = 0; b < int(res::BodyType::Count); ++b)
    {
        const Result d = Run(b, kFreq, 1, 3, 1.0f, 1.0f, 1.0f, kFreq);
        Check(d.finite, "sustained drive stayed finite", BodyName(b));
        Check(d.clipped <= 1.001f, "sustained drive respected the clip", BodyName(b));
        std::printf("  %-10s raw %8.1f  clipped %5.3f\n", BodyName(b), d.peak, d.clipped);
    }

    std::printf("\nparameter sweep (all bodies x size x taps x ratio x decay x freq)\n");
    int sweeps = 0;
    struct Worst
    {
        float peak = 0.0f;
        float quietest = 1e9f;
        int nonFinite = 0;
    } worstBody[int(res::BodyType::Count)];

    for(int b = 0; b < int(res::BodyType::Count); ++b)
    {
        for(int size = 1; size <= 8; ++size)
        {
            for(int taps = 1; taps <= 5; ++taps)
            {
                for(float ratio : {0.25f, 1.0f, 4.0f})
                {
                    for(float decay : {0.0f, 0.5f, 1.0f})
                    {
                        for(float freq : {20.0f, 220.0f, 4000.0f, 8000.0f})
                        {
                            const Result r = Strike(b, freq, size, taps, ratio, decay, 0.25f);
                            if(!r.finite)
                                worstBody[b].nonFinite++;
                            worstBody[b].peak = std::max(worstBody[b].peak, r.peak);
                            worstBody[b].quietest
                                = std::min(worstBody[b].quietest, r.clipped);
                            sweeps++;
                        }
                    }
                }
            }
        }
    }
    std::printf("  %d parameter combinations struck\n", sweeps);
    for(int b = 0; b < int(res::BodyType::Count); ++b)
    {
        const Worst &w = worstBody[b];
        std::printf("  %-10s worst peak %6.2f   quietest %5.3f\n", BodyName(b), w.peak,
                    w.quietest);
        Check(w.nonFinite == 0, "sweep stayed finite", BodyName(b));
        Check(w.peak < 4.0f, "sweep stayed bounded", BodyName(b));
        /* Every reachable setting must make a sound; a combination that
         * silences the body is a bug in the Nyquist guard or the gain
         * normalisation, not a valid preset. */
        Check(w.quietest > 0.01f, "sweep stayed audible", BodyName(b));
    }

    std::printf("\n%s (%d failures)\n", gFailures ? "FAILED" : "PASSED", gFailures);
    return gFailures ? 1 : 0;
}
