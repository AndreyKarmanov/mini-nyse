import socket
import threading
from datetime import datetime

# Keep track of active connections
active_connections = 0
connection_lock = threading.Lock()

def handle_client(conn, addr):
    global active_connections
    
    # Disable Nagle's algorithm for the client connection
    conn.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    
    client_id = f"{addr[0]}:{addr[1]}"
    with connection_lock:
        active_connections += 1
        print(f"[{datetime.now().strftime('%H:%M:%S')}] New connection from {client_id}. Active connections: {active_connections}")

    with conn:
        total_bytes = 0
        try:
            while True:
                data = conn.recv(1024)
                if not data:
                    break
                total_bytes += len(data)
                conn.sendall(data)
        except Exception as e:
            print(f"[{datetime.now().strftime('%H:%M:%S')}] Error with {client_id}: {str(e)}")
        finally:
            with connection_lock:
                active_connections -= 1
                print(f"[{datetime.now().strftime('%H:%M:%S')}] {client_id} disconnected. Bytes processed: {total_bytes}. Active connections: {active_connections}")

def main():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    # Disable Nagle's algorithm for the server socket
    server.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    server.bind(('127.0.0.1', 8000))
    server.listen()
    
    print(f"[{datetime.now().strftime('%H:%M:%S')}] Server started on port 8000")
    
    while True:
        conn, addr = server.accept()
        thread = threading.Thread(target=handle_client, args=(conn, addr))
        thread.daemon = True
        thread.start()

if __name__ == '__main__':
    main()