#!/usr/bin/env python3
import sys
import numpy as np
import matplotlib.pyplot as plt
from scipy.io import wavfile

def plot_audio(filename):
    sr, data = wavfile.read(filename)

    # Handle stereo
    if len(data.shape) > 1:
        data = data[:, 0]  # left channel

    t = np.linspace(0, len(data)/sr, len(data))

    # FFT
    N = len(data)
    fft = np.fft.rfft(data)
    freqs = np.fft.rfftfreq(N, 1/sr)
    magnitude = np.abs(fft)

    plt.figure(figsize=(12, 10))

    # 1. Waveform
    plt.subplot(3, 1, 1)
    plt.title(f"Waveform: {filename}")
    plt.plot(t, data, linewidth=0.8)
    plt.xlabel("Time (s)")
    plt.ylabel("Amplitude")

    # 2. Spectrum
    plt.subplot(3, 1, 2)
    plt.title("FFT Magnitude Spectrum")
    plt.plot(freqs, magnitude, linewidth=0.8)
    plt.xlabel("Frequency (Hz)")
    plt.ylabel("Magnitude")
    plt.xlim(0, 5000)  # zoom (snare has noise up to high frequencies)

    # 3. Spectrogram
    plt.subplot(3, 1, 3)
    plt.specgram(data, Fs=sr, NFFT=1024, noverlap=512, cmap="magma")
    plt.title("Spectrogram")
    plt.xlabel("Time (s)")
    plt.ylabel("Frequency (Hz)")

    plt.tight_layout()

    out_path = filename.rsplit(".", 1)[0] + ".png"
    plt.savefig(out_path, dpi=200)
    print(f"Saved plot to {out_path}")



if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python3 plot_audio.py file.wav")
        sys.exit(1)

    filename = sys.argv[1]
    plot_audio(filename)

