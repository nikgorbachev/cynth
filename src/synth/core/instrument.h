#pragma once
#include <vector>
#include <memory>
#include "voice.h"

class Instrument
{
public:
    virtual ~Instrument() = default;

    virtual void noteOn(double freq) = 0;
    virtual void noteOff(double freq) = 0;

    virtual float process() = 0;
};