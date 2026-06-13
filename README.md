# BUG Local AI Voice Assistant Gateway

This repository contains the custom firmware and laptop server to convert the **BUG (ESP32-S3 Hacking USB Tool)** into a physical AI voice recorder and local assistant using **Google Gemini 3.5 Flash** as the brain.

## 🚀 How It Works
1. **The BUG (ESP32-S3):** Hold down the BOOT button (GPIO 0). The BUG records 5 seconds of high-gain 16kHz audio from its onboard digital I2S microphone, saves it to the MicroSD card as `/bug_record.wav`, and streams it over Wi-Fi to your laptop server.
2. **The Laptop Server:** A local Flask server receives the `.wav` file, Base64-encodes it, and sends it directly to Google's Gemini 3.5 Flash API as a multimodal payload. 
3. **The Voice Response:** The server receives the text response from Gemini, converts it to speech locally using Google TTS (`gTTS`), and plays it out loud through your laptop's speakers (`paplay`).

---

## 📂 Repository Contents
* `gemini_voice_bug.ino` - Custom Arduino C++ firmware for the ESP32-S3 BUG device.
* `gemini_voice_server.py` - Flask gateway server running on the laptop to bridge the BUG device to Gemini.
* `original_firmware/` - Original backer credit scroll CircuitPython firmware that was preloaded on the BUG.

---

## 🛠️ Setup Instructions

### 1. Laptop Server Setup
Ensure Python 3 and system audio utilities are installed:
```bash
# Install Python dependencies
pip install Flask requests gTTS

# Set your Gemini API Key
export GEMINI_API_KEY="your-api-key-here"

# Run the server
python3 gemini_voice_server.py
```

### 2. BUG Firmware Setup
Open `gemini_voice_bug.ino` in Arduino IDE:
1. Ensure the following libraries are installed:
   * **Adafruit GFX Library**
   * **Adafruit ST7735 and ST7789 Library**
   * **SD** (ESP32 standard library)
2. Under **Tools > Board**, select **ESP32S3 Dev Module** (or `Waveshare ESP32-S3-Zero`).
3. Compile and flash to your BUG device.
