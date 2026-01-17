#pragma once

#include <cstddef>

struct SpectralFrame
{
    size_t bins = 0;
    float *re = nullptr;
    float *im = nullptr;
    float *mag = nullptr;
    float *phase = nullptr;
    float *temp = nullptr;
    float *tempIm = nullptr;
    float *smoothMag = nullptr;
    float *freezeMag = nullptr;
};

class SpectralProcessor
{
  public:
    virtual ~SpectralProcessor() = default;
    virtual void Process(SpectralFrame &frame, float vibe) const = 0;
    virtual const char *Name() const = 0;
};

class ThruProcessor : public SpectralProcessor
{
  public:
    void Process(SpectralFrame &frame, float vibe) const override;
    const char *Name() const override;
};

class SmearProcessor : public SpectralProcessor
{
  public:
    void Process(SpectralFrame &frame, float vibe) const override;
    const char *Name() const override;
};

class ShiftProcessor : public SpectralProcessor
{
  public:
    void Process(SpectralFrame &frame, float vibe) const override;
    const char *Name() const override;
};

class CombProcessor : public SpectralProcessor
{
  public:
    void Process(SpectralFrame &frame, float vibe) const override;
    const char *Name() const override;
};

class FreezeProcessor : public SpectralProcessor
{
  public:
    void Process(SpectralFrame &frame, float vibe) const override;
    const char *Name() const override;
};

class GateProcessor : public SpectralProcessor
{
  public:
    void Process(SpectralFrame &frame, float vibe) const override;
    const char *Name() const override;
};

class TiltProcessor : public SpectralProcessor
{
  public:
    void Process(SpectralFrame &frame, float vibe) const override;
    const char *Name() const override;
};

class FoldProcessor : public SpectralProcessor
{
  public:
    void Process(SpectralFrame &frame, float vibe) const override;
    const char *Name() const override;
};

class PhaseProcessor : public SpectralProcessor
{
  public:
    void Process(SpectralFrame &frame, float vibe) const override;
    const char *Name() const override;
};

const SpectralProcessor &GetProcessor(int processIndex);
