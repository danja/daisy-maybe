/* dirac_app.cpp — hardware lifecycle, audio callback, and main loop.
 * SPDX-License-Identifier: Apache-2.0 */

#include "dirac_app.h"

#include <cmath>

namespace dirac
{

namespace
{
/* ── SDRAM ────────────────────────────────────────────────────────────────
 * The Daisy Seed has 64 MB of SDRAM and nothing else in this repo uses it.
 *   • capture ring: 262144 frames = 5.46 s at 48 kHz (double the ER-301's,
 *     which was bounded by its host). MUST stay a power of two — the render
 *     loop masks rather than wraps.
 *   • sample store: 8 M frames of mono float = 32 MB = ~2.9 minutes at 48 kHz.
 * Both are fixed allocations; there is no malloc anywhere in this firmware. */
constexpr int kCapSize = 1 << 18;
constexpr uint32_t kStoreFrames = 8u * 1024u * 1024u;

float DSY_SDRAM_BSS gCapBuf[kCapSize];
float DSY_SDRAM_BSS gSampleStore[kStoreFrames];

constexpr size_t kBlockSize = 48;

inline float Clampf(float v, float lo, float hi)
{
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}
} // namespace

void DiracApp::Init()
{
    hw_.Init();
    hw_.SetAudioBlockSize(kBlockSize);
    hw_.StartAdc();

    const float sr = hw_.AudioSampleRate();
    engine_.Init(sr, gCapBuf, kCapSize);
    params_.Reset();
    store_.Init(gSampleStore, kStoreFrames);
    ui_.Init();
    midi_.Init(hw_);
    cpuMeter_.Init(sr, kBlockSize);

    /* V/Oct calibration lives in QSPI, same pattern as resonators. */
    static daisy::PersistentStorage<CalibSettings> storage(hw_.seed.qspi);
    CalibSettings defaults{1000, 0};
    storage.Init(defaults);
    calStore_ = &storage;
    savedCal_ = storage.GetSettings();
    params_.setting[S_CAL_SCALE] = savedCal_.scale;
    params_.setting[S_CAL_OFS] = savedCal_.offset;

    ctx_.params = &params_;
    ctx_.store = &store_;
    ctx_.engine = &engine_;

    /* The card is optional: with no card Dirac is a live granulator. */
    if(store_.Mount())
        store_.Scan();

    lastHeartbeatMs_ = daisy::System::GetNow();
}

void DiracApp::StartAudio(daisy::AudioHandle::AudioCallback cb)
{
    hw_.StartAudio(cb);
}

void DiracApp::ProcessAudio(daisy::AudioHandle::InputBuffer in,
                            daisy::AudioHandle::OutputBuffer out, size_t size)
{
    cpuMeter_.OnBlockStart();

    float mono[DiracEngine::kMaxBlock];
    float trig[DiracEngine::kMaxBlock];
    float wetL[DiracEngine::kMaxBlock];
    float wetR[DiracEngine::kMaxBlock];

    const size_t n = (size > DiracEngine::kMaxBlock) ? DiracEngine::kMaxBlock : size;
    const int inrMode = params_.setting[S_INR_MODE];

    /* IN-L is always the capture source. IN-R is mode-switched, because the
     * module has no gate input and a granulator badly wants one. */
    for(size_t i = 0; i < n; ++i)
    {
        mono[i] = (inrMode == INR_CAPTURE) ? 0.5f * (in[0][i] + in[1][i])
                                           : in[0][i];
        trig[i] = 0.0f;
    }

    /* Queued triggers (MIDI notes, MIDI clock) fire one grain each. One edge
     * per block: the engine's edge detector needs the level to return low in
     * between, which it does because the rest of the block is zero. */
    if(pendingTrigs_ > 0)
    {
        pendingTrigs_--;
        trig[0] = 1.0f;
    }

    if(inrMode == INR_TRIGGER)
    {
        /* Audio inputs are AC-coupled, so a sustained gate droops — but a
         * trigger pulse is a clean edge. Schmitt levels plus a short lockout
         * keep one pulse from spawning a burst. */
        for(size_t i = 0; i < n; ++i)
        {
            const float v = in[1][i];
            if(inrLockout_ > 0)
            {
                inrLockout_--;
            }
            else if(inrArmed_ && v > 0.15f)
            {
                trig[i] = 1.0f;
                inrArmed_ = false;
                inrLockout_ = 240; // 5 ms at 48 kHz
            }
            if(!inrArmed_ && v < 0.05f)
                inrArmed_ = true;
        }
    }
    else if(inrMode == INR_MOD)
    {
        /* Envelope follower: a third modulation source with its own
         * destination on the MOD page. */
        float e = modEnv_;
        for(size_t i = 0; i < n; ++i)
        {
            const float a = fabsf(in[1][i]);
            e += (a > e) ? 0.05f * (a - e) : 0.0008f * (a - e);
        }
        modEnv_ = Clampf(e * 2.0f, 0.0f, 1.0f);
    }

    engine_.Process(mono, trig, wetL, wetR, int(n), engineParams_);

    switch(params_.setting[S_OUT_MODE])
    {
        case OUT_WETDRY: // L = cloud, R = dry input, for parallel patching
            for(size_t i = 0; i < n; ++i)
            {
                out[0][i] = 0.5f * (wetL[i] + wetR[i]);
                out[1][i] = in[0][i];
            }
            break;
        case OUT_MONO:
            for(size_t i = 0; i < n; ++i)
            {
                const float m = 0.5f * (wetL[i] + wetR[i]);
                out[0][i] = m;
                out[1][i] = m;
            }
            break;
        case OUT_STEREO:
        default:
            for(size_t i = 0; i < n; ++i)
            {
                out[0][i] = wetL[i];
                out[1][i] = wetR[i];
            }
            break;
    }

    cpuMeter_.OnBlockEnd();
}

void DiracApp::HandleRequests()
{
    if(ctx_.requestRescan)
    {
        ctx_.requestRescan = false;
        store_.Scan();
        if(ctx_.fileSel >= store_.Count())
            ctx_.fileSel = 0;
    }

    if(ctx_.requestDetach)
    {
        ctx_.requestDetach = false;
        engine_.SetSample(DiracSample{});
    }

    if(ctx_.requestLoad)
    {
        ctx_.requestLoad = false;
        /* Detach FIRST. The audio callback keeps running through the load, and
         * this is what stops it reading the store while it is being rewritten:
         * with data = nullptr and frames = 0 the engine falls back to live
         * mode, so every interleaving of the descriptor write is safe. */
        engine_.SetSample(DiracSample{});
        DiracSample s;
        if(store_.Load(ctx_.fileSel, hw_.AudioSampleRate(), s))
            engine_.SetSample(s);
    }
}

void DiracApp::HandleCalibSave()
{
    if(!calStore_)
        return;
    const uint32_t now = daisy::System::GetNow();
    if(params_.setting[S_CAL_SCALE] != savedCal_.scale
       || params_.setting[S_CAL_OFS] != savedCal_.offset)
    {
        if(!calDirty_)
            calChangedMs_ = now;
        calDirty_ = true;
    }
    /* Debounce a second past the last edit so a knob sweep is one write. */
    if(calDirty_ && (now - calChangedMs_) > 1000)
    {
        calStore_->GetSettings().scale = params_.setting[S_CAL_SCALE];
        calStore_->GetSettings().offset = params_.setting[S_CAL_OFS];
        calStore_->Save();
        savedCal_ = calStore_->GetSettings();
        calDirty_ = false;
    }
}

void DiracApp::Update()
{
    hw_.ProcessAnalogControls();
    hw_.ProcessDigitalControls();

    const uint32_t now = daisy::System::GetNow();

    /* ── Encoder: rotate, short press, long press ─────────────────────── */
    const int inc = hw_.encoder.Increment();
    if(inc != 0)
        ui_.OnRotate(inc, ctx_);

    if(hw_.encoder.RisingEdge())
    {
        pressStartMs_ = now;
        longFired_ = false;
    }
    if(hw_.encoder.Pressed() && !longFired_ && (now - pressStartMs_) > 500)
    {
        /* Fire on the threshold rather than on release, so the view flips
         * while the button is still down and the press is unambiguous. */
        ui_.OnLongPress(ctx_);
        longFired_ = true;
    }
    if(hw_.encoder.FallingEdge() && !longFired_)
        ui_.OnShortPress(ctx_);

    /* ── MIDI ─────────────────────────────────────────────────────────── */
    midi_.Process(hw_, params_, ui_.SelectedParam(ctx_));
    midi_.UpdateClock(params_);
    const int trigs = midi_.TakeTriggers();
    if(trigs > 0)
        pendingTrigs_ += trigs;
    const int pc = midi_.TakeProgramChange();
    if(pc >= 0 && pc < store_.Count())
    {
        ctx_.fileSel = pc;
        ctx_.requestLoad = true;
    }

    HandleRequests();

    /* ── Controls → engine parameters ─────────────────────────────────── */
    ModInputs mod;
    mod.knob[0] = hw_.GetKnobValue(kxmx::Bluemchen::CTRL_1);
    mod.knob[1] = hw_.GetKnobValue(kxmx::Bluemchen::CTRL_2);
    mod.cv[0] = hw_.GetKnobValue(kxmx::Bluemchen::CTRL_3);
    mod.cv[1] = hw_.GetKnobValue(kxmx::Bluemchen::CTRL_4);
    mod.midiVoct = midi_.Voct();
    mod.midiLevel = midi_.Level();
    mod.midiGate = midi_.Gate();
    mod.modEnv = modEnv_;

    BuildEngineParams(params_, mod, engineParams_);

    HandleCalibSave();

    /* ── Display at ~30 Hz ────────────────────────────────────────────── */
    if(now - lastHeartbeatMs_ > 250)
    {
        heartbeat_ = !heartbeat_;
        lastHeartbeatMs_ = now;
    }
    if(now - lastDisplayMs_ > 33)
    {
        ctx_.heartbeat = heartbeat_;
        ctx_.cpuLoad = cpuMeter_.GetAvgCpuLoad();
        ui_.Render(hw_, ctx_);
        lastDisplayMs_ = now;
    }
}

} // namespace dirac
