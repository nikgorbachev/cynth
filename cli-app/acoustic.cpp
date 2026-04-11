#include <iostream>
#include <cmath>
#include <random>
#include <atomic>
#include <termios.h>
#include <unistd.h>
#include <algorithm>
#include <vector>
#include <portaudio.h>

// =====================
// CONFIG
// =====================
constexpr int SAMPLE_RATE = 48000;
constexpr double PIANO_DUR = 3.0; // Slightly longer for "Air"
constexpr int PIANO_N = int(PIANO_DUR * SAMPLE_RATE);
constexpr int MAX_PIANO_NOTES = 20;

// Drum lengths
constexpr double SNARE_DUR = 0.15;
constexpr double KICK_DUR = 0.5;
constexpr double HAT_DUR = 0.08;
constexpr int SNARE_N = int(SNARE_DUR * SAMPLE_RATE);
constexpr int KICK_N = int(KICK_DUR * SAMPLE_RATE);
constexpr int HAT_N = int(HAT_DUR * SAMPLE_RATE);

enum class Mode
{
    Drum,
    Piano
};
std::atomic<Mode> currentMode(Mode::Drum);
int octave = 0;
bool sustainPedal = false;

// =====================
// BUFFERS
// =====================
float snare[SNARE_N];
float kick[KICK_N];
float hihat[HAT_N];
float piano[MAX_PIANO_NOTES][PIANO_N];

// =====================
// STATE
// =====================
std::atomic<int> snarePH(-1);
std::atomic<int> kickPH(-1);
std::atomic<int> hatPH(-1);
std::atomic<int> pianoPH[MAX_PIANO_NOTES];

// Simple Reverb/Delay Buffer for "Space"
// 0.2 seconds of delay for a "Room" slapback
constexpr int DELAY_LEN = int(0.2 * SAMPLE_RATE);
float delayLine[DELAY_LEN] = {0};
int delayHead = 0;

// =====================
// UTILS
// =====================
double lowpass(double input, double &state, double cutoffHz)
{
    double RC = 1.0 / (2.0 * M_PI * cutoffHz);
    double dt = 1.0 / SAMPLE_RATE;
    double alpha = dt / (RC + dt);
    state += alpha * (input - state);
    return state;
}

struct Resonator
{
    double y1 = 0.0, y2 = 0.0;
    double a1, a2, b0;
    // Lower Q = "Woodier", Higher Q = "Metallic"
    void setup(double freq, double bandwidth)
    {
        double r = exp(-M_PI * bandwidth / SAMPLE_RATE);
        double w = 2.0 * M_PI * freq / SAMPLE_RATE;
        a1 = -2.0 * r * cos(w);
        a2 = r * r;
        b0 = 1.0 - r; // Normalize gain
    }
    inline double process(double x)
    {
        double y = b0 * x - a1 * y1 - a2 * y2;
        y2 = y1;
        y1 = y;
        return y;
    }
};

// =====================
// DRUMS (Unchanged)
// =====================
void generateSnare()
{
    std::mt19937 rng(1234);
    std::uniform_real_distribution<double> noise(-1.0, 1.0);
    double noiseLP = 0.0, outLP = 0.0;
    for (int i = 0; i < SNARE_N; i++)
    {
        double t = double(i) / SAMPLE_RATE;
        double noiseEnv = exp(-t * 14.0) * (1.0 - exp(-t * 180.0));
        double toneEnv = exp(-t * 22.0);
        double n = noise(rng) * noiseEnv;
        n = lowpass(n, noiseLP, 5500.0);
        double tone = sin(2.0 * M_PI * 150.0 * t);
        double s = 0.9 * n + 0.25 * tone * toneEnv;
        s = lowpass(s, outLP, 6000.0);
        snare[i] = (float)tanh(s * 1.4);
    }
}

void generateKick()
{
    double phase = 0.0;
    for (int i = 0; i < KICK_N; i++)
    {
        double t = double(i) / SAMPLE_RATE;
        double ampEnv = exp(-t * 8.0);
        double freq = 40.0 + (80.0 - 40.0) * exp(-t * 20.0);
        phase += 2.0 * M_PI * freq / SAMPLE_RATE;
        double s = sin(phase) * ampEnv;
        kick[i] = (float)tanh(s * 1.2);
    }
}

void generateHiHat()
{
    std::mt19937 rng(5678);
    std::uniform_real_distribution<double> noise(-1.0, 1.0);
    double hp = 0.0, lp = 0.0;
    for (int i = 0; i < HAT_N; i++)
    {
        double t = double(i) / SAMPLE_RATE;
        double env = exp(-t * 60.0);
        double n = noise(rng);
        double high = n - lowpass(n, hp, 6000.0);
        double band = lowpass(high, lp, 10000.0);
        hihat[i] = (float)tanh(band * env * 0.7);
    }
}

// =====================
// PIANO (The "Acoustic" Update)
// =====================

void generatePianoNote(float *buffer, double freq, bool sustain)
{
    double pitch = std::clamp(log2(freq / 27.5) / 7.2, 0.0, 1.0);
    bool isDeepBass = freq < 90.0;
    // REDUCED Inharmonicity: Makes it less "Cosmic/Bell-like"
    double inharmAmount = 0.0001 * (1.0 - pitch * 0.5);

    double attackRate = 40.0 + pitch * 100.0;
    double decayRate = sustain ? 0.15 : (0.7 + (1.0 - pitch));

    // Hammer: "Thud" filter cutoff
    double hammerCutoff = 100.0 + pitch * 2000.0;

    // Soundboard Resonators (The "Wood" & "Air")
    Resonator board[4];
    // Deep Wood (Broad bandwidth for warmth)
    board[0].setup(61.0, 27.0);
    // Mid Body (The main character)
    board[1].setup(400.0, 200.0);
    // Upper Wood
    board[2].setup(1200.0, 800.0);
    // The "Air" / Sparkle (High freq, broad)
    board[3].setup(4000.0, 1000.0);

    // Hammer Lowpass State
    double hammerLP = 0.0;

    for (int i = 0; i < PIANO_N; i++)
    {
        double t = double(i) / SAMPLE_RATE;
        double ampEnv = (1.0 - exp(-t * attackRate)) * exp(-t * decayRate);

        // 1. STRINGS (Clean Physics)
        double stringSum = 0.0;
        int numStrings = (pitch > 0.8) ? 1 : 3;
        // Very subtle detuning
        double detune = 0.0004;

        for (int s = 0; s < numStrings; s++)
        {
            double d = (s - (numStrings - 1) / 2.0) * detune;
            double f_str = freq * (1.0 + d);

            // Limit harmonics to audible range to prevent "aliasing birds"
            int maxH = int(18000.0 / freq);
            if (maxH > 30)
                maxH = 30; // Limit computation

            for (int k = 1; k <= maxH; k++)
            {
                double inharm = sqrt(1.0 + inharmAmount * k * k);
                double hFreq = f_str * k * inharm;

                // SPECTRAL TILT: 1/k^2 instead of 1/k
                // This is the KEY to removing "Telephony" sound.
                // It makes harmonics quieter much faster.
                double hAmp = pow(1.0 / k, 2.0 + pitch * 0.5);

                double hDecay = exp(-t * k * (0.8 + pitch));

                // if (isDeepBass) {
                //     hAmp = pow(1.0 / k, 3.0 + pitch * 0.5);
                //     // if (k == 1) hAmp *= 3.0; // Massive boost to root
                //     // if (k == 2) hAmp *= 1.5; // Boost to 2nd harmonic

                // }

                // High harmonics decay faster

                stringSum += sin(2.0 * M_PI * hFreq * t) * hAmp * hDecay;
            }
        }

        // 2. HAMMER (Soft Thud)
        double hammerEnv = exp(-t * 200.0);
        double noise = ((rand() / (double)RAND_MAX) * 2.0 - 1.0);
        hammerLP = lowpass(noise, hammerLP, hammerCutoff);
        double hammer = hammerLP * hammerEnv * (0.2 + (1.0 - pitch) * 0.4);

        // 3. BODY RESONANCE
        double raw = stringSum * 0.5 + hammer;
        // piano.cpp - Inside PianoVoice::process()
        double woody = 0.0;
        woody += board[0].process(raw) * 0.8;  // Bass warmth
        woody += board[1].process(raw) * 0.4;  // Body
        woody += board[2].process(raw) * 0.1;  // Upper wood
        woody += board[3].process(raw) * 0.05; // Air splash

        // 4. MIX
        // We do NOT use tanh() here. Linear mix.
        // We scale down significantly (0.2) to leave headroom.
        double mix = (raw * 0.4 + woody * 0.6) * ampEnv * 0.2;

        buffer[i] = (float)mix;
    }
}

double pianoFreqs[MAX_PIANO_NOTES] = {
    261.63, 277.18, 293.66, 311.13, 329.63, 349.23, 369.99, 392.00, 415.30, 440.00,
    466.16, 493.88, 523.25, 554.37, 587.33, 622.25, 659.25, 698.46, 739.99, 783.99};

void regeneratePiano()
{
    for (int i = 0; i < MAX_PIANO_NOTES; i++)
    {
        generatePianoNote(piano[i], pianoFreqs[i] * pow(2.0, octave), sustainPedal);
    }
}

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
        float mix = 0.0f;

        // Drums
        if (snarePH >= 0 && snarePH < SNARE_N)
            mix += snare[snarePH++];
        if (kickPH >= 0 && kickPH < KICK_N)
            mix += kick[kickPH++];
        if (hatPH >= 0 && hatPH < HAT_N)
            mix += hihat[hatPH++];

        // Piano Summation
        for (int n = 0; n < MAX_PIANO_NOTES; n++)
        {
            int p = pianoPH[n].load();
            if (p >= 0 && p < PIANO_N)
            {
                mix += piano[n][p];
                pianoPH[n].fetch_add(1);
            }
        }

        // --- SPACE / AIR SIMULATION ---
        // Simple "Room" Delay
        // Read delay line
        float delayed = delayLine[delayHead];
        // Write back (feedback) with low gain to create "Space"
        // 0.3 feedback = subtle room, 0.5 = large hall
        delayLine[delayHead] = mix + delayed * 0.3f;

        delayHead++;
        if (delayHead >= DELAY_LEN)
            delayHead = 0;

        // Mix dry + wet (Space)
        mix = mix + delayed * 0.15f;

        // --- FINAL OUTPUT ---
        // NO TANH. Gentle hard clip only if we explode.
        if (mix > 1.0f)
            mix = 1.0f;
        if (mix < -1.0f)
            mix = -1.0f;

        out[i] = mix;
    }
    return paContinue;
}

// =====================
// MAIN & INPUT
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
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}

int main()
{
    // Generate Sounds
    generateSnare();
    generateKick();
    generateHiHat();
    for (int i = 0; i < MAX_PIANO_NOTES; i++)
    {
        pianoPH[i] = -1;
        generatePianoNote(piano[i], pianoFreqs[i], false);
    }

    // PortAudio Init
    Pa_Initialize();

    // LINUX FIX: Manual Device Selection
    int numDevices = Pa_GetDeviceCount();
    if (numDevices < 0)
        return 1;

    int deviceId = Pa_GetDefaultOutputDevice();
    if (deviceId == paNoDevice)
    {
        for (int i = 0; i < numDevices; i++)
        {
            const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
            if (info->maxOutputChannels > 0)
            {
                deviceId = i;
                break;
            }
        }
    }

    if (deviceId == -1)
        return 1;

    PaStreamParameters outParams;
    outParams.device = deviceId;
    outParams.channelCount = 1;
    outParams.sampleFormat = paFloat32;
    outParams.suggestedLatency = Pa_GetDeviceInfo(deviceId)->defaultLowOutputLatency;
    outParams.hostApiSpecificStreamInfo = nullptr;

    PaStream *stream;
    Pa_OpenStream(&stream, nullptr, &outParams, SAMPLE_RATE, 256, paClipOff, audioCallback, nullptr);
    Pa_StartStream(stream);

    std::cout << "[ PIANO ENGINE STARTED ]\nPress Enter to toggle modes.\n";
    setRawMode(true);

    while (true)
    {
        char c = readChar();
        if (c == 3)
            break; // Ctrl+C

        if (c == '\n')
        {
            currentMode = (currentMode == Mode::Drum) ? Mode::Piano : Mode::Drum;
            std::cout << (currentMode == Mode::Drum ? "\n[ DRUMS ]\n" : "\n[ PIANO ]\n");
        }

        if (currentMode == Mode::Drum)
        {
            if (c == 'j')
                snarePH = 0;
            if (c == ' ')
                kickPH = 0;
            if (c == 'f')
                hatPH = 0;
        }
        else
        {
            if (c == 'D')
            {
                octave = std::max(-2, octave - 1);
                regeneratePiano();
            }
            else if (c == 'C')
            {
                octave = std::min(2, octave + 1);
                regeneratePiano();
            }

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
                pianoPH[n] = 0;
        }
    }

    setRawMode(false);
    Pa_StopStream(stream);
    Pa_Terminate();
    return 0;
}
