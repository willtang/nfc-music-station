
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
  // PN532 nfc(pn532i2c);	
#endif

#include <NfcAdapter.h>

#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>

NfcAdapter nfc = NfcAdapter(pn532i2c);

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

void setup(void) {
  // delay(3000);
  Serial.begin(115200);
  while (!Serial && millis() < 5000);

  Serial.println("\n==========================================");
  Serial.println("--- PN532 NDEF Record Reader Initialized ---");
  Serial.println("==========================================");

  // Initialize the NDEF adapter layer (module connection)
  nfc.begin();
  Serial.println("[NFC] PN532 module initialized successfully.");

  // Connect to Wi-Fi after module connection
  initWiFi();
  
  Serial.println("\n==========================================");
  Serial.println("Place your NFC card close to the antenna...");
  Serial.println("==========================================");
}

void loop(void) {
  if (nfc.tagPresent()) {
    Serial.println("\n>>> Tag Detected! Reading NDEF Structure...");
    
    // Automatically reads the tag footprint and safely determines storage sizing
    NfcTag tag = nfc.read();
    
    Serial.println("------------------------------------------");
    Serial.print("Tag Type:     "); Serial.println(tag.getTagType());
    Serial.print("UID Value:    "); Serial.println(tag.getUidString());
    
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
    delay(3000); // Guard time to allow card removal
  }
  delay(100);
}
