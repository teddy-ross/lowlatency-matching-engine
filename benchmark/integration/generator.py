# generator.py – spam SUBMITs to the engine and measure RTT
import socket
import time
import sys
import random

HOST = "127.0.0.1"
PORT = 6767
N = 200000

def recv_line(sock, buf):
    """Read one newline-terminated line; return (line_str, buf)."""
    while b"\n" not in buf:
        chunk = sock.recv(4096)
        if not chunk:
            raise ConnectionError("server closed connection")
        buf += chunk
    j = buf.find(b"\n")
    line = buf[:j].decode(errors="ignore").rstrip("\n")
    buf = buf[j + 1 :]
    return line, buf


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

    # Drain all lines for this request until we see ACK
    while True:
        try:
            resp, buf = recv_line(s, buf)
        except socket.timeout:
            print("timeout waiting for response")
            s.close()
            sys.exit(1)

        # ignore FILL lines, stops when we see the ACK for this id
        if resp.startswith("ACK "):
            parts = resp.split()
            if len(parts) >= 2 and parts[1].isdigit() and int(parts[1]) == i:
                break

    recv_t = time.time()
    rtt_us = (recv_t - send_t) * 1e6

    if i % 10000 == 0:
        print(f"sent {i}, last RTT ≈ {rtt_us:.1f} µs, elapsed {recv_t - start:.2f}s")

end = time.time()
print(f"done {N} orders in {end - start:.2f}s")
s.close()




