# generator.py – spam SUBMITs to the engine and measure RTT
import socket
import time
import sys
import random

HOST = "127.0.0.1"
PORT = 6666
N = 200000

if len(sys.argv) > 1:
    N = int(sys.argv[1])

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((HOST, PORT))
s.settimeout(2.0)

start = time.time()
buf = b""

for i in range(1, N + 1):
    side = random.choice(["B", "S"])
    price = random.randint(95, 105)
    qty = random.randint(1, 10)
    line = f"SUBMIT {i} {side} {price} {qty}\n".encode()

    send_t = time.time()
    s.sendall(line)

    # Very simple: read until we see at least one newline in response
    while b"\n" not in buf:
        try:
            chunk = s.recv(4096)
        except socket.timeout:
            print("timeout waiting for response")
            s.close()
            sys.exit(1)
        if not chunk:
            print("server closed connection")
            s.close()
            sys.exit(1)
        buf += chunk

    # take one line (you might get more than one in buf)
    line_end = buf.find(b"\n")
    resp = buf[:line_end].decode(errors="ignore")
    buf = buf[line_end + 1 :]

    recv_t = time.time()
    rtt_us = (recv_t - send_t) * 1e6

    if i % 10000 == 0:
        print(f"sent {i}, last RTT ≈ {rtt_us:.1f} µs, elapsed {recv_t - start:.2f}s")

end = time.time()
print(f"done {N} orders in {end - start:.2f}s")
s.close()
