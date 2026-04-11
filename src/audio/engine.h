#pragma once
#include "../synth/instruments/piano.h"
#include "../effects/delay.h"

class AudioEngine
{
public:
    AudioEngine();

    float process();

    PianoInstrument piano;

private:
    Delay delay;
};