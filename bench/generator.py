# quick generator: connect to engine and send many SUBMIT orders, measure RTT for ACKs.
import socket, time, sys
import random

HOST = '127.0.0.1'
PORT = 6666
N = 200000

if len(sys.argv) > 1:
    N = int(sys.argv[1])

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((HOST, PORT))
s.settimeout(2.0)

start = time.time()
for i in range(1, N+1):
    side = 'B' if (i % 2 == 0) else 'S'
    price = 1000 + random.randint(0, 10)
    qty = random.randint(1, 10)
    line = f"SUBMIT {i} {side} {price} {qty}\n"
    send_t = time.time()
    s.sendall(line.encode())
    # read responses until ACK or FILL lines consumed (non-robust but fine for starter)
    try:
        data = s.recv(4096)
    except socket.timeout:
        print("timeout waiting for response")
        break
    recv_t = time.time()
    # crude RTT
    rtt_us = (recv_t - send_t) * 1e6
    if i % 10000 == 0:
        print(f"sent {i}, rtt_us ~ {rtt_us:.1f}, elapsed {recv_t-start:.1f}s")
end = time.time()
print("done, total sec:", end-start)
s.close()

