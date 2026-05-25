"""
Ollama Service Manager
======================
Handles installing, starting, stopping, and interacting with Ollama.
"""
import subprocess
import time
import urllib.request
import os
import sys
import shutil

OLLAMA_INSTALLER_URL = "https://ollama.com/download/OllamaSetup.exe"


def is_installed():
    return shutil.which("ollama") is not None


def is_running(base_url="http://localhost:11434"):
    try:
        import urllib.request
        req = urllib.request.Request(f"{base_url}/api/tags", method="GET")
        urllib.request.urlopen(req, timeout=3)
        return True
    except Exception:
        return False


def install(progress_callback=None):
    if is_installed():
        if progress_callback:
            progress_callback("Ollama already installed.")
        return True
    if progress_callback:
        progress_callback("Downloading Ollama installer...")
    installer_path = os.path.join(os.environ.get("TEMP", "."), "OllamaSetup.exe")
    try:
        urllib.request.urlretrieve(OLLAMA_INSTALLER_URL, installer_path)
    except Exception as e:
        if progress_callback:
            progress_callback(f"Download failed: {e}")
        return False
    if progress_callback:
        progress_callback("Running Ollama installer (GUI window will open)...")
    subprocess.run([installer_path], shell=True)
    time.sleep(2)
    return is_installed()


def start_service(port=11434, progress_callback=None):
    if is_running(f"http://localhost:{port}"):
        if progress_callback:
            progress_callback("Ollama service already running.")
        return True
    env = os.environ.copy()
    env["OLLAMA_HOST"] = f"127.0.0.1:{port}"
    try:
        subprocess.Popen(
            ["ollama", "serve"],
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            creationflags=subprocess.CREATE_NO_WINDOW
        )
        if progress_callback:
            progress_callback(f"Starting Ollama on port {port}...")
        for _ in range(15):
            time.sleep(1)
            if is_running(f"http://localhost:{port}"):
                if progress_callback:
                    progress_callback("Ollama service started.")
                return True
        if progress_callback:
            progress_callback("Ollama service start timed out.")
        return False
    except Exception as e:
        if progress_callback:
            progress_callback(f"Failed to start Ollama: {e}")
        return False


def stop_service(progress_callback=None):
    try:
        subprocess.run(["taskkill", "/F", "/IM", "ollama.exe"],
                       capture_output=True, timeout=10)
        time.sleep(1)
        if progress_callback:
            progress_callback("Ollama service stopped.")
    except Exception:
        pass


def pull_model(model_name, progress_callback=None):
    if progress_callback:
        progress_callback(f"Pulling model: {model_name}...")
    try:
        proc = subprocess.Popen(
            ["ollama", "pull", model_name],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True, creationflags=subprocess.CREATE_NO_WINDOW
        )
        for line in proc.stdout:
            line = line.strip()
            if line and progress_callback:
                progress_callback(f"  {line}")
        proc.wait()
        return proc.returncode == 0
    except Exception as e:
        if progress_callback:
            progress_callback(f"Pull failed: {e}")
        return False


def list_models():
    try:
        result = subprocess.run(
            ["ollama", "list"],
            capture_output=True, text=True, timeout=10,
            creationflags=subprocess.CREATE_NO_WINDOW
        )
        return result.stdout.strip()
    except Exception:
        return ""


def is_model_available(model_name):
    return model_name in list_models()
