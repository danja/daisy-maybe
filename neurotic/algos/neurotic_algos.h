#pragma once

#include "daisysp.h"
#include "neurotic_state.h"

class AlgoNcr;
class AlgoLsb;
class AlgoNth;
class AlgoBgm;
class AlgoNff;
class AlgoNdm;
class AlgoNes;
class AlgoNhc;
class AlgoNpl;
class AlgoNmg;
class AlgoNsm;

class NeuroticAlgoBank
{
public:
    void Init(float sampleRate);
    void Reset(int algoIndex);
    void Process(int algoIndex, float inL, float inR, const NeuroticRuntime &rt, float &outL, float &outR);

private:
    float sampleRate_ = 48000.0f;

    AlgoNcr *ncr_ = nullptr;
    AlgoLsb *lsb_ = nullptr;
    AlgoNth *nth_ = nullptr;
    AlgoBgm *bgm_ = nullptr;
    AlgoNff *nff_ = nullptr;
    AlgoNdm *ndm_ = nullptr;
    AlgoNes *nes_ = nullptr;
    AlgoNhc *nhc_ = nullptr;
    AlgoNpl *npl_ = nullptr;
    AlgoNmg *nmg_ = nullptr;
    AlgoNsm *nsm_ = nullptr;
};
