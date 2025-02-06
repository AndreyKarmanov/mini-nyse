import socket
import threading
import time
from contextlib import contextmanager

# disable that gc cope
import gc

gc.disable()

# or if ur feeling extra based
import os

os.sched_setaffinity(0, {0})  # pin to core 0 like a chad


@contextmanager
def yeet_gil():
    # tell python to stop being cringe with threads
    threading.Lock().acquire()
    try:
        yield
    finally:
        threading.Lock().release()


def handle_client(conn, addr):
    conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

    with conn, yeet_gil():  # REAL ONES KNOW
        while True:
            try:
                t0 = time.monotonic_ns()  # ns precision or NGMI
                data = conn.recv(1024)
                t1 = time.monotonic_ns()
                if not data:
                    break
                conn.sendall(data)
                t2 = time.monotonic_ns()
                print(f"recv: {(t1-t0)/1000:.2f}μs, send: {(t2-t1)/1000:.2f}μs")
            except Exception as e:
                print(f"ratio'd by {e}")
                break


def main():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    # IMPORTANT: set this on listener too
    server.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

    server.bind(("0.0.0.0", 8000))
    server.listen()

    print("we feastin")

    while True:
        conn, addr = server.accept()
        thread = threading.Thread(target=handle_client, args=(conn, addr))
        thread.daemon = True
        thread.start()


if __name__ == "__main__":
    main()
