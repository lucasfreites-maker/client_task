import socket
import threading
import matplotlib.pyplot as plt
from collections import deque
import json

ports = [4001, 4002, 4003]

buffers = {p: deque(maxlen=200) for p in ports}

def read_from_port(port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(("127.0.0.1", port))
    print(f"✅ Connected to port {port}")
    while True:
        data = sock.recv(1024)
        if not data:
            break
        try:
            msg = data.decode().strip()
            val = float(msg)
            buffers[port].append(val)
        except:
            pass

# A thread for each tcp port
for p in ports:
    threading.Thread(target=read_from_port, args=(p,), daemon=True).start()

plt.ion()
fig, ax = plt.subplots()
lines = [ax.plot([], [], label=f"out{i+1}")[0] for i in range(3)]
ax.legend()

while True:
    for i, port in enumerate(ports):
        lines[i].set_data(range(len(buffers[port])), list(buffers[port]))
    ax.relim()
    ax.autoscale_view()
    plt.pause(0.1)
