/*
 * 3D Tracking System: WEARABLE TARGET TAGS (T0 - T5) - LIVE HEALTH DIAGNOSTICS FIXED
 * Updated: Embedded Storage Routine Writes Laptop Baseline Time Markers upon Initialization
 */
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <LittleFS.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>

#define SERIAL_LOG Serial
#define SERIAL_AT mySerial2
HardwareSerial SERIAL_AT(2);

// =========================================================================
#define TAG_INDEX 4  // <-- CHANGE TO 0, 1, 2, 3, 4, or 5 FOR EACH PHYSICAL TAG
// =========================================================================

#define RESET 16
#define IO_RXD2 18
#define IO_TXD2 17
#define I2C_SDA 8
#define I2C_SCL 9

const char* dataPath = "/telemetry.csv";
bool isLogging = false;
bool bnoActive = false;
unsigned long lastLogTime = 0;
const unsigned long logIntervalMs = 50; // 20 Hz Frame Rate

int distA0 = 0, distA1 = 0, distA2 = 0, distA3 = 0;
String uwbBuffer = "";

volatile bool triggerPull = false;
volatile bool triggerStatusReport = false;
uint8_t masterMacAddress[6];

// MATCHED STRUCT: Includes space variables for incoming timestamp processing
struct __attribute__((packed)) CommandPacket {
    char cmd[8];
    uint8_t tagId;
    char timestamp[28];
};

struct __attribute__((packed)) DataPacket {
    uint8_t tagId;
    uint8_t isEOF; 
    char data[240];
};

Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28, &Wire);

void onDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
    if (len == sizeof(CommandPacket)) {
        CommandPacket packet;
        memcpy(&packet, incomingData, sizeof(CommandPacket));
        
        if (packet.tagId == TAG_INDEX || packet.tagId == 255) {
            String command = String(packet.cmd);
            
            if (command.equalsIgnoreCase("START")) {
                if (!isLogging) {
                    File file = LittleFS.open(dataPath, FILE_WRITE);
                    if (file) {
                        // Write the laptop baseline anchor stamp to the top of the flash file layout
                        file.println("[START_TIME]:" + String(packet.timestamp));
                        file.println("relative_us,dist_A0,dist_A1,dist_A2,dist_A3,Euler_H,Euler_P,Euler_R,Quat_W,Quat_X,Quat_Y,Quat_Z,LinAcc_X,LinAcc_Y,LinAcc_Z,Acc_X,Acc_Y,Acc_Z,Gyro_X,Gyro_Y,Gyro_Z,Mag_X,Mag_Y,Mag_Z,Grav_X,Grav_Y,Grav_Z");
                        file.close();
                        isLogging = true;
                    }
                }
            } 
            else if (command.equalsIgnoreCase("STOP")) {
                isLogging = false;
            } 
            else if (command.equalsIgnoreCase("PULL")) {
                memcpy(masterMacAddress, recv_info->src_addr, 6); 
                triggerPull = true; 
            }
            else if (command.equalsIgnoreCase("STATUS")) {
                memcpy(masterMacAddress, recv_info->src_addr, 6); 
                triggerStatusReport = true;
            }
        }
    }
}

void parseUwbString(String entry) {
    if (entry.indexOf("range:(") != -1) {
        int rangeIdx = entry.indexOf("range:(");
        String rangeStr = entry.substring(rangeIdx);
        sscanf(rangeStr.c_str(), "range:(%d,%d,%d,%d", &distA0, &distA1, &distA2, &distA3);
    }
}

void sendStatusReport() {
    triggerStatusReport = false;
    bnoActive = bno.begin();
    if (bnoActive) {
        bno.setExtCrystalUse(true);
    }

    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(esp_now_peer_info_t));
    memcpy(peerInfo.peer_addr, masterMacAddress, 6);
    peerInfo.channel = 1;
    peerInfo.encrypt = false;
    if (!esp_now_is_peer_exist(masterMacAddress)) {
        esp_now_add_peer(&peerInfo);
    }

    DataPacket statusPacket;
    statusPacket.tagId = TAG_INDEX;
    statusPacket.isEOF = 2;
    
    if (bnoActive) {
        strcpy(statusPacket.data, "BNO055_ACTIVE");
    } else {
        strcpy(statusPacket.data, "BNO055_FAULT_OFFLINE");
    }
    esp_now_send(masterMacAddress, (uint8_t *)&statusPacket, sizeof(statusPacket));
}

void processFileTransmission() {
    triggerPull = false;
    bool currentLoggingState = isLogging;
    isLogging = false; 

    esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(esp_now_peer_info_t));
    memcpy(peerInfo.peer_addr, masterMacAddress, 6);
    peerInfo.channel = 1;
    peerInfo.encrypt = false;
    if (!esp_now_is_peer_exist(masterMacAddress)) {
        esp_now_add_peer(&peerInfo);
    }

    if (!LittleFS.exists(dataPath)) {
        DataPacket errPacket;
        errPacket.tagId = TAG_INDEX;
        errPacket.isEOF = 2;
        strcpy(errPacket.data, "ERROR: Log file missing.");
        esp_now_send(masterMacAddress, (uint8_t *)&errPacket, sizeof(errPacket));
        isLogging = currentLoggingState;
        return;
    }

    File file = LittleFS.open(dataPath, FILE_READ);
    if (!file) {
        DataPacket errPacket;
        errPacket.tagId = TAG_INDEX;
        errPacket.isEOF = 2;
        strcpy(errPacket.data, "ERROR: Failed to open file handle.");
        esp_now_send(masterMacAddress, (uint8_t *)&errPacket, sizeof(errPacket));
        isLogging = currentLoggingState;
        return;
    }

    DataPacket dataPacket;
    dataPacket.tagId = TAG_INDEX;
    dataPacket.isEOF = 0;

    while (file.available()) {
        memset(dataPacket.data, 0, sizeof(dataPacket.data));
        int bytesRead = file.readBytes(dataPacket.data, 239);
        if (bytesRead <= 0) break; 
        
        dataPacket.data[bytesRead] = '\0';
        esp_now_send(masterMacAddress, (uint8_t *)&dataPacket, sizeof(dataPacket));
        delay(10); 
    }
    file.close();

    DataPacket eofPacket;
    eofPacket.tagId = TAG_INDEX;
    eofPacket.isEOF = 1;
    memset(eofPacket.data, 0, sizeof(eofPacket.data));
    esp_now_send(masterMacAddress, (uint8_t *)&eofPacket, sizeof(eofPacket));

    isLogging = currentLoggingState;
}

void sendCmd(String cmd) {
    SERIAL_AT.println(cmd);
    delay(500);
}

void setup() {
    pinMode(RESET, OUTPUT);
    digitalWrite(RESET, HIGH);
    
    SERIAL_LOG.begin(921600);
    SERIAL_AT.begin(115200, SERIAL_8N1, IO_RXD2, IO_TXD2);
    delay(1000);

    Wire.begin(I2C_SDA, I2C_SCL);
    bnoActive = bno.begin();
    if (bnoActive) {
        bno.setExtCrystalUse(true);
    }

    LittleFS.begin(true);

    sendCmd("AT?");
    sendCmd("AT+RESTORE"); 
    delay(1500);
    sendCmd("AT+SETCFG=" + String(TAG_INDEX) + ",0,1,1"); 
    sendCmd("AT+SETCAP=64,10,1"); 
    sendCmd("AT+SETANT=16399");   
    sendCmd("AT+SETRPT=1");      
    sendCmd("AT+SAVE");          
    sendCmd("AT+RESTART");        
    delay(1000);

    WiFi.mode(WIFI_STA);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    esp_now_init();
    esp_now_register_recv_cb(onDataRecv);
}

void loop() {
    if (triggerStatusReport) {
        sendStatusReport();
    }
    if (triggerPull) {
        processFileTransmission();
    }

    while (SERIAL_AT.available() > 0) {
        char c = SERIAL_AT.read();
        if (c == '\n') {
            parseUwbString(uwbBuffer);
            uwbBuffer = "";
        } else if (c != '\r') {
            uwbBuffer += c;
        }
    }

    if (isLogging && (millis() - lastLogTime >= logIntervalMs)) {
        lastLogTime = millis();
        File file = LittleFS.open(dataPath, FILE_APPEND);
        if (file) {
            file.print(String(micros()) + ",");
            file.print(String(distA0) + "," + String(distA1) + "," + String(distA2) + "," + String(distA3) + ",");
            
            if (bnoActive) {
                imu::Vector<3> grav = bno.getVector(Adafruit_BNO055::VECTOR_GRAVITY);

                if (grav.x() == 0.0 && grav.y() == 0.0 && grav.z() == 0.0) {
                    bnoActive = false; 
                    file.print("0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0");
                } 
                else {
                    imu::Vector<3> euler  = bno.getVector(Adafruit_BNO055::VECTOR_EULER);
                    imu::Quaternion quat  = bno.getQuat();
                    imu::Vector<3> linacc = bno.getVector(Adafruit_BNO055::VECTOR_LINEARACCEL);
                    imu::Vector<3> acc    = bno.getVector(Adafruit_BNO055::VECTOR_ACCELEROMETER);
                    imu::Vector<3> gyro   = bno.getVector(Adafruit_BNO055::VECTOR_GYROSCOPE);
                    imu::Vector<3> mag    = bno.getVector(Adafruit_BNO055::VECTOR_MAGNETOMETER);

                    file.print(String(euler.x()) + "," + String(euler.y()) + "," + String(euler.z()) + ",");
                    file.print(String(quat.w()) + "," + String(quat.x()) + "," + String(quat.y()) + "," + String(quat.z()) + ",");
                    file.print(String(linacc.x()) + "," + String(linacc.y()) + "," + String(linacc.z()) + ",");
                    file.print(String(acc.x()) + "," + String(acc.y()) + "," + String(acc.z()) + ",");
                    file.print(String(gyro.x()) + "," + String(gyro.y()) + "," + String(gyro.z()) + ",");
                    file.print(String(mag.x()) + "," + String(mag.y()) + "," + String(mag.z()) + ",");
                    file.print(String(grav.x()) + "," + String(grav.y()) + "," + String(grav.z()));
                }
            } else {
                file.print("0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0");
            }
            file.println();
            file.close();
        }
    }
}