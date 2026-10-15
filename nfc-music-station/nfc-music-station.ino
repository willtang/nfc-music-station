
#if 0
  #include <SPI.h>
  #include <PN532_SPI.h>
  #include "PN532.h"

  PN532_SPI pn532spi(SPI, 10);
  PN532 nfc(pn532spi);
#elif 0
  #include <PN532_HSU.h>
  #include <PN532.h>

  PN532_HSU pn532hsu(Serial1);
  PN532 nfc(pn532hsu);
#else 
  #include <Wire.h>
  #include <PN532_I2C.h>
  #include <PN532.h>
  PN532_I2C pn532i2c(Wire);
  PN532 pn532(pn532i2c);
#endif

#include <NfcAdapter.h>

#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>

// Remote cliamp server configuration
// Remote cliamp is running in daemon mode, and the socket is redirected to TCP port 8787
const char* CLIAMP_SERVER_IP = "192.168.0.102";
const uint16_t CLIAMP_SERVER_PORT = 8787;

NfcAdapter nfc = NfcAdapter(pn532i2c);

// Track active NFC card UID to prevent duplicate processing
String savedCardId = "";
int cardMissCount = 0;

// Helper function to decode URL percent-encoded characters (e.g. %20 -> space, + -> space)
String urlDecode(const String &input) {
  String decoded = "";
  for (unsigned int i = 0; i < input.length(); i++) {
    char c = input.charAt(i);
    if (c == '%') {
      if (i + 2 < input.length()) {
        char a = input.charAt(i + 1);
        char b = input.charAt(i + 2);
        if (isxdigit(a) && isxdigit(b)) {
          int val = 0;
          if (a >= '0' && a <= '9') val += (a - '0') << 4;
          else if (a >= 'a' && a <= 'f') val += (a - 'a' + 10) << 4;
          else if (a >= 'A' && a <= 'F') val += (a - 'A' + 10) << 4;

          if (b >= '0' && b <= '9') val += (b - '0');
          else if (b >= 'a' && b <= 'f') val += (b - 'a' + 10);
          else if (b >= 'A' && b <= 'F') val += (b - 'A' + 10);

          decoded += (char)val;
          i += 2;
          continue;
        }
      }
      decoded += c;
    } else if (c == '+') {
      decoded += ' ';
    } else {
      decoded += c;
    }
  }
  return decoded;
}

// Helper function to escape special characters for JSON payloads
String escapeJsonString(const String &input) {
  String output = "";
  for (unsigned int i = 0; i < input.length(); i++) {
    char c = input.charAt(i);
    if (c == '"') {
      output += "\\\"";
    } else if (c == '\\') {
      output += "\\\\";
    } else if (c == '\b') {
      output += "\\b";
    } else if (c == '\f') {
      output += "\\f";
    } else if (c == '\n') {
      output += "\\n";
    } else if (c == '\r') {
      output += "\\r";
    } else if (c == '\t') {
      output += "\\t";
    } else {
      output += c;
    }
  }
  return output;
}

// Read a newline-delimited response line from cliamp
String readCliampResponse(WiFiClient &client, unsigned long timeoutMs = 2000) {
  String response = "";
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    while (client.available()) {
      char c = client.read();
      response += c;
      if (c == '\n') {
        return response;
      }
    }
    if (!client.connected() && !client.available()) {
      break;
    }
    delay(5);
  }
  return response;
}

// Call remote cliamp daemon socket redirected to TCP port 8787 to load and play a playlist
bool callCliampLoadAndPlay(const String &playlistId) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[cliamp] ERROR: Wi-Fi is offline. Cannot communicate with cliamp.");
    return false;
  }

  IPAddress serverIP;
  if (!serverIP.fromString(CLIAMP_SERVER_IP)) {
    Serial.print("[cliamp] ERROR: Invalid server IP format: ");
    Serial.println(CLIAMP_SERVER_IP);
    return false;
  }

  Serial.println("\n------------------------------------------");
  Serial.print("[cliamp] Connecting to remote cliamp server: ");
  Serial.print(CLIAMP_SERVER_IP);
  Serial.print(":");
  Serial.println(CLIAMP_SERVER_PORT);

  WiFiClient client;
  if (!client.connect(serverIP, CLIAMP_SERVER_PORT)) {
    Serial.println("[cliamp] ERROR: Failed to connect to remote cliamp server!");
    Serial.println("[cliamp] Please ensure cliamp is running in daemon mode and socket is redirected to TCP 8787.");
    Serial.println("------------------------------------------");
    return false;
  }

  Serial.println("[cliamp] Connected successfully.");

  // Construct V2 IPC operation.submit request for "load"
  String escapedId = escapeJsonString(playlistId);
  String loadRequest = "{\"version\":2,\"id\":\"load-1\",\"method\":\"provider.load\",\"wait\":true,\"params\":{\"provider\":\"navidrome\",\"playlist\":\"" + escapedId + "\"}}\n";

  Serial.print("[cliamp] Sending 'load' operation for playlist: ");
  Serial.println(playlistId);
  Serial.print("[cliamp] Request Payload: ");
  Serial.print(loadRequest);

  client.print(loadRequest);
  client.flush();

  String loadResponse = readCliampResponse(client, 2000);
  if (loadResponse.length() > 0) {
    Serial.print("[cliamp] Load Response: ");
    Serial.print(loadResponse);
  } else {
    Serial.println("[cliamp] No immediate response received for load operation.");
  }

  // Allow a short delay for cliamp daemon to queue the playlist tracks before starting playback
  delay(200);

  // If the server closed the connection after one request, reconnect for play
  if (!client.connected()) {
    Serial.println("[cliamp] Socket closed by server, reconnecting for 'play' operation...");
    client.stop();
    if (!client.connect(serverIP, CLIAMP_SERVER_PORT)) {
      Serial.println("[cliamp] ERROR: Reconnection for play operation failed!");
      Serial.println("------------------------------------------");
      return false;
    }
  }

  // Construct V2 IPC operation.submit request for "play"
  String playRequest = "{\"version\":2,\"id\":\"play-1\",\"method\":\"operation.submit\",\"operation\":\"play\",\"params\":{}}\n";

  Serial.println("[cliamp] Sending 'play' operation...");
  Serial.print("[cliamp] Request Payload: ");
  Serial.print(playRequest);

  client.print(playRequest);
  client.flush();

  String playResponse = readCliampResponse(client, 2000);
  if (playResponse.length() > 0) {
    Serial.print("[cliamp] Play Response: ");
    Serial.print(playResponse);
  } else {
    Serial.println("[cliamp] No immediate response received for play operation.");
  }

  client.stop();
  Serial.println("[cliamp] Finished request, connection closed.");
  Serial.println("------------------------------------------");
  return true;
}

// Callback executed when WiFiManager enters AP (Access Point) mode
void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("\n------------------------------------------");
  Serial.println("[Wi-Fi] No saved Wi-Fi credentials found (or connection failed).");
  Serial.println("[Wi-Fi] Starting Access Point (AP) Configuration Portal...");
  Serial.print("[Wi-Fi] AP SSID:        ");
  Serial.println(myWiFiManager->getConfigPortalSSID());
  Serial.print("[Wi-Fi] AP IP Address:  http://");
  Serial.println(WiFi.softAPIP());
  Serial.println("[Wi-Fi] Setup Instructions:");
  Serial.print("  1. Connect your phone or computer to Wi-Fi network: ");
  Serial.println(myWiFiManager->getConfigPortalSSID());
  Serial.print("  2. Open browser and go to: http://");
  Serial.println(WiFi.softAPIP());
  Serial.println("  3. Select your local Wi-Fi network, enter password, and save.");
  Serial.println("------------------------------------------");
}

// Connect to Wi-Fi or launch captive portal AP if credentials are not configured
void initWiFi() {
  Serial.println("\n------------------------------------------");
  Serial.println("--- Wi-Fi Initialization ---");
  Serial.println("------------------------------------------");

  WiFiManager wm;

  // Set web configuration portal branding
  wm.setTitle("NFC Music Station Setup");

  // Register callback when AP mode is triggered
  wm.setAPCallback(configModeCallback);

  // Optional: Set portal timeout in seconds (0 = wait indefinitely until user configures Wi-Fi)
  // wm.setConfigPortalTimeout(180);

  // Optional: Uncomment to clear stored Wi-Fi credentials for debugging
  // wm.resetSettings();

  Serial.println("[Wi-Fi] Checking saved Wi-Fi connection...");

  // autoConnect attempts to connect using saved credentials in NVS.
  // If no saved Wi-Fi data exists or connection fails, it starts an AP
  // named "NFC-Music-Station-Setup" and runs a captive portal web page.
  bool connected = wm.autoConnect("NFC-Music-Station-Setup");

  if (!connected) {
    Serial.println("\n[Wi-Fi] WARNING: Failed to connect to Wi-Fi or config portal timed out.");
  } else {
    Serial.println("\n[Wi-Fi] Connected successfully!");
    Serial.print("[Wi-Fi] SSID:       ");
    Serial.println(WiFi.SSID());
    Serial.print("[Wi-Fi] IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("[Wi-Fi] Signal:     ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  }
}

// Handle and parse decoded URL from NFC tag
void handleDecodedUrl(const String &url) {
  // Extract host from URL
  int schemeEnd = url.indexOf("://");
  int hostStart = (schemeEnd != -1) ? schemeEnd + 3 : 0;

  int hostEnd = url.length();
  int slashIdx = url.indexOf('/', hostStart);
  int colonIdx = url.indexOf(':', hostStart);
  int questionIdx = url.indexOf('?', hostStart);

  if (slashIdx != -1 && slashIdx < hostEnd) hostEnd = slashIdx;
  if (colonIdx != -1 && colonIdx < hostEnd) hostEnd = colonIdx;
  if (questionIdx != -1 && questionIdx < hostEnd) hostEnd = questionIdx;

  String host = url.substring(hostStart, hostEnd);
  host.toLowerCase();

  // Check if host is music.apple.com
  if (host != "music.apple.com") {
    return;
  }

  Serial.println("[Apple Music] Valid host identified: music.apple.com");

  // Locate and parse query string
  int queryIdx = url.indexOf('?');
  if (queryIdx == -1) {
    Serial.println("[Apple Music] No query parameters present in URL.");
    return;
  }

  String queryString = url.substring(queryIdx + 1);
  int hashIdx = queryString.indexOf('#');
  if (hashIdx != -1) {
    queryString = queryString.substring(0, hashIdx);
  }

  // Iterate over query parameters (e.g. key=val separated by &)
  bool nfcFound = false;
  String nfcValue = "";
  int startIdx = 0;
  while (startIdx < queryString.length()) {
    int ampIdx = queryString.indexOf('&', startIdx);
    String param;
    if (ampIdx == -1) {
      param = queryString.substring(startIdx);
      startIdx = queryString.length();
    } else {
      param = queryString.substring(startIdx, ampIdx);
      startIdx = ampIdx + 1;
    }

    if (param.length() == 0) continue;

    int eqIdx = param.indexOf('=');
    String key, value;
    if (eqIdx != -1) {
      key = param.substring(0, eqIdx);
      value = param.substring(eqIdx + 1);
    } else {
      key = param;
      value = "";
    }

    // Check if 'nfc' key is defined
    if (key == "nfc") {
      nfcFound = true;
      nfcValue = urlDecode(value);
      Serial.print("[Apple Music] 'nfc' key detected. Raw: '");
      Serial.print(value);
      Serial.print("', Decoded: '");
      Serial.print(nfcValue);
      Serial.println("'");
      break;
    }
  }

  if (nfcFound) {
    if (nfcValue.length() > 0) {
      // Call remote cliamp to load and play the playlist
      callCliampLoadAndPlay(nfcValue);
    } else {
      Serial.println("[Apple Music] 'nfc' key value is empty.");
    }
  } else {
    Serial.println("[Apple Music] 'nfc' key is not defined in query.");
  }
}

void setup(void) {
  // delay(3000);
  Serial.begin(115200);
  while (!Serial && millis() < 5000);

  Serial.println("\n==========================================");
  Serial.println("--- PN532 NDEF Record Reader Initialized ---");
  Serial.println("==========================================");

  // Initialize the NDEF adapter layer (module connection)
  nfc.begin();
  // Configure PN532 RF retries so presence checking is fast and non-blocking
  pn532.setPassiveActivationRetries(0x02);
  Serial.println("[NFC] PN532 module initialized successfully.");

  // Connect to Wi-Fi after module connection
  initWiFi();
  
  Serial.println("\n==========================================");
  Serial.println("Place your NFC card close to the antenna...");
  Serial.println("==========================================");
}

void loop(void) {
  // If a card is already active on the reader, monitor its presence without calling nfc.read().
  // Calling nfc.read() on every loop cycle causes redundant sector/block reads over RF;
  // when the user pulls the card away mid-read, the RF link is broken during a block transfer,
  // causing the driver to print "Read failed <page/block>" instead of detecting tag removal.
  if (savedCardId.length() > 0) {
    if (nfc.tagPresent(100)) {
      // Card is still present on the reader
      cardMissCount = 0;
      delay(100);
      return;
    } else {
      // Card was not detected in this polling cycle
      cardMissCount++;
      // Debounce: require 2 consecutive misses (~150-200ms) to confirm card has been removed
      if (cardMissCount >= 2) {
        Serial.print("\n[NFC] Card removed (UID: ");
        Serial.print(savedCardId);
        Serial.println("). Ready for next card.");
        savedCardId = "";
        cardMissCount = 0;
      }
      delay(50);
      return;
    }
  }

  // When no card is active, check for the arrival of a new tag
  cardMissCount = 0;
  if (nfc.tagPresent(100)) {
    // A new tag was detected! Read NDEF structure once.
    NfcTag tag = nfc.read();
    String cardId = tag.getUidString();

    // Verify a valid UID was obtained
    if (cardId.length() > 0) {
      savedCardId = cardId;

      Serial.println("\n>>> Tag Detected! Reading NDEF Structure...");
      Serial.println("------------------------------------------");
      Serial.print("Tag Type:     "); Serial.println(tag.getTagType());
      Serial.print("UID Value:    "); Serial.println(cardId);

      if (tag.hasNdefMessage()) {
        NdefMessage message = tag.getNdefMessage();

        for (int i = 0; i < message.getRecordCount(); i++) {
          NdefRecord record = message.getRecord(i);

          // Target Well-Known Type: URI (TNF 1, Type "U")
          if (record.getTnf() == 1 && record.getType() == "U") {
            int payloadLength = record.getPayloadLength();

            // Safe explicit heap allocation to protect the ESP32-C3 stack boundaries
            byte* payloadBuffer = (byte*)malloc(payloadLength);

            if (payloadBuffer != NULL) {
              record.getPayload(payloadBuffer);

              // Extract the NDEF URI prefix shorthand code
              byte prefixId = payloadBuffer[0];
              String fullUrl = "";

              if (prefixId == 0x01)      fullUrl = "http://www.";
              else if (prefixId == 0x02) fullUrl = "https://www.";
              else if (prefixId == 0x03) fullUrl = "http://";
              else if (prefixId == 0x04) fullUrl = "https://";

              // Append the remaining alphanumeric string payload bytes cleanly
              for (int j = 1; j < payloadLength; j++) {
                char c = (char)payloadBuffer[j];
                if (c >= 32 && c <= 126) { // Drop non-printable padding noise
                  fullUrl += c;
                }
              }

              Serial.println("------------------------------------------");
              Serial.print(">>> DECODED URL: "); 
              Serial.println(fullUrl);
              Serial.println("------------------------------------------");

              // Process the decoded URL
              handleDecodedUrl(fullUrl);

              if (WiFi.status() == WL_CONNECTED) {
                Serial.println("[Wi-Fi] Status: Online (IP: " + WiFi.localIP().toString() + ")");
              } else {
                Serial.println("[Wi-Fi] Status: Offline");
              }

              // Free the allocated memory allocation immediately to avoid future system crashes
              free(payloadBuffer);
            }
          } else {
            Serial.print("Skipping non-URI Record Type: "); 
            Serial.println(record.getType());
          }
        }
      } else {
        Serial.println("\n[Notice] Tag is discovered but it contains no active NDEF records.");
      }

      Serial.println("==========================================");
    }
  }

  delay(100);
}
