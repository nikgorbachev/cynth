#include <iostream>
#include <fstream>
#include <cmath>

using namespace std;

const int SAMPLE_RATE = 44100;
const float PI = 3.141592653589793f;

// Write a simple .wav file header
void writeWavHeader(ofstream &file, int numSamples) {
    int byteRate = SAMPLE_RATE * 2;
    int subchunk2Size = numSamples * 2;

    // RIFF chunk descriptor
    file.write("RIFF", 4);
    int chunkSize = 36 + subchunk2Size;
    file.write((char*)&chunkSize, 4);
    file.write("WAVE", 4);

    // fmt subchunk
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

    // data subchunk
    file.write("data", 4);
    file.write((char*)&subchunk2Size, 4);
}

int main() {
    float duration = 2.0f;
    int numSamples = (int)(duration * SAMPLE_RATE);

    // C major chord frequencies (C4–E4–G4)
    float f1 = 261.63f;
    float f2 = 329.63f;
    float f3 = 392.00f;

    ofstream file("out.wav", ios::binary);
    writeWavHeader(file, numSamples);

    for (int n = 0; n < numSamples; n++) {
        float t = (float)n / SAMPLE_RATE;

        // amplitude envelope (simple)
        float attack = 0.5f;   // first 0.5 sec fade in
        float decay  = 0.5f;   // last 0.5 sec fade out
        float amp = 1.0f;

        // attack
        if (t < attack)
            amp *= (t / attack);

        // decay
        if (t > duration - decay)
            amp *= (duration - t) / decay;

        // chord: sum of three sines
        float sample =
            sin(2 * PI * f1 * t) +
            sin(2 * PI * f2 * t) +
            sin(2 * PI * f3 * t);

        sample *= 0.3f;     // reduce volume  
        sample *= amp;      // apply envelope

        // Convert to 16-bit PCM
        short s = (short)(sample * 32767);
        file.write((char*)&s, 2);
    }

    file.close();
    cout << "Wrote out.wav (C major chord)" << endl;
}

