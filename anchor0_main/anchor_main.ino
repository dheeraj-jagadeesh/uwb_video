/*
 * 3D Tracking System: MASTER RECEIVER ANCHOR 0 (A0) - ESP-NOW BRIDGE
 * Updated: Expanded Command Structure to Relay Live Time Synchronization Vectors
 */
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SERIAL_LOG Serial
#define SERIAL_AT mySerial2
HardwareSerial SERIAL_AT(2);

#define UWB_INDEX 0
#define UWB_TAG_COUNT 64
#define RESET 16
#define IO_RXD2 18
#define IO_TXD2 17
#define I2C_SDA 39
#define I2C_SCL 38

Adafruit_SSD1306 display(128, 64, &Wire, -1);
String response = "";

// UPDATED STRUCT: Configured with space layout to route laptop baseline timestamps down the air
struct __attribute__((packed)) CommandPacket {
    char cmd[8];     
    uint8_t tagId;   
    char timestamp[28]; // Holds "YYYY_MM_DD_HH_MM_SS_ffffff"
};

struct __attribute__((packed)) DataPacket {
    uint8_t tagId;
    uint8_t isEOF;   
    char data[240];
};

uint8_t broadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void onDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
    if (len == sizeof(DataPacket)) {
        DataPacket packet;
        memcpy(&packet, incomingData, sizeof(DataPacket));
        
        if (packet.isEOF == 2) {
            SERIAL_LOG.println("[STATUS]:" + String(packet.tagId) + ":" + String(packet.data));
        } else if (packet.isEOF == 1) {
            SERIAL_LOG.println("[EOF]:" + String(packet.tagId));
        } else {
            SERIAL_LOG.print(packet.data);
        }
    }
}

void sendData(String command, const int timeout) {
    SERIAL_AT.println(command);
    long int time = millis();
    while ((time + timeout) > millis()) {
        while (SERIAL_AT.available()) { SERIAL_AT.read(); }
    }
}

void setup() {
    pinMode(RESET, OUTPUT);
    digitalWrite(RESET, HIGH);
    
    SERIAL_LOG.begin(921600);
    SERIAL_AT.begin(115200, SERIAL_8N1, IO_RXD2, IO_TXD2);

    Wire.begin(I2C_SDA, I2C_SCL);
    if(display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        display.clearDisplay();
        display.display();
    }

    sendData("AT?", 500);
    sendData("AT+RESTORE", 2000); 
    sendData("AT+SETCFG=" + String(UWB_INDEX) + ",1,1,1", 500); 
    sendData("AT+SETCAP=" + String(UWB_TAG_COUNT) + ",10,1", 500); 
    sendData("AT+SETANT=16399", 500); 
    sendData("AT+SETRPT=1", 500);    
    sendData("AT+SAVE", 500);        
    sendData("AT+RESTART", 1000);     

    WiFi.mode(WIFI_STA);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    
    if (esp_now_init() != ESP_OK) {
        SERIAL_LOG.println("Error initializing ESP-NOW");
        return;
    }
    
    esp_now_register_recv_cb(onDataRecv);

    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(esp_now_peer_info_t)); 
    memcpy(peerInfo.peer_addr, broadcastMac, 6);
    peerInfo.channel = 1;  
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
}

void loop() {
    if (SERIAL_LOG.available() > 0) {
        String input = SERIAL_LOG.readStringUntil('\n');
        input.trim();

        // Parse explicit 3-part syntax payload strings (CMD:TARGET:TIMESTAMP)
        int firstColon = input.indexOf(':');
        int secondColon = input.indexOf(':', firstColon + 1);
        
        if (firstColon != -1 && secondColon != -1) {
            String commandType = input.substring(0, firstColon);
            String targetStr = input.substring(firstColon + 1, secondColon);
            String timestampStr = input.substring(secondColon + 1);
            
            CommandPacket packet;
            memset(&packet, 0, sizeof(CommandPacket)); 
            
            commandType.toCharArray(packet.cmd, 8);
            timestampStr.toCharArray(packet.timestamp, 28);
            
            if (targetStr.equalsIgnoreCase("all")) {
                packet.tagId = 255;
            } else {
                packet.tagId = targetStr.toInt();
            }
            
            esp_now_send(broadcastMac, (uint8_t *) &packet, sizeof(packet));
        }
    }
    while (SERIAL_AT.available() > 0) { SERIAL_AT.read(); }
}