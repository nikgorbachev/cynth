#pragma once
#include "../core/instrument.h"
#include "../core/voice.h"
#include "../../dsp/resonator.h"
#include "../../dsp/utils.h"
#include <vector>

class PianoVoice : public Voice
{
public:
    void noteOn(double freq) override;
    void noteOff() override;
    float process() override;

private:
    double freq = 440.0;
    double t = 0.0;
    double amp = 1.0;

    double hammerLP = 0.0;

    Resonator board[4];
};

class PianoInstrument : public Instrument
{
public:
    PianoInstrument(int numVoices);

    void noteOn(double freq) override;
    void noteOff(double freq) override;

    float process() override;

private:
    std::vector<PianoVoice> voices;
};