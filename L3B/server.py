import socket
import os
import mimetypes

class HTTPProcessor:
    def __init__(self, static_dir='www'):
        self.static_dir = static_dir
        if not os.path.exists(self.static_dir):
            os.makedirs(self.static_dir)

    def process_request(self, raw_request):
        try:
            parts = raw_request.split(b'\r\n\r\n', 1)
            header_part = parts[0].decode('utf-8', errors='ignore')
            body_part = parts[1] if len(parts) > 1 else b""

            lines = header_part.splitlines()
            if not lines:
                return b""
            
            request_line = lines[0].split()
            if len(request_line) < 2:
                return self._build_error_response(400, "Bad Request")

            method = request_line[0]
            path = request_line[1]
            
            clean_path = path.replace('\\', '/')
            path_parts = clean_path.lstrip('/').split('/')
            file_path = os.path.join(self.static_dir, *path_parts)

            if method == "GET":
                if os.path.isdir(file_path):
                    file_path = os.path.join(file_path, "index.html")
                
                if os.path.exists(file_path) and os.path.isfile(file_path):
                    return self._build_file_response(file_path)
                return self._build_error_response(404, "Not Found")

            elif method == "POST":
                with open(file_path, 'wb') as f:
                    f.write(body_part)
                print(f"[*] Išsaugotas failas: {file_path} ({len(body_part)} bytes)")
                return self._build_success_response(201, "Created")

            elif method == "PUT":
                new_name = body_part.decode('utf-8', errors='ignore').strip()
                if not new_name:
                    return self._build_error_response(400, "Missing new name in body")
                
                new_file_path = os.path.join(os.path.dirname(file_path), new_name)
                
                if os.path.exists(file_path):
                    os.rename(file_path, new_file_path)
                    print(f"[*] Pervadinta: {file_path} -> {new_file_path}")
                    return self._build_success_response(200, f"Renamed to {new_name}")
                return self._build_error_response(404, "Original file not found")

            elif method == "DELETE":
                if os.path.exists(file_path) and os.path.isfile(file_path):
                    os.remove(file_path)
                    print(f"[*] Ištrinta: {file_path}")
                    return self._build_success_response(200, "Deleted")
                return self._build_error_response(404, "File not found")

            else:
                return self._build_error_response(405, "Method Not Allowed")

        except Exception as e:
            print(f"[!] Logikos klaida: {e}")
            return self._build_error_response(500, "Internal Server Error")

    def _build_file_response(self, file_path):
        with open(file_path, 'rb') as f:
            content = f.read()
        mime_type, _ = mimetypes.guess_type(file_path)
        mime_type = mime_type or 'application/octet-stream'
        return self._create_header(200, "OK", mime_type, len(content)) + content

    def _build_success_response(self, code, message):
        body = f"<html><body><h1>{code} {message}</h1></body></html>".encode('utf-8')
        return self._create_header(code, message, "text/html", len(body)) + body

    def _build_error_response(self, code, message):
        body = f"<html><body><h1>{code} {message}</h1></body></html>".encode('utf-8')
        return self._create_header(code, message, "text/html", len(body)) + body

    def _create_header(self, code, message, mime_type, length):
        header = f"HTTP/1.1 {code} {message}\r\n"
        header += f"Content-Type: {mime_type}\r\n"
        header += f"Content-Length: {length}\r\n"
        header += "Connection: close\r\n\r\n"
        return header.encode('utf-8')

class SimpleHTTPServer:
    def __init__(self, host='127.0.0.1', port=8888):
        self.host = host
        self.port = port
        self.processor = HTTPProcessor()
        self.server_socket = None

    def start(self):
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server_socket.bind((self.host, self.port))
        self.server_socket.listen(5)
        print(f"[*] Serveris paruoštas: http://{self.host}:{self.port}")

        try:
            while True:
                client_socket, addr = self.server_socket.accept()
                client_socket.settimeout(2.0)
                
                request_data = b""
                try:
                    while True:
                        chunk = client_socket.recv(8192)
                        if not chunk:
                            break
                        request_data += chunk
                        if len(chunk) < 8192: 
                            break
                except socket.timeout:
                    pass
                
                if request_data:
                    response = self.processor.process_request(request_data)
                    client_socket.sendall(response)
                
                client_socket.close()
        except KeyboardInterrupt:
            print("\n[!] Serveris stabdomas...")
        finally:
            if self.server_socket:
                self.server_socket.close()

if __name__ == "__main__":
    server = SimpleHTTPServer(port=8888)
    server.start()