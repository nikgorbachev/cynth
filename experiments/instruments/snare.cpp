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
// Generate a simple electronic closed hi-hat
int main() {
    double durationSec = 0.08; // 80 ms closed hat
    int numSamples = durationSec * SAMPLE_RATE;

    short* buffer = new short[numSamples];

    std::mt19937 rng(5678);
    std::uniform_real_distribution<double> noise(-1.0, 1.0);

    // metallic partial frequencies (inharmonic)
    double freqs[] = { 8000.0, 9500.0, 12000.0, 14500.0 };
    int numPartials = 4;

    for (int i = 0; i < numSamples; i++) {
        double t = double(i) / SAMPLE_RATE;

        // very fast envelope
        double env = exp(-t * 45.0);

        // noise component (high-frequency emphasis)
        double n = noise(rng);

        // crude high-pass: subtract low component
        static double prevNoise = 0.0;
        double hpNoise = n - 0.98 * prevNoise;
        prevNoise = n;

        // metallic ringing
        double metal = 0.0;
        for (int k = 0; k < numPartials; k++) {
            metal += sin(2.0 * M_PI * freqs[k] * t);
        }
        metal /= numPartials;

        double sample =
            0.7 * hpNoise +
            0.5 * metal;

        sample *= env;

        buffer[i] = (short)(sample * 24000);
    }

    writeWav("hihat.wav", buffer, numSamples);
    delete[] buffer;

    cout << "Generated hihat.wav" << endl;
    return 0;
}


