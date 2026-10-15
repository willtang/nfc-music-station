# NFC Music Station 🎵📻

An ESP32-powered NFC music station using a PN532 NFC/RFID module to scan NFC tags or cards and trigger music playlists remotely.

Turn physical NFC cards, album art coasters, or cassette-style tags into an interactive music player that launches your favorite Spotify, Apple Music, or Home Assistant playlists with a simple tap.

---

## 🌟 Features

- **NDEF URI Record Parsing**: Automatically reads and decodes standardized NDEF URI records (`https://`, `http://`, etc.) stored on NFC cards and tags.
- **Multiple Bus Interfaces Supported**:
  - **I2C** (default, requires only 2 data pins).
  - **SPI** (pre-configured code block ready to toggle).
  - **HSU / High-Speed UART** (serial-based alternative).
- **Remote Playback Ready**: Easily expandable to trigger webhooks (Home Assistant, Node-RED), MQTT brokers, or cloud APIs (Spotify Web API).

---

## 🧰 Hardware Requirements

| Component | Description |
|---|---|
| **Microcontroller** | ESP32 development board (ESP32, ESP32-C3, ESP32-S3, etc.) |
| **NFC Reader** | PN532 NFC / RFID Module (Breakout v3) |
| **NFC Tags / Cards** | NTAG213, NTAG215, NTAG216, or Mifare Classic tags formatted with NDEF records |
| **Wiring** | 4-pin female-to-female jumper wires |

---

## 📦 Required Libraries

Install the following libraries using the **Arduino IDE Library Manager** (`Sketch` -> `Include Library` -> `Manage Libraries...`) or download them from GitHub:

1. **PN532** (by Seeed-Studio / Elechouse)
   - Source: [Seeed-Studio/PN532](https://github.com/Seeed-Studio/PN532)
   - Provides low-level communication drivers for I2C, SPI, and HSU.
2. **NDEF Library** (by Don Coleman)
   - Source: [don/NDEF](https://github.com/don/NDEF)
   - Enables parsing of NDEF messages, payloads, and record types.
3. **WiFiManager** (by tzapu)
   - Source: [tzapu/WiFiManager](https://github.com/tzapu/WiFiManager)
   - Provides runtime Wi-Fi credentials management with an automatic Access Point (AP) captive portal for network selection and password setup.

---

## 🚀 Getting Started

### 1. Program the ESP32

1. Open the Arduino IDE.
2. Ensure you have the **esp32 by Espressif Systems** board package installed.
3. Open [`nfc-music-station/nfc-music-station.ino`](nfc-music-station/nfc-music-station.ino).
4. Select your ESP32 board and the corresponding serial COM port under **Tools**.
5. Click **Upload**.
6. Open the **Serial Monitor** at **115200 baud**.

### 2. Wi-Fi Setup (First Boot / AP Mode)

On initial startup (or if the saved network is unavailable):
1. The ESP32 initializes the PN532 NFC module first.
2. If no saved Wi-Fi connection data is found, it automatically starts an Access Point named **`NFC-Music-Station-Setup`**.
3. Connect to **`NFC-Music-Station-Setup`** from your phone or PC.
4. The captive portal web page will open automatically (or navigate to `http://192.168.4.1`).
5. Select your local Wi-Fi SSID from the scanned list, enter the password, and click **Save**.
6. The ESP32 saves the credentials and connects automatically. On subsequent boots, it connects directly without creating an AP.

> [!TIP]
> To reset stored Wi-Fi credentials for reconfiguration or testing, uncomment `wm.resetSettings();` in `initWiFi()` inside [`nfc-music-station/nfc-music-station.ino`](nfc-music-station/nfc-music-station.ino).

### 3. Prepare Your NFC Tags

Use a smartphone app like **NFC Tools** (available on iOS and Android) to write NDEF records to your tags:

1. Open **NFC Tools** and select **Write**.
2. Tap **Add a record** -> **Custom URL / URI**.
3. Enter your target music URL or webhook endpoint:
   - **Spotify playlist link**: `https://open.spotify.com/playlist/...`
   - **Home Assistant Webhook**: `https://your-ha-instance/api/webhook/nfc_jukebox?media_id=playlist_1`
   - **Apple Music / YouTube Music / Sonos URI**
4. Tap **Write** and hold your NFC card against your phone's NFC antenna.

### 4. Test Tag Scanning

Bring your configured NFC card near the PN532 antenna. The Serial Monitor will output:

```text
==========================================
--- PN532 NDEF Record Reader Initialized ---
==========================================
[NFC] PN532 module initialized successfully.

------------------------------------------
--- Wi-Fi Initialization ---
------------------------------------------
[Wi-Fi] Checking saved Wi-Fi connection...
[Wi-Fi] Connected successfully!
[Wi-Fi] SSID:       Home-Network
[Wi-Fi] IP Address: 192.168.1.120
[Wi-Fi] Signal:     -54 dBm

==========================================
Place your NFC card close to the antenna...
==========================================

>>> Tag Detected! Reading NDEF Structure...
------------------------------------------
Tag Type:     NFC Forum Type 2
UID Value:    04 A1 B2 C3 D4 E5 F6
------------------------------------------
>>> DECODED URL: https://open.spotify.com/playlist/37i9dQZF1DXcBWIGoYBM5M
------------------------------------------
[Wi-Fi] Status: Online (IP: 192.168.1.120)
==========================================
```

---

## 🌐 Next Steps: Triggering Remote Playback

Once the URL is decoded, you can connect the ESP32 to Wi-Fi to send the URL to your home audio system:

### Option A: Home Assistant Webhook
Using the standard `WiFi.h` and `HTTPClient.h` libraries:
```cpp
HTTPClient http;
http.begin(fullUrl); // or an HA webhook passing fullUrl as payload
int httpCode = http.GET();
http.end();
```

### Option B: Spotify Connect / Media Controller
Send an MQTT message or an HTTP POST request to a local server (Raspberry Pi, Home Assistant, or Spotify Web API daemon) to start playback on a target smart speaker or receiver.

---

## 📁 Project Structure

```text
nfc-music-station/
├── .gitignore
├── LICENSE
├── README.md
└── nfc-music-station/
    └── nfc-music-station.ino   # Main Arduino sketch
```

---

## 📄 License

This project is open-source and licensed under the [MIT License](LICENSE).
