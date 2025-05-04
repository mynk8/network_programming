import socket

def request_file(file_path):
    host = '127.0.0.1'
    port = 8989

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        print("[+] Connecting to server...")
        s.connect((host, port))

        print(f"[+] Sending file path: {file_path}")
        s.sendall((file_path + "\n").encode())

        print("[+] Receiving file contents...")
        data = b''
        while True:
            chunk = s.recv(4096)
            if not chunk:
                break
            data += chunk

        print("[+] File received:\n")
        print(data.decode(errors="replace"))

if __name__ == "__main__":
    request_file("/home/lucky11/.bash_history")
