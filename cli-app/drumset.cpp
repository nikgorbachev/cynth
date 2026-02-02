#include <iostream>
#include <cmath>
#include <random>
#include <atomic>
#include <termios.h>
#include <unistd.h>

#include <portaudio.h>

constexpr int SAMPLE_RATE = 44100;

// =====================
// LENGTHS
// =====================
constexpr double SNARE_DUR = 0.15;
constexpr double KICK_DUR  = 0.5;
constexpr double HAT_DUR   = 0.08;

constexpr int SNARE_N = int(SNARE_DUR * SAMPLE_RATE);
constexpr int KICK_N  = int(KICK_DUR  * SAMPLE_RATE);
constexpr int HAT_N   = int(HAT_DUR   * SAMPLE_RATE);

// =====================
// BUFFERS
// =====================
float snare[SNARE_N];
float kick[KICK_N];
float hihat[HAT_N];

// =====================
// PLAYHEADS
// =====================
std::atomic<int> snarePH(-1);
std::atomic<int> kickPH(-1);
std::atomic<int> hatPH(-1);

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
// SNARE
// =====================
void generateSnare() {
    std::mt19937 rng(1234);
    std::uniform_real_distribution<double> noise(-1.0, 1.0);

    double noiseLP = 0.0;
    double outLP = 0.0;

    for (int i = 0; i < SNARE_N; i++) {
        double t = double(i) / SAMPLE_RATE;

        double noiseEnv = exp(-t * 14.0) * (1.0 - exp(-t * 180.0));
        double toneEnv  = exp(-t * 22.0);

        double n = noise(rng) * noiseEnv;
        n = lowpass(n, noiseLP, 5500.0);

        double tone = sin(2.0 * M_PI * 150.0 * t);
        tone = tanh(tone * 2.0) * toneEnv;

        double s = 0.9 * n + 0.25 * tone;
        s = lowpass(s, outLP, 6000.0);
        s = tanh(s * 1.4);

        snare[i] = (float)s;
    }
}

// =====================
// KICK
// =====================
void generateKick() {
    double phase = 0.0;

    for (int i = 0; i < KICK_N; i++) {
        double t = double(i) / SAMPLE_RATE;

        double ampEnv = exp(-t * 8.0);
        double freq = 40.0 + (80.0 - 40.0) * exp(-t * 20.0);

        phase += 2.0 * M_PI * freq / SAMPLE_RATE;

        double s = sin(phase) * ampEnv;
        kick[i] = (float)tanh(s * 1.2);
    }
}

// =====================
// HI-HAT (closed, Linn-ish)
// =====================
void generateHiHat() {
    std::mt19937 rng(5678);
    std::uniform_real_distribution<double> noise(-1.0, 1.0);

    double hp = 0.0;
    double lp = 0.0;

    for (int i = 0; i < HAT_N; i++) {
        double t = double(i) / SAMPLE_RATE;

        // very fast decay
        double env = exp(-t * 60.0);

        double n = noise(rng);

        // crude band-pass: HP then LP
        double high = n - lowpass(n, hp, 6000.0);
        double band = lowpass(high, lp, 10000.0);

        double s = band * env * 0.7;
        hihat[i] = (float)tanh(s);
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
        float mix = 0.0f;

        int p;

        p = snarePH.load();
        if (p >= 0 && p < SNARE_N) {
            mix += snare[p];
            snarePH.fetch_add(1);
        }

        p = kickPH.load();
        if (p >= 0 && p < KICK_N) {
            mix += kick[p];
            kickPH.fetch_add(1);
        }

        p = hatPH.load();
        if (p >= 0 && p < HAT_N) {
            mix += hihat[p];
            hatPH.fetch_add(1);
        }

        out[i] = tanh(mix * 0.8f);
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
    generateKick();
    generateHiHat();

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

    std::cout <<
        "j = snare | space = kick | f = hi-hat\n"
        "Ctrl+C to exit\n";

    setRawMode(true);

    while (true) {
        char c = readChar();
        if (c == 'j') snarePH = 0;
        if (c == ' ') kickPH  = 0;
        if (c == 'f') hatPH   = 0;
    }

    setRawMode(false);
    Pa_StopStream(stream);
    Pa_Terminate();
}
