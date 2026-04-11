#include "piano.h"
#include <cmath>
#include <algorithm>

constexpr int SAMPLE_RATE = 48000;

void PianoVoice::noteOn(double f)
{
    freq = f;
    t = 0.0;
    amp = 1.0;
    active = true;

    board[0].setup(61.0, 27.0, SAMPLE_RATE);
    board[1].setup(400.0, 200.0, SAMPLE_RATE);
    board[2].setup(1200.0, 800.0, SAMPLE_RATE);
    board[3].setup(4000.0, 1000.0, SAMPLE_RATE);

    for (int i = 0; i < 4; i++)
    {
        board[i].y1 = 0.0; // <--- CRITICAL: Reset the "memory"
        board[i].y2 = 0.0; // <--- CRITICAL: Reset the "memory"
    }
}

void PianoVoice::noteOff()
{
    active = false;
}

float PianoVoice::process()
{
    if (!active)
        return 0.0f;

    t += 1.0 / SAMPLE_RATE;

    double pitch = std::clamp(log2(freq / 27.5) / 7.2, 0.0, 1.0);

    double attackRate = 40.0 + pitch * 100.0;
    double decayRate = 2.5;

    double env = (1.0 - exp(-t * attackRate)) * exp(-t * decayRate);

    double stringSum = 0.0;

    int maxH = std::min(30, int(18000.0 / freq));

    for (int k = 1; k <= maxH; k++)
    {
        double hAmp = pow(1.0 / k, 2.0);
        stringSum += sin(2.0 * M_PI * freq * k * t) * hAmp;
    }

    double raw = stringSum * 0.5;

    double woody = 0.0;
    for (int i = 0; i < 4; i++)
    {
        woody += board[i].process(raw);
    }

    double out = (raw * 0.4 + woody * 0.6) * env * 0.05;

    if (env < 0.0001)
        active = false;

    return (float)out;
}

// ------------------------

PianoInstrument::PianoInstrument(int numVoices)
{
    voices.resize(numVoices);
}

void PianoInstrument::noteOn(double freq)
{
    for (auto &v : voices)
    {
        if (!v.active)
        {
            v.noteOn(freq);
            return;
        }
    }
}

void PianoInstrument::noteOff(double) {}

// piano.cpp - Inside PianoInstrument::process()
// piano.cpp - Inside PianoInstrument::process()
float PianoInstrument::process()
{
    float sum = 0.0f;
    int activeCount = 0;
    for (auto &v : voices)
    {
        if (v.active)
        {
            sum += v.process();
            activeCount++;
        }
    }
    // Simple fix: Reduce total output gain to leave room for the delay
    return sum * 0.5f;
}