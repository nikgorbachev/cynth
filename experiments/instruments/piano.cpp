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

    ofstream file("pia.wav", ios::binary);
    writeWavHeader(file, numSamples);

    for (int n = 0; n < numSamples; n++) {
        float t = (float)n / SAMPLE_RATE;

        float amp = exp(-t * 2.5f);
        float sample = 0.0f;

	float freqs[3] = {f1, f2, f3};

	for (int i = 0; i < 3; i++) {
		float f = freqs[i];
		for (int k = 1; k <= 8; k++) {
			float harmonicAmp = (1.0f / k) * exp(-t * k * 3.0f);
			sample += harmonicAmp * sin(2 * PI * k * f * t);
		}
	}

        sample *= 0.2f;     // reduce volume  
        sample *= amp;      // apply envelope

        // Convert to 16-bit PCM
        short s = (short)(sample * 32767);
        file.write((char*)&s, 2);
    }

    file.close();
    cout << "Wrote pia.wav (C major chord)" << endl;
    
    return 0;
}

