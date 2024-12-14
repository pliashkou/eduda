#include <Arduino.h>
// #include "BluetoothSerial.h"

// #if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
// #error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
// #endif

// int ledPin = 2;
// BluetoothSerial SerialBT;

// void setup() {
//   Serial.begin(9600);
//   SerialBT.begin("duda-1"); //Bluetooth device name
//   pinMode(ledPin, OUTPUT);
//   Serial.println("The device started, now you can pair it with bluetooth!");
// }

// void loop() {
//    if (Serial.available()) {
//     SerialBT.write(Serial.read());
//   }
//   if (SerialBT.available()) {
//     Serial.write(SerialBT.read());
//   }
//   delay(20);
//   // Serial.println(touchRead(T0));

//   // if (touchRead(T0) < 50) {
//   //   digitalWrite(ledPin, HIGH);
//   // } else {
//   //   digitalWrite(ledPin, LOW);
//   // }
// }

/*
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Complete project details at https://RandomNerdTutorials.com/esp32-web-bluetooth/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

BLEServer* pServer = NULL;
BLECharacteristic* pSensorCharacteristic = NULL;
BLECharacteristic* pLedCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t value = 0;
String oldState = "";

const int ledPin = 2; // Use the appropriate GPIO pin for your setup

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID        "19b10000-e8f2-537e-4f6c-d104768a1214"
#define SENSOR_CHARACTERISTIC_UUID "19b10001-e8f2-537e-4f6c-d104768a1214"
#define LED_CHARACTERISTIC_UUID "19b10002-e8f2-537e-4f6c-d104768a1214"

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};

class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pLedCharacteristic) {
    std::string val = pLedCharacteristic->getValue();
    int receivedValue = (uint8_t)val[0];

    Serial.print("Characteristic event, written: ");
    Serial.println(receivedValue); // Print the integer value

    if (receivedValue == 1) {
      digitalWrite(ledPin, HIGH);
    } else {
      digitalWrite(ledPin, LOW);
    }
  }
};

String readState() {
  const int LEVEL = 50;
  
  return String(touchRead(T0) < LEVEL ? '1': '0') + /* String(touchRead(T1) < LEVEL ? '1': '0') + String(touchRead(T2) < LEVEL ? '1': '0') + */
    String(touchRead(T3) < LEVEL ? '1': '0') + String(touchRead(T4) < LEVEL ? '1': '0') + String(touchRead(T5) < LEVEL ? '1': '0' ) +
    String(touchRead(T6) < LEVEL ? '1': '0') +String(touchRead(T7) < LEVEL ? '1': '0') +String(touchRead(T8) < LEVEL ? '1': '0') +
    String(touchRead(T9) < LEVEL ? '1': '0') ;
}

void setup() {
  Serial.begin(9600);
  pinMode(ledPin, OUTPUT);

  // Create the BLE Device
  BLEDevice::init("duda-piatro");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pSensorCharacteristic = pService->createCharacteristic(
                      SENSOR_CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );

  // Create the ON button Characteristic
  pLedCharacteristic = pService->createCharacteristic(
                      LED_CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_WRITE
                    );

  // Register the callback for the ON button characteristic
  pLedCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Create a BLE Descriptor
  pSensorCharacteristic->addDescriptor(new BLE2902());
  pLedCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");
}

void loop() {
  // notify changed value
  if (deviceConnected) {
    String newState = readState();
    // Serial.print(oldState);
    // Serial.print("-");
    // Serial.println(newState);
    if (oldState != newState) {
      String s = String(newState);
      pSensorCharacteristic->setValue(newState.c_str());
      pSensorCharacteristic->notify();  
      Serial.print("New value notified: ");
      Serial.println(s);
      oldState = newState;
    }
    // delay(30);
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