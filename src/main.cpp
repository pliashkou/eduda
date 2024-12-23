#include <Arduino.h>
#include "NimBLEDevice.h"
#include <nlohmann/json.hpp>
#include "SPIFFS.h"
#include <Update.h>
// #include <unordered_map>
// #include <string>
// #include <mpark/variant.hpp>

using json = nlohmann::json;

#define VERSION "1.2"
#define DEBUG 0

#define NORMAL_MODE   0   // normal
#define UPDATE_MODE   1   // receiving firmware
#define OTA_MODE      2   // installing firmware

#define SERVICE_UUID        "19b10000-e8f2-537e-4f6c-d104768a1214"
#define SEND_CHARACTERISTIC_UUID "19b10001-e8f2-537e-4f6c-d104768a1214"
#define SETTINGS_CHARACTERISTIC_UUID "19b10002-e8f2-537e-4f6c-d104768a1214" // receive this
#define OTA_CHARACTERISTIC_UUID "19b10003-e8f2-537e-4f6c-d104768a1214" // OTA updates

bool deviceConnected = false;
bool oldDeviceConnected = false;
String oldState; // if remove this line - bluetoth will brake :/
uint8_t mac;
static int parts = 0, next = 0, cur = 0, MTU = 0;
static int MODE = NORMAL_MODE;
// long startEventTime;

int oldValues[9]  = {0};
int beforeState[9] = {0};
int newState[9] = {0};
int sensors[9] = {T0, T2, T3, T4, T5, T6, T7, T8, T9};

NimBLEServer* pServer = NULL;
NimBLECharacteristic* txCharacteristic = NULL;
NimBLECharacteristic* settingsCharacteristic = NULL; // receive this
NimBLECharacteristic* otaCharacteristic = NULL; // receive this
NimBLEService* pService = NULL;

// using VariantType = mpark::variant<int, float, char, std::string>;
// std::unordered_map<std::string, VariantType> settings = {
//   {"level", "10"}
// };
int LEVEL = 30;

static void writeBinary(fs::FS &fs, const char * path, uint8_t *dat, int len) {

  File file = fs.open(path, FILE_APPEND);

  if (!file) {
    Serial.println("- failed to open file for writing");
    return;
  }
  file.write(dat, len);
  file.close();
}

class MyServerCallbacks: public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
    deviceConnected = true;
  };

  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
    deviceConnected = false;
  }
};

class SettingsCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
      NimBLEAttValue jsonString = pCharacteristic->getValue();

      #if DEBUG
        Serial.print("Received new settings: ");
        Serial.println(jsonString); // Print the value
      #endif
      
      // #if DEBUG
      //   long resultTime = micros() - startEventTime;
      //   Serial.println(resultTime);
      //   Serial.printf("Delay: %ums \n", resultTime);
      // #endif

      json data = json::parse(jsonString);

      for (json::iterator it = data.begin(); it != data.end(); ++it) {
        Serial.printf("%s : %s \n", it.key().c_str(), it.value().dump());
      }

      // if (data['level']) {
        // Serial.println(data.at('level'));
      //   // LEVEL = std::atoi(data['level']);
      // }



      // for (auto& [key, value] : data.items()) {
      //   Serial.printf("%s : %d \n", key, value);
      // }
    
    }
} settingsCallbacks;

class OTACallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {

      std::string data = pCharacteristic->getValue();
      int len = data.length();

      #if DEBUG
        Serial.print(data.c_str()); // Print the value
      #endif

      if (len != 0) {
        uint8_t* buffer = new uint8_t[len];
        memcpy(buffer, data.c_str(), len);

        writeBinary(SPIFFS, "/update.bin", buffer, 512);

        delete[] buffer;
      } else {
        Serial.println("END!");
        MODE = OTA_MODE;
      };
    }
} otaCallbacks;

void updateState() {
  for (int i = 0; i < 9; i++) {
    int newValue = touchRead(sensors[i]);
    int diff = oldValues[i] - newValue;

    if (diff < -LEVEL) { // button is released
        newState[i] = 0;  // Assume 0 means released
    } else if (diff > LEVEL) { // button is pushed
        newState[i] = 1;  // Assume 1 means pressed
    } else {
        newState[i] = beforeState[i];  // Maintain previous state if there's no significant change
    }

    oldValues[i] = newValue; // Update oldValues for next comparison
  }
}

void performUpdate(Stream &updateSource, size_t updateSize) {
  char s1 = 0x0F;
  String result = String(s1);
  if (Update.begin(updateSize)) {
    size_t written = Update.writeStream(updateSource);
    if (written == updateSize) {
      Serial.println("Written : " + String(written) + " successfully");
    }
    else {
      Serial.println("Written only : " + String(written) + "/" + String(updateSize) + ". Retry?");
    }
    result += "Written : " + String(written) + "/" + String(updateSize) + " [" + String((written / updateSize) * 100) + "%] \n";
    if (Update.end()) {
      Serial.println("OTA done!");
      result += "OTA Done: ";
      if (Update.isFinished()) {
        Serial.println("Update successfully completed. Rebooting...");
        result += "Success!\n";
      }
      else {
        Serial.println("Update not finished? Something went wrong!");
        result += "Failed!\n";
      }

    }
    else {
      Serial.println("Error Occurred. Error #: " + String(Update.getError()));
      result += "Error #: " + String(Update.getError());
    }
  }
  else
  {
    Serial.println("Not enough space to begin OTA");
    result += "Not enough space for OTA";
  }
  if (deviceConnected) {
    delay(5000);
  }
}

void updateFromFS(fs::FS &fs) {
  File updateBin = fs.open("/update.bin");
  if (updateBin) {

    size_t updateSize = updateBin.size();

    if (updateSize > 0) {
      Serial.println("Trying to start update");
      performUpdate(updateBin, updateSize);
    }
    else {
      Serial.println("Error, file is empty");
    }

    updateBin.close();

    fs.remove("/update.bin");

    ESP.restart();
  }
  else {
    Serial.println("Could not load update.bin from spiffs root");
  }
}

void setup()  
{
  esp_efuse_mac_get_default(&mac);

  Serial.begin(9600);

  String NAME = "eDuda v" + String(VERSION) + " " + String(mac) ;

  NimBLEDevice::init(NAME.c_str());

  NimBLEServer *pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  NimBLEService *pService = pServer->createService(SERVICE_UUID);

  txCharacteristic = pService->createCharacteristic(SEND_CHARACTERISTIC_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  settingsCharacteristic = pService->createCharacteristic(SETTINGS_CHARACTERISTIC_UUID, NIMBLE_PROPERTY::WRITE);
  otaCharacteristic = pService->createCharacteristic(OTA_CHARACTERISTIC_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_NR);

  settingsCharacteristic->setCallbacks(&settingsCallbacks);
  otaCharacteristic->setCallbacks(&otaCallbacks);

  pService->start();

  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->enableScanResponse(false);
  pAdvertising->setMinInterval(0x0);
  //   pAdvertising->enableScanResponse(true);
  // pAdvertising->setMinInterval(0x06);  // functions that help with iPhone connections issue
  // pAdvertising->setMinInterval(0x12);
  pAdvertising->start();

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }

  if (SPIFFS.exists("/update.bin")) {
    SPIFFS.remove("/update.bin");
  }
}

void loop() {

  switch (MODE) {
    case NORMAL_MODE: 

      if (deviceConnected) {
        // startEventTime = millis();
        updateState();
        bool shouldSendNewState = false;
        String result = "";

        // Update oldState with the new state
        for (int i = 0; i < 9; i++) {
            if (beforeState[i] != newState[i]) {
                shouldSendNewState = true;
            }
            beforeState[i] = newState[i];
            result+= newState[i];
        }
        if (shouldSendNewState) {
          txCharacteristic->setValue(result);
          txCharacteristic->notify(); 

          #if DEBUG
            Serial.print("Sent: ");
            Serial.println(result);
          #endif
        }
      }
    break;
    case OTA_MODE: 
      Serial.println("OTA_MODE");
      updateFromFS(SPIFFS);
    break;
  };

  // disconnecting
  if (!deviceConnected && oldDeviceConnected) {
    Serial.println("Device disconnected.");
    delay(1000); // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // restart advertising
    Serial.println("Start advertising");
    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected) {
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
    Serial.println("Device Connected");

    #if DEBUG
      unsigned long totalBytes = SPIFFS.totalBytes();
      unsigned long usedBytes = SPIFFS.usedBytes();
      Serial.printf("Total bytes: %d, used bytes: %d \n" , totalBytes, usedBytes);
    #endif
  }
}
