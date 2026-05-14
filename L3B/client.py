import requests
import os


class WebClient:
    def __init__(self, base_url='http://127.0.0.1:8888'):
        self.base_url = base_url
        self.client_dir = 'client_files'

        os.makedirs(self.client_dir, exist_ok=True)

    def get_file(self, filename):
        url = f"{self.base_url}/{filename}"

        try:
            response = requests.get(url, timeout=10)

            response.raise_for_status()

            save_path = os.path.join(
                self.client_dir,
                filename
            )

            os.makedirs(
                os.path.dirname(save_path),
                exist_ok=True
            )

            with open(save_path, 'wb') as f:
                f.write(response.content)

            print(f"[+] Downloaded: {filename}")

        except Exception as e:
            print(f"[-] GET error: {e}")

    def post_file(self, filename):
        local_path = os.path.join(
            self.client_dir,
            filename
        )

        if not os.path.exists(local_path):
            print("[-] File not found")
            return

        url = f"{self.base_url}/{filename}"

        try:
            with open(local_path, 'rb') as f:
                response = requests.post(
                    url,
                    data=f,
                    headers={
                        "Content-Type":
                        "application/octet-stream"
                    },
                    timeout=30
                )

            response.raise_for_status()

            print(f"[+] Uploaded: {filename}")

        except Exception as e:
            print(f"[-] POST error: {e}")

    def put_rename(self, old_name, new_name):
        url = f"{self.base_url}/{old_name}"

        try:
            response = requests.put(
                url,
                data=new_name.encode(),
                timeout=10
            )

            response.raise_for_status()

            print(f"[+] Renamed to: {new_name}")

        except Exception as e:
            print(f"[-] PUT error: {e}")

    def delete_file(self, filename):
        url = f"{self.base_url}/{filename}"

        try:
            response = requests.delete(
                url,
                timeout=10
            )

            response.raise_for_status()

            print(f"[+] Deleted: {filename}")

        except Exception as e:
            print(f"[-] DELETE error: {e}")


if __name__ == "__main__":
    client = WebClient("http://[::1]:8888")

    # Upload
    #client.post_file("city.png")

    # Download
    #client.get_file("city.png")

    # Rename
    #client.put_rename("city.png", "backup.png")

    # Delete
    client.delete_file("backup.png")