/*
 * 3D Tracking System: PASSIVE BEACON ANCHORS (A1 - A3)
 * Use this exact code for your remaining 3 anchors.
 * Simply change the index below for each physical board.
 */
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =========================================================================
#define ANCHOR_INDEX 3  // <-- CHANGE TO 1, 2, or 3 FOR EACH PHYSICAL ANCHOR
// =========================================================================

#define SERIAL_LOG Serial
#define SERIAL_AT mySerial2
HardwareSerial SERIAL_AT(2);

#define UWB_TAG_COUNT 64
#define RESET 16
#define IO_RXD2 18
#define IO_TXD2 17
#define I2C_SDA 39
#define I2C_SCL 38

Adafruit_SSD1306 display(128, 64, &Wire, -1);

void sendUwbCommand(String command, const int timeout) {
    SERIAL_AT.println(command);
    long int time = millis();
    while ((time + timeout) > millis()) {
        while (SERIAL_AT.available()) { 
            SERIAL_AT.read(); // Flush incoming response out of buffer
        }
    }
}

void setup() {
    pinMode(RESET, OUTPUT);
    digitalWrite(RESET, HIGH);
    
    // Serial debugging line set to your high-speed 921600 system standard
    SERIAL_LOG.begin(921600);
    // UWB Module connection line
    SERIAL_AT.begin(115200, SERIAL_8N1, IO_RXD2, IO_TXD2);

    // Turn off internal Wi-Fi/Bluetooth radios to maximize battery runtimes on tripods
    WiFi.mode(WIFI_OFF);

    // Initialize display layout
    Wire.begin(I2C_SDA, I2C_SCL);
    if(display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(10, 15);
        display.printf("ANCHOR A%d", ANCHOR_INDEX);
        display.setTextSize(1);
        display.setCursor(10, 45);
        display.print("UWB Beacon Online");
        display.display();
    }

    SERIAL_LOG.printf("[SYSTEM]: Initializing Passive Anchor Hardware ID: A%d...\n", ANCHOR_INDEX);

    // Configure Native UWB Transceiver Hardware Subsystem
    sendUwbCommand("AT?", 500);
    sendUwbCommand("AT+RESTORE", 2000); 
    
    // Set role configuration parameter to 1 (Anchor Role mode)
    sendUwbCommand("AT+SETCFG=" + String(ANCHOR_INDEX) + ",1,1,1", 500); 
    
    sendUwbCommand("AT+SETCAP=" + String(UWB_TAG_COUNT) + ",10,1", 500); 
    sendUwbCommand("AT+SETANT=16399", 500); 
    sendUwbCommand("AT+SETRPT=1", 500);    
    sendUwbCommand("AT+SAVE", 500);        
    sendUwbCommand("AT+RESTART", 1000);     

    SERIAL_LOG.printf("[SYSTEM]: Anchor A%d configuration complete. Standing by for Tag ranging pings.\n", ANCHOR_INDEX);
}

void loop() {
    // Keep the local anchor ring buffer drained continuously to prevent memory leaks
    while (SERIAL_AT.available() > 0) { 
        SERIAL_AT.read(); 
    }
    delay(100); // Low duty-cycle idle state loop
}