import json
import numpy as np
import matplotlib.pyplot as plt

# === CONFIG ===
filename = "data.txt"   # your JSON lines file
signal_keys = ["out1", "out2", "out3"]

# === LOAD DATA ===
timestamps = []
signals = {k: [] for k in signal_keys}

with open(filename) as f:
    for line in f:
        if not line.strip():
            continue
        data = json.loads(line)
        timestamps.append(data["timestamp"])
        for k in signal_keys:
            v = data[k]
            if v == "--":
                signals[k].append(np.nan)
            else:
                signals[k].append(float(v))

timestamps = np.array(timestamps)
dt = np.median(np.diff(timestamps)) / 1000.0  # convert ms → seconds

print(f"Estimated sampling interval: {dt:.6f} s  ({1/dt:.2f} Hz sample rate)")

# === PROCESS EACH SIGNAL ===
for k in signal_keys:
    y = np.array(signals[k], dtype=float)
    # Fill missing values (linear interpolation)
    nans = np.isnan(y)
    if np.any(nans):
        y[nans] = np.interp(np.flatnonzero(nans), np.flatnonzero(~nans), y[~nans])

    N = len(y)
    Y = np.fft.fft(y)
    freqs = np.fft.fftfreq(N, dt)

    # Keep only positive frequencies
    idx = freqs >= 0
    freqs = freqs[idx]
    amp = np.abs(Y[idx]) / N  # amplitude spectrum

    plt.figure()
    plt.plot(freqs, amp)
    plt.title(f"Amplitude Spectrum for {k}")
    plt.xlabel("Frequency [Hz]")
    plt.ylabel("Amplitude")
    plt.grid(True)

plt.show()
