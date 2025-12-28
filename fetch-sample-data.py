#!/usr/bin/env python3
"""
Script to download glTF Sample Assets from GitHub, unzip, and copy to a desired location.
"""

import urllib.request
import zipfile
import shutil
import os
import tempfile
import requests
from pathlib import Path


def download_and_extract_gltf_assets(destination: str, keep_zip: bool = False) -> None:
    url = "https://github.com/KhronosGroup/glTF-Sample-Assets/archive/refs/heads/main.zip"
    destination = os.path.abspath(destination)
    os.makedirs(destination, exist_ok=True)

    with tempfile.TemporaryDirectory() as temp_dir:
        zip_path = os.path.join(temp_dir, "main.zip")

        print("Downloading...")
        response = requests.get(url, stream=True)
        response.raise_for_status()

        with open(zip_path, 'wb') as f:
            for chunk in response.iter_content(chunk_size=8192):
                f.write(chunk)

        print("Extracting...")
        with zipfile.ZipFile(zip_path, 'r') as zip_ref:
            zip_ref.extractall(temp_dir)

        src = os.path.join(temp_dir, "glTF-Sample-Assets-main", "Models")
        dst = destination

        if os.path.exists(dst):
            shutil.rmtree(dst)
        shutil.copytree(src, dst)

        print(f"Done!\nAssets at: {dst}")


def main():
    try:
        destination = os.getcwd() + "\\assets\\sample-models"
        download_and_extract_gltf_assets(destination)
    except urllib.error.URLError as e:
        print(f"Error downloading file: {e}")
        exit(1)
    except zipfile.BadZipFile:
        print("Error: Downloaded file is not a valid zip file")
        exit(1)
    except PermissionError as e:
        print(f"Permission error: {e}")
        exit(1)
    except Exception as e:
        print(f"An error occurred: {e}")
        exit(1)


if __name__ == "__main__":
    main()
