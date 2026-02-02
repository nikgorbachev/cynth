#include <iostream>
#include <cmath>
#include <random>
#include <atomic>
#include <termios.h>
#include <unistd.h>

#include <portaudio.h>

constexpr int SAMPLE_RATE = 44100;
constexpr double DURATION = 0.15;
constexpr int NUM_SAMPLES = int(SAMPLE_RATE * DURATION);

// =====================
// GLOBAL STATE (MVP)
// =====================
float snare[NUM_SAMPLES];
std::atomic<int> playhead(-1);

// =====================
// LOWPASS
// =====================
double lowpass(double input, double& state, double cutoffHz) {
    double RC = 1.0 / (2.0 * M_PI * cutoffHz);
    double dt = 1.0 / SAMPLE_RATE;
    double alpha = dt / (RC + dt);
    state += alpha * (input - state);
    return state;
}

// =====================
// GENERATE SNARE ONCE
// =====================
void generateSnare() {
    std::mt19937 rng(1234);
    std::uniform_real_distribution<double> noise(-1.0, 1.0);

    double noiseLP = 0.0;
    double outLP = 0.0;

    for (int i = 0; i < NUM_SAMPLES; i++) {
        double t = double(i) / SAMPLE_RATE;

        double noiseEnv = exp(-t * 14.0) * (1.0 - exp(-t * 180.0));
        double toneEnv  = exp(-t * 22.0);

        double n = noise(rng) * noiseEnv;
        n = lowpass(n, noiseLP, 5500.0);

        double tone = sin(2.0 * M_PI * 150.0 * t);
        tone = tanh(tone * 2.0) * toneEnv;

        double sample = 0.9 * n + 0.25 * tone;
        sample = lowpass(sample, outLP, 6000.0);
        sample = tanh(sample * 1.4);

        snare[i] = (float)sample;
    }
}

// =====================
// AUDIO CALLBACK
// =====================
static int audioCallback(
    const void*,
    void* output,
    unsigned long frameCount,
    const PaStreamCallbackTimeInfo*,
    PaStreamCallbackFlags,
    void*
) {
    float* out = (float*)output;

    for (unsigned long i = 0; i < frameCount; i++) {
        int p = playhead.load();
        if (p >= 0 && p < NUM_SAMPLES) {
            out[i] = snare[p];
            playhead++;
        } else {
            out[i] = 0.0f;
        }
    }
    return paContinue;
}

// =====================
// RAW KEYBOARD
// =====================
char readChar() {
    char c;
    read(STDIN_FILENO, &c, 1);
    return c;
}

void setRawMode(bool enable) {
    static termios oldt;
    termios newt;

    if (enable) {
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    } else {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    }
}

// =====================
// MAIN
// =====================
int main() {
    generateSnare();

    Pa_Initialize();

    PaStream* stream;
    Pa_OpenDefaultStream(
        &stream,
        0,
        1,
        paFloat32,
        SAMPLE_RATE,
        256,
        audioCallback,
        nullptr
    );

    Pa_StartStream(stream);

    std::cout << "Press 'j' to play snare. Ctrl+C to exit.\n";

    setRawMode(true);

    while (true) {
        char c = readChar();
        if (c == 'j') {
            playhead = 0;
        }
    }

    setRawMode(false);
    Pa_StopStream(stream);
    Pa_Terminate();
}
