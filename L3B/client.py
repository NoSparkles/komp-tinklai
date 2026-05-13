import requests
import os

class WebClient:
    def __init__(self, base_url='http://127.0.0.1:8888'):
        self.base_url = base_url

    def download_and_open(self, filename):
        """Atsisiunčia failą ir jį atidaro."""
        url = f"{self.base_url}/{filename}"
        
        try:
            # Siunčiame GET užklausą
            response = requests.get(url, timeout=5)
            
            # Patikriname, ar serveris negrąžino klaidos (pvz., 404)
            response.raise_for_status()
            
            # Išsaugome failą
            output_path = f"client_files/{filename}"
            os.makedirs(os.path.dirname(output_path), exist_ok=True)
            
            with open(output_path, 'wb') as f:
                f.write(response.content)
            
            print(f"[+] Failas '{filename}' gautas sėkmingai.")
            
            full_path = os.path.abspath(output_path)

            if os.name == 'nt':  # Windows
                # Naudojame pilną kelią
                if os.path.exists(full_path):
                    os.startfile(full_path)
                else:
                    print(f"[-] Klaida: Failas nerastas pilnu keliu: {full_path}")
            else:  # Linux / MacOS
                import subprocess
                subprocess.run(['xdg-open', full_path])
                
        except requests.exceptions.HTTPError as err:
            print(f"[-] HTTP klaida: {err}")
        except Exception as e:
            print(f"[-] Klaida: {e}")

if __name__ == "__main__":
    client = WebClient()
    # Testuojame su tavo sukurtais failais
    client.download_and_open("index.html")
    client.download_and_open("city.png")