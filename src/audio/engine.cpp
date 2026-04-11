#include "engine.h"

AudioEngine::AudioEngine()
    : piano(16),
      delay(48000 * 0.2) {}

float AudioEngine::process()
{
    float mix = piano.process();
    mix = delay.process(mix);
    return mix;
}