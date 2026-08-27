/* loop_fixture.cpp -- the resonator models as the firmware actually runs them:
 * inside the feed matrix at the default X-X of 0.8, with the feed filter in the
 * loop, each at its own ModelFeedScale.
 *
 * Testing a model open-loop tells you almost nothing here. Modal and FM both
 * decayed perfectly well on their own and still droned in the module, because
 * the feed matrix was carrying them -- which is the whole reason this fixture
 * closes the loop. It is also how ModelFeedScale's numbers were measured.
 *
 * Build: make -C host_dsp loop_fixture && host_dsp/loop_fixture */
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>
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

int main()
{
    const Row rows[] = {{"Modal", ResonatorModel::ModalBank},
                        {"SympString", ResonatorModel::SympString},
                        {"FM", ResonatorModel::FMRes},
                        {"Beam", ResonatorModel::Beam},
                        {"Plate", ResonatorModel::Plate}};

    std::printf("each model at its own ModelFeedScale, X-X at the 0.80 default\n");
    std::printf("(t60 = seconds for the ring to fall 60 dB below the strike)\n\n");
    std::printf("model       taps  feed  strike   t60\n");
    std::printf("------------------------------------------\n");
    int fails = 0;

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
        ff.SetParams(0.2f, 0.25f, 0.7f, 220.0f, 220.0f);

        const float feed = 0.8f * fscale; // Wiring X-X default

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
        if(t60 < 0.0f)
        {
            /* Bodies legitimately outlast the 8 s window (Plate's tail is ~15 s);
             * a feedback model that never decays is a drone. */
            /* Only the struck bodies legitimately outlast the window (Plate's
             * tail is ~15 s). Every feedback model must decay. */
            const bool expected = IsBodyModel(row.m);
            if(!expected) fails++;
            std::printf("%-11s %3d  %4.2f %7.4f   >8 s%s\n", row.name, taps, feed, s,
                        expected ? " (sustains by design)" : "  <-- DRONES");
        }
        else
        {
            if(s < 1e-3f) { fails++; std::printf("  (too quiet)\n"); }
            std::printf("%-11s %3d  %4.2f %7.4f  %5.2f s\n", row.name, taps, feed, s, t60);
        }
      }
      std::printf("\n");
    }
    std::printf("\n%s\n", fails ? "FAILED" : "PASSED");
    return fails ? 1 : 0;
}
