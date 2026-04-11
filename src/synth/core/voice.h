#pragma once

class Voice
{
public:
    virtual ~Voice() = default;

    virtual void noteOn(double freq) = 0;
    virtual void noteOff() = 0;

    virtual float process() = 0;

    bool active = false;
};