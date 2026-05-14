import requests
import os

class WebClient:
    def __init__(self, base_url='http://127.0.0.1:8888'):
        self.base_url = base_url
        self.client_dir = 'client_files'
        if not os.path.exists(self.client_dir):
            os.makedirs(self.client_dir)

    def get_file(self, filename):
        """Atsisiunčia failą iš serverio (GET)."""
        url = f"{self.base_url}/{filename}"
        try:
            response = requests.get(url, timeout=5)
            response.raise_for_status()
            
            output_path = os.path.join(self.client_dir, filename)
            os.makedirs(os.path.dirname(output_path), exist_ok=True)
            
            with open(output_path, 'wb') as f:
                f.write(response.content)
            
            print(f"[+] GET sėkmingas: '{filename}' išsaugotas lokaliai.")
            
            full_path = os.path.abspath(output_path)
            if os.name == 'nt':
                os.startfile(full_path)
            else:
                import subprocess
                subprocess.run(['xdg-open', full_path])
        except Exception as e:
            print(f"[-] GET klaida ({filename}): {e}")

    def post_file(self, local_filename):
        """Nuskaito failą iš client_files ir siunčia į serverį (POST)."""
        local_path = os.path.join(self.client_dir, local_filename)
        url = f"{self.base_url}/{local_filename}"
        
        try:
            if not os.path.exists(local_path):
                print(f"[-] POST klaida: Failas '{local_path}' neegzistuoja.")
                return

            # Nuskaitome failą iš client_files aplanko binary režimu
            with open(local_path, 'rb') as f:
                file_data = f.read()
            
            # Siunčiame failo turinį serveriui
            response = requests.post(url, data=file_data, timeout=5)
            response.raise_for_status()
            print(f"[+] POST sėkmingas: Failas '{local_filename}' nusiųstas į serverį.")
        except Exception as e:
            print(f"[-] POST klaida: {e}")

    def put_rename(self, old_filename, new_name):
        """Pervadinti failą serveryje (PUT)."""
        url = f"{self.base_url}/{old_filename}"
        try:
            response = requests.put(url, data=new_name, timeout=5)
            response.raise_for_status()
            print(f"[+] PUT sėkmingas: '{old_filename}' pervadintas į '{new_name}'.")
        except Exception as e:
            print(f"[-] PUT klaida: {e}")

    def delete_file(self, filename):
        """Ištrinti failą iš serverio (DELETE)."""
        url = f"{self.base_url}/{filename}"
        try:
            response = requests.delete(url, timeout=5)
            response.raise_for_status()
            print(f"[+] DELETE sėkmingas: Failas '{filename}' ištrintas.")
        except Exception as e:
            print(f"[-] DELETE klaida: {e}")

if __name__ == "__main__":
    client = WebClient()

    #client.post_file("camera.png")

    #client.get_file("camera.png")

    #client.put_rename("city.png", "city_backup.png")

    client.delete_file("city_backup.png")