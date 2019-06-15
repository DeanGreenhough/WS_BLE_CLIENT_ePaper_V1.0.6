/*
  Author: Dean Greenhough 2019 Block Water Softener Project Client Side
*/



// DEBUG
#define DEBUG 1

// BLE
#include "BLEDevice.h"							//#include "BLEScan.h"

//MQTT
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include "credentials.h"						// Credentials comment out and use defines below for your WiFi
//#define STA_SSID "*********"
//#define STA_PASS "*********"
#define AP_SSID  "Kinetico"
//WIFI & MQTT
WiFiClient espClient;
PubSubClient client(espClient);
//MQTT
#define mqtt_server       "192.168.0.200"     // server name or IP
#define mqtt_user         "admin"             // username
#define mqtt_password     "admin"             // password
#define looptime_topic    "BLE/looptime"      // Topic looptime
#define voltage_topic     "BLE/voltage"
#define current_topic     "BLE/current"
#define leftBlock_topic   "BLE/leftBlock"
#define rightBlock_topic  "BLE/rightBlock"
#define MQTTdelay 20


// DISPLAY
#include <GxEPD.h>
#include <GxGDEP015OC1/GxGDEP015OC1.h>        // 1.54" b/w
//#include <GxGDEW0154Z04/GxGDEW0154Z04.h>    // 1.54" b/w/r 200x200
//#include <GxGDEW0154Z17/GxGDEW0154Z17.h>    // 1.54" b/w/r 152x152
//#include <GxGDEW042T2/GxGDEW042T2.h>        // 4.2" b/w
//#include <GxGDEW042Z15/GxGDEW042Z15.h>      // 4.2" b/w/r
#include GxEPD_BitmapExamples
// FreeFonts from Adafruit_GFX
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
//#include <Fonts/FreeMonoBold18pt7b.h>
//#include <Fonts/FreeMonoBold24pt7b.h>
#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>
// BUSY -> 4, RST -> 16, DC -> 17, CS -> SS(5), CLK -> SCK(18), DIN -> MOSI(23), GND -> GND, 3.3V -> 3.3V
GxIO_Class io(SPI, /*CS=5*/ SS, /*DC=*/ 17, /*RST=*/ 16); // arbitrary selection of 17, 16
GxEPD_Class display(io, /*RST=*/ 16, /*BUSY=*/ 4); // arbitrary selection of (16), 4
// END DISPLAY


//static BLEUUID serviceUUID(BLEUUID((uint16_t)0x180D));
//static BLEUUID    charUUID(BLEUUID((uint16_t)0x2A37));
static BLEUUID serviceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static BLEUUID    charUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");
static BLEAddress *pServerAddress;
static boolean doConnect = false;
static boolean connected = false;
static boolean doScan    = false;

static BLERemoteCharacteristic* pRemoteCharacteristic;

unsigned int RX_LEFT  = 0;
unsigned int RX_RIGHT = 0;
int RX_VOLTAGE        = 0;
int RX_CURRENT        = 0;

//LoopTimer
unsigned long startMillis;         //completeLoopTime TIMER connectToServer
unsigned long currentMillis;       //completeLoopTime TIMER connectToServer
unsigned long completeLoopTime = 99;


enum { STEP_BTON, STEP_BTOFF, STEP_STA, STEP_END };
void onButton() {
  static uint32_t step = STEP_BTON;						 // DECLARES A KNOWN STATE
  switch (step) {										 // PASSES step TO switch CASE
    case STEP_BTON://BT Only
      Serial.println("** Starting BT");
      Serial.print("step =  " );
      Serial.println(step);
      btStart();
      break;
    case STEP_BTOFF://All Off
      Serial.println("** Stopping BT");
      Serial.print("step =  " );
      Serial.println(step);
      btStop();
      break;
    case STEP_STA://STA Only
      Serial.println("** Starting STA");
      Serial.print("step =  " );
      Serial.println(step);
      WiFi.begin(STA_SSID, STA_PASS);
      break;
    case STEP_END:
     Serial.println("Restart after sending MQTT ");
      esp_restart();									  //BLE NOT RESTARTING - RESTART AFTER SENDING MQTT
/*
 *    WiFi Off
      Serial.println("** Stopping WiFi");
      Serial.println(step);
      WiFi.mode(WIFI_OFF);
*/
      break;
    default:
      break;
  }
  if (step == STEP_END) {
    step = STEP_BTON;
	if (DEBUG) Serial.print("STEP_BTON  " );
	if (DEBUG)Serial.println(step);
  } else {
    step++;
    if (DEBUG) Serial.print("step_end++  =  " );
	if (DEBUG) Serial.println(step);
  }
  delay(100);
}

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case SYSTEM_EVENT_AP_START:
      Serial.println("AP Started");
      WiFi.softAPsetHostname(AP_SSID);
      break;
    case SYSTEM_EVENT_AP_STOP:
      Serial.println("AP Stopped");
      break;
    case SYSTEM_EVENT_STA_START:
      Serial.println("STA Started");
      WiFi.setHostname(AP_SSID);
      break;
    case SYSTEM_EVENT_STA_CONNECTED:
      Serial.println("STA Connected");
      WiFi.enableIpV6();
      break;
    case SYSTEM_EVENT_AP_STA_GOT_IP6:
      Serial.print("STA IPv6: ");
      Serial.println(WiFi.localIPv6());
      break;
    case SYSTEM_EVENT_STA_GOT_IP:
      //Serial.print("STA IPv4: ");
      Serial.println(WiFi.localIP());
      delay(500);
      sendMQTT();                                   //sendMQTT
      delay(500);
      //onButton();
      break;    
      case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("STA Disconnected");
      break;
      case SYSTEM_EVENT_STA_STOP:
      Serial.println("STA Stopped");
      break;    
    default:
      break;
  }
}
void SALT_BLOCK_READ()
{
  const GFXfont* f = &FreeMonoBold9pt7b;
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  display.setFont(f);
  //LEFT BLOCK
  display.setCursor(30, 30);
  display.print(RX_LEFT);
  //RIGHT BLOCK
  display.setCursor(130, 30);
  display.print(RX_RIGHT);

  //DRAW OUTLINE LEFT SALT BLOCK
  display.drawRect(5, 50, 90, 145, GxEPD_BLACK);
  //DRAW OUTLINE RIGHT SALT BLOCK
  display.drawRect(105, 50, 90, 145, GxEPD_BLACK);
  //TOP OUTLINE BOX
  display.drawRect(5, 5, 190, 40, GxEPD_BLACK);
}

void UPDATE_BLOCK_LEVELS()
{
  //LEFT SALT BLOCK DISPLAY LEVELS

  if (RX_LEFT <= 70)
  { //100%
    display.drawRect(10, 84, 80,  24, GxEPD_BLACK);
    display.drawRect(10, 113, 80, 24, GxEPD_BLACK);
    display.drawRect(10, 142, 80, 24, GxEPD_BLACK);
    display.drawRect(10, 171, 80, 19, GxEPD_BLACK);

    display.fillRect(10, 55, 80,  24, GxEPD_BLACK);
    display.fillRect(10, 84, 80,  24, GxEPD_BLACK);
    display.fillRect(10, 113, 80, 24, GxEPD_BLACK);
    display.fillRect(10, 142, 80, 24, GxEPD_BLACK);
    display.fillRect(10, 171, 80, 19, GxEPD_BLACK);

  }

  else if (RX_LEFT <= 115)
  { //80%
    display.drawRect(10, 55, 80,  24, GxEPD_BLACK);
    display.drawRect(10, 84, 80,  24, GxEPD_BLACK);
    display.drawRect(10, 113, 80, 24, GxEPD_BLACK);
    display.drawRect(10, 142, 80, 24, GxEPD_BLACK);
    display.drawRect(10, 171, 80, 19, GxEPD_BLACK);

    display.fillRect(10, 84, 80,  24, GxEPD_BLACK);
    display.fillRect(10, 113, 80, 24, GxEPD_BLACK);
    display.fillRect(10, 142, 80, 24, GxEPD_BLACK);
    display.fillRect(10, 171, 80, 19, GxEPD_BLACK);

  }

  else if (RX_LEFT <= 160)
  { //60%
    display.drawRect(10, 55, 80,  24, GxEPD_BLACK);
    display.drawRect(10, 84, 80,  24, GxEPD_BLACK);
    display.drawRect(10, 113, 80, 24, GxEPD_BLACK);
    display.drawRect(10, 142, 80, 24, GxEPD_BLACK);
    display.drawRect(10, 171, 80, 19, GxEPD_BLACK);

    display.fillRect(10, 113, 80, 24, GxEPD_BLACK);
    display.fillRect(10, 142, 80, 24, GxEPD_BLACK);
    display.fillRect(10, 171, 80, 19, GxEPD_BLACK);

  }
  else if (RX_LEFT <= 205)
  { //40%
    display.drawRect(10, 55, 80,  24, GxEPD_BLACK);
    display.drawRect(10, 84, 80,  24, GxEPD_BLACK);
    display.drawRect(10, 113, 80, 24, GxEPD_BLACK);
    display.drawRect(10, 142, 80, 24, GxEPD_BLACK);
    display.drawRect(10, 171, 80, 19, GxEPD_BLACK);

    display.fillRect(10, 142, 80, 24, GxEPD_BLACK);
    display.fillRect(10, 171, 80, 19, GxEPD_BLACK);

  }

  else if (RX_LEFT <= 245)
  { //20%
    display.drawRect(10, 55, 80,  24, GxEPD_BLACK);
    display.drawRect(10, 84, 80,  24, GxEPD_BLACK);
    display.drawRect(10, 113, 80, 24, GxEPD_BLACK);
    display.drawRect(10, 142, 80, 24, GxEPD_BLACK);
    display.drawRect(10, 171, 80, 19, GxEPD_BLACK);

    display.fillRect(10, 171, 80, 19, GxEPD_BLACK);

  }

  else if (RX_LEFT <= 300)
  {
    //Serial.println ("LEFT EMPTY"); //FILL RECT WITH RED

    display.drawRect(10, 55, 80,  24, GxEPD_BLACK);
    display.drawRect(10, 84, 80,  24, GxEPD_BLACK);
    display.drawRect(10, 113, 80, 24, GxEPD_BLACK);
    display.drawRect(10, 142, 80, 24, GxEPD_BLACK);
    display.drawRect(10, 171, 80, 19, GxEPD_BLACK);
    display.fillRect(10, 171, 80, 19, GxEPD_BLACK);
  }

  else if (RX_LEFT > 300)
  {
    //Serial.println ("LEFT EMPTY_2");
    display.drawRect(10, 55, 80,  24, GxEPD_BLACK);
    display.drawRect(10, 84, 80,  24, GxEPD_BLACK);
    display.drawRect(10, 113, 80, 24, GxEPD_BLACK);
    display.drawRect(10, 142, 80, 24, GxEPD_BLACK);
    display.drawRect(10, 171, 80, 19, GxEPD_BLACK);

    //display.fillRect(5, 50, 90, 145, GxEPD_RED);

    /*
      display.fillRect(10, 55,  80, 24, GxEPD_RED);
      display.fillRect(10, 84,  80, 24, GxEPD_RED);
      display.fillRect(10, 113, 80, 24, GxEPD_RED);
      display.fillRect(10, 142, 80, 24, GxEPD_RED);
      display.fillRect(10, 171, 80, 19, GxEPD_RED);
    */
  }


  delay(500);                                        //REQUIRED OR FAILS TO GET VL53 MEASUREMENT

  //RIGHT SALT BLOCK DISPLAY LEVELS

  if (RX_RIGHT <= 70)
  { //100%
    // display.fillRect(110,  55, 80, 24, GxEPD_BLACK);
    display.drawRect(110, 84, 80,  24, GxEPD_BLACK);
    display.drawRect(110, 113, 80, 24, GxEPD_BLACK);
    display.drawRect(110, 142, 80, 24, GxEPD_BLACK);
    display.drawRect(110, 171, 80, 19, GxEPD_BLACK);

    display.fillRect(110, 55, 80,  24, GxEPD_BLACK);
    display.fillRect(110, 84, 80,  24, GxEPD_BLACK);
    display.fillRect(110, 113, 80, 24, GxEPD_BLACK);
    display.fillRect(110, 142, 80, 24, GxEPD_BLACK);
    display.fillRect(110, 171, 80, 19, GxEPD_BLACK);

  }

  else if (RX_RIGHT <= 115)
  { //80%
    display.drawRect(110, 55, 80,  24, GxEPD_BLACK);
    display.drawRect(110, 84, 80,  24, GxEPD_BLACK);
    display.drawRect(110, 113, 80, 24, GxEPD_BLACK);
    display.drawRect(110, 142, 80, 24, GxEPD_BLACK);
    display.drawRect(110, 171, 80, 19, GxEPD_BLACK);

    display.fillRect(110, 84, 80,  24, GxEPD_BLACK);
    display.fillRect(110, 113, 80, 24, GxEPD_BLACK);
    display.fillRect(110, 142, 80, 24, GxEPD_BLACK);
    display.fillRect(110, 171, 80, 19, GxEPD_BLACK);

  }

  else if (RX_RIGHT <= 160)
  { //60%
    display.drawRect(110, 55, 80,  24, GxEPD_BLACK);
    display.drawRect(110, 84, 80,  24, GxEPD_BLACK);
    display.drawRect(110, 113, 80, 24, GxEPD_BLACK);
    display.drawRect(110, 142, 80, 24, GxEPD_BLACK);
    display.drawRect(110, 171, 80, 19, GxEPD_BLACK);

    display.fillRect(110, 113, 80, 24, GxEPD_BLACK);
    display.fillRect(110, 142, 80, 24, GxEPD_BLACK);
    display.fillRect(110, 171, 80, 19, GxEPD_BLACK);

  }
  else if (RX_RIGHT <= 205)
  { //40%
    display.drawRect(110, 55, 80,  24, GxEPD_BLACK);
    display.drawRect(110, 84, 80,  24, GxEPD_BLACK);
    display.drawRect(110, 113, 80, 24, GxEPD_BLACK);
    display.drawRect(110, 142, 80, 24, GxEPD_BLACK);
    display.drawRect(110, 171, 80, 19, GxEPD_BLACK);

    display.fillRect(110, 142, 80, 24, GxEPD_BLACK);
    display.fillRect(110, 171, 80, 19, GxEPD_BLACK);

  }

  else if (RX_RIGHT <= 245)
  { //20%
    display.drawRect(110, 55, 80,  24, GxEPD_BLACK);
    display.drawRect(110, 84, 80,  24, GxEPD_BLACK);
    display.drawRect(110, 113, 80, 24, GxEPD_BLACK);
    display.drawRect(110, 142, 80, 24, GxEPD_BLACK);
    display.drawRect(110, 171, 80, 19, GxEPD_BLACK);

    display.fillRect(110, 171, 80, 19, GxEPD_BLACK);

  }

  else if (RX_RIGHT <= 300)
  {
    //Serial.println ("RIGHT EMPTY");                       //TODO FILL RECT WITH RED
    display.drawRect(110, 55, 80,  24, GxEPD_BLACK);
    display.drawRect(110, 84, 80,  24, GxEPD_BLACK);
    display.drawRect(110, 113, 80, 24, GxEPD_BLACK);
    display.drawRect(110, 142, 80, 24, GxEPD_BLACK);
    display.drawRect(110, 171, 80, 19, GxEPD_BLACK);
    display.fillRect(110, 171, 80, 19, GxEPD_BLACK);
  }

  else if (RX_RIGHT > 300)
  {
    Serial.println("RIGHT EMPTY_2");
    display.drawRect(110, 55, 80,  24, GxEPD_BLACK);     //TODO FILL RECT WITH RED
    display.drawRect(110, 84, 80,  24, GxEPD_BLACK);
    display.drawRect(110, 113, 80, 24, GxEPD_BLACK);
    display.drawRect(110, 142, 80, 24, GxEPD_BLACK);
    display.drawRect(110, 171, 80, 19, GxEPD_BLACK);

    //display.fillRect(105, 50, 90, 145, GxEPD_RED);


    /*
      display.fillRect(110, 55,  80, 24, GxEPD_RED);
      display.fillRect(110, 84,  80, 24, GxEPD_RED);
      display.fillRect(110, 113, 80, 24, GxEPD_RED);
      display.fillRect(110, 142, 80, 24, GxEPD_RED);
      display.fillRect(110, 171, 80, 19, GxEPD_RED);
    */
  }

  delay(500); 
  display.update();


}
static void notifyCallback(BLERemoteCharacteristic * pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify)
{
  completeLoopTime = 0;
  startMillis = millis();      //START LOOPTIME
  if (DEBUG)Serial.println("");
  if (DEBUG)Serial.println("Notify Callback");

  RX_LEFT = ((int)(pData[0]) << 8) + pData[1];  //DECODE CAST 2 BYTES INTO AN INT
  if (DEBUG)Serial.print ("RX_LEFT     ");
  if (DEBUG)Serial.println(RX_LEFT);
  delay(10);
  RX_RIGHT = ((int)(pData[2]) << 8) + pData[3]; //DECODE CAST 2 BYTES INTO AN INT
  if (DEBUG)Serial.print ("RX_RIGHT    ");
  if (DEBUG)Serial.println(RX_RIGHT);
  delay(10);
  RX_VOLTAGE = ((int)(pData[4]) << 8) + pData[5];  //DECODE CAST 2 BYTES INTO AN INT
  if (DEBUG)Serial.print ("RX_VOLTAGE  ");
  if (DEBUG)Serial.println(RX_VOLTAGE);
  delay(10);
  RX_CURRENT = ((int)(pData[6]) << 8) + pData[7];  //DECODE CAST 2 BYTES INTO AN INT
  if (DEBUG)Serial.print ("RX_CURRENT  ");
  if (DEBUG)Serial.println(RX_CURRENT);
  delay(10);

  SALT_BLOCK_READ();             //DISPLAY
  UPDATE_BLOCK_LEVELS();         //DISPLAY
  delay(10);

  currentMillis = millis();      //END LOOPTIME
  completeLoopTime = (currentMillis - startMillis) ;
  if (DEBUG)Serial.print ("Callback LoopTime ");
  if (DEBUG)Serial.println(completeLoopTime);
  if (DEBUG)Serial.println("");

  onButton();									  //TURN BT OFF READY FOR WIFI CONNECTION
  if (DEBUG)Serial.println("onButton STEP_BTOFF");
  delay(100);

  onButton();
  if (DEBUG)Serial.println("onButton STEP_STA");
  if (DEBUG)Serial.println("onButton STEP_STA");

  
}

bool connectToServer(BLEAddress pAddress) {
	  
  if (DEBUG)Serial.print("Forming a connection to ");
  if (DEBUG)Serial.println(pAddress.toString().c_str());

  BLEClient*  pClient  = BLEDevice::createClient();
  if (DEBUG)Serial.println("Created client");

  pClient->connect(pAddress);
  if (DEBUG)Serial.println("Connected to server");


  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    if (DEBUG)Serial.print("Failed to find our service UUID: ");
    if (DEBUG)Serial.println(serviceUUID.toString().c_str());
    return false;
  }
  if (DEBUG)Serial.println("Found our service");

  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (pRemoteCharacteristic == nullptr) {
    if (DEBUG)Serial.print("Failed to find our characteristic UUID: ");
    if (DEBUG)Serial.println(charUUID.toString().c_str());
    return false;
  }
  if (DEBUG)Serial.println("Found our characteristic");

  pRemoteCharacteristic->registerForNotify(notifyCallback);
  
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    /**
        Called for each advertising BLE server.
    */
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      if (DEBUG)Serial.print("BLE Advertised Device found: ");
      Serial.println("");
      if (DEBUG)Serial.println(advertisedDevice.toString().c_str());
      delay(10);
      // We have found a device, let us now see if it contains the service we are looking for.
      if (advertisedDevice.haveServiceUUID() && advertisedDevice.getServiceUUID().equals(serviceUUID)) {

        if (DEBUG)Serial.println("Found our device!  address: ");
        Serial.println("");
        delay(10);
        advertisedDevice.getScan()->stop();

        pServerAddress = new BLEAddress(advertisedDevice.getAddress());
        doConnect = true;
        doScan = true;

      } // Found our server
    } // onResult
}; // MyAdvertisedDeviceCallbacks

//MQTT
void reconnect() {

  while (!client.connected())
  {
    Serial.println("");
    Serial.print       ("Connecting to MQTT broker ...");
    if (client.connect ("ESP32Client", mqtt_user, mqtt_password)) {
      Serial.println   ("OK");
    } else {
      Serial.print     ("[Error] Not connected: ");
      Serial.print(client.state());
      Serial.println   ("Wait 0.5 seconds before retry.");
      delay(500);
    }
  }
}

void sendMQTT() {                          //SEND MQTT TO SERVER

  client.setServer(mqtt_server, 1883);
  if (!client.connected()) {
    reconnect();
  }

  client.publish(looptime_topic, String(completeLoopTime).c_str(), true);
  delay(MQTTdelay);
  if (DEBUG) {
    Serial.print(completeLoopTime);
    Serial.println("  RX looptime  sent to MQTT.");
    delay(MQTTdelay);
  }
  client.publish(voltage_topic, String(RX_VOLTAGE).c_str(), true);
  delay(MQTTdelay);
  if (DEBUG) {
    Serial.print(RX_VOLTAGE);
    Serial.println("  RX_VOLTAGE sent to MQTT.");
    delay(MQTTdelay);
  }
  client.publish(current_topic, String(RX_CURRENT).c_str(), true);
  delay(MQTTdelay);
  if (DEBUG) {
    Serial.print(RX_CURRENT);
    Serial.println("  RX_CURRENT sent to MQTT.");
    delay(MQTTdelay);
  }
  client.publish(leftBlock_topic, String(RX_LEFT).c_str(), true);
  delay(MQTTdelay);
  if (DEBUG) {
    Serial.print(RX_LEFT);
    Serial.println("  RX_LEFT sent to MQTT.");
    delay(MQTTdelay);
  }
  client.publish(rightBlock_topic, String(RX_RIGHT).c_str(), true);
  delay(MQTTdelay);
  if (DEBUG) {
    Serial.print(RX_RIGHT);
    Serial.println("  RX_RIGHT sent to MQTT.");
    delay(MQTTdelay);
    Serial.println("MQTT Sent");
    onButton();
    Serial.println("onButton STEP_END");
  }
  delay(2000);
}
void INIT_BLE()
{
  Serial.println("");
  BLEDevice::init("Water Softener");
  if (DEBUG)Serial.println("Starting BLE Client");
  
  BLEDevice::init("");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5);
}



void setup()
{ 
  startMillis = millis();      //START LOOPTIME
  Serial.begin(115200);
  Serial.println("");
  Serial.println(__FILE__);
  Serial.println(__DATE__);
  Serial.println(__TIME__);
  Serial.println("");
  display.init();
  // WIFI EVENT HANDLER
  WiFi.onEvent(WiFiEvent);
  //WiFi.mode(WIFI_OFF);
  onButton();                   //TURN BT ON
  Serial.println("onButton STEP_BTON");
  

}

void loop()
{
  INIT_BLE();

  if (doConnect == true) {
    if (connectToServer(*pServerAddress)) {
      if (DEBUG)Serial.println("Connected to BLE Server.");
      Serial.println("");
      connected = true;
    } else {
      if (DEBUG)Serial.println("Failed to connect to the server");
    }
    doConnect = false;
  }

 
  delay(3000); //REQUIRED 3 SECS..................REQUIRES RETESTING
}
