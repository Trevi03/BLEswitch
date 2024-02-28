#include <Keyboard.h>
#include <ArduinoBLE.h>
//#include <SPI.h>
#include <SD.h>
#include <DS3232RTC.h>
//#include <TimeLib.h>

DS3232RTC myRTC;
File logger;

// On/off bluetooth: pin D4 == high is Bluetooth ON
volatile static int bDisconnected = 0;
const int batPin = 4;
const int usbPin = 12;
const int bLED = 2;


typedef struct pinKey_t{
    uint8_t pin;
    const uint8_t key;
    char keyLiteral;
    uint8_t pinStateLast;
} pinKey_t;

//setting up values for pins based on the typedef struct
pinKey_t pinKey[] = {
    { 7, '\n', 'a', 0 }, // Pin 2 sends a
    { 8, ' ', 'b', 0 },// Pin 3 sends b
};

typedef struct keyboard_report_t {
    uint8_t report_id;
    uint8_t modifiers;
    uint8_t reserved;
    uint8_t key_codes[6];
} keyboard_report_t;

keyboard_report_t kbd_report;

uint8_t report_descriptor[] = {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x06,                    // USAGE (Keyboard)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x85, 0x01,                    //   REPORT_ID (1)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
    0x19, 0x01,                    //   USAGE_MINIMUM
    0x29, 0x7f,                    //   USAGE_MAXIMUM
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x95, 0x08,                    //   REPORT_COUNT (8)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x81, 0x01,                    //   INPUT (Cnst,Ary,Abs)
    0x95, 0x05,                    //   REPORT_COUNT (5)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x05, 0x08,                    //   USAGE_PAGE (LEDs)
    0x19, 0x01,                    //   USAGE_MINIMUM (Num Lock)
    0x29, 0x05,                    //   USAGE_MAXIMUM (Kana)
    0x91, 0x02,                    //   OUTPUT (Data,Var,Abs)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x03,                    //   REPORT_SIZE (3)
    0x91, 0x01,                    //   OUTPUT (Cnst,Ary,Abs)
    0x95, 0x06,                    //   REPORT_COUNT (6)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x65,                    //   LOGICAL_MAXIMUM (101)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
    0x19, 0x00,                    //   USAGE_MINIMUM (Reserved (no event indicated))
    0x29, 0x65,                    //   USAGE_MAXIMUM (Keyboard Application)
    0x81, 0x00,                    //   INPUT (Data,Ary,Abs)
    0xc0                           // END_COLLECTION
};

uint8_t pnpID[] = {0x02, 0x8a, 0x24, 0x66, 0x82,0x34,0x36};

BLEService hidService("1812"); // BLE HID (keyboard) service UUID, same as "00001812-0000-1000-8000-00805f9b34fb"
BLEService deviceService("180A"); // BLE device information
BLEService battService("180F"); // BLE HID Battery Service
// Note: should I include battery service

BLEByteCharacteristic hidProtocolModeCharacteristic("2A4E", BLERead | BLEWriteWithoutResponse); // BLE HID protocol mode characteristic
BLEByteCharacteristic hidControlPointCharacteristic("2A4C", BLEWriteWithoutResponse); // BLE HID control point characteristic

BLECharacteristic pnpCharacteristic("2A50", BLERead, sizeof(pnpID), true); // BLE Device Information characteristic
BLECharacteristic hidReportCharacteristic("2A4D", BLERead | BLENotify, sizeof(kbd_report), true); // BLE HID Report characteristic
BLECharacteristic hidReportMapCharacteristic("2A4B", BLERead, sizeof(report_descriptor), true); // BLE HID Report Map characteristic
BLEByteCharacteristic hidBatteryLevelCharacteristic("2A19", BLERead); // BLE HID battery level characteristic



void SendKeyReport(uint8_t ucKey)
{
  kbd_report.report_id = 1;
  if (ucKey != 0){
    if (ucKey == '\n'){
      ucKey = 0x28;
    }
    else if (ucKey == ' '){
      ucKey = 0x2c;
    }
    else{
      ucKey -= 93; // convert the ASCII code of the key to the corresponding HID Usage ID
    }
  }
  kbd_report.key_codes[0] = ucKey;
  hidReportCharacteristic.writeValue((uint8_t *)&kbd_report, sizeof(kbd_report));  
} /* SendKeyReport() */


void blePeripheralDisconnectHandler(BLEDevice central) {
  // central disconnected event handler
  bDisconnected = 1;
}

void logToSD(char key, String method) {
  if (!SD.begin(14)) {
   Serial.println("No SD Card");
    while (1);
  }
  logger = SD.open("log.txt", FILE_WRITE);
  
  if (logger) {
    Serial.println(myRTC.get());
    logger.print(myRTC.get());
    logger.print(",");
    logger.print(key);
    logger.print(",");
    logger.println(method);
    logger.close();
    Serial.println("done.");
  } else {
    // if the file didn't open, print an error:
    Serial.println("error opening test.txt");
  }
}




void setup()
{
  Serial.begin(9600);
  // make the pushbutton's pin an input:
  pinMode(usbPin, INPUT);
  //Serial.println("Serial begin");
  //Serial.begin(115200);
  myRTC.begin();
  myRTC.set(1686517249);

  pinMode(bLED,OUTPUT);
  
// Initialize pins
  for (uint8_t i = 0; i < sizeof(pinKey) / sizeof(pinKey_t); i++) {
      // Set pin to input and digitalWrite
      pinMode(pinKey[i].pin, INPUT);
      
      // Set current pin state
      pinKey[i].pinStateLast = digitalRead(pinKey[i].pin);
  }
  
  if (digitalRead(usbPin)==HIGH){
    //Place if function for Bluetooth mode ON
    BLE.begin();
    
    // set advertised name, appearance and service UUID:
    BLE.setDeviceName("Arduino Nano 33 IoT Keyboard");
    BLE.setAppearance(961); // BLE_APPEARANCE_HID_KEYBOARD
    BLE.setAdvertisedService(hidService);
    BLE.setConnectable(true);
    BLE.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);
  
    // add the characteristics to the services
    hidService.addCharacteristic(hidReportCharacteristic);
    hidService.addCharacteristic(hidReportMapCharacteristic);
    hidService.addCharacteristic(hidProtocolModeCharacteristic);
    hidService.addCharacteristic(hidControlPointCharacteristic);
    deviceService.addCharacteristic(pnpCharacteristic);
    battService.addCharacteristic(hidBatteryLevelCharacteristic);
  
    hidReportMapCharacteristic.writeValue((uint8_t *)report_descriptor, sizeof(report_descriptor));
    hidProtocolModeCharacteristic.writeValue((uint8_t)1); // set to default value
    hidBatteryLevelCharacteristic.writeValue((uint8_t)100); // set to 100%
    pnpCharacteristic.writeValue(pnpID, sizeof(pnpID));
    memset(&kbd_report, 0, sizeof(kbd_report));
    // Writing empty key report
    SendKeyReport(0);
    
    // Add the services we need
    BLE.addService(hidService);
    BLE.addService(deviceService);
    BLE.addService(battService);
  
    // start advertising
    BLE.advertise();  
  }

  else{
    //nothing
  }
}




void loop() {
  uint8_t pinState;

  if (digitalRead(usbPin)==HIGH){
    // listen for BLE peripherals to connect:
    BLEDevice central = BLE.central();
   digitalWrite(bLED, HIGH);
   delay(1000);
   digitalWrite(bLED, LOW);
   delay(100);
   // if a central is connected to peripheral:
    if (central) {
      while (central.connected()) {
   // Check which pins changed
      for (uint8_t i = 0; i < sizeof(pinKey) / sizeof(pinKey_t); i++) {
          // Read current pin state
          pinState = digitalRead(pinKey[i].pin);
  
          // Check if pin changed since last read
          if (pinKey[i].pinStateLast != pinState) {
              pinKey[i].pinStateLast = pinState;
  
              if (pinState == HIGH) {
                  SendKeyReport(pinKey[i].key);
                  logToSD(pinKey[i].keyLiteral, "ble");
                  delay(100);
              }
              else{
                  SendKeyReport(0);
                  delay(50);
              }
          }
            
            else{
              SendKeyReport(0);
              delay(50);
              }
           } // for loop
        
        
        if (bDisconnected){
          bDisconnected = 0;
          return;
          }
        } // while connected
      } // if connected
  }// if bPin on

  else{
    // Check which pins changed
    for (uint8_t i = 0; i < sizeof(pinKey) / sizeof(pinKey_t); i++) {
        // Read current pin state
        pinState = digitalRead(pinKey[i].pin);

        // Check if pin changed since last read
        if (pinKey[i].pinStateLast != pinState) {
            pinKey[i].pinStateLast = pinState;

            if (pinState == HIGH) {
                // Send key
                Keyboard.press(pinKey[i].key);
                logToSD(pinKey[i].keyLiteral, "serial");
                delay(100);
                Keyboard.releaseAll();
            }
        }
    }

    // Wait 50ms
    delay(50);
  }
}// void loop()
