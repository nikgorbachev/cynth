#pragma once
#include <cmath>

inline double lowpass(double input, double &state, double cutoffHz, int sampleRate)
{
    double RC = 1.0 / (2.0 * M_PI * cutoffHz);
    double dt = 1.0 / sampleRate;
    double alpha = dt / (RC + dt);
    state += alpha * (input - state);
    return state;
}