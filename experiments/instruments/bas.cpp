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
    float duration = 2.5f;
    int numSamples = (int)(duration * SAMPLE_RATE);

    // C major chord frequencies (C4–E4–G4)
    float baseFreq = 65.41f;

    ofstream file("bas.wav", ios::binary);
    writeWavHeader(file, numSamples);

    for (int n = 0; n < numSamples; n++) {
        float t = (float)n / SAMPLE_RATE;

	float attack = 0.01f;
        float sample = 0.0f;

	float amp;
	if (t < attack) {
	    amp = t / attack; 
	} else {
	    amp = exp(-(t - attack) * 1.5f);
	}

	for (int k = 1; k <= 6; k++) {
            float harmonicAmp =
                (1.0f / (k * k)) * exp(-t * k * 1.5f);

            sample += harmonicAmp *
                      sin(2 * PI * k * baseFreq * t);
        }


        sample *= 0.4f;     // reduce volume  
        sample *= amp;      // apply envelope

        
        short s = (short)(sample * 32767);
        file.write((char*)&s, 2);
    }

    file.close();
    cout << "Wrote bas.wav (C major chord)" << endl;
    
    return 0;
}

