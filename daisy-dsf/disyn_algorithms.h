#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "disyn_algorithm_output.h"
#include "disyn_algorithm_utils.h"

namespace disyn {

enum class AlgorithmType : int {
    DIRICHLET_PULSE = 0,
    DSF_SINGLE = 1,
    DSF_DOUBLE = 2,
    TANH_SQUARE = 3,
    TANH_SAW = 4,
    PAF = 5,
    MOD_FM = 6,
    COMBINATION_1_HYBRID_FORMANT = 7,
    COMBINATION_2_CASCADED = 8,
    COMBINATION_3_PARALLEL_BANK = 9,
    COMBINATION_4_FEEDBACK = 10,
    COMBINATION_5_MORPHING = 11,
    COMBINATION_6_INHARMONIC = 12,
    COMBINATION_7_ADAPTIVE_FILTER = 13,
    NOVEL_1_MULTISTAGE = 14,
    NOVEL_2_FREQ_ASYMMETRY = 15,
    NOVEL_3_CROSS_MOD = 16,
    NOVEL_4_TAYLOR = 17,
    TRAJECTORY = 18
};

class DirichletPulseAlgorithm {
public:
    explicit DirichletPulseAlgorithm(float sampleRate)
        : sampleRate(sampleRate), phase(0.0f) {}

    void Reset() { phase = 0.0f; }

    AlgorithmOutput Process(float pitch, float param1, float param2, float param3) {
        const int harmonics = std::max(1, static_cast<int>(std::round(1.0f + param1 * 63.0f)));
        const float tilt = -3.0f + param2 * 18.0f;
        const float shape = std::clamp(param3, 0.0f, 1.0f);

        phase = StepPhase(phase, pitch, sampleRate);
        const float theta = phase * kTwoPi;

        const float numerator = std::sin((2.0f * harmonics + 1.0f) * theta * 0.5f);
        const float denominator = std::sin(theta * 0.5f);

        float value = 1.0f;
        if (std::abs(denominator) >= kEpsilon) {
            value = (numerator / denominator) - 1.0f;
        }

        const float tiltFactor = std::pow(10.0f, tilt / 20.0f);
        const float base = (value / static_cast<float>(harmonics)) * tiltFactor;
        const float shaped = std::tanh(base * (1.0f + shape * 4.0f));
        const float output = base * (1.0f - shape) + shaped * shape;
        return {output, base};
    }

private:
    float sampleRate;
    float phase;
};

class DSFSingleAlgorithm {
public:
    explicit DSFSingleAlgorithm(float sampleRate)
        : sampleRate(sampleRate), phase(0.0f), secondaryPhase(0.0f) {}

    void Reset() {
        phase = 0.0f;
        secondaryPhase = 0.0f;
    }

    AlgorithmOutput Process(float pitch, float param1, float param2, float param3) {
        const float decay = std::min(param1 * 0.98f, 0.98f);
        const float ratio = ExpoMap(param2, 0.5f, 4.0f);
        const float mix = std::clamp(param3, 0.0f, 1.0f);

        phase = StepPhase(phase, pitch, sampleRate);
        secondaryPhase = StepPhase(secondaryPhase, pitch * ratio, sampleRate);

        const float w = phase * kTwoPi;
        const float t = secondaryPhase * kTwoPi;

        const float dsf = ComputeDSFComponent(w, t, decay) * 0.5f;
        const float sine = std::sin(w) * 0.5f;
        const float output = dsf * (1.0f - mix) + sine * mix;
        return {output, dsf};
    }

private:
    float sampleRate;
    float phase;
    float secondaryPhase;
};

class DSFDoubleAlgorithm {
public:
    explicit DSFDoubleAlgorithm(float sampleRate)
        : sampleRate(sampleRate), phase(0.0f), secondaryPhase(0.0f), secondaryPhaseNeg(0.0f) {}

    void Reset() {
        phase = 0.0f;
        secondaryPhase = 0.0f;
        secondaryPhaseNeg = 0.0f;
    }

    AlgorithmOutput Process(float pitch, float param1, float param2, float param3) {
        const float decay = std::min(param1 * 0.96f, 0.96f);
        const float ratio = ExpoMap(param2, 0.5f, 4.5f);
        const float balance = std::clamp(param3, 0.0f, 1.0f) * 2.0f - 1.0f;
        const float weightPos = 0.5f + balance * 0.5f;
        const float weightNeg = 1.0f - weightPos;

        phase = StepPhase(phase, pitch, sampleRate);
        secondaryPhase = StepPhase(secondaryPhase, pitch * ratio, sampleRate);
        secondaryPhaseNeg = StepPhase(secondaryPhaseNeg, pitch * ratio, sampleRate);

        const float w = phase * kTwoPi;
        const float tPos = secondaryPhase * kTwoPi;
        const float tNeg = -secondaryPhaseNeg * kTwoPi;

        const float positive = ComputeDSFComponent(w, tPos, decay);
        const float negative = ComputeDSFComponent(w, tNeg, decay);

        const float output = 0.5f * (positive * weightPos + negative * weightNeg);
        const float secondary = 0.5f * (positive - negative);
        return {output, secondary};
    }

private:
    float sampleRate;
    float phase;
    float secondaryPhase;
    float secondaryPhaseNeg;
};

class TanhSquareAlgorithm {
public:
    explicit TanhSquareAlgorithm(float sampleRate)
        : sampleRate(sampleRate), phase(0.0f) {}

    void Reset() { phase = 0.0f; }

    AlgorithmOutput Process(float pitch, float param1, float param2, float param3) {
        const float drive = ExpoMap(param1, 0.05f, 5.0f);
        const float trim = ExpoMap(param2, 0.2f, 1.2f);
        const float bias = (std::clamp(param3, 0.0f, 1.0f) - 0.5f) * 0.8f;

        phase = StepPhase(phase, pitch, sampleRate);
        const float carrier = std::sin(phase * kTwoPi) + bias;
        const float output = std::tanh(carrier * drive) * trim;
        const float secondary = std::tanh(std::sin(phase * kTwoPi) * drive) * trim;
        return {output, secondary};
    }

private:
    float sampleRate;
    float phase;
};

class TanhSawAlgorithm {
public:
    explicit TanhSawAlgorithm(float sampleRate)
        : sampleRate(sampleRate), phase(0.0f), secondaryPhase(0.0f) {}

    void Reset() {
        phase = 0.0f;
        secondaryPhase = 0.0f;
    }

    AlgorithmOutput Process(float pitch, float param1, float param2, float param3) {
        const float drive = ExpoMap(param1, 0.05f, 4.5f);
        const float blend = std::clamp(param2, 0.0f, 1.0f);
        const float edge = 0.5f + std::clamp(param3, 0.0f, 1.0f) * 1.5f;

        phase = StepPhase(phase, pitch, sampleRate);
        const float sine = std::sin(phase * kTwoPi);
        const float square = std::tanh(sine * drive);

        secondaryPhase = StepPhase(secondaryPhase, pitch, sampleRate);
        const float cosine = std::cos(secondaryPhase * kTwoPi);
        const float saw = square + cosine * (1.0f - square * square) * edge;

        const float output = square * (1.0f - blend) + saw * blend;
        return {output, square};
    }

private:
    float sampleRate;
    float phase;
    float secondaryPhase;
};

class PAFAlgorithm {
public:
    explicit PAFAlgorithm(float sampleRate)
        : sampleRate(sampleRate), phase(0.0f), secondaryPhase(0.0f), modPhase(0.0f) {}

    void Reset() {
        phase = 0.0f;
        secondaryPhase = 0.0f;
        modPhase = 0.0f;
    }

    AlgorithmOutput Process(float pitch, float param1, float param2, float param3) {
        const float ratio = ExpoMap(param1, 0.5f, 6.0f);
        const float bandwidth = ExpoMap(param2, 50.0f, 3000.0f);
        const float depth = 0.2f + std::clamp(param3, 0.0f, 1.0f) * 0.8f;

        phase = StepPhase(phase, pitch, sampleRate);
        secondaryPhase = StepPhase(secondaryPhase, pitch * ratio, sampleRate);

        const float carrier = std::sin(secondaryPhase * kTwoPi);
        const float mod = std::sin(phase * kTwoPi);
        const float decay = std::exp(-bandwidth / sampleRate);
        modPhase = decay * modPhase + (1.0f - decay) * mod;

        const float output = carrier * ((1.0f - depth) + depth * modPhase) * 0.5f;
        const float secondary = carrier * (0.5f + 0.5f * modPhase) * 0.5f;
        return {output, secondary};
    }

private:
    float sampleRate;
    float phase;
    float secondaryPhase;
    float modPhase;
};

class ModFMAlgorithm {
public:
    explicit ModFMAlgorithm(float sampleRate)
        : sampleRate(sampleRate), phase(0.0f), modPhase(0.0f) {}

    void Reset() {
        phase = 0.0f;
        modPhase = 0.0f;
    }

    AlgorithmOutput Process(float pitch, float param1, float param2, float param3) {
        const float index = ExpoMap(param1, 0.01f, 8.0f);
        const float ratio = ExpoMap(param2, 0.25f, 6.0f);
        const float feedback = std::clamp(param3, 0.0f, 1.0f) * 0.8f;

        phase = StepPhase(phase, pitch, sampleRate);
        modPhase = StepPhase(modPhase, pitch * ratio, sampleRate);

        const float carrier = std::cos(phase * kTwoPi);
        const float modPhaseRad = modPhase * kTwoPi;
        const float modulator = std::cos(modPhaseRad + feedback * std::sin(modPhaseRad));
        const float envelope = std::exp(-index);

        const float output = carrier * std::exp(index * (modulator - 1.0f)) * envelope * 0.6f;
        const float secondary = carrier * modulator * envelope * 0.6f;
        return {output, secondary};
    }

private:
    float sampleRate;
    float phase;
    float modPhase;
};

class Combination1HybridFormantAlgorithm {
public:
    explicit Combination1HybridFormantAlgorithm(float sampleRate)
        : sampleRate(sampleRate),
          phase(0.0f),
          modPhase(0.0f),
          formant1Phase(0.0f),
          formant2Phase(0.0f),
          formant3Phase(0.0f) {}

    void Reset() {
        phase = 0.0f;
        modPhase = 0.0f;
        formant1Phase = 0.0f;
        formant2Phase = 0.0f;
        formant3Phase = 0.0f;
    }

    AlgorithmOutput Process(float pitch, float param1, float param2, float param3) {
        (void)param2;
        const float modfmIndex = ExpoMap(param1, 0.01f, 3.0f);
        const float formantSpacing = 0.8f + param3 * 0.4f;

        phase = StepPhase(phase, pitch, sampleRate);
        modPhase = StepPhase(modPhase, pitch, sampleRate);
        const float modulator = std::sin(kTwoPi * modPhase);
        const float carrier = std::sin(kTwoPi * phase);
        const float base = carrier * std::exp(-modfmIndex * (std::abs(modulator) - 1.0f)) * 0.4f;

        formant1Phase = StepPhase(formant1Phase, 800.0f * formantSpacing, sampleRate);
        formant2Phase = StepPhase(formant2Phase, 1200.0f * formantSpacing, sampleRate);
        formant3Phase = StepPhase(formant3Phase, 2400.0f * formantSpacing, sampleRate);

        const float formant1 = std::sin(kTwoPi * formant1Phase) * 0.5f;
        const float formant2 = std::sin(kTwoPi * formant2Phase) * 0.5f;
        const float formant3 = std::sin(kTwoPi * formant3Phase) * 0.5f;

        const float output = (base + formant1 + formant2 + formant3) * 0.25f;
        return {output, base * 0.5f};
    }

private:
    float sampleRate;
    float phase;
    float modPhase;
    float formant1Phase;
    float formant2Phase;
    float formant3Phase;
};

class Combination2CascadedAlgorithm {
public:
    explicit Combination2CascadedAlgorithm(float sampleRate)
        : sampleRate(sampleRate), phase(0.0f), cascade1Phase(0.0f), cascade2Phase(0.0f) {}

    void Reset() {
        phase = 0.0f;
        cascade1Phase = 0.0f;
        cascade2Phase = 0.0f;
    }

    AlgorithmOutput Process(float pitch, float param1, float param2, float param3) {
        const float dsfDecay = 0.5f + param1 * 0.45f;
        const float asymRatio = param2;
        const float tanhDrive = param3 * 5.0f;

        phase = StepPhase(phase, pitch, sampleRate);
        const float theta = kTwoPi * 1.5f;
        const float denom = 1.0f - 2.0f * dsfDecay * std::cos(theta) + dsfDecay * dsfDecay;
        const float stage1 = (std::sin(kTwoPi * phase) - dsfDecay * std::sin(kTwoPi * phase - theta))
            / (denom + kEpsilon);

        const float stage2 = ProcessAsymmetricFM(std::abs(stage1), asymRatio, pitch, sampleRate,
                                                 cascade1Phase, cascade2Phase);

        const float stage3 = std::tanh(stage2 * tanhDrive);
        return {stage3 * 0.6f, stage2 * 0.6f};
    }

private:
    float sampleRate;
    float phase;
    float cascade1Phase;
    float cascade2Phase;
};

class Combination3ParallelBankAlgorithm {
public:
    explicit Combination3ParallelBankAlgorithm(float sampleRate)
        : sampleRate(sampleRate),
          parallel1Phase(0.0f),
          parallel2Phase(0.0f),
          parallel3Phase(0.0f),
          parallel4Phase(0.0f),
          parallel5Phase(0.0f),
          formant1Phase(0.0f),
          formant2Phase(0.0f),
          formant3Phase(0.0f) {}

    void Reset() {
        parallel1Phase = 0.0f;
        parallel2Phase = 0.0f;
        parallel3Phase = 0.0f;
        parallel4Phase = 0.0f;
        parallel5Phase = 0.0f;
        formant1Phase = 0.0f;
        formant2Phase = 0.0f;
        formant3Phase = 0.0f;
    }

    AlgorithmOutput Process(float pitch, float param1, float param2, float param3) {
        (void)param2;
        const float modfmIndex = ExpoMap(param1, 0.01f, 8.0f);
        const float mixBalance = param3;

        parallel1Phase = StepPhase(parallel1Phase, pitch, sampleRate);
        parallel2Phase = StepPhase(parallel2Phase, pitch * 1.0f, sampleRate);
        const float mod1 = std::cos(kTwoPi * parallel2Phase);
        const float modfm1 = std::cos(kTwoPi * parallel1Phase) * std::exp(modfmIndex * (mod1 - 1.0f));

        parallel3Phase = StepPhase(parallel3Phase, pitch, sampleRate);
        parallel4Phase = StepPhase(parallel4Phase, pitch * 1.5f, sampleRate);
        const float mod2 = std::cos(kTwoPi * parallel4Phase);
        const float modfm2 = std::cos(kTwoPi * parallel3Phase) * std::exp(modfmIndex * (mod2 - 1.0f));

        parallel5Phase = StepPhase(parallel5Phase, pitch, sampleRate);
        formant1Phase = StepPhase(formant1Phase, pitch * 1.333f, sampleRate);
        const float mod3 = std::cos(kTwoPi * formant1Phase);
        const float modfm3 = std::cos(kTwoPi * parallel5Phase) * std::exp(modfmIndex * (mod3 - 1.0f));

        formant2Phase = StepPhase(formant2Phase, 800.0f, sampleRate);
        formant3Phase = StepPhase(formant3Phase, 2400.0f, sampleRate);
        const float paf1 = std::sin(kTwoPi * formant2Phase) * 0.5f;
        const float paf2 = std::sin(kTwoPi * formant3Phase) * 0.5f;

        const float modfmMix = (modfm1 + modfm2 + modfm3) / 3.0f;
        const float pafMix = (paf1 + paf2) / 2.0f;
        const float output = (modfmMix * (1.0f - mixBalance) + pafMix * mixBalance) * 0.5f;
        const float secondary = (pafMix - modfmMix) * 0.5f;
        return {output, secondary};
    }

private:
    float sampleRate;
    float parallel1Phase;
    float parallel2Phase;
    float parallel3Phase;
    float parallel4Phase;
    float parallel5Phase;
    float formant1Phase;
    float formant2Phase;
    float formant3Phase;
};

class Combination4FeedbackAlgorithm {
public:
    explicit Combination4FeedbackAlgorithm(float sampleRate)
        : sampleRate(sampleRate), phase(0.0f), modPhase(0.0f), feedbackSample(0.0f) {}

    void Reset() {
        phase = 0.0f;
        modPhase = 0.0f;
        feedbackSample = 0.0f;
    }

    AlgorithmOutput Process(float pitch, float param1, float param2, float param3) {
        const float modfmIndex = ExpoMap(param1, 0.01f, 8.0f);
        const float feedbackGain = param2 * 0.95f;
        const float drive = 1.0f + std::clamp(param3, 0.0f, 1.0f) * 4.0f;

        const float modifiedFreq = pitch + feedbackSample * feedbackGain * pitch;

        phase = StepPhase(phase, modifiedFreq, sampleRate);
        modPhase = StepPhase(modPhase, modifiedFreq, sampleRate);
        const float modulator = std::cos(kTwoPi * modPhase);
        const float carrier = std::cos(kTwoPi * phase);
        const float output = carrier * std::exp(modfmIndex * (modulator - 1.0f));

        feedbackSample = output;

        const float shaped = std::tanh(output * drive);
        return {shaped * 0.5f, output * 0.5f};
    }

private:
    float sampleRate;
    float phase;
    float modPhase;
    float feedbackSample;
};

class Combination5MorphingAlgorithm {
public:
    explicit Combination5MorphingAlgorithm(float sampleRate)
        : sampleRate(sampleRate),
          phase(0.0f),
          modPhase(0.0f),
          secondaryPhase(0.0f),
          formant1Phase(0.0f) {}

    void Reset() {
        phase = 0.0f;
        modPhase = 0.0f;
        secondaryPhase = 0.0f;
        formant1Phase = 0.0f;
    }

    AlgorithmOutput Process(float pitch, float param1, float param2, float param3) {
        const float morphCurve = 0.5f + std::clamp(param3, 0.0f, 1.0f) * 1.5f;
        const float morphPos = std::pow(std::clamp(param1, 0.0f, 1.0f), morphCurve);
        const float character = param2;

        float output = 0.0f;
        float secondary = 0.0f;

        if (morphPos < 0.5f) {
            const float alpha = morphPos * 2.0f;

            phase = StepPhase(phase, pitch, sampleRate);
            const float dsfDecay = 0.5f + character * 0.4f;
            const float theta = kTwoPi * 1.5f;
            const float denom = 1.0f - 2.0f * dsfDecay * std::cos(theta) + dsfDecay * dsfDecay;
            const float dsf = (std::sin(kTwoPi * phase) - dsfDecay * std::sin(kTwoPi * phase - theta))
                / (denom + kEpsilon);

            modPhase = StepPhase(modPhase, pitch, sampleRate);
            secondaryPhase = StepPhase(secondaryPhase, pitch, sampleRate);
            const float modfmIndex = ExpoMap(character, 0.01f, 8.0f);
            const float mod = std::cos(kTwoPi * secondaryPhase);
            const float modfm = std::cos(kTwoPi * modPhase) * std::exp(modfmIndex * (mod - 1.0f));

            output = dsf * (1.0f - alpha) + modfm * alpha;
            secondary = modfm;
        } else {
            const float alpha = (morphPos - 0.5f) * 2.0f;

            modPhase = StepPhase(modPhase, pitch, sampleRate);
            secondaryPhase = StepPhase(secondaryPhase, pitch, sampleRate);
            const float modfmIndex = ExpoMap(character, 0.01f, 8.0f);
            const float mod = std::cos(kTwoPi * secondaryPhase);
            const float modfm = std::cos(kTwoPi * modPhase) * std::exp(modfmIndex * (mod - 1.0f));

            formant1Phase = StepPhase(formant1Phase, pitch * 2.0f, sampleRate);
            const float paf = std::sin(kTwoPi * formant1Phase) * 0.5f;

            output = modfm * (1.0f - alpha) + paf * alpha;
            secondary = paf;
        }

        return {output * 0.6f, secondary * 0.6f};
    }

private:
    float sampleRate;
    float phase;
    float modPhase;
    float secondaryPhase;
    float formant1Phase;
};

class Combination6InharmonicAlgorithm {
public:
    explicit Combination6InharmonicAlgorithm(float sampleRate)
        : sampleRate(sampleRate), phase(0.0f), formant1Phase(0.0f) {}

    void Reset() {
        phase = 0.0f;
        formant1Phase = 0.0f;
    }

    AlgorithmOutput Process(float pitch, float param1, float param2, float param3) {
        const float phiRatio = 1.618034f;
        const float pafShift = ExpoMap(param2, 5.0f, 50.0f);
        const float dsfDecay = 0.5f + param1 * 0.4f;
        const float mix = std::clamp(param3, 0.0f, 1.0f);

        phase = StepPhase(phase, pitch, sampleRate);
        const float theta = kTwoPi * phiRatio;
        const float denom = 1.0f - 2.0f * dsfDecay * std::cos(theta) + dsfDecay * dsfDecay;
        const float dsf = (std::sin(kTwoPi * phase) - dsfDecay * std::sin(kTwoPi * phase - theta))
            / (denom + kEpsilon);

        const float formantFreq = pitch * 2.0f + pafShift;
        formant1Phase = StepPhase(formant1Phase, formantFreq, sampleRate);
        const float paf = std::sin(kTwoPi * formant1Phase) * 0.5f;

        const float output = dsf * (1.0f - mix) + paf * mix;
        return {output, dsf};
    }

private:
    float sampleRate;
    float phase;
    float formant1Phase;
};

class Combination7AdaptiveFilterAlgorithm {
public:
    explicit Combination7AdaptiveFilterAlgorithm(float sampleRate)
        : sampleRate(sampleRate), phase(0.0f), modPhase(0.0f), secondaryPhase(0.0f) {}

    void Reset() {
        phase = 0.0f;
        modPhase = 0.0f;
        secondaryPhase = 0.0f;
    }

    AlgorithmOutput Process(float pitch, float param1, float param2, float param3) {
        const float cutoff = param1;
        const float resonance = param2;
        const float mix = std::clamp(param3, 0.0f, 1.0f);

        const float dsfDecay = 0.5f + resonance * 0.49f;
        phase = StepPhase(phase, pitch, sampleRate);
        const float theta = kTwoPi * (1.0f + cutoff * 2.0f);
        const float denom = 1.0f - 2.0f * dsfDecay * std::cos(theta) + dsfDecay * dsfDecay;
        const float dsf = (std::sin(kTwoPi * phase) - dsfDecay * std::sin(kTwoPi * phase - theta))
            / (denom + kEpsilon);

        const float modfmIndex = ExpoMap(cutoff, 0.01f, 2.0f);
        modPhase = StepPhase(modPhase, pitch, sampleRate);
        secondaryPhase = StepPhase(secondaryPhase, pitch, sampleRate);
        const float mod = std::cos(kTwoPi * secondaryPhase);
        const float modfm = std::cos(kTwoPi * modPhase) * std::exp(modfmIndex * (mod - 1.0f));

        const float output = (dsf * (1.0f - mix) + modfm * mix) * 0.3f;
        return {output, modfm * 0.3f};
    }

private:
    float sampleRate;
    float phase;
    float modPhase;
    float secondaryPhase;
};

class Novel1MultistageAlgorithm {
public:
    explicit Novel1MultistageAlgorithm(float sampleRate)
        : sampleRate(sampleRate), phase(0.0f), modPhase(0.0f) {}

    void Reset() {
        phase = 0.0f;
        modPhase = 0.0f;
    }

    AlgorithmOutput Process(float pitch, float param1, float param2, float param3) {
        const float tanhDrive = ExpoMap(param1, 0.1f, 10.0f);
        const float expDepth = ExpoMap(param2, 0.1f, 1.5f);
        const float ringCarrierMult = 0.5f + param3 * 4.5f;

        phase = StepPhase(phase, pitch, sampleRate);
        const float input = std::sin(kTwoPi * phase);

        const float stage1 = std::tanh(tanhDrive * input);
        const float stage2 = stage1 * std::exp(expDepth * stage1);

        modPhase = StepPhase(modPhase, pitch * ringCarrierMult, sampleRate);
        const float carrier = std::sin(kTwoPi * modPhase);
        const float stage3 = stage2 * (1.0f + carrier);

        return {stage3 * 0.25f, stage2 * 0.25f};
    }

private:
    float sampleRate;
    float phase;
    float modPhase;
};

class Novel2FreqAsymmetryAlgorithm {
public:
    explicit Novel2FreqAsymmetryAlgorithm(float sampleRate)
        : sampleRate(sampleRate), phase(0.0f), modPhase(0.0f) {}

    void Reset() {
        phase = 0.0f;
        modPhase = 0.0f;
    }

    AlgorithmOutput Process(float pitch, float param1, float param2, float param3) {
        const float lowR = 0.5f + param1 * 0.5f;
        const float highR = 1.0f + param2 * 1.0f;
        const float index = 0.2f + std::clamp(param3, 0.0f, 1.0f) * 0.8f;

        float r = lowR;
        if (pitch > 2000.0f) {
            r = highR;
        } else if (pitch > 500.0f) {
            const float alpha = (pitch - 500.0f) / 1500.0f;
            r = lowR * (1.0f - alpha) + highR * alpha;
        }

        const float output = ProcessAsymmetricFM(index, r / 2.0f, pitch, sampleRate, phase, modPhase);
        const float mod = std::sin(kTwoPi * modPhase);
        const float secondary = std::cos(kTwoPi * phase + index * mod) * 0.5f;
        return {output, secondary};
    }

private:
    float sampleRate;
    float phase;
    float modPhase;
};

class Novel3CrossModAlgorithm {
public:
    explicit Novel3CrossModAlgorithm(float sampleRate)
        : sampleRate(sampleRate), phase(0.0f), modPhase(0.0f), secondaryPhase(0.0f) {}

    void Reset() {
        phase = 0.0f;
        modPhase = 0.0f;
        secondaryPhase = 0.0f;
    }

    AlgorithmOutput Process(float pitch, float param1, float param2, float param3) {
        const float mod1Depth = param1;
        const float mod2Depth = param2;
        const float mix = std::clamp(param3, 0.0f, 1.0f);

        const float baseDsfDecay = 0.7f;
        const float baseDsfRatio = 1.5f;
        const float baseModfmIndex = 0.25f;

        const float dsfRatio = baseDsfRatio + mod2Depth * baseModfmIndex * 0.5f;
        const float modfmIndex = baseModfmIndex + mod1Depth * baseDsfDecay * 1.0f;

        phase = StepPhase(phase, pitch, sampleRate);
        const float theta = kTwoPi * dsfRatio;
        const float denom = 1.0f - 2.0f * baseDsfDecay * std::cos(theta) + baseDsfDecay * baseDsfDecay;
        const float dsf = (std::sin(kTwoPi * phase) - baseDsfDecay * std::sin(kTwoPi * phase - theta))
            / (denom + kEpsilon);

        modPhase = StepPhase(modPhase, pitch, sampleRate);
        secondaryPhase = StepPhase(secondaryPhase, pitch, sampleRate);
        const float mod = std::cos(kTwoPi * secondaryPhase);
        const float modfm = std::cos(kTwoPi * modPhase) * std::exp(modfmIndex * (mod - 1.0f));

        const float output = (dsf * (1.0f - mix) + modfm * mix) * 0.7f;
        const float secondary = (dsf - modfm) * 0.7f;
        return {output, secondary};
    }

private:
    float sampleRate;
    float phase;
    float modPhase;
    float secondaryPhase;
};

class Novel4TaylorAlgorithm {
public:
    explicit Novel4TaylorAlgorithm(float sampleRate)
        : sampleRate(sampleRate), phase(0.0f) {}

    void Reset() { phase = 0.0f; }

    AlgorithmOutput Process(float pitch, float param1, float param2, float param3) {
        const int firstTerms = std::max(1, static_cast<int>(std::round(1.0f + param1 * 9.0f)));
        const int secondTerms = std::max(1, static_cast<int>(std::round(1.0f + param2 * 9.0f)));
        const float blend = std::clamp(param3, 0.0f, 1.0f);

        phase = StepPhase(phase, pitch, sampleRate);
        const float theta = phase * kTwoPi;

        const float fundamental = ComputeTaylorSine(theta, firstTerms);
        const float secondHarmonic = ComputeTaylorSine(2.0f * theta, secondTerms);

        const float output = fundamental * (1.0f - blend) + secondHarmonic * blend;
        const float clamped = std::clamp(output, -1.0f, 1.0f);
        const float secondary = std::clamp(secondHarmonic, -1.0f, 1.0f);
        return {clamped, secondary};
    }

private:
    float sampleRate;
    float phase;
};

class TrajectoryAlgorithm {
public:
    explicit TrajectoryAlgorithm(float sampleRate)
        : sampleRate(sampleRate),
          sides(6),
          startAngle(0.0f),
          startPositionAngle(0.0f),
          bounceJitter(0.0f),
          frequency(440.0f),
          speed(ComputeSpeed(440.0f)),
          position({0.0f, 0.0f}),
          velocity({speed, 0.0f}),
          rngState(0x12345678u),
          vertexCount(0),
          edgeCount(0) {
        RebuildPolygon();
        Reset();
    }

    void Reset() {
        ResetPosition();
        UpdateVelocity();
    }

    AlgorithmOutput Process(float pitch, float param1, float param2, float param3) {
        UpdateParams(pitch, param1, param2, param3);

        if (edgeCount == 0) {
            return {0.0f, 0.0f};
        }

        Vec2 current = position;
        Vec2 currentVelocity = velocity;

        for (int bounce = 0; bounce < 2; ++bounce) {
            const Vec2 next = {current.x + currentVelocity.x, current.y + currentVelocity.y};

            if (IsInside(next)) {
                current = next;
                break;
            }

            const PenetrationHit hit = FindPenetrationEdge(next);
            if (!hit.hit) {
                current = next;
                break;
            }

            const Vec2 reflected = Reflect(currentVelocity, hit.normal);
            const Vec2 jittered = ApplyBounceJitter(reflected);
            const float nudge = 1e-4f;
            current = {
                next.x - hit.normal.x * (hit.distance + nudge),
                next.y - hit.normal.y * (hit.distance + nudge)
            };
            currentVelocity = jittered;
        }

        position = current;
        velocity = currentVelocity;

        return {position.x, position.y};
    }

private:
    struct Vec2 {
        float x;
        float y;
    };

    struct Edge {
        Vec2 start;
        Vec2 end;
        Vec2 normal;
    };

    struct PenetrationHit {
        bool hit;
        float distance;
        Vec2 normal;
    };

    struct RayHit {
        bool hit;
        float t;
        Vec2 point;
    };

    static constexpr int kMaxSides = 12;

    float ComputeSpeed(float freq) const {
        return (freq * 4.0f) / sampleRate;
    }

    void UpdateParams(float pitch, float param1, float param2, float param3) {
        const int nextSides = ClampInt(3 + static_cast<int>(std::round(param1 * 9.0f)), 3, 12);
        const float nextAngle = DegToRad(param2 * 360.0f);
        const float nextJitter = DegToRad(param3 * 10.0f);

        const bool sidesChanged = nextSides != sides;
        const bool launchChanged = std::abs(nextAngle - startAngle) > 1e-6f;
        const bool jitterChanged = std::abs(nextJitter - bounceJitter) > 1e-6f;
        const bool pitchChanged = std::abs(pitch - frequency) > 1e-6f;

        if (sidesChanged) {
            sides = nextSides;
            RebuildPolygon();
        }

        if (launchChanged) {
            startAngle = nextAngle;
            startPositionAngle = nextAngle;
        }

        if (jitterChanged) {
            bounceJitter = nextJitter;
        }

        if (pitchChanged) {
            frequency = pitch;
            speed = ComputeSpeed(frequency);
        }

        if (sidesChanged || launchChanged) {
            ResetPosition();
            UpdateVelocity();
        } else if (pitchChanged) {
            UpdateVelocity();
        }
    }

    void RebuildPolygon() {
        vertexCount = 0;
        edgeCount = 0;

        const float rotation = static_cast<float>(M_PI) / static_cast<float>(sides);

        for (int i = 0; i < sides; ++i) {
            const float theta = (kTwoPi * static_cast<float>(i)) / static_cast<float>(sides) + rotation;
            vertices[i] = {std::cos(theta), std::sin(theta)};
        }
        vertexCount = sides;

        for (int i = 0; i < vertexCount; ++i) {
            const Vec2 start = vertices[i];
            const Vec2 end = vertices[(i + 1) % vertexCount];
            const Vec2 edge = {end.x - start.x, end.y - start.y};
            const Vec2 normal = Normalize({edge.y, -edge.x});
            edges[i] = {start, end, normal};
        }
        edgeCount = vertexCount;
    }

    void ResetPosition() {
        const Vec2 dir = {std::cos(startPositionAngle), std::sin(startPositionAngle)};
        const RayHit hit = FindRayIntersection(dir);
        if (hit.hit) {
            position = {hit.point.x * 0.995f, hit.point.y * 0.995f};
        } else {
            position = {0.0f, 0.0f};
        }
    }

    void UpdateVelocity() {
        const Vec2 dir = {std::cos(startAngle), std::sin(startAngle)};
        velocity = {dir.x * speed, dir.y * speed};
    }

    RayHit FindRayIntersection(const Vec2& direction) const {
        RayHit closest{false, 0.0f, {0.0f, 0.0f}};
        for (int i = 0; i < edgeCount; ++i) {
            RayHit hit = IntersectRaySegment({0.0f, 0.0f}, direction, edges[i].start, edges[i].end);
            if (!hit.hit) {
                continue;
            }
            if (!closest.hit || hit.t < closest.t) {
                closest = hit;
            }
        }
        return closest;
    }

    RayHit IntersectRaySegment(const Vec2& origin, const Vec2& direction,
                               const Vec2& start, const Vec2& end) const {
        const Vec2 segment = {end.x - start.x, end.y - start.y};
        const float denom = Cross(direction, segment);
        if (std::abs(denom) < 1e-6f) {
            return {false, 0.0f, {0.0f, 0.0f}};
        }

        const Vec2 toStart = {start.x - origin.x, start.y - origin.y};
        const float t = Cross(toStart, segment) / denom;
        const float u = Cross(toStart, direction) / denom;

        if (t >= 0.0f && u >= 0.0f && u <= 1.0f) {
            return {true, t, {origin.x + direction.x * t, origin.y + direction.y * t}};
        }

        return {false, 0.0f, {0.0f, 0.0f}};
    }

    PenetrationHit FindPenetrationEdge(const Vec2& point) const {
        PenetrationHit worst{false, 0.0f, {0.0f, 0.0f}};

        for (int i = 0; i < edgeCount; ++i) {
            const Edge& edge = edges[i];
            const Vec2 toPoint = {point.x - edge.start.x, point.y - edge.start.y};
            const float distance = toPoint.x * edge.normal.x + toPoint.y * edge.normal.y;
            if (distance > 0.0f && (!worst.hit || distance > worst.distance)) {
                worst = {true, distance, edge.normal};
            }
        }

        return worst;
    }

    bool IsInside(const Vec2& point) const {
        for (int i = 0; i < edgeCount; ++i) {
            const Edge& edge = edges[i];
            const Vec2 edgeVector = {edge.end.x - edge.start.x, edge.end.y - edge.start.y};
            const Vec2 toPoint = {point.x - edge.start.x, point.y - edge.start.y};
            if (Cross(edgeVector, toPoint) < -1e-6f) {
                return false;
            }
        }
        return true;
    }

    Vec2 Reflect(const Vec2& vector, const Vec2& normal) const {
        const float dot = vector.x * normal.x + vector.y * normal.y;
        return {vector.x - 2.0f * dot * normal.x, vector.y - 2.0f * dot * normal.y};
    }

    Vec2 ApplyBounceJitter(const Vec2& vector) {
        if (bounceJitter <= 0.0f) {
            return vector;
        }
        const float randValue = RandomUnit();
        const float angle = (randValue * 2.0f - 1.0f) * bounceJitter;
        const float cosAngle = std::cos(angle);
        const float sinAngle = std::sin(angle);
        return {
            vector.x * cosAngle - vector.y * sinAngle,
            vector.x * sinAngle + vector.y * cosAngle
        };
    }

    Vec2 Normalize(const Vec2& vector) const {
        const float magnitude = std::hypot(vector.x, vector.y);
        if (magnitude < 1e-6f) {
            return {0.0f, 0.0f};
        }
        return {vector.x / magnitude, vector.y / magnitude};
    }

    int ClampInt(int value, int minValue, int maxValue) const {
        if (value < minValue) {
            return minValue;
        }
        if (value > maxValue) {
            return maxValue;
        }
        return value;
    }

    float DegToRad(float degrees) const {
        return (degrees * static_cast<float>(M_PI)) / 180.0f;
    }

    float Cross(const Vec2& a, const Vec2& b) const {
        return a.x * b.y - a.y * b.x;
    }

    float RandomUnit() {
        rngState = rngState * 1664525u + 1013904223u;
        return static_cast<float>((rngState >> 8) & 0xFFFFFF) / 16777216.0f;
    }

    float sampleRate;
    int sides;
    float startAngle;
    float startPositionAngle;
    float bounceJitter;
    float frequency;
    float speed;

    Vec2 vertices[kMaxSides];
    Edge edges[kMaxSides];
    int vertexCount;
    int edgeCount;

    Vec2 position;
    Vec2 velocity;
    uint32_t rngState;
};

} // namespace disyn
