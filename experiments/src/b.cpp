#include <iostream>
#include <fstream>
#include <cmath>
#include <random>

using namespace std;

const int SAMPLE_RATE = 44100;

// Write a proper WAV header + samples
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

    // write samples
    for (int i = 0; i < numSamples; i++) {
        file.write((char*)&samples[i], 2);
    }
}

// Generate a very simple electronic snare
int main() {
    double durationSec = 0.15; // 150 ms snare
    int numSamples = durationSec * SAMPLE_RATE;

    short* buffer = new short[numSamples];

    std::mt19937 rng(1234);
    std::uniform_real_distribution<double> noise(-1.0, 1.0);

    double toneFreq = 180.0;

    for (int i = 0; i < numSamples; i++) {
        double t = double(i) / SAMPLE_RATE;

        // envelopes (fast decay)
        double noiseEnv = exp(-t * 25.0);  // noise decays quickly
        double toneEnv  = exp(-t * 32.0);  // tone decays even faster

        // components
        double n = noise(rng) * noiseEnv;
        double tone = sin(2.0 * M_PI * toneFreq * t) * toneEnv;

        // mix
        double sample = 0.8 * n + 0.4 * tone;

        // convert to 16-bit PCM
        buffer[i] = (short)(sample * 30000);
    }

    writeWav("snare.wav", buffer, numSamples);

    delete[] buffer;

    cout << "Generated snare.wav" << endl;
    return 0;
}

