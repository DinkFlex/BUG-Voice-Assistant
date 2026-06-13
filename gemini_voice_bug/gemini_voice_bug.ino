#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SD.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "driver/i2s_std.h"

// ==================== CONFIGURATION ====================
const char* WIFI_SSID     = "SETUP-93D0";
const char* WIFI_PASSWORD = "better4755diner";
const char* SERVER_URL    = "http://192.168.0.96:5000/upload";

// Pins and recording specs
const uint32_t SAMPLE_RATE  = 16000;
const uint16_t SAMPLE_BITS  = 16;
const uint16_t CHANNELS     = 1;
const uint16_t RECORD_SECONDS = 5;

#define TRIGGER_BUTTON 0  // Boot button on ESP32-S3

// ==================== PINS (ESP32-S3 BUG) ====================
#define TFT_DC    14
#define TFT_CS    4
#define TFT_RST   5
#define TFT_BL    38
#define TFT_SCLK  18
#define TFT_MOSI  17

#define SD_CS     10
#define SD_MOSI   11
#define SD_CLK    12
#define SD_MISO   13

#define MIC_BCLK  7
#define MIC_WS    9
#define MIC_DATA  8

// ==================== INSTANCES ====================
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);
i2s_chan_handle_t rx_handle;

// ==================== WAV HEADER GENERATOR ====================
void writeWavHeader(File &file, uint32_t dataSize) {
  uint8_t h[44];

  uint32_t byteRate   = SAMPLE_RATE * CHANNELS * SAMPLE_BITS / 8;
  uint16_t blockAlign = CHANNELS * SAMPLE_BITS / 8;
  uint32_t chunkSize  = dataSize + 36;

  memcpy(h + 0,  "RIFF", 4);
  memcpy(h + 4,  &chunkSize, 4);
  memcpy(h + 8,  "WAVE", 4);
  memcpy(h + 12, "fmt ", 4);

  uint32_t subChunk1 = 16;
  uint16_t audioFmt  = 1;

  memcpy(h + 16, &subChunk1, 4);
  memcpy(h + 20, &audioFmt, 2);
  memcpy(h + 22, &CHANNELS, 2);
  memcpy(h + 24, &SAMPLE_RATE, 4);
  memcpy(h + 28, &byteRate, 4);
  memcpy(h + 32, &blockAlign, 2);
  memcpy(h + 34, &SAMPLE_BITS, 2);
  memcpy(h + 36, "data", 4);
  memcpy(h + 40, &dataSize, 4);

  file.seek(0);
  file.write(h, 44);
}

// ==================== DISPLAY HELPERS ====================
void showMsg(const char* line1, const char* line2 = "") {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(1);
  tft.setCursor(10, 15);
  tft.println(line1);
  
  if (strlen(line2) > 0) {
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(10, 40);
    tft.println(line2);
  }
}

// ==================== MICROPHONE INIT ====================
void initMic() {
  i2s_chan_config_t chan_cfg = {
    .id = I2S_NUM_0,
    .role = I2S_ROLE_MASTER,
    .dma_desc_num = 8,
    .dma_frame_num = 256,
    .auto_clear = true
  };

  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));

  i2s_std_config_t std_cfg = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
    .slot_cfg = {
      .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
      .slot_bit_width = I2S_SLOT_BIT_WIDTH_16BIT,
      .slot_mode      = I2S_SLOT_MODE_MONO,
      .slot_mask      = I2S_STD_SLOT_RIGHT,
      .ws_width       = 16,
      .ws_pol         = true,
      .bit_shift      = false
    },
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = (gpio_num_t)MIC_BCLK,
      .ws   = (gpio_num_t)MIC_WS,
      .dout = I2S_GPIO_UNUSED,
      .din  = (gpio_num_t)MIC_DATA
    }
  };

  ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));
  ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));
  Serial.println("Mic Initialized");
}

// ==================== WIFI HELPERS ====================
void connectWiFi() {
  showMsg("CONNECTING WIFI", WIFI_SSID);
  Serial.print("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    showMsg("WIFI CONNECTED", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\nWiFi Connection Failed.");
    showMsg("WIFI FAILED", "Check SSID/PSK");
  }
  delay(1500);
}

// ==================== AUDIO RECORD & UPLOAD ====================
void recordAndUpload() {
  showMsg("RECORDING...", "Speak now!");
  Serial.println("Recording started...");

  File f = SD.open("/bug_record.wav", FILE_WRITE);
  if (!f) {
    showMsg("SD FILE ERR", "Cannot open /rec.wav");
    Serial.println("[-] Failed to open WAV file for writing");
    return;
  }

  // Pre-allocate space for 44-byte WAV header
  uint8_t blankHeader[44] = {0};
  f.write(blankHeader, 44);

  int16_t buffer[256];
  size_t bytesRead;
  uint32_t totalBytes = 0;
  unsigned long start = millis();

  while (millis() - start < RECORD_SECONDS * 1000) {
    // Read raw 16-bit mono audio from I2S
    ESP_ERROR_CHECK(i2s_channel_read(rx_handle, buffer, sizeof(buffer), &bytesRead, portMAX_DELAY));

    // Boost volume/apply digital gain
    int samples = bytesRead / 2;
    for (int i = 0; i < samples; i++) {
      int32_t boosted = (int32_t)buffer[i] * 3; // digital gain factor (x3)
      if (boosted > 32767) boosted = 32767;
      if (boosted < -32768) boosted = -32768;
      
      // Simple gate to reduce ambient noise floors
      if (abs(boosted) < 100) boosted = 0;
      buffer[i] = (int16_t)boosted;
    }

    f.write((uint8_t*)buffer, bytesRead);
    totalBytes += bytesRead;
  }

  // Write finalized WAV header and close
  writeWavHeader(f, totalBytes);
  f.close();
  Serial.println("[+] Recording complete!");

  // --- UPLOAD TO LAPTOP SERVER ---
  if (WiFi.status() != WL_CONNECTED) {
    showMsg("WIFI DISCONNECTED", "Reconnecting...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      showMsg("UPLOAD ERR", "No connection");
      return;
    }
  }

  showMsg("UPLOADING...", "To Laptop");
  Serial.println("[*] Uploading file to server...");

  f = SD.open("/bug_record.wav", FILE_READ);
  if (!f) {
    showMsg("SD FILE ERR", "Cannot read file");
    return;
  }

  HTTPClient http;
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "audio/wav");

  // Stream WAV block-by-block directly from SD Card to HTTP connection (highly memory-efficient)
  int httpCode = http.sendRequest("POST", &f, f.size());
  
  if (httpCode > 0) {
    String response = http.getString();
    Serial.printf("[+] Server Response Code: %d\n", httpCode);
    Serial.println(response);
    if (httpCode == 200) {
      showMsg("SUCCESS!", "Check Laptop!");
    } else {
      showMsg("SERVER ERR", response.substring(0, 15).c_str());
    }
  } else {
    Serial.printf("[-] Upload failed, error: %s\n", http.errorToString(httpCode).c_str());
    showMsg("UPLOAD FAILED", http.errorToString(httpCode).c_str());
  }

  f.close();
  http.end();
  delay(2000);
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);

  // Trigger button setup
  pinMode(TRIGGER_BUTTON, INPUT_PULLUP);

  // TFT Backlight
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // Initialize TFT LCD
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_MINI160x80);
  tft.setRotation(1);
  tft.invertDisplay(true);

  showMsg("BUG READY", "Connecting Wi-Fi...");

  // Initialize SD Card (using standard secondary SPI host on ESP32-S3)
  SPIClass SPI_SD(HSPI);
  SPI_SD.begin(SD_CLK, SD_MISO, SD_MOSI, SD_CS);

  if (!SD.begin(SD_CS, SPI_SD)) {
    showMsg("SD CARD FAIL", "Insert MicroSD");
    Serial.println("[-] SD card initialization failed!");
    while (1) delay(10);
  }
  Serial.println("[+] SD Card ready");

  // Setup WiFi and I2S Microphone
  connectWiFi();
  initMic();

  showMsg("HERMZ ONLINE", "Hold BOOT to talk");
  Serial.println("[+] Push and hold BOOT button (GPIO 0) to record voice command!");
}

// ==================== LOOP ====================
void loop() {
  // Check if Boot button (GPIO 0) is held down
  if (digitalRead(TRIGGER_BUTTON) == LOW) {
    delay(50); // debounce
    if (digitalRead(TRIGGER_BUTTON) == LOW) {
      recordAndUpload();
      showMsg("HERMZ ONLINE", "Hold BOOT to talk");
    }
  }
  delay(10);
}
