#pragma once
#include <vector>

class Delay
{
public:
    Delay(int size) : buffer(size, 0.0f) {}

    // delay.h
    // delay.h - Updated process function
    // delay.h
    // delay.h - Updated process function
    float process(float input)
    {
        float delayed = buffer[head];

        // Dampen the input so it doesn't "stack" infinitely with the feedback
        buffer[head] = (input * 0.05f) + (delayed * feedback);

        head++;
        if (head >= (int)buffer.size())
            head = 0;

        // Standard Dry/Wet mix
        return (input * 0.5f) + (delayed * mix);
    }

    float feedback = 0.1f;
    float mix = 0.1f;

private:
    std::vector<float> buffer;
    int head = 0;
};