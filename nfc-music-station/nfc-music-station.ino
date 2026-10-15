
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

NfcAdapter nfc = NfcAdapter(pn532i2c);

void setup(void) {
  // delay(3000);
  Serial.begin(115200);
  while (!Serial && millis() < 5000);

  Serial.println("\n==========================================");
  Serial.println("--- PN532 NDEF Record Reader Initialized ---");
  Serial.println("==========================================");

  // Initialize the NDEF adapter layer
  nfc.begin();
  
  Serial.println("\nPlace your NFC card close to the antenna...");
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
