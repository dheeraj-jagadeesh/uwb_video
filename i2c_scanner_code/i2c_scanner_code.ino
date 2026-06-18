#include <Wire.h>

// Define your I2C pins for the ESP32-S3
// NOTE: Change these numbers to match the physical pins you are using!
#define I2C_SDA 8
#define I2C_SCL 9

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10); // Wait for serial monitor to open
  }
  
  Serial.println("\n==================================");
  Serial.println("   ESP32-S3 I2C Scanner Active    ");
  Serial.println("==================================");
  
  // Initialize I2C with your specified pins
  bool success = Wire.begin(I2C_SDA, I2C_SCL);
  if (!success) {
    Serial.println("I2C initialization failed! Check your pin definitions.");
  }
}

void loop() {
  byte error, address;
  int nDevices = 0;

  Serial.println("Scanning for devices...");

  for (address = 1; address < 127; address++) {
    // The i2c_scanner uses the return value of
    // the Wire.endTransmission to see if a device
    // acknowledged the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("Device found at address 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.print(address, HEX);

      // Explicitly call out the BNO055 expected addresses
      if (address == 0x28 || address == 0x29) {
        Serial.print(" [ Potential BNO055 Sensor ]");
      }
      
      Serial.println();
      nDevices++;
    } 
    else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.println(address, HEX);
    }
  }

  if (nDevices == 0) {
    Serial.println("No I2C devices found. Check your wiring, pull-ups, and power!\n");
  } else {
    Serial.println("Scan done.\n");
  }

  delay(5000); // Wait 5 seconds before scanning again
}