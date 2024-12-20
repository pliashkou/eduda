#include <Arduino.h>
#include "NimBLEDevice.h"

#define VERSION "1.0"
// define DEBUG 1

#define SERVICE_UUID        "19b10000-e8f2-537e-4f6c-d104768a1214"
#define SENSOR_CHARACTERISTIC_UUID "19b10001-e8f2-537e-4f6c-d104768a1214"
#define RX_CHARACTERISTIC_UUID "19b10002-e8f2-537e-4f6c-d104768a1214" // receive this

bool deviceConnected = false;
bool oldDeviceConnected = false;
String oldState;
uint8_t mac;
// long startEventTime;

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

      Serial.print("Received: ");
      Serial.println(val.c_str()); // Print the value
    }
} chrCallbacks;

String readState() {
  const int LEVEL = 40;
  
  return 
    String(touchRead(T0) < LEVEL ? '1': '0') +
    String(touchRead(T2) < LEVEL ? '1': '0') +
    String(touchRead(T3) < LEVEL ? '1': '0') + 
    String(touchRead(T4) < LEVEL ? '1': '0') + 
    String(touchRead(T5) < LEVEL ? '1': '0') +
    String(touchRead(T6) < LEVEL ? '1': '0') +
    String(touchRead(T7) < LEVEL ? '1': '0') +
    String(touchRead(T8) < LEVEL ? '1': '0') +
    String(touchRead(T9) < LEVEL ? '1': '0') ;
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
    // startEventTime = micros();
    String newState = readState();

    if (oldState != newState) {

      // Closed manner playing
      int fingersUpCount = 0;
      if (newState[0] == oldState[0] && newState != "0000000000") { // functional pin is not changed
        for (int i = 1; i < newState.length(); i++) {
          if (newState[i] == '0') {
            fingersUpCount++;
          }

          if (fingersUpCount > 3) { // allow 3 open pins
            oldState = newState;
            return;
          }
        }
      }
      oldState = newState;

      Serial.print("Sent: ");
      Serial.println(newState);
      txCharacteristic->setValue(newState);
      txCharacteristic->notify();  
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
