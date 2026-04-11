#pragma once
#include <cmath>

class Resonator
{
public:
    double y1 = 0.0, y2 = 0.0;
    double a1, a2, b0;

    void setup(double freq, double bandwidth, int sampleRate)
    {
        double r = exp(-M_PI * bandwidth / sampleRate);
        double w = 2.0 * M_PI * freq / sampleRate;
        a1 = -2.0 * r * cos(w);
        a2 = r * r;
        b0 = 1.0 - r;
    }

    inline double process(double x)
    {
        double y = b0 * x - a1 * y1 - a2 * y2;
        y2 = y1;
        y1 = y;
        return y;
    }
};