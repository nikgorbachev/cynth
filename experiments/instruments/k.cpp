#include <iostream>
#include <fstream>
#include <cmath>

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

    for (int i = 0; i < numSamples; i++) {
        file.write((char*)&samples[i], 2);
    }
}

int main() {
    double durationSec = 0.5; // longer than snare
    int numSamples = durationSec * SAMPLE_RATE;

    short* buffer = new short[numSamples];

    // Kick parameters
    double startFreq = 120.0;
    double endFreq   = 40.0;

    for (int i = 0; i < numSamples; i++) {
        double t = double(i) / SAMPLE_RATE;

        // Amplitude envelope (fast decay)
        double ampEnv = exp(-t * 8.0);

        // Pitch envelope (exponential drop)
        double freq = endFreq + (startFreq - endFreq) * exp(-t * 20.0);

        // Phase accumulation
        double phase = 2.0 * M_PI * freq * t;

        double sample = sin(phase) * ampEnv;

        buffer[i] = (short)(sample * 30000);
    }

    writeWav("kick.wav", buffer, numSamples);
    delete[] buffer;

    cout << "Generated kick.wav" << endl;
    return 0;
}

