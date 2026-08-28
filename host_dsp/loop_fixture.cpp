/* loop_fixture.cpp -- the resonator models as the firmware actually runs them:
 * inside the feedback loop with Pot 2 hard right, the feed filter in the loop,
 * each at its own ModelFeedScale.
 *
 * Testing a model open-loop tells you almost nothing here. Modal and FM both
 * decayed perfectly well on their own and still droned in the module, because
 * the loop was carrying them -- which is the whole reason this fixture closes
 * it. It is also how ModelFeedScale's numbers were measured. Feedback is a
 * knob now rather than a stored setting, so the maximum is reachable at any
 * moment and is the case that has to hold.
 *
 * Build: make -C host_dsp loop_fixture && host_dsp/loop_fixture */
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>
#include "delay_lines.h"
#include "filters.h"
#include "resonator_models.h"

static constexpr float SR = 48000.0f;

static float Rms(const std::vector<float> &x, size_t from, size_t len)
{
    double a = 0.0; size_t n = 0;
    for(size_t i = from; i < from + len && i < x.size(); ++i, ++n) a += double(x[i]) * x[i];
    return n ? float(std::sqrt(a / n)) : 0.0f;
}

struct Row { const char *name; ResonatorModel m; };

/* The Delay branch does not live in RingsStyleModels -- main.cpp reads the
 * delay line directly -- so it is replayed here sample for sample. The point of
 * measuring it is level: it is the one model whose output is nothing but the
 * input fed back through the loop, so every saturator in that path costs it
 * directly -- it read a third of everything else on hardware because it was
 * being soft-clipped twice over. Measured against the dry path, which is what
 * the Mix control crossfades it against. */
static int MeasureDelay(int taps, float knob)
{
    static DelayBuffer<kMaxDelaySamples> line;
    line.Init();
    FeedFilters ff;
    ff.Init(SR);
    ff.SetParams(0.2f, 0.25f, 0.7f, 220.0f, 220.0f);

    /* Knob in, loop coefficient out — the same two steps the firmware takes. */
    const float feed = FeedTaper(knob) * 0.99f;
    const float delaySamples = SR / 220.0f;
    line.SetDelay(delaySamples);

    const size_t n = size_t(4.0f * SR);
    double inSq = 0.0, outSq = 0.0;
    size_t counted = 0;
    uint32_t rng = 0x9e3779b9u;
    for(size_t i = 0; i < n; ++i)
    {
        /* Broadband noise, not a sine: prime-spaced taps are a comb, and a
         * single tone lands wherever the comb happens to put it. Noise measures
         * the level the model actually delivers on program material. */
        rng = rng * 1664525u + 1013904223u;
        const float raw = float(int32_t(rng)) * (1.0f / 2147483648.0f);
        const float in = SoftClipSample(raw);
        const float filtered = ff.ProcessX(line.Read());
        const float preDist = in + SoftClipSample(filtered * feed);
        line.Write(LimitSample(preDist));
        const float tap = ReadPrimeSpacedTaps(line, delaySamples, taps);
        const float outv = LimitSample(tap);
        if(i > n / 2)
        {
            inSq += double(in) * in; // the dry side of the Mix control
            outSq += double(outv) * outv;
            counted++;
        }
    }
    const float inRms = float(std::sqrt(inSq / counted));
    const float outRms = float(std::sqrt(outSq / counted));
    const float ratio = outRms / inRms;

    /* Measured against the dry path, not against the raw input: Mix crossfades
     * wet against SoftClipSample(in), so parity there is what "same level"
     * means. Was 0.33x before; anything under half the dry level is the bug
     * this row exists to catch. At maximum feed the loop must also stay inside
     * LimitSample's asymptote rather than run away. */
    const bool ok = std::isfinite(outRms) && ratio > 0.5f && outRms < 2.0f;
    std::printf("%-11s %3d  %4.2f  dry %.3f -> wet %.3f (%.2fx)%s\n", "Delay", taps, knob,
                inRms, outRms, ratio, ok ? "" : "  <-- LEVEL");
    return ok ? 0 : 1;
}

/* Largest feed at which a model still decays, found by bisection-free sweep.
 * This is where ModelFeedScale's numbers come from: run at the worst case
 * (Taps 5, which raises the models' own internal feedback too), then back off
 * for margin. Modal and the struck bodies were set to a flat 0 when feedback
 * was a stored setting that defaulted high -- with a centre-off knob the user
 * has to ask for it, so the question is what they can take, not whether to
 * offer it at all. */
static float MaxSafeFeed(ResonatorModel m)
{
    float best = 0.0f;
    for(float feed = 0.02f; feed <= 0.90f; feed += 0.02f)
    {
        RingsStyleModels r; r.Init(SR);
        FeedFilters ff; ff.Init(SR);
        ModelProcessParams p;
        p.freqX = 220.0f; p.freqY = 220.0f; p.ratio = 1.0f;
        /* Worst case on every axis: Taps 5 raises the models' own internal
         * feedback, and Ring 1.0 multiplies the bodies' mode Q by ~4. */
        p.size = 1; p.taps = 5; p.decay = 1.0f; p.feed = 1.0f;
        ff.SetParams(0.2f, 0.25f, 0.7f, 220.0f, 220.0f);

        const size_t n = size_t(12.0f * SR);
        const size_t burst = size_t(0.02f * SR);
        const size_t win = size_t(0.25f * SR);
        std::vector<float> out(n);
        float fbX = 0.0f, fbY = 0.0f;
        uint32_t rng = 0x1234567u;
        for(size_t i = 0; i < n; ++i)
        {
            float in = 0.0f;
            if(i < burst)
            {
                rng = rng * 1664525u + 1013904223u;
                in = float(int32_t(rng)) * (1.0f / 2147483648.0f) * 0.5f;
            }
            const float fx = ff.ProcessX(fbX);
            const float fy = ff.ProcessY(fbY);
            float l = 0.0f, rr = 0.0f;
            r.Process(m, SoftClipSample(SoftClipSample(in) + fx * feed),
                      SoftClipSample(SoftClipSample(in) + fy * feed), p, l, rr);
            fbX = l; fbY = rr;
            out[i] = l;
        }
        /* Must be genuinely falling across the second half, not merely
         * not-growing: a marginal loop sits at constant amplitude for a long
         * time before it audibly runs away. */
        const float strike = Rms(out, burst, win);
        const float late = Rms(out, n - win, win);
        const float mid = Rms(out, n / 2, win);
        if(late > mid * 0.9f && late > strike * 0.02f) break;
        best = feed;
    }
    return best;
}

int main()
{
    const Row rows[] = {{"Modal", ResonatorModel::ModalBank},
                        {"SympString", ResonatorModel::SympString},
                        {"FM", ResonatorModel::FMRes},
                        {"Beam", ResonatorModel::Beam},
                        {"Marimba", ResonatorModel::Marimba},
                        {"Drumhead", ResonatorModel::Drumhead},
                        {"Membrane", ResonatorModel::Membrane},
                        {"Plate", ResonatorModel::Plate}};

    std::printf("each model at its own ModelFeedScale, Pot 2 hard right\n");
    std::printf("(t60 = seconds for the ring to fall 60 dB below the strike)\n\n");
    std::printf("model       taps  feed  strike   t60\n");
    std::printf("------------------------------------------\n");
    int fails = 0;

    /* Pot 2 centred and hard right — the knob, not the loop coefficient. */
    for(float knob : {0.0f, 0.99f})
        for(int taps : {1, 3, 5})
            fails += MeasureDelay(taps, knob);
    std::printf("\n");

    for(const Row &row : rows)
    {
      for(int taps : {1, 3, 5})
      {
        const float fscale = ModelFeedScale(row.m);
        RingsStyleModels r; r.Init(SR);
        FeedFilters ff; ff.Init(SR);
        ModelProcessParams p;
        p.freqX = 220.0f; p.freqY = 220.0f; p.ratio = 1.0f;
        p.size = 1; p.taps = taps; p.decay = 0.5f;
        /* Pot 2 hard right, as the models see it: the raw knob position, before
         * ModelFeedScale. Modal reads this as damping rather than as loop gain. */
        p.feed = 0.99f;
        ff.SetParams(0.2f, 0.25f, 0.7f, 220.0f, 220.0f);

        /* The same knob, scaled into the loop. Feedback is a knob now rather
         * than a stored setting, so the maximum is reachable at any moment and
         * is what has to hold. */
        const float feed = FeedTaper(0.99f) * 0.99f * fscale;

        const size_t n = size_t(8.0f * SR);
        const size_t burst = size_t(0.02f * SR);
        std::vector<float> out(n);
        float fbX = 0.0f, fbY = 0.0f;
        uint32_t rng = 0x1234567u;

        for(size_t i = 0; i < n; ++i)
        {
            float in = 0.0f;
            if(i < burst)
            {
                rng = rng * 1664525u + 1013904223u;
                in = float(int32_t(rng)) * (1.0f / 2147483648.0f) * 0.5f;
            }
            const float fx = ff.ProcessX(fbX);
            const float fy = ff.ProcessY(fbY);
            const float preX = SoftClipSample(in) + fx * feed;
            const float preY = SoftClipSample(in) + fy * feed;
            float l = 0.0f, rr = 0.0f;
            r.Process(row.m, SoftClipSample(preX), SoftClipSample(preY), p, l, rr);
            /* Feed path takes the natural level; the mix path takes makeup.
             * Mirrors AudioCallback exactly -- measuring the wrong one here is
             * what would hide a level problem. */
            fbX = l; fbY = rr;
            out[i] = (ModelOutGain(row.m) != 1.0f)
                         ? SoftClipSample(l * ModelOutGain(row.m))
                         : l;
        }

        const size_t win = size_t(0.25f * SR);
        const float s = Rms(out, burst, win);
        float t60 = -1.0f;
        for(size_t i = burst; i + win < out.size(); i += win / 2)
        {
            if(Rms(out, i, win) < s * 0.001f) { t60 = float(i) / SR; break; }
        }

        /* Growth, not just failure to decay. A long tail and an oscillation
         * look identical to a t60 that never arrives, and the struck bodies
         * legitimately outlast the window -- Plate's tail is ~15 s -- so the
         * t60 test alone can only ever exempt them, never check them. What
         * actually distinguishes a resonator from an oscillator is whether the
         * ring is still growing long after the strike. */
        const float late = Rms(out, out.size() - win, win);
        const float mid = Rms(out, out.size() / 2, win);
        const bool growing = late > mid * 1.05f && late > s * 0.02f;

        if(growing) fails++;
        if(t60 < 0.0f)
        {
            std::printf("%-11s %3d  %4.2f %7.4f   >8 s%s\n", row.name, taps, feed, s,
                        growing ? "  <-- DRONES" : " (sustains)");
        }
        else
        {
            if(s < 1e-3f) { fails++; std::printf("  (too quiet)\n"); }
            std::printf("%-11s %3d  %4.2f %7.4f  %5.2f s%s\n", row.name, taps, feed, s,
                        t60, growing ? "  <-- DRONES" : "");
        }
      }
      std::printf("\n");
    }
    /* Does Pot 2 actually do anything as you turn it? RMS against continuous
     * noise does not answer that -- a feedback control's job is the tail, and
     * the Delay model measured a flat 55 ms across three quarters of the knob
     * with the whole audible range crammed into the last few percent. It was
     * working the entire time; there was just nowhere to find it. This row is
     * here so that cannot come back silently. */
    std::printf("Pot 2 taper on Delay: ring time across the knob\n");
    {
        float prev = 0.0f;
        int flat = 0;
        for(float knob : {0.15f, 0.30f, 0.45f, 0.60f, 0.75f, 0.90f, 1.00f})
        {
            static DelayBuffer<kMaxDelaySamples> line; line.Init();
            FeedFilters ff; ff.Init(SR);
            const float f0 = 220.0f;
            ff.SetParams(0.2f, 0.25f, 0.7f, f0, f0);
            const float d = SR / f0; line.SetDelay(d);
            const float feed = FeedTaper(knob) * 0.99f;

            const size_t n = size_t(6.0f * SR), burst = size_t(0.005f * SR);
            const size_t w = size_t(0.05f * SR);
            std::vector<float> out(n);
            uint32_t rng = 0x1234567u;
            for(size_t i = 0; i < n; ++i)
            {
                float in = 0.0f;
                if(i < burst)
                {
                    rng = rng * 1664525u + 1013904223u;
                    in = float(int32_t(rng)) * (1.0f / 2147483648.0f);
                }
                const float filt = ff.ProcessX(line.Read());
                line.Write(LimitSample(SoftClipSample(in) + SoftClipSample(filt * feed)));
                out[i] = LimitSample(ReadPrimeSpacedTaps(line, d, 1));
            }
            const float st = Rms(out, burst, w);
            float t60 = -1.0f;
            for(size_t i = burst; i + w < n; i += w)
                if(Rms(out, i, w) < st * 0.001f) { t60 = float(i) / SR; break; }
            std::printf("  knob %.2f  feed %.2f  ring %s", knob, feed,
                        t60 < 0.0f ? ">6 s" : "");
            if(t60 >= 0.0f) std::printf("%.3f s", t60);
            /* Every step of the knob must lengthen the ring. Two consecutive
             * steps that do not is a dead zone, which is the whole bug. */
            if(t60 >= 0.0f && t60 <= prev + 1e-4f) { flat++; std::printf("   <-- flat"); }
            else flat = 0;
            std::printf("\n");
            if(flat >= 2) { fails++; std::printf("  <-- dead zone in Pot 2's travel\n"); }
            if(t60 >= 0.0f) prev = t60;
        }
    }
    std::printf("\n");

    /* Pot 2 has to do something audible on every model, and Modal is the one
     * that cannot take a feedback loop at all -- it reads the knob as damping
     * instead. Anticlockwise must ring measurably shorter than clockwise, or
     * the knob is dead there however good the excuse. */
    std::printf("Pot 2 as damping on Modal (no loop; the knob moves mode Q)\n");
    float modalT60[2] = {0.0f, 0.0f};
    for(int k = 0; k < 2; ++k)
    {
        const float knob = (k == 0) ? -1.0f : 1.0f;
        RingsStyleModels r; r.Init(SR);
        ModelProcessParams p;
        p.freqX = 220.0f; p.freqY = 220.0f; p.ratio = 1.0f;
        p.size = 1; p.taps = 3; p.decay = 0.5f; p.feed = knob;

        const size_t n = size_t(8.0f * SR);
        const size_t burst = size_t(0.02f * SR);
        const size_t win = size_t(0.25f * SR);
        std::vector<float> out(n);
        uint32_t rng = 0x1234567u;
        for(size_t i = 0; i < n; ++i)
        {
            float in = 0.0f;
            if(i < burst)
            {
                rng = rng * 1664525u + 1013904223u;
                in = float(int32_t(rng)) * (1.0f / 2147483648.0f) * 0.5f;
            }
            float l = 0.0f, rr = 0.0f;
            r.Process(ResonatorModel::ModalBank, in, in, p, l, rr);
            out[i] = l;
        }
        const float strike = Rms(out, burst, win);
        for(size_t i = burst; i + win < out.size(); i += win / 2)
            if(Rms(out, i, win) < strike * 0.001f) { modalT60[k] = float(i) / SR; break; }
        std::printf("  knob %+.0f   t60 %.2f s\n", knob, modalT60[k]);
    }
    if(!(modalT60[1] > modalT60[0] * 1.5f))
    {
        fails++;
        std::printf("  <-- Pot 2 has no useful effect on Modal\n");
    }
    std::printf("\n");

    std::printf("headroom sweep (Taps 5, worst case): largest feed that still decays\n");
    for(const Row &row : rows)
        std::printf("  %-11s %.2f   (in use: %.2f)\n", row.name, MaxSafeFeed(row.m),
                    ModelFeedScale(row.m));

    std::printf("\n%s\n", fails ? "FAILED" : "PASSED");
    return fails ? 1 : 0;
}
