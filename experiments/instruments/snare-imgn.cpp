#include <iostream>
#include <fstream>
#include <cmath>
#include <random>

using namespace std;

const int SAMPLE_RATE = 44100;

// ------------------------------------------------------------
// Simple one-pole low-pass filter
// ------------------------------------------------------------
double lowpass(double input, double& state, double cutoffHz) {
    double RC = 1.0 / (2.0 * M_PI * cutoffHz);
    double dt = 1.0 / SAMPLE_RATE;
    double alpha = dt / (RC + dt);
    state += alpha * (input - state);
    return state;
}

// ------------------------------------------------------------
// Write a proper WAV header + samples
// ------------------------------------------------------------
void writeWav(const char* filename, const short* samples, int numSamples) {
    ofstream file(filename, ios::binary);

    int subchunk2Size = numSamples * 2;
    int chunkSize = 36 + subchunk2Size;
    int byteRate = SAMPLE_RATE * 2;

    // RIFF header
    file.write("RIFF", 4);
    file.write((char*)&chunkSize, 4);
    file.write("WAVE", 4);

    // fmt chunk
    file.write("fmt ", 4);
    int subchunk1Size = 16;
    short audioFormat = 1;
    short numChannels = 1;
    short bitsPerSample = 16;
    short blockAlign = numChannels * bitsPerSample / 8;

    file.write((char*)&subchunk1Size, 4);
    file.write((char*)&audioFormat, 2);
    file.write((char*)&numChannels, 2);
    file.write((char*)&SAMPLE_RATE, 4);
    file.write((char*)&byteRate, 4);
    file.write((char*)&blockAlign, 2);
    file.write((char*)&bitsPerSample, 2);

    // data chunk
    file.write("data", 4);
    file.write((char*)&subchunk2Size, 4);

    // samples
    for (int i = 0; i < numSamples; i++) {
        file.write((char*)&samples[i], 2);
    }
}

// ------------------------------------------------------------
// Generate Imagine-style acoustic snare
// ------------------------------------------------------------
int main() {
    double durationSec = 0.15;
    int numSamples = int(durationSec * SAMPLE_RATE);

    short* buffer = new short[numSamples];

    std::mt19937 rng(1234);
    std::uniform_real_distribution<double> noise(-1.0, 1.0);

    double noiseLP = 0.0;
    double outLP = 0.0;

    for (int i = 0; i < numSamples; i++) {
        double t = double(i) / SAMPLE_RATE;

        // Rounded envelopes (very important)
        double noiseEnv = exp(-t * 14.0) * (1.0 - exp(-t * 180.0));
        double toneEnv  = exp(-t * 22.0);

        // Band-limited noise (snare wires)
        double n = noise(rng) * noiseEnv;
        n = lowpass(n, noiseLP, 5500.0);

        // Low, dirty body tone
        double tone = sin(2.0 * M_PI * 150.0 * t);
        tone = tanh(tone * 2.0);
        tone *= toneEnv;

        // Mix
        double sample = 0.9 * n + 0.25 * tone;

        // Tape / console shaping
        sample = lowpass(sample, outLP, 6000.0);
        sample = tanh(sample * 1.4);

        buffer[i] = (short)(sample * 28000);
    }

    writeWav("snare_imagine.wav", buffer, numSamples);

    delete[] buffer;

    cout << "Generated snare_imagine.wav" << endl;
    return 0;
}
