import json
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

filename = "data.txt"
interpolate_missing = True

# Reading from file with data extracted from client2
with open(filename, "r") as f:
    lines = [json.loads(line) for line in f if line.strip()]

df = pd.DataFrame(lines)
df["time_s"] = (df["timestamp"] - df["timestamp"].iloc[0]) / 1000.0

for col in ["out1", "out2", "out3"]:
    df[col] = pd.to_numeric(df[col], errors="coerce")

# Interpolate missing data for smoother lines
if interpolate_missing:
    df[["out1", "out2", "out3"]] = df[["out1", "out2", "out3"]].interpolate()

plt.figure(figsize=(12, 6))
colors = ["tab:blue", "tab:orange", "tab:green"]

for i, col in enumerate(["out1", "out2", "out3"]):
    if df[col].notna().any():
        plt.plot(df["time_s"], df[col], label=col, color=colors[i], lw=1.5)

plt.title("Signals from data.txt")
plt.xlabel("Time [s]")
plt.ylabel("Amplitude")
plt.legend()
plt.grid(True)
plt.ylim(-8, 8)
plt.yticks(np.arange(-8, 9, 1))
plt.tight_layout()
plt.show()