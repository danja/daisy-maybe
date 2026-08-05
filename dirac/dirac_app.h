/* dirac_app.h — hardware lifecycle, audio callback, and main loop.
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "daisy_seed.h"
#include "kxmx_bluemchen.h"
#include "util/PersistentStorage.h"

#include "dirac_engine.h"
#include "dirac_midi.h"
#include "dirac_params.h"
#include "dirac_sample.h"
#include "dirac_ui.h"

namespace dirac
{

/* V/Oct tracking, persisted to QSPI so a calibrated module stays calibrated. */
struct CalibSettings
{
    int scale;
    int offset;
    bool operator!=(const CalibSettings &rhs) const
    {
        return scale != rhs.scale || offset != rhs.offset;
    }
};

class DiracApp
{
  public:
    void Init();
    void StartAudio(daisy::AudioHandle::AudioCallback cb);
    void Update();
    void ProcessAudio(daisy::AudioHandle::InputBuffer in,
                      daisy::AudioHandle::OutputBuffer out, size_t size);

  private:
    kxmx::Bluemchen hw_{};
    DiracEngine engine_{};
    ParamState params_{};
    SampleStore store_{};
    DiracMidi midi_{};
    DiracUi ui_{};
    UiContext ctx_{};

    /* The audio callback reads this; the main loop writes it. Each field is a
     * word-sized scalar, so a torn read costs at most one block of a stale
     * value — no locking needed and none affordable. */
    DiracParams engineParams_{};

    daisy::CpuLoadMeter cpuMeter_{};
    daisy::PersistentStorage<CalibSettings> *calStore_ = nullptr;
    CalibSettings savedCal_{1000, 0};
    uint32_t calChangedMs_ = 0;
    bool calDirty_ = false;

    /* Trigger plumbing: MIDI and the IN-R edge detector both queue grains,
     * consumed one per block by the callback. */
    volatile int pendingTrigs_ = 0;
    bool inrArmed_ = true;
    uint32_t inrLockout_ = 0;
    float modEnv_ = 0.0f;

    bool heartbeat_ = false;
    uint32_t lastHeartbeatMs_ = 0;
    uint32_t lastDisplayMs_ = 0;
    int lastEncPress_ = 0;
    uint32_t pressStartMs_ = 0;
    bool longFired_ = false;

    void HandleRequests();
    void HandleCalibSave();
};

} // namespace dirac
