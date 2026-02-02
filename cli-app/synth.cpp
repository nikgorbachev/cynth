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

constexpr double PIANO_DUR = 2.5;
constexpr int PIANO_N = int(PIANO_DUR * SAMPLE_RATE);
constexpr int MAX_PIANO_NOTES = 20;

constexpr int SNARE_N = int(SNARE_DUR * SAMPLE_RATE);
constexpr int KICK_N  = int(KICK_DUR  * SAMPLE_RATE);
constexpr int HAT_N   = int(HAT_DUR   * SAMPLE_RATE);


enum class Mode {
    Drum,
    Piano
};

std::atomic<Mode> currentMode(Mode::Drum);

int octave = 0;  // 0 = C4, +1 = C5, -1 = C3
// octave = std::max(-2, std::min(2, octave));

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


float piano[MAX_PIANO_NOTES][PIANO_N];
std::atomic<int> pianoPH[MAX_PIANO_NOTES];


bool sustainPedal = false;


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






// void generatePianoNote(float* buffer, double freq) {
//     for (int i = 0; i < PIANO_N; i++) {
//         double t = double(i) / SAMPLE_RATE;

//         // soft attack + long decay
//         double env =
//             (1.0 - exp(-t * 6.0)) *   // attack
//             exp(-t * 1.6);            // decay

//         double s = 0.0;

//         for (int k = 1; k <= 8; k++) {
//             double harmAmp = (1.0 / k) * exp(-t * k * 2.5);
//             s += harmAmp * sin(2.0 * M_PI * k * freq * t);
//         }

//         buffer[i] = (float)(s * env * 0.25);
//     }
// }



struct Resonator {
    double y1 = 0.0, y2 = 0.0;
    double a1, a2, b0;

    void setup(double freq, double decay) {
        double r = exp(-decay);
        double w = 2.0 * M_PI * freq / SAMPLE_RATE;
        a1 = -2.0 * r * cos(w);
        a2 = r * r;
        b0 = 1.0 - r;
    }

    inline double process(double x) {
        double y = b0 * x - a1 * y1 - a2 * y2;
        y2 = y1;
        y1 = y;
        return y;
    }
};




void generatePianoNote(float* buffer, double freq, bool sustain) {
    // 3 strings per note
    double detune[3] = { -0.0025, 0.0, +0.002 };

    // Soundboard resonances (broad + musical)
    Resonator board[3];
    board[0].setup(180.0, 0.002);
    board[1].setup(420.0, 0.003);
    board[2].setup(920.0, 0.004);

    for (int i = 0; i < PIANO_N; i++) {
        double t = double(i) / SAMPLE_RATE;

        // --- Envelope ---
        double attack = 1.0 - exp(-t * 35.0);
        double decayRate = sustain ? 0.25 : 1.2;
        double env = attack * exp(-t * decayRate);

        double s = 0.0;

        // --- Strings ---
        for (int st = 0; st < 3; st++) {
            double f = freq * (1.0 + detune[st]);

            for (int k = 1; k <= 12; k++) {
                double inharm = 1.0 + 0.001 * k * k;
                double hf = f * k * inharm;

                // High partials die fast
                double partialDecay = exp(-t * k * (sustain ? 1.8 : 3.5));
                double amp = (1.0 / k) * partialDecay;

                s += amp * sin(2.0 * M_PI * hf * t);
            }
        }

        // --- Hammer excitation (felt, not metal) ---
        double hammer =
            exp(-t * 180.0) *
            sin(2.0 * M_PI * freq * 2.5 * t);

        // --- Soundboard ---
        double boardOut = 0.0;
        for (int r = 0; r < 3; r++)
            boardOut += board[r].process(s);

        // --- Combine ---
        double sample =
            (s * 0.7 + boardOut * 0.6 + hammer * 0.15) * env;

        // Gentle saturation (tape / console)
        sample = tanh(sample * 1.1);

        buffer[i] = (float)(sample * 0.28);
    }
}




double pianoFreqs[MAX_PIANO_NOTES] = {
    261.63, // C
    277.18, // C#
    293.66, // D
    311.13, // D#
    329.63, // E
    349.23, // F
    369.99, // F#
    392.00, // G
    415.30, // G#
    440.00, // A
    466.16, // A#
    493.88, // B
    523.25, // C
    554.37, // C#
    587.33, // D
    622.25, // D#
    659.25, // E
    698.46, // F
    739.99, // F#
    783.99,  // G
    
};




void regeneratePiano() {
    for (int i = 0; i < MAX_PIANO_NOTES; i++) {
        generatePianoNote(
            piano[i],
            pianoFreqs[i] * pow(2.0, octave),
            sustainPedal
        );
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

        for (int n = 0; n < MAX_PIANO_NOTES; n++) {
            int p = pianoPH[n].load();
            if (p >= 0 && p < PIANO_N) {
                mix += piano[n][p];
                pianoPH[n].fetch_add(1);
            }
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
    for (int i = 0; i < MAX_PIANO_NOTES; i++) {
        pianoPH[i] = -1;
        generatePianoNote(piano[i], pianoFreqs[i], false);
    }


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

        if (c == '\n') {
            currentMode = (currentMode == Mode::Drum)
                ? Mode::Piano
                : Mode::Drum;

            std::cout << (currentMode == Mode::Drum
                ? "\n[ DRUM MODE ]\n"
                : "\n[ PIANO MODE ]\n");
        }

         // D or C

        

        if (currentMode == Mode::Drum) {
            if (c == 'j') snarePH = 0;
            if (c == ' ') kickPH  = 0;
            if (c == 'f') hatPH   = 0;
        }


        if (currentMode == Mode::Piano) {

            if (c == 'D') {
                octave = std::max(-2, octave - 1);
                regeneratePiano();
                std::cout << "Octave: " << octave << "\n";
            }
            else if (c == 'C') {
                octave = std::min(2, octave + 1);
                regeneratePiano();
                std::cout << "Octave: " << octave << "\n";
            }

            switch (c) {
                case 'a': pianoPH[0] = 0; break;
                case 'w': pianoPH[1] = 0; break;
                case 's': pianoPH[2] = 0; break;
                case 'e': pianoPH[3] = 0; break;
                case 'd': pianoPH[4] = 0; break;
                case 'f': pianoPH[5] = 0; break;
                case 't': pianoPH[6] = 0; break;
                case 'g': pianoPH[7] = 0; break;
                case 'y': pianoPH[8] = 0; break;
                case 'h': pianoPH[9] = 0; break;
                case 'u': pianoPH[10] = 0; break;
                case 'j': pianoPH[11] = 0; break;
                case 'k': pianoPH[12] = 0; break;
                case 'o': pianoPH[13] = 0; break;
                case 'l': pianoPH[14] = 0; break;
                case 'p': pianoPH[15] = 0; break;
                case ';': pianoPH[16] = 0; break;
                case '\'': pianoPH[17] = 0; break;
                case ']': pianoPH[18] = 0; break;
                case '\\': pianoPH[19] = 0; break;
            }
        }


    }

    setRawMode(false);
    Pa_StopStream(stream);
    Pa_Terminate();
}
