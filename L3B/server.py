import socket
import os
import mimetypes
import json
import secrets
from urllib.parse import unquote, urlparse, parse_qs


class HTTPProcessor:
    def __init__(self, static_dir='www'):
        self.static_dir = static_dir
        self.admin_user = "admin"
        self.admin_pass = "admin"
        self.sessions = set()
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

            header_text = header_data.decode(
                'utf-8',
                errors='ignore'
            )

            lines = header_text.splitlines()

            if not lines:
                return self.error_response(400, "Bad Request")

            request_line = lines[0].split()

            if len(request_line) < 2:
                return self.error_response(400, "Bad Request")

            method = request_line[0]
            raw_path = request_line[1]

            parsed = urlparse(raw_path)

            path = unquote(parsed.path)
            query = parse_qs(parsed.query)

            headers = self.parse_headers(header_text)

            # LOGIN
            if path == "/login" and method == "POST":
                creds = json.loads(body.decode())

                if (creds.get("username") ==
                        self.admin_user and
                        creds.get("password") ==
                        self.admin_pass):

                    token = secrets.token_hex(16)

                    self.sessions.add(token)

                    response = b'{"success": true}'

                    return self.build_header(
                        200,
                        "OK",
                        "application/json",
                        len(response),
                        [
                            f"Set-Cookie: session={token}; HttpOnly"
                        ]
                    ) + response

                return self.error_response(
                    403,
                    "Invalid credentials"
                )

            # ADMIN API
            if path.startswith("/admin-api"):

                if not self.is_authenticated(headers):
                    return self.error_response(
                        403,
                        "Forbidden"
                    )

                rel_path = query.get("path", [""])[0]

                folder = self.safe_path(rel_path)

                # LIST
                if path == "/admin-api/list":

                    if not os.path.exists(folder):
                        return self.error_response(
                            404,
                            "Not Found"
                        )

                    data = self.list_directory(folder)

                    return self.json_response(data)

                # UPLOAD
                elif path == "/admin-api/upload":

                    filename = headers.get(
                        "x-filename",
                        "file.bin"
                    )

                    filename = os.path.basename(filename)

                    save_path = os.path.join(
                        folder,
                        filename
                    )

                    os.makedirs(folder, exist_ok=True)

                    with open(save_path, "wb") as f:
                        f.write(body)

                    return self.success_response(
                        201,
                        "Uploaded"
                    )

                # DELETE
                elif path == "/admin-api/delete":

                    if not os.path.exists(folder):
                        return self.error_response(
                            404,
                            "Not Found"
                        )

                    os.remove(folder)

                    return self.success_response(
                        200,
                        "Deleted"
                    )

                # RENAME
                elif path == "/admin-api/rename":

                    new_name = body.decode().strip()

                    new_name = os.path.basename(new_name)

                    new_path = os.path.join(
                        os.path.dirname(folder),
                        new_name
                    )

                    os.rename(folder, new_path)

                    return self.success_response(
                        200,
                        "Renamed"
                    )

            file_path = self.safe_path(path)

            if method == "GET":
                return self.handle_get(file_path)

            elif method == "POST":
                return self.handle_post(file_path, body)

            elif method == "PUT":
                return self.handle_put(file_path, body)

            elif method == "DELETE":
                return self.handle_delete(file_path)

            return self.error_response(
                405,
                "Method Not Allowed"
            )

        except PermissionError:
            return self.error_response(403, "Forbidden")

        except Exception as e:
            print("[!] Server error:", e)

            return self.error_response(
                500,
                "Internal Server Error"
            )

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
    
    def parse_headers(self, header_text):
        headers = {}

        for line in header_text.splitlines()[1:]:
            if ":" in line:
                k, v = line.split(":", 1)
                headers[k.strip().lower()] = v.strip()

        return headers


    def is_authenticated(self, headers):
        cookie = headers.get("cookie", "")

        for part in cookie.split(";"):
            if "session=" in part:
                token = part.split("=", 1)[1].strip()

                if token in self.sessions:
                    return True

        return False


    def json_response(self, data, code=200, message="OK"):
        body = json.dumps(data).encode()

        return self.build_header(
            code,
            message,
            "application/json",
            len(body)
        ) + body


    def build_header(self, code, message, mime, length,
                    extra_headers=None):

        headers = [
            f"HTTP/1.1 {code} {message}",
            f"Content-Type: {mime}",
            f"Content-Length: {length}",
            "Connection: close"
        ]

        if extra_headers:
            headers.extend(extra_headers)

        headers.extend(["", ""])

        return "\r\n".join(headers).encode()


    def list_directory(self, folder):
        result = []

        for item in os.listdir(folder):
            full = os.path.join(folder, item)

            result.append({
                "name": item,
                "is_dir": os.path.isdir(full)
            })

        return result


import threading


class SimpleHTTPServer:
    def __init__(self, ipv4_host='127.0.0.1',
                 ipv6_host='::1',
                 port=8888):

        self.ipv4_host = ipv4_host
        self.ipv6_host = ipv6_host
        self.port = port

        self.processor = HTTPProcessor()

        self.ipv4_socket = None
        self.ipv6_socket = None

    def receive_request(self, client_socket):
        data = b""

        # Read headers
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
                content_length = int(
                    line.split(":", 1)[1].strip()
                )

        # Read remaining body
        while len(body) < content_length:
            chunk = client_socket.recv(4096)

            if not chunk:
                break

            body += chunk

        return headers + b"\r\n\r\n" + body

    def handle_client(self, client_socket, addr):
        try:
            print(f"[+] Connection from: {addr}")

            request = self.receive_request(client_socket)

            if request:
                response = self.processor.process_request(
                    request
                )

                client_socket.sendall(response)

        except Exception as e:
            print("[!] Client error:", e)

        finally:
            client_socket.close()

    def accept_loop(self, server_socket, label):
        while True:
            try:
                client_socket, addr = server_socket.accept()

                thread = threading.Thread(
                    target=self.handle_client,
                    args=(client_socket, addr),
                    daemon=True
                )

                thread.start()

            except Exception as e:
                print(f"[!] {label} accept error:", e)

    def start(self):
        # IPv4 socket
        self.ipv4_socket = socket.socket(
            socket.AF_INET,
            socket.SOCK_STREAM
        )

        self.ipv4_socket.setsockopt(
            socket.SOL_SOCKET,
            socket.SO_REUSEADDR,
            1
        )

        self.ipv4_socket.bind(
            (self.ipv4_host, self.port)
        )

        self.ipv4_socket.listen(5)

        # IPv6 socket
        self.ipv6_socket = socket.socket(
            socket.AF_INET6,
            socket.SOCK_STREAM
        )

        self.ipv6_socket.setsockopt(
            socket.SOL_SOCKET,
            socket.SO_REUSEADDR,
            1
        )

        self.ipv6_socket.bind(
            (self.ipv6_host, self.port)
        )

        self.ipv6_socket.listen(5)

        print(f"[*] IPv4 running on "
              f"http://{self.ipv4_host}:{self.port}")

        print(f"[*] IPv6 running on "
              f"http://[{self.ipv6_host}]:{self.port}")

        # Start accept loops
        ipv4_thread = threading.Thread(
            target=self.accept_loop,
            args=(self.ipv4_socket, "IPv4"),
            daemon=True
        )

        ipv6_thread = threading.Thread(
            target=self.accept_loop,
            args=(self.ipv6_socket, "IPv6"),
            daemon=True
        )

        ipv4_thread.start()
        ipv6_thread.start()

        try:
            while True:
                threading.Event().wait(1)

        except KeyboardInterrupt:
            print("\n[!] Server stopping...")

        finally:
            if self.ipv4_socket:
                self.ipv4_socket.close()

            if self.ipv6_socket:
                self.ipv6_socket.close()


if __name__ == "__main__":
    server = SimpleHTTPServer()
    server.start()