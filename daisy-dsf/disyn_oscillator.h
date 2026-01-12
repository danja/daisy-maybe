#pragma once

#include "disyn_algorithms.h"

namespace disyn {

class DisynOscillator {
public:
    explicit DisynOscillator(float sampleRate = 48000.0f)
        : sampleRate(sampleRate),
          algorithmType(AlgorithmType::TANH_SQUARE),
          frequency(440.0f),
          param1(0.5f),
          param2(0.5f),
          param3(0.5f),
          dirichlet(sampleRate),
          dsfSingle(sampleRate),
          dsfDouble(sampleRate),
          tanhSquare(sampleRate),
          tanhSaw(sampleRate),
          paf(sampleRate),
          modfm(sampleRate),
          combination1(sampleRate),
          combination2(sampleRate),
          combination3(sampleRate),
          combination4(sampleRate),
          combination5(sampleRate),
          combination6(sampleRate),
          combination7(sampleRate),
          novel1(sampleRate),
          novel2(sampleRate),
          novel3(sampleRate),
          novel4(sampleRate),
          trajectory(sampleRate),
          fallbackPhase(0.0f) {}

    void Init(float sampleRateIn) {
        *this = DisynOscillator(sampleRateIn);
    }

    void Reset() {
        fallbackPhase = 0.0f;
        dirichlet.Reset();
        dsfSingle.Reset();
        dsfDouble.Reset();
        tanhSquare.Reset();
        tanhSaw.Reset();
        paf.Reset();
        modfm.Reset();
        combination1.Reset();
        combination2.Reset();
        combination3.Reset();
        combination4.Reset();
        combination5.Reset();
        combination6.Reset();
        combination7.Reset();
        novel1.Reset();
        novel2.Reset();
        novel3.Reset();
        novel4.Reset();
        trajectory.Reset();
    }

    void SetAlgorithm(int type) {
        if (type < 0 || type > 18) {
            return;
        }
        algorithmType = static_cast<AlgorithmType>(type);
    }

    void SetFrequency(float freq) {
        frequency = std::max(freq, 0.0f);
    }

    void SetParam1(float value) { param1 = std::clamp(value, 0.0f, 1.0f); }
    void SetParam2(float value) { param2 = std::clamp(value, 0.0f, 1.0f); }
    void SetParam3(float value) { param3 = std::clamp(value, 0.0f, 1.0f); }

    AlgorithmOutput Process() {
        switch (algorithmType) {
            case AlgorithmType::DIRICHLET_PULSE:
                return dirichlet.Process(frequency, param1, param2, param3);
            case AlgorithmType::DSF_SINGLE:
                return dsfSingle.Process(frequency, param1, param2, param3);
            case AlgorithmType::DSF_DOUBLE:
                return dsfDouble.Process(frequency, param1, param2, param3);
            case AlgorithmType::TANH_SQUARE:
                return tanhSquare.Process(frequency, param1, param2, param3);
            case AlgorithmType::TANH_SAW:
                return tanhSaw.Process(frequency, param1, param2, param3);
            case AlgorithmType::PAF:
                return paf.Process(frequency, param1, param2, param3);
            case AlgorithmType::MOD_FM:
                return modfm.Process(frequency, param1, param2, param3);
            case AlgorithmType::COMBINATION_1_HYBRID_FORMANT:
                return combination1.Process(frequency, param1, param2, param3);
            case AlgorithmType::COMBINATION_2_CASCADED:
                return combination2.Process(frequency, param1, param2, param3);
            case AlgorithmType::COMBINATION_3_PARALLEL_BANK:
                return combination3.Process(frequency, param1, param2, param3);
            case AlgorithmType::COMBINATION_4_FEEDBACK:
                return combination4.Process(frequency, param1, param2, param3);
            case AlgorithmType::COMBINATION_5_MORPHING:
                return combination5.Process(frequency, param1, param2, param3);
            case AlgorithmType::COMBINATION_6_INHARMONIC:
                return combination6.Process(frequency, param1, param2, param3);
            case AlgorithmType::COMBINATION_7_ADAPTIVE_FILTER:
                return combination7.Process(frequency, param1, param2, param3);
            case AlgorithmType::NOVEL_1_MULTISTAGE:
                return novel1.Process(frequency, param1, param2, param3);
            case AlgorithmType::NOVEL_2_FREQ_ASYMMETRY:
                return novel2.Process(frequency, param1, param2, param3);
            case AlgorithmType::NOVEL_3_CROSS_MOD:
                return novel3.Process(frequency, param1, param2, param3);
            case AlgorithmType::NOVEL_4_TAYLOR:
                return novel4.Process(frequency, param1, param2, param3);
            case AlgorithmType::TRAJECTORY:
                return trajectory.Process(frequency, param1, param2, param3);
            default:
                return ProcessSine();
        }
    }

private:
    AlgorithmOutput ProcessSine() {
        fallbackPhase = StepPhase(fallbackPhase, frequency, sampleRate);
        const float output = std::sin(fallbackPhase * kTwoPi);
        return {output, output};
    }

    float sampleRate;
    AlgorithmType algorithmType;
    float frequency;
    float param1;
    float param2;
    float param3;

    DirichletPulseAlgorithm dirichlet;
    DSFSingleAlgorithm dsfSingle;
    DSFDoubleAlgorithm dsfDouble;
    TanhSquareAlgorithm tanhSquare;
    TanhSawAlgorithm tanhSaw;
    PAFAlgorithm paf;
    ModFMAlgorithm modfm;
    Combination1HybridFormantAlgorithm combination1;
    Combination2CascadedAlgorithm combination2;
    Combination3ParallelBankAlgorithm combination3;
    Combination4FeedbackAlgorithm combination4;
    Combination5MorphingAlgorithm combination5;
    Combination6InharmonicAlgorithm combination6;
    Combination7AdaptiveFilterAlgorithm combination7;
    Novel1MultistageAlgorithm novel1;
    Novel2FreqAsymmetryAlgorithm novel2;
    Novel3CrossModAlgorithm novel3;
    Novel4TaylorAlgorithm novel4;
    TrajectoryAlgorithm trajectory;

    float fallbackPhase;
};

} // namespace disyn
