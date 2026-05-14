import socket
import os
import mimetypes


class HTTPProcessor:
    def __init__(self, static_dir='www'):
        self.static_dir = static_dir
        os.makedirs(self.static_dir, exist_ok=True)

    def safe_path(self, path):
        path = os.path.normpath(path).lstrip("/\\")
        full_path = os.path.abspath(
            os.path.join(self.static_dir, path)
        )

        root = os.path.abspath(self.static_dir)

        if not full_path.startswith(root):
            raise PermissionError("Forbidden")

        return full_path

    def process_request(self, request_data):
        try:
            header_data, body = request_data.split(b'\r\n\r\n', 1)

            header_text = header_data.decode('utf-8', errors='ignore')
            lines = header_text.splitlines()

            if not lines:
                return self.error_response(400, "Bad Request")

            request_line = lines[0].split()

            if len(request_line) < 2:
                return self.error_response(400, "Bad Request")

            method = request_line[0]
            path = request_line[1]

            file_path = self.safe_path(path)

            if method == "GET":
                return self.handle_get(file_path)

            elif method == "POST":
                return self.handle_post(file_path, body)

            elif method == "PUT":
                return self.handle_put(file_path, body)

            elif method == "DELETE":
                return self.handle_delete(file_path)

            else:
                return self.error_response(405, "Method Not Allowed")

        except PermissionError:
            return self.error_response(403, "Forbidden")

        except Exception as e:
            print("[!] Server error:", e)
            return self.error_response(500, "Internal Server Error")

    def handle_get(self, file_path):
        if os.path.isdir(file_path):
            file_path = os.path.join(file_path, "index.html")

        if not os.path.exists(file_path):
            return self.error_response(404, "Not Found")

        with open(file_path, 'rb') as f:
            content = f.read()

        mime_type, _ = mimetypes.guess_type(file_path)
        mime_type = mime_type or 'application/octet-stream'

        return self.build_header(
            200,
            "OK",
            mime_type,
            len(content)
        ) + content

    def handle_post(self, file_path, body):
        os.makedirs(os.path.dirname(file_path), exist_ok=True)

        with open(file_path, 'wb') as f:
            f.write(body)

        print(f"[+] Saved: {file_path}")

        return self.success_response(201, "Created")

    def handle_put(self, file_path, body):
        if not os.path.exists(file_path):
            return self.error_response(404, "File Not Found")

        new_name = body.decode().strip()

        if not new_name:
            return self.error_response(400, "Missing new name")

        new_name = os.path.basename(new_name)

        new_path = os.path.join(
            os.path.dirname(file_path),
            new_name
        )

        os.rename(file_path, new_path)

        print(f"[+] Renamed: {file_path} -> {new_path}")

        return self.success_response(200, "Renamed")

    def handle_delete(self, file_path):
        if not os.path.exists(file_path):
            return self.error_response(404, "Not Found")

        os.remove(file_path)

        print(f"[+] Deleted: {file_path}")

        return self.success_response(200, "Deleted")

    def success_response(self, code, message):
        body = f"<h1>{code} {message}</h1>".encode()

        return self.build_header(
            code,
            message,
            "text/html",
            len(body)
        ) + body

    def error_response(self, code, message):
        body = f"<h1>{code} {message}</h1>".encode()

        return self.build_header(
            code,
            message,
            "text/html",
            len(body)
        ) + body

    def build_header(self, code, message, mime, length):
        headers = [
            f"HTTP/1.1 {code} {message}",
            f"Content-Type: {mime}",
            f"Content-Length: {length}",
            "Connection: close",
            "",
            ""
        ]

        return "\r\n".join(headers).encode()


class SimpleHTTPServer:
    def __init__(self, host='127.0.0.1', port=8888):
        self.host = host
        self.port = port
        self.processor = HTTPProcessor()

    def receive_request(self, client_socket):
        data = b""

        while b"\r\n\r\n" not in data:
            chunk = client_socket.recv(4096)

            if not chunk:
                return b""

            data += chunk

        headers, body = data.split(b"\r\n\r\n", 1)

        header_text = headers.decode(errors='ignore')

        content_length = 0

        for line in header_text.splitlines():
            if line.lower().startswith("content-length"):
                content_length = int(line.split(":")[1].strip())

        while len(body) < content_length:
            chunk = client_socket.recv(4096)

            if not chunk:
                break

            body += chunk

        return headers + b"\r\n\r\n" + body

    def start(self):
        server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        server_socket.setsockopt(
            socket.SOL_SOCKET,
            socket.SO_REUSEADDR,
            1
        )

        server_socket.bind((self.host, self.port))
        server_socket.listen(5)

        print(f"[*] Running: http://{self.host}:{self.port}")

        try:
            while True:
                client_socket, addr = server_socket.accept()

                print(f"[+] Connection: {addr}")

                request = self.receive_request(client_socket)

                if request:
                    response = self.processor.process_request(request)
                    client_socket.sendall(response)

                client_socket.close()

        except KeyboardInterrupt:
            print("\n[!] Server stopped")

        finally:
            server_socket.close()


if __name__ == "__main__":
    server = SimpleHTTPServer()
    server.start()