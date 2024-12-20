#include <Arduino.h>
#include "NimBLEDevice.h"

#define VERSION "1.1"
#define DEBUG 0

#define SERVICE_UUID        "19b10000-e8f2-537e-4f6c-d104768a1214"
#define SENSOR_CHARACTERISTIC_UUID "19b10001-e8f2-537e-4f6c-d104768a1214"
#define RX_CHARACTERISTIC_UUID "19b10002-e8f2-537e-4f6c-d104768a1214" // receive this

bool deviceConnected = false;
bool oldDeviceConnected = false;
String oldState; // if remove this line - bluetoth will brake :/
uint8_t mac;
// long startEventTime;

int oldValues[9]  = {0};
int beforeState[9] = {0};
int newState[9] = {0};
int sensors[9] = {T0, T2, T3, T4, T5, T6, T7, T8, T9};

NimBLEServer* pServer = NULL;
NimBLECharacteristic* txCharacteristic = NULL;
NimBLECharacteristic* rxCharacteristic = NULL; // receive this
NimBLEService* pService = NULL;

class MyServerCallbacks: public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) {
    deviceConnected = true;
  };

  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) {
    deviceConnected = false;
  }
};

class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
      NimBLEAttValue val = pCharacteristic->getValue();
      
      // #if DEBUG
      //   long resultTime = micros() - startEventTime;
      //   Serial.println(resultTime);
      //   Serial.printf("Delay: %ums \n", resultTime);
      // #endif

      #if DEBUG
        Serial.print("Received: ");
        Serial.println(val.c_str()); // Print the value
      #endif
    }
} chrCallbacks;

void updateState() {
  int LEVEL = 10;

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

void setup()  
{
  esp_efuse_mac_get_default(&mac);

  Serial.begin(9600);

  String NAME = "eDuda v" + String(VERSION) + " " + String(mac) ;

  NimBLEDevice::init(NAME.c_str());

  NimBLEServer *pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  NimBLEService *pService = pServer->createService(SERVICE_UUID);

  txCharacteristic = pService->createCharacteristic(SENSOR_CHARACTERISTIC_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  rxCharacteristic = pService->createCharacteristic(RX_CHARACTERISTIC_UUID, NIMBLE_PROPERTY::WRITE);

  rxCharacteristic->setCallbacks(&chrCallbacks);

  pService->start();

  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->enableScanResponse(false);
  pAdvertising->setMinInterval(0x0);
  pAdvertising->start();
}

void loop() {
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
      #endif;
    }
  }

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
  }
}
