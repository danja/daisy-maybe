/* dirac_fixture.cpp — host harness for the Dirac granular engine.
 * SPDX-License-Identifier: Apache-2.0
 *
 * The bluemchen has no simulator and a 64x32 OLED, so the DSP is verified here
 * before it ever reaches hardware. Ported from er301-Dirac/test/host.
 *
 *   make -C host_dsp dirac_fixture && ./host_dsp/dirac_fixture all
 *
 * Modes:
 *   ident   deterministic renders — checksum/RMS, for bit-exactness diffs
 *   torture extreme configs — every output must stay finite and bounded
 *   seam    click metric at the sample loop boundary and live write head
 *   snapt   mean |grain start - nearest onset| on a click track
 *   tanh    the fast feedback saturator vs libm tanhf
 *   cpu     realtime factor at heavy settings
 *   wav     malformed WAV headers must be refused, not loaded as garbage
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include "dirac_engine.h"
#include "dirac_wav.h"

using dirac::DiracEngine;
using dirac::DiracParams;
using dirac::DiracSample;

namespace
{
constexpr float kSR = 48000.0f;
constexpr int kBlock = 48;
constexpr int kCapSize = 1 << 18; // 262144, matches the firmware

int gFail = 0;

void Check(bool ok, const char *what)
{
    printf("  %-52s %s\n", what, ok ? "PASS" : "*** FAIL ***");
    if(!ok)
        gFail++;
}

struct Rig
{
    DiracEngine eng;
    std::vector<float> cap;
    Rig() : cap(kCapSize, 0.0f) { eng.Init(kSR, cap.data(), kCapSize); }
};

/* A deterministic noise+tone source so renders are reproducible. */
struct Src
{
    uint32_t st = 0x9e3779b9u;
    float ph = 0.0f;
    float Next()
    {
        st ^= st << 13;
        st ^= st >> 17;
        st ^= st << 5;
        const float n = float(st & 0xFFFF) / 32768.0f - 1.0f;
        ph += 220.0f / kSR;
        if(ph >= 1.0f)
            ph -= 1.0f;
        return 0.6f * sinf(6.28318531f * ph) + 0.2f * n;
    }
};

/* 2 s click track: a decaying burst every 0.5 s. The decay is ~40 ms rather
 * than a single-sample impulse, because Snap scores energy at taps spanning
 * ±200 samples — an impulse narrower than the tap spacing is invisible to the
 * search, and to any real onset detector. This is a drum hit, not a Dirac
 * delta. */
std::vector<float> ClickTrack(std::vector<uint32_t> &onsets)
{
    const int n = int(kSR) * 2;
    std::vector<float> v(n, 0.0f);
    for(int k = 0; k < 4; ++k)
    {
        const uint32_t at = uint32_t(k * kSR * 0.5f);
        onsets.push_back(at);
        for(int i = 0; i < 2000 && at + i < uint32_t(n); ++i)
            v[at + i] = expf(-float(i) * 0.003f) * sinf(float(i) * 0.7f);
    }
    return v;
}

bool AllFinite(const float *p, int n, float bound = 1.0001f)
{
    for(int i = 0; i < n; ++i)
        if(!std::isfinite(p[i]) || fabsf(p[i]) > bound)
            return false;
    return true;
}

DiracParams Defaults()
{
    DiracParams p;
    p.density = 3.0f;
    p.grainLenSec = 0.05f;
    p.grains = 12;
    p.level = 0.7f;
    p.spread = 0.5f;
    p.mix = 1.0f;
    return p;
}

/* Render `seconds`, return (checksum, rms). Optionally collect grain spawns. */
struct RenderStats
{
    double checksum = 0.0;
    double rms = 0.0;
    bool finite = true;
    float peak = 0.0f;
};

RenderStats Render(Rig &rig, const DiracParams &p, float seconds, Src *src,
                   const float *feed = nullptr, int feedLen = 0,
                   std::vector<float> *spawns = nullptr)
{
    RenderStats st;
    float in[kBlock], outL[kBlock], outR[kBlock];
    const int blocks = int(seconds * kSR) / kBlock;
    bool wasActive[DiracEngine::kMaxGrains] = {false};
    double acc = 0.0;
    int feedPos = 0;

    for(int b = 0; b < blocks; ++b)
    {
        for(int s = 0; s < kBlock; ++s)
        {
            if(feed)
                in[s] = (feedPos < feedLen) ? feed[feedPos++] : 0.0f;
            else if(src)
                in[s] = src->Next();
            else
                in[s] = 0.0f;
        }
        rig.eng.Process(in, nullptr, outL, outR, kBlock, p);

        if(spawns)
        {
            for(int i = 0; i < DiracEngine::kMaxGrains; ++i)
            {
                const bool a = rig.eng.IsGrainActive(i);
                if(a && !wasActive[i])
                    spawns->push_back(rig.eng.GetGrainSpawnPos(i));
                wasActive[i] = a;
            }
        }

        if(!AllFinite(outL, kBlock) || !AllFinite(outR, kBlock))
            st.finite = false;
        for(int s = 0; s < kBlock; ++s)
        {
            acc += double(outL[s]) * outL[s] + double(outR[s]) * outR[s];
            st.checksum += double(outL[s]) * 1.0 + double(outR[s]) * 0.5;
            st.peak = std::max(st.peak, std::max(fabsf(outL[s]), fabsf(outR[s])));
        }
    }
    const double n = double(blocks) * kBlock * 2.0;
    st.rms = (n > 0.0) ? sqrt(acc / n) : 0.0;
    return st;
}

/* ── ident ────────────────────────────────────────────────────────────── */
int ModeIdent()
{
    printf("[ident] deterministic renders\n");

    struct Case
    {
        const char *name;
        DiracParams p;
    };
    std::vector<Case> cases;
    {
        Case c{"defaults", Defaults()};
        cases.push_back(c);
    }
    {
        DiracParams p = Defaults();
        p.window = 2;
        p.texture = 0.8f;
        p.tilt = -0.6f;
        p.tiltSprd = 0.4f;
        cases.push_back({"sinc window + tilt", p});
    }
    {
        DiracParams p = Defaults();
        p.multiHead = true;
        p.heads = 4;
        p.headSpread = 0.6f;
        p.headTune = 7.0f;
        p.scale = 2;
        p.psprd = 5.0f;
        cases.push_back({"4 heads, diatonic", p});
    }
    {
        DiracParams p = Defaults();
        p.feedback = 0.8f;
        p.diffuse = true;
        p.binaural = 0.7f;
        p.color = 0.5f;
        p.compress = 0.6f;
        cases.push_back({"feedback+diffuse+binaural", p});
    }

    for(auto &c : cases)
    {
        Rig rig;
        Src src;
        const RenderStats st = Render(rig, c.p, 1.0f, &src);
        printf("  %-26s rms %.6f  peak %.4f  sum %+.4f  %s\n", c.name, st.rms,
               st.peak, st.checksum, st.finite ? "finite" : "NON-FINITE");
        Check(st.finite, "output finite");
        Check(st.rms > 1e-4, "output non-silent");
    }
    return 0;
}

/* ── torture ──────────────────────────────────────────────────────────── */
int ModeTorture()
{
    printf("[torture] extreme configs stay finite and bounded\n");

    struct Case
    {
        const char *name;
        DiracParams p;
    };
    std::vector<Case> cases;

    DiracParams base = Defaults();

    {
        DiracParams p = base;
        p.grainLenSec = 0.001f; // 1 ms
        p.density = 16.0f;
        p.grains = 16;
        p.semiShift = 24.0f;
        p.voctOct = 3.0f; // clamps to +4 oct
        p.snap = 1.0f;
        cases.push_back({"1ms grains, dens 16, +4oct, snap", p});
    }
    {
        DiracParams p = base;
        p.grainLenSec = 2.0f; // ceiling
        p.density = 16.0f;
        p.detune = 2.0f;
        p.binaural = 1.0f;
        cases.push_back({"2s grains, max detune + binaural", p});
    }
    {
        DiracParams p = base;
        p.semiShift = -24.0f;
        p.voctOct = -4.0f;
        p.revProb = 1.0f;
        p.posJtr = 1.0f;
        p.psprd = 12.0f;
        cases.push_back({"-4oct, all reverse, max jitter", p});
    }
    {
        DiracParams p = base;
        p.feedback = 1.0f;
        p.diffuse = true;
        p.multiHead = true;
        p.heads = 4;
        p.headSpread = 1.0f;
        p.compress = 1.0f;
        p.level = 1.0f;
        cases.push_back({"unity feedback, 4 heads, max compress", p});
    }
    {
        DiracParams p = base;
        p.density = 0.0f; // trigger-only, no triggers
        cases.push_back({"trigger-only with no triggers", p});
    }
    {
        DiracParams p = base;
        p.hold = true;
        p.freeze = true;
        p.feedback = 0.9f;
        cases.push_back({"hold + freeze + feedback", p});
    }
    {
        DiracParams p = base;
        p.window = 7;
        p.texture = 1.0f;
        p.tilt = 1.0f;
        p.tiltSprd = 1.0f;
        p.color = 1.0f;
        cases.push_back({"square window, full tilt spread", p});
    }

    for(auto &c : cases)
    {
        Rig rig;
        Src src;
        const RenderStats st = Render(rig, c.p, 3.0f, &src);
        printf("  %-38s rms %.6f peak %.4f\n", c.name, st.rms, st.peak);
        Check(st.finite, "finite and within +/-1.0");
    }

    /* NaN injection: a corrupt input must not poison the ring or the output. */
    {
        Rig rig;
        DiracParams p = base;
        p.feedback = 0.9f;
        float in[kBlock], outL[kBlock], outR[kBlock];
        bool clean = true;
        for(int b = 0; b < 400; ++b)
        {
            for(int s = 0; s < kBlock; ++s)
                in[s] = (b == 10) ? NAN : ((b == 12) ? INFINITY : 0.3f);
            rig.eng.Process(in, nullptr, outL, outR, kBlock, p);
            if(b > 12 && (!AllFinite(outL, kBlock) || !AllFinite(outR, kBlock)))
                clean = false;
        }
        Check(clean, "NaN/Inf input does not poison the ring");
    }

    /* A degenerate sample (2 frames) with extreme pitch must not read OOB. */
    {
        Rig rig;
        float tiny[2] = {0.5f, -0.5f};
        DiracSample s;
        s.data = tiny;
        s.frames = 2;
        rig.eng.SetSample(s);
        DiracParams p = base;
        p.semiShift = 24.0f;
        p.psprd = 12.0f;
        p.density = 16.0f;
        Src src;
        const RenderStats st = Render(rig, p, 1.0f, &src);
        Check(st.finite, "2-frame sample at extreme pitch stays finite");
    }

    /* Very short block (1 frame) — the host may call with an odd size. */
    {
        Rig rig;
        DiracParams p = base;
        float in[1] = {0.4f}, oL[1], oR[1];
        bool ok = true;
        for(int i = 0; i < 5000; ++i)
        {
            rig.eng.Process(in, nullptr, oL, oR, 1, p);
            if(!AllFinite(oL, 1) || !AllFinite(oR, 1))
                ok = false;
        }
        Check(ok, "1-frame blocks render safely");
    }

    return 0;
}

/* ── seam ─────────────────────────────────────────────────────────────── */
/* The edge fade guards two seams: the live read/write collision and the sample
 * loop boundary. A grain crossing either unfaded is an audible click, so the
 * metric is the largest sample-to-sample step in the output. Detune is on so
 * the R read drifts away from L — the v0.1.17 R-channel bug. */
int ModeSeam()
{
    printf("[seam] click metric at loop boundaries\n");

    /* A 0.5 s sine sample: continuous, so any large step is the seam, not the
     * material. */
    const int n = int(kSR * 0.5f);
    std::vector<float> smp(n);
    for(int i = 0; i < n; ++i)
        smp[i] = 0.7f * sinf(6.28318531f * 220.0f * float(i) / kSR);

    struct Case
    {
        const char *name;
        float detune;
        float binaural;
    };
    const Case cases[] = {
        {"sample loop, mono", 0.0f, 0.0f},
        {"sample loop, detuned R", 1.5f, 0.0f},
        {"sample loop, binaural ITD", 0.0f, 1.0f},
    };

    for(const auto &c : cases)
    {
        Rig rig;
        DiracSample s;
        s.data = smp.data();
        s.frames = n;
        rig.eng.SetSample(s);

        DiracParams p = Defaults();
        p.playhead = 0.995f; // park grains right on the boundary
        p.grainLenSec = 0.2f;
        p.density = 4.0f;
        p.detune = c.detune;
        p.binaural = c.binaural;
        p.spread = 0.0f; // keep the pan out of the metric

        float in[kBlock] = {0.0f}, outL[kBlock], outR[kBlock];
        float prevL = 0.0f, prevR = 0.0f, maxStep = 0.0f;
        bool first = true;
        for(int b = 0; b < int(2.0f * kSR) / kBlock; ++b)
        {
            rig.eng.Process(in, nullptr, outL, outR, kBlock, p);
            for(int i = 0; i < kBlock; ++i)
            {
                if(!first)
                {
                    maxStep = std::max(maxStep, fabsf(outL[i] - prevL));
                    maxStep = std::max(maxStep, fabsf(outR[i] - prevR));
                }
                prevL = outL[i];
                prevR = outR[i];
                first = false;
            }
        }
        /* One sample of a 220 Hz sine at 0.7 steps ~0.02; grain onsets and
         * overlap add to that. 0.15 is comfortably above the material and well
         * below an unfaded seam jump (which lands near full scale). */
        printf("  %-26s max step %.4f\n", c.name, maxStep);
        Check(maxStep < 0.15f, "no unfaded seam discontinuity");
    }

    /* Live mode: grains march toward the write head. */
    {
        Rig rig;
        DiracParams p = Defaults();
        p.playhead = 0.0f; // tightest anchor = closest to the write head
        p.grainLenSec = 0.3f;
        p.density = 4.0f;
        p.detune = 1.5f;
        p.spread = 0.0f;

        Src src;
        float in[kBlock], outL[kBlock], outR[kBlock];
        float prevL = 0.0f, prevR = 0.0f, maxStep = 0.0f;
        bool first = true;
        for(int b = 0; b < int(3.0f * kSR) / kBlock; ++b)
        {
            for(int i = 0; i < kBlock; ++i)
                in[i] = 0.7f * sinf(6.28318531f * 220.0f
                                    * float(b * kBlock + i) / kSR);
            (void)src;
            rig.eng.Process(in, nullptr, outL, outR, kBlock, p);
            for(int i = 0; i < kBlock; ++i)
            {
                if(!first)
                {
                    maxStep = std::max(maxStep, fabsf(outL[i] - prevL));
                    maxStep = std::max(maxStep, fabsf(outR[i] - prevR));
                }
                prevL = outL[i];
                prevR = outR[i];
                first = false;
            }
        }
        printf("  %-26s max step %.4f\n", "live write-head", maxStep);
        Check(maxStep < 0.15f, "no unfaded write-head collision");
    }

    return 0;
}

/* ── snapt ────────────────────────────────────────────────────────────── */
/* Snap should pull grain reads onto transients. Metric: mean distance from each
 * spawn position to the nearest onset in a click track. */
int ModeSnap()
{
    printf("[snapt] onset locking\n");

    std::vector<uint32_t> onsets;
    const std::vector<float> track = ClickTrack(onsets);

    auto meanDist = [&](float snap, float posJtr) {
        Rig rig;
        DiracSample s;
        s.data = track.data();
        s.frames = int(track.size());
        rig.eng.SetSample(s);

        DiracParams p = Defaults();
        p.snap = snap;
        p.posJtr = posJtr;
        p.density = 1.0f;
        p.grainLenSec = 0.05f;
        p.playhead = 0.25f;

        std::vector<float> spawns;
        Src src;
        Render(rig, p, 8.0f, &src, nullptr, 0, &spawns);

        if(spawns.empty())
            return -1.0;
        double sum = 0.0;
        for(float sp : spawns)
        {
            double best = 1e30;
            for(uint32_t o : onsets)
                best = std::min(best, fabs(double(sp) - double(o)));
            sum += best;
        }
        return sum / double(spawns.size());
    };

    /* posJtr 1.0 sprays +/-4800 samples but the onset search only reaches
     * +/-3600, so the grains thrown furthest are unreachable by construction
     * and cap the achievable ratio. posJtr 0.5 (+/-2400, inside the window) is
     * the fair measure of the search itself.
     *
     * Neither figure reaches the 3.2x the ER-301 README reports, and that is
     * expected rather than a port defect: the coarse stage tests 12 candidates
     * across +/-3600 (a 654-sample grid), but the energy-rise score is only
     * positive over roughly the 240 samples leading into an onset — past that
     * the `before` taps start catching the transient themselves and the score
     * goes negative. So stage 1 lands in the positive region about a third of
     * the time, and `bestScore > 0` makes the rest spawn unsnapped. The
     * geometry (kCand 12, kTaps 5, kStep 40, win 3600) is identical to the
     * original, so the hit rate is a property of the algorithm and of the
     * material, not of this port. Verified against the reference source. */
    const double freeWide = meanDist(0.0f, 1.0f);
    const double lockWide = meanDist(1.0f, 1.0f);
    const double freeIn = meanDist(0.0f, 0.5f);
    const double lockIn = meanDist(1.0f, 0.5f);
    printf("  spray +/-4800 (wider than the search): %.0f -> %.0f  = %.2fx\n",
           freeWide, lockWide, (lockWide > 0.0) ? freeWide / lockWide : 0.0);
    printf("  spray +/-2400 (inside the search):     %.0f -> %.0f  = %.2fx\n",
           freeIn, lockIn, (lockIn > 0.0) ? freeIn / lockIn : 0.0);
    Check(freeWide > 0.0 && lockWide > 0.0, "grains spawned in both runs");
    Check(lockWide < freeWide, "snap 1 lands closer to onsets than snap 0");
    Check(lockWide * 1.5 < freeWide, "wide spray: >1.5x closer to onsets");
    Check(lockIn * 1.5 < freeIn, "in-window spray: >1.5x closer to onsets");
    return 0;
}

/* ── cpu ──────────────────────────────────────────────────────────────── */
int ModeCpu()
{
    printf("[cpu] realtime factor (host, not the M7 — relative cost only)\n");

    struct Case
    {
        const char *name;
        DiracParams p;
    };
    std::vector<Case> cases;
    {
        DiracParams p = Defaults();
        cases.push_back({"defaults (12 grains, dens 3)", p});
    }
    {
        DiracParams p = Defaults();
        p.grains = 16;
        p.density = 16.0f;
        p.grainLenSec = 0.05f;
        cases.push_back({"16 grains, dens 16", p});
    }
    {
        DiracParams p = Defaults();
        p.grains = 16;
        p.density = 16.0f;
        p.binaural = 1.0f;
        p.color = 1.0f;
        p.detune = 1.0f;
        p.diffuse = true;
        p.feedback = 0.8f;
        p.compress = 1.0f;
        p.snap = 1.0f;
        p.multiHead = true;
        p.heads = 4;
        cases.push_back({"worst case: everything on", p});
    }

    for(auto &c : cases)
    {
        Rig rig;
        Src src;
        const float secs = 10.0f;
        const clock_t t0 = clock();
        Render(rig, c.p, secs, &src);
        const double dt = double(clock() - t0) / CLOCKS_PER_SEC;
        printf("  %-32s %6.2f s audio in %5.3f s  = %6.1fx realtime\n", c.name,
               secs, dt, secs / dt);
    }
    return 0;
}

/* ── wav ──────────────────────────────────────────────────────────────── */
struct MemReader : dirac::WavReader
{
    const std::vector<uint8_t> &b;
    uint32_t pos = 0;
    explicit MemReader(const std::vector<uint8_t> &v) : b(v) {}
    uint32_t Read(void *dst, uint32_t bytes) override
    {
        const uint32_t avail = (pos < b.size()) ? uint32_t(b.size()) - pos : 0;
        const uint32_t n = (bytes < avail) ? bytes : avail;
        if(n)
            memcpy(dst, b.data() + pos, n);
        pos += n;
        return n;
    }
    bool Seek(uint32_t p) override
    {
        if(p > b.size())
            return false;
        pos = p;
        return true;
    }
    uint32_t Size() const override { return uint32_t(b.size()); }
};

void Put32(std::vector<uint8_t> &v, uint32_t x)
{
    v.push_back(uint8_t(x));
    v.push_back(uint8_t(x >> 8));
    v.push_back(uint8_t(x >> 16));
    v.push_back(uint8_t(x >> 24));
}
void Put16(std::vector<uint8_t> &v, uint16_t x)
{
    v.push_back(uint8_t(x));
    v.push_back(uint8_t(x >> 8));
}
void PutTag(std::vector<uint8_t> &v, const char *s)
{
    for(int i = 0; i < 4; ++i)
        v.push_back(uint8_t(s[i]));
}

std::vector<uint8_t> MakeWav(uint16_t fmt, uint16_t ch, uint16_t bits,
                             uint32_t rate, uint32_t frames, bool listChunk)
{
    std::vector<uint8_t> v;
    PutTag(v, "RIFF");
    Put32(v, 0); // patched below
    PutTag(v, "WAVE");
    if(listChunk)
    { // odd-sized junk chunk before fmt, to exercise the pad byte
        PutTag(v, "LIST");
        Put32(v, 5);
        for(int i = 0; i < 5; ++i)
            v.push_back(0xAA);
        v.push_back(0); // pad
    }
    PutTag(v, "fmt ");
    Put32(v, 16);
    Put16(v, fmt);
    Put16(v, ch);
    Put32(v, rate);
    Put32(v, rate * ch * (bits / 8));
    Put16(v, uint16_t(ch * (bits / 8)));
    Put16(v, bits);
    PutTag(v, "data");
    const uint32_t dataBytes = frames * ch * (bits / 8);
    Put32(v, dataBytes);
    for(uint32_t i = 0; i < dataBytes; ++i)
        v.push_back(uint8_t(i & 0xFF));
    const uint32_t riff = uint32_t(v.size()) - 8;
    v[4] = uint8_t(riff);
    v[5] = uint8_t(riff >> 8);
    v[6] = uint8_t(riff >> 16);
    v[7] = uint8_t(riff >> 24);
    return v;
}

int ModeWav()
{
    printf("[wav] header parsing\n");

    auto parse = [](const std::vector<uint8_t> &v, dirac::WavInfo &info) {
        MemReader r(v);
        return dirac::ParseWav(r, info);
    };

    struct Good
    {
        const char *name;
        uint16_t fmt, ch, bits;
        uint32_t rate;
        bool list;
    };
    const Good goods[] = {
        {"16-bit mono 48k", 1, 1, 16, 48000, false},
        {"16-bit stereo 44.1k", 1, 2, 16, 44100, false},
        {"24-bit stereo 48k", 1, 2, 24, 48000, false},
        {"32-bit float mono", 3, 1, 32, 48000, false},
        {"8-bit mono", 1, 1, 8, 22050, false},
        {"16-bit with odd LIST chunk", 1, 1, 16, 48000, true},
    };
    for(const auto &g : goods)
    {
        dirac::WavInfo info;
        const auto v = MakeWav(g.fmt, g.ch, g.bits, g.rate, 1000, g.list);
        const auto r = parse(v, info);
        char msg[96];
        snprintf(msg, sizeof(msg), "%s -> %s, %u frames @ %u", g.name,
                 dirac::WavResultName(r), info.frames, info.sampleRate);
        Check(r == dirac::WavResult::Ok && info.frames == 1000
                  && info.sampleRate == g.rate && info.channels == g.ch,
              msg);
    }

    /* Malformed inputs must be REFUSED, never loaded as garbage. */
    {
        dirac::WavInfo info;
        std::vector<uint8_t> v = MakeWav(1, 1, 16, 48000, 100, false);

        auto expect = [&](std::vector<uint8_t> b, dirac::WavResult want,
                          const char *what) {
            dirac::WavInfo i2;
            MemReader r(b);
            const auto got = dirac::ParseWav(r, i2);
            char msg[96];
            snprintf(msg, sizeof(msg), "%s -> %s", what,
                     dirac::WavResultName(got));
            Check(got == want, msg);
        };

        expect({}, dirac::WavResult::NotRiff, "empty file");
        expect({'R', 'I', 'F', 'F'}, dirac::WavResult::NotRiff, "4-byte file");
        {
            auto b = v;
            b[8] = 'X'; // WAVE -> XAVE
            expect(b, dirac::WavResult::NotRiff, "bad WAVE magic");
        }
        {
            auto b = v;
            b.resize(20); // header cut short
            expect(b, dirac::WavResult::Truncated, "truncated mid-fmt chunk");
        }
        {
            auto b = MakeWav(1, 1, 12, 48000, 100, false); // 12-bit
            expect(b, dirac::WavResult::Unsupported, "12-bit depth");
        }
        {
            auto b = MakeWav(0x11, 1, 16, 48000, 100, false); // IMA ADPCM tag
            expect(b, dirac::WavResult::Unsupported, "ADPCM format tag");
        }
        {
            auto b = v; // data chunk claims far more than the file holds
            b[b.size() - 100 - 4] = 0xFF;
            MemReader r(b);
            dirac::WavInfo i2;
            const auto got = dirac::ParseWav(r, i2);
            const bool ok = (got != dirac::WavResult::Ok)
                            || (i2.dataOffset + i2.dataBytes <= b.size());
            Check(ok, "oversized data chunk is clamped to the file");
        }
        {
            auto b = v;
            // zero-size chunk before data must not spin the parser
            std::vector<uint8_t> ins;
            PutTag(ins, "junk");
            Put32(ins, 0);
            b.insert(b.begin() + 12, ins.begin(), ins.end());
            expect(b, dirac::WavResult::Ok, "zero-size chunk before fmt");
        }
        (void)info;
    }

    /* Decode round trip: a known 16-bit stereo pattern must mono-sum. */
    {
        dirac::WavInfo info;
        info.channels = 2;
        info.bits = 16;
        info.format = 1;
        int16_t pcm[4] = {32767, -32768, 16384, 16384};
        float out[2];
        dirac::WavDecodeMono(reinterpret_cast<const uint8_t *>(pcm), 2, info, out);
        Check(fabsf(out[0] - (-0.00001526f)) < 1e-3f
                  && fabsf(out[1] - 0.5f) < 1e-3f,
              "16-bit stereo decodes and mono-sums");
    }

    return 0;
}

/* ── tanh ─────────────────────────────────────────────────────────────── */
/* The firmware replaces libm tanhf in the feedback saturator to save flash.
 * That is only acceptable if it is genuinely the same curve. */
int ModeTanh()
{
    printf("[tanh] feedback saturator vs libm tanhf\n");

    double worstIn = 0.0, worstOut = 0.0;
    float worstX = 0.0f;
    for(int i = -80000; i <= 80000; ++i)
    {
        const float x = float(i) * 0.0001f; // -8 .. +8
        const double ref = tanh(double(x));
        const double err = fabs(double(dirac::FastTanh(x)) - ref);
        if(fabsf(x) <= 3.0f)
        {
            if(err > worstIn)
            {
                worstIn = err;
                worstX = x;
            }
        }
        else if(err > worstOut)
        {
            worstOut = err;
        }
    }
    printf("  max error |x|<=3: %.3e (at x=%.3f)   |x|>3: %.3e\n", worstIn,
           worstX, worstOut);
    /* Measured 1.05e-6 at the edge of the range (~-120 dBFS, below a 24-bit
     * converter's noise floor and near float epsilon at this magnitude). */
    Check(worstIn < 5e-6, "matches libm to <5e-6 over the working range");
    /* Beyond |x| = 4 the clamp is exact-ish; the gap is tanh(4) vs 1. */
    Check(worstOut < 1e-3, "clamped tail within 0.001 of libm");
    Check(dirac::FastTanh(0.0f) == 0.0f, "odd symmetry preserved at 0");
    Check(fabsf(dirac::FastTanh(-1.7f) + dirac::FastTanh(1.7f)) < 1e-7f,
          "odd symmetry preserved");
    return 0;
}

/* ── sample-mode smoke test ───────────────────────────────────────────── */
int ModeSample()
{
    printf("[sample] sample mode and rate ratio\n");

    const int n = int(kSR);
    std::vector<float> smp(n);
    for(int i = 0; i < n; ++i)
        smp[i] = 0.5f * sinf(6.28318531f * 440.0f * float(i) / kSR);

    {
        Rig rig;
        DiracSample s;
        s.data = smp.data();
        s.frames = n;
        rig.eng.SetSample(s);
        Check(rig.eng.IsSampleMode(), "attaching a sample enters sample mode");

        DiracParams p = Defaults();
        p.playhead = 0.5f;
        Src src;
        const RenderStats st = Render(rig, p, 1.0f, &src);
        Check(st.finite && st.rms > 1e-3, "sample mode renders audio");

        /* Detach must return to live mode and keep rendering. */
        rig.eng.SetSample(DiracSample{});
        Check(!rig.eng.IsSampleMode(), "detaching returns to live mode");
        const RenderStats st2 = Render(rig, p, 1.0f, &src);
        Check(st2.finite, "live mode after detach stays finite");
    }

    /* Sample-mode isolation: with a silent sample and feedback up, lane input
     * must NOT bleed into the cloud (er301 v0.2.6 fix). */
    {
        Rig rig;
        std::vector<float> silent(n, 0.0f);
        DiracSample s;
        s.data = silent.data();
        s.frames = n;
        rig.eng.SetSample(s);

        DiracParams p = Defaults();
        p.feedback = 0.6f;
        Src src;
        const RenderStats st = Render(rig, p, 2.0f, &src);
        printf("  silent sample + live input + fdbk 0.6: rms %.8f\n", st.rms);
        Check(st.rms < 1e-6, "sample mode does not capture the input");
    }

    /* Speed: the scan should move the playhead through the sample. */
    {
        Rig rig;
        DiracSample s;
        s.data = smp.data();
        s.frames = n;
        rig.eng.SetSample(s);
        DiracParams p = Defaults();
        p.playhead = 0.0f;
        p.speed = 1.0f;
        Src src;
        Render(rig, p, 0.5f, &src);
        const float ph = rig.eng.GetPlayheadEff();
        printf("  speed 1.0 for 0.5 s -> playhead %.3f\n", ph);
        Check(ph > 0.3f && ph < 0.7f, "speed scans at ~original tempo");
    }

    /* Rate ratio: a 24 kHz file should read at half speed. */
    {
        Rig rig;
        DiracSample s;
        s.data = smp.data();
        s.frames = n;
        s.rateRatio = 24000.0f / 48000.0f;
        rig.eng.SetSample(s);
        DiracParams p = Defaults();
        Src src;
        const RenderStats st = Render(rig, p, 0.5f, &src);
        Check(st.finite && st.rms > 1e-3, "rate-ratio sample renders");
    }

    return 0;
}

/* ── trigger smoke test ───────────────────────────────────────────────── */
int ModeTrig()
{
    printf("[trig] trigger-only spawning\n");

    Rig rig;
    DiracParams p = Defaults();
    p.density = 0.0f; // free-run off
    p.grainLenSec = 0.02f;

    float in[kBlock], trig[kBlock], outL[kBlock], outR[kBlock];
    Src src;
    int spawned = 0;
    bool wasActive[DiracEngine::kMaxGrains] = {false};

    for(int b = 0; b < 200; ++b)
    {
        for(int s = 0; s < kBlock; ++s)
        {
            in[s] = src.Next();
            trig[s] = 0.0f;
        }
        if(b % 10 == 0)
            trig[0] = 1.0f; // one rising edge every 10 blocks
        rig.eng.Process(in, trig, outL, outR, kBlock, p);
        for(int i = 0; i < DiracEngine::kMaxGrains; ++i)
        {
            const bool a = rig.eng.IsGrainActive(i);
            if(a && !wasActive[i])
                spawned++;
            wasActive[i] = a;
        }
    }
    printf("  20 triggers -> %d grains\n", spawned);
    Check(spawned == 20, "one grain per rising edge, none free-run");

    /* Held-high trigger must not retrigger. */
    {
        Rig rig2;
        int n2 = 0;
        bool wa[DiracEngine::kMaxGrains] = {false};
        for(int b = 0; b < 50; ++b)
        {
            for(int s = 0; s < kBlock; ++s)
            {
                in[s] = 0.0f;
                trig[s] = 1.0f; // stuck high
            }
            rig2.eng.Process(in, trig, outL, outR, kBlock, p);
            for(int i = 0; i < DiracEngine::kMaxGrains; ++i)
            {
                const bool a = rig2.eng.IsGrainActive(i);
                if(a && !wa[i])
                    n2++;
                wa[i] = a;
            }
        }
        printf("  stuck-high gate -> %d grains\n", n2);
        Check(n2 == 1, "a held gate fires exactly once");
    }
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    const std::string mode = (argc > 1) ? argv[1] : "all";
    const bool all = (mode == "all");

    if(all || mode == "ident")
        ModeIdent();
    if(all || mode == "torture" || mode == "asan")
        ModeTorture();
    if(all || mode == "seam")
        ModeSeam();
    if(all || mode == "snapt")
        ModeSnap();
    if(all || mode == "tanh")
        ModeTanh();
    if(all || mode == "sample")
        ModeSample();
    if(all || mode == "trig")
        ModeTrig();
    if(all || mode == "wav")
        ModeWav();
    if(mode == "cpu")
        ModeCpu();

    printf("\n%s: %d failure(s)\n", gFail ? "FAILED" : "OK", gFail);
    return gFail ? 1 : 0;
}
