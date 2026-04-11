#include "audio/engine.h"
#include <portaudio.h>
#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <atomic>
#include <cmath>

constexpr int SAMPLE_RATE = 48000;

AudioEngine engine;

// =====================
// AUDIO CALLBACK
// =====================
static int audioCallback(
    const void *, void *output,
    unsigned long frameCount,
    const PaStreamCallbackTimeInfo *,
    PaStreamCallbackFlags, void *)
{
    float *out = (float *)output;

    for (unsigned long i = 0; i < frameCount; i++)
    {
        // main.cpp - Inside audioCallback
        float s = engine.process();

        // A simple Master Gain to prevent the clipper from working too hard
        s *= 0.5f;

        // True Soft Clipping (using math.h tanh)
        // This rounds the peaks naturally.
        s = std::tanh(s);

        out[i] = s * 0.9f; // Final safety margin
    }

    return paContinue;
}

// =====================
// INPUT
// =====================
char readChar()
{
    char c;
    read(STDIN_FILENO, &c, 1);
    return c;
}

void setRawMode(bool enable)
{
    static termios oldt;
    termios newt;

    if (enable)
    {
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    }
    else
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    }
}

// =====================
// NOTE MAPPING
// =====================
double baseFreqs[20] = {
    261.63, 277.18, 293.66, 311.13, 329.63,
    349.23, 369.99, 392.00, 415.30, 440.00,
    466.16, 493.88, 523.25, 554.37, 587.33,
    622.25, 659.25, 698.46, 739.99, 783.99};

// =====================
// MAIN
// =====================
int main()
{
    // --- PortAudio ---
    Pa_Initialize();

    PaStreamParameters outParams;
    outParams.device = Pa_GetDefaultOutputDevice();
    outParams.channelCount = 1;
    outParams.sampleFormat = paFloat32;
    outParams.suggestedLatency =
        Pa_GetDeviceInfo(outParams.device)->defaultLowOutputLatency;
    outParams.hostApiSpecificStreamInfo = nullptr;

    PaStream *stream;
    Pa_OpenStream(&stream, nullptr, &outParams,
                  SAMPLE_RATE, 256, paClipOff,
                  audioCallback, nullptr);

    Pa_StartStream(stream);

    std::cout << "[ CYNTH READY ]\n";
    std::cout << "Press keys to play. Ctrl+C to exit.\n";

    setRawMode(true);

    int octave = 0;

    while (true)
    {
        char c = readChar();

        if (c == 3)
            break; // Ctrl+C

        if (c == 'z')
            octave = std::max(-2, octave - 1);
        if (c == 'x')
            octave = std::min(2, octave + 1);

        int n = -1;

        switch (c)
        {
        case 'a':
            n = 0;
            break;
        case 'w':
            n = 1;
            break;
        case 's':
            n = 2;
            break;
        case 'e':
            n = 3;
            break;
        case 'd':
            n = 4;
            break;
        case 'f':
            n = 5;
            break;
        case 't':
            n = 6;
            break;
        case 'g':
            n = 7;
            break;
        case 'y':
            n = 8;
            break;
        case 'h':
            n = 9;
            break;
        case 'u':
            n = 10;
            break;
        case 'j':
            n = 11;
            break;
        case 'k':
            n = 12;
            break;
        case 'o':
            n = 13;
            break;
        case 'l':
            n = 14;
            break;
        case 'p':
            n = 15;
            break;
        case ';':
            n = 16;
            break;
        case '\'':
            n = 17;
            break;
        case ']':
            n = 18;
            break;
        case '#':
            n = 19;
            break;
        }

        if (n >= 0)
        {
            double freq = baseFreqs[n] * pow(2.0, octave);
            engine.piano.noteOn(freq);
        }
    }

    setRawMode(false);
    Pa_StopStream(stream);
    Pa_Terminate();

    return 0;
}