import os
import base64
import requests
import subprocess
from flask import Flask, request, jsonify

app = Flask(__name__)

# Config
PORT = 5000
GEMINI_MODEL = "gemini-1.5-flash"  # Supports high-fidelity native audio processing
TEMP_WAV_PATH = "uploaded.wav"
RESPONSE_MP3_PATH = "response.mp3"

def get_api_key():
    # Try environment variable first, then fallback to telegram-bot's .env if exists
    api_key = os.environ.get("GEMINI_API_KEY")
    if api_key:
        return api_key
    
    # Try reading from telegram-bot's .env if present
    env_path = "/home/claw/telegram-bot/.env"
    if os.path.exists(env_path):
        try:
            with open(env_path, "r") as f:
                for line in f:
                    if "GEMINI_API_KEY" in line:
                        parts = line.strip().split("=", 1)
                        if len(parts) == 2:
                            return parts[1].strip().replace('"', '').replace("'", '')
        except Exception:
            pass
            
    return None

def speak_text(text):
    print(f"\nHermz says: {text}\n")
    try:
        # Check if gTTS is installed
        import gtts
        tts = gtts.gTTS(text=text, lang='en')
        tts.save(RESPONSE_MP3_PATH)
        
        # Play the audio using PulseAudio (paplay), Alsa (aplay), or Sox (play)
        if os.path.exists("/bin/paplay"):
            subprocess.run(["paplay", RESPONSE_MP3_PATH], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        elif os.path.exists("/bin/play"):
            subprocess.run(["play", RESPONSE_MP3_PATH], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        else:
            print("System audio player not found. Install 'pulseaudio-utils' or 'sox'.")
    except ImportError:
        # Fallback to espeak if gTTS is not installed
        if os.path.exists("/bin/espeak") or os.path.exists("/usr/bin/espeak"):
            subprocess.run(["espeak", text], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        else:
            print("No TTS engine installed (gTTS or espeak). Outputting text-only.")

@app.route("/upload", methods=["POST"])
def upload_audio():
    print("[+] Audio uploaded from BUG device!")
    
    # Get audio bytes from request body
    audio_data = request.data
    if not audio_data:
        return jsonify({"error": "No audio data received"}), 400
        
    # Save the file for debugging / inspection
    with open(TEMP_WAV_PATH, "wb") as f:
        f.write(audio_data)
    print(f"[+] Saved {len(audio_data)} bytes to {TEMP_WAV_PATH}")
    
    # Get Gemini API key
    api_key = get_api_key()
    if not api_key:
        err_msg = "Gemini API Key is missing. Please set GEMINI_API_KEY environment variable."
        print(f"[-] {err_msg}")
        speak_text("API key is missing.")
        return jsonify({"error": err_msg}), 400

    # Base64 encode the audio data for the Gemini REST API
    audio_b64 = base64.b64encode(audio_data).decode("utf-8")
    
    # Prepare Gemini REST payload
    url = f"https://generativelanguage.googleapis.com/v1beta/models/{GEMINI_MODEL}:generateContent?key={api_key}"
    headers = {"Content-Type": "application/json"}
    payload = {
        "contents": [{
            "parts": [
                {
                    "inlineData": {
                        "mimeType": "audio/wav",
                        "data": audio_b64
                    }
                },
                {
                    "text": "You are Hermz, a friendly and expert hacking companion built for Iden. Answer the voice instruction concisely (in 2-3 sentences max) like a tech buddy."
                }
            ]
        }]
    }

    try:
        response = requests.post(url, headers=headers, json=payload, timeout=15)
        response.raise_for_status()
        resp_json = response.json()
        
        # Extract text response from Gemini JSON structure
        text_response = resp_json["candidates"][0]["content"]["parts"][0]["text"]
        
        # Speak the response out loud
        speak_text(text_response)
        
        return jsonify({"success": True, "response": text_response})
    except Exception as e:
        err_msg = f"Failed to query Gemini API: {str(e)}"
        print(f"[-] {err_msg}")
        speak_text("I encountered an error querying the model.")
        return jsonify({"error": err_msg}), 500

if __name__ == "__main__":
    api_key = get_api_key()
    if not api_key:
        print("[!] Warning: GEMINI_API_KEY not found in environment or bot .env!")
        print("    Please run: export GEMINI_API_KEY='your_key'")
    else:
        print("[+] Gemini API Key found and configured.")
        
    print(f"[*] Starting local AI Voice Assistant Server on port {PORT}...")
    app.run(host="0.0.0.0", port=PORT)
