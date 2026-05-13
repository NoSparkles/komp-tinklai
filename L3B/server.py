import socket
import os
import mimetypes

class HTTPProcessor:
    def __init__(self, static_dir='www'):
        self.static_dir = static_dir
        if not os.path.exists(self.static_dir):
            os.makedirs(self.static_dir)

    def process_request(self, raw_request):
        """Pagrindinis metodas: užklausa -> pilnas HTTP atsakymas (baitais)"""
        try:
            lines = raw_request.splitlines()
            if not lines:
                return b""
            
            request_line = lines[0].split()
            if len(request_line) < 2:
                return self._build_error_response(400, "Bad Request")

            method = request_line[0]

            if method != "GET":
                return self._build_error_response(405, "Method Not Allowed")
            
            path = request_line[1]

            clean_path = path.replace('\\', '/')

            path_parts = clean_path.lstrip('/').split('/')

            file_path = os.path.join(self.static_dir, *path_parts)

            if os.path.isdir(file_path):
                file_path = os.path.join(file_path, "index.html")
            
            if os.path.exists(file_path) and os.path.isfile(file_path):
                return self._build_file_response(file_path)
            else:
                return self._build_error_response(404, "Not Found")

        except Exception as e:
            print(f"Logikos klaida: {e}")
            return self._build_error_response(500, "Internal Server Error")

    def _build_file_response(self, file_path):
        """Suformuoja 200 OK atsakymą su failo turiniu."""
        with open(file_path, 'rb') as f:
            content = f.read()
        
        mime_type, _ = mimetypes.guess_type(file_path)
        mime_type = mime_type or 'application/octet-stream'
        
        header = f"HTTP/1.1 200 OK\r\n"
        header += f"Content-Type: {mime_type}\r\n"
        header += f"Content-Length: {len(content)}\r\n"
        header += "Connection: close\r\n"
        header += "\r\n"
        
        return header.encode('utf-8') + content

    def _build_error_response(self, code, message):
        """Suformuoja klaidos atsakymą."""
        body = f"<html><body><h1>{code} {message}</h1></body></html>"
        header = f"HTTP/1.1 {code} {message}\r\n"
        header += "Content-Type: text/html\r\n"
        header += f"Content-Length: {len(body)}\r\n"
        header += "Connection: close\r\n"
        header += "\r\n"
        return (header + body).encode('utf-8')

class SimpleHTTPServer:
    def __init__(self, host='127.0.0.1', port=8888):
        self.host = host
        self.port = port
        self.processor = HTTPProcessor()
        self.server_socket = None

    def setup_server(self):
        """Paruošia TCP soketą klausymuisi."""
        self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        
        self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        
        self.server_socket.bind((self.host, self.port))
        
        self.server_socket.listen(5)
        print(f"[*] Serveris paleistas: http://{self.host}:{self.port}")

    def start(self):
        """Pagrindinis ciklas, priimantis klientus."""
        self.setup_server()
        try:
            while True:
                client_socket, client_address = self.server_socket.accept()
                print(f"[+] Prisijungė: {client_address}")
                
                self.handle_client(client_socket)
        except KeyboardInterrupt:
            print("\n[!] Serveris stabdomas...")
        finally:
            if self.server_socket:
                self.server_socket.close()

    def handle_client(self, client_socket):
        try:
            data = client_socket.recv(4096).decode('utf-8', errors='ignore')
            if data:
                response = self.processor.process_request(data)
                client_socket.sendall(response)
        finally:
            client_socket.close()

if __name__ == "__main__":
    server = SimpleHTTPServer(port=8888)
    server.start()