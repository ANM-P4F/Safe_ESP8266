#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "utilsP4F.h"

#define PWM1   5   // PWM1 output
#define DIR1   0   // Dir 1
#define LED_OUT    16

#define PMW_VAL 1024

#define DEBUG

const char* ssid = "xxxxxx"; //your wifi ssid
const char* password = "yyyyyyy";  //your wifi password 
const char* ssid_ap = "SafeESP8266";
const char* password_ap = "esp8266Testing";

int ledState = LOW;

unsigned long previousMillis = 0;
unsigned long previousMillis2 = 0;
const long interval = 1000;
const long interval2 = 1;

const int PACKAGE_SIZE = 512;
byte packetBuffer[PACKAGE_SIZE];

//Creating UDP Listener Object. 
WiFiUDP UDPServer;
unsigned int UDPPort = 6868;
unsigned int clientPort = 6969;

int seed = 0;
int key = 0;

String state = "";
String requestedState = "";
String safePass = "12345678";

uint8_t keyAES[32];

const String MESSAGE_REQUEST_LOCK   = "requestLock00000";
const String MESSAGE_REQUEST_UNLOCK = "requestUnlock000";
const String MESSAGE_CHANGE_PASS    = "Password";

const String MESSAGE_RESPONSE_SEED  = "seed:";

void testAES(){
  String test = "Password12345679";
  String passWord = "12345678";

  std::string encryptedStr = P4F::encrypt(test.c_str(), passWord.c_str());
  std::string decryptedStr = P4F::decrypt(encryptedStr, passWord.c_str());

  String tmpEncryptedStr = String(encryptedStr.c_str());
  String tmpDecryptedStr = String(decryptedStr.c_str());
  Serial.println(tmpEncryptedStr);
  Serial.println(tmpDecryptedStr);

}

/*calculate key from seed, it must be same with android side*/
int calculateKey(int seed){
  return sqrt(seed);
}

void sendEncryptedMessageUDP(String message, IPAddress addr, int clientPort){
  UDPServer.beginPacket(addr, clientPort);
  std::string encryptedMessage = P4F::encrypt(message.c_str(), safePass.c_str());
  UDPServer.write(encryptedMessage.c_str());
  UDPServer.endPacket();
}

/* Process post request from UDP android client */
void processUDPData(){
  int cb = UDPServer.parsePacket();
  if (cb) {
    UDPServer.read(packetBuffer, PACKAGE_SIZE);
    // UDPServer.flush();
    IPAddress addr = UDPServer.remoteIP();
    std::string messageRecv = std::string((char*)packetBuffer);
    std::string decryptedMsg = P4F::decrypt(messageRecv, safePass.c_str());
    String message = String(decryptedMsg.c_str());
#ifdef DEBUG
    Serial.print("receive: ");
    Serial.println(message);
    Serial.print("from: ");
    Serial.println(addr);
#endif
    if (message == MESSAGE_REQUEST_LOCK || message == MESSAGE_REQUEST_UNLOCK){
      seed = random(0, 1000);
      key = calculateKey(seed);
      char buf[15];
      sprintf(buf, "%.11u", seed);
      String response = MESSAGE_RESPONSE_SEED + String(buf);
#ifdef DEBUG
      Serial.print("seed: ");
      Serial.println(seed);
      Serial.print("key calc: ");
      Serial.println(key);
#endif
      sendEncryptedMessageUDP(response, addr, clientPort);
#ifdef DEBUG
      Serial.print("response: ");
      Serial.println(response);
#endif
      requestedState = message;
    }else if (message.indexOf("key")!=-1){
      int keyFromClient = message.substring(4).toInt();
#ifdef DEBUG
      Serial.print("Receive key: ");
      Serial.println(keyFromClient);
#endif
      if (key==keyFromClient){
#ifdef DEBUG
        Serial.println("Key Matched !!!");
#endif
        if (requestedState==MESSAGE_REQUEST_LOCK){
          setLock(0);
          state = "Locked";
        }else if(requestedState==MESSAGE_REQUEST_UNLOCK){
          setLock(PMW_VAL);
          state = "UnLocked";
        }
#ifdef DEBUG
        Serial.println(state);
#endif
        sendEncryptedMessageUDP(state, addr, clientPort);
      }else{
#ifdef DEBUG
        Serial.println("Wrong key !!!");
#endif
      }
    }else if(message.indexOf(MESSAGE_CHANGE_PASS)!=-1){
      String safePassTmp = message.substring(8);
      String response = MESSAGE_CHANGE_PASS+safePassTmp;
#ifdef DEBUG
      Serial.print("response: ");
      Serial.println(response);
#endif
      sendEncryptedMessageUDP(response, addr, clientPort);
      safePass = safePassTmp;
    }
    memset(packetBuffer, 0, PACKAGE_SIZE);
  }
}

/* Set Lock motor */
void setLock(int val){
   analogWrite(PWM1, val);
   digitalWrite(DIR1, LOW);
}

void setup() {
  Serial.begin(9600);

  Serial.println();
  Serial.println();
  Serial.println();

  pinMode(LED_OUT, OUTPUT);
  digitalWrite(LED_OUT, 1);

  //motor setting
  pinMode(PWM1, OUTPUT);
  pinMode(DIR1, OUTPUT);
  analogWrite(PWM1, LOW);
  digitalWrite(DIR1, LOW);

  setLock(0);

#if 0
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
#else
  WiFi.softAP(ssid_ap, password_ap);
  Serial.println("");

  Serial.println();
  Serial.print("Server IP address: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("Server MAC address: ");
  Serial.println(WiFi.softAPmacAddress());
#endif

  UDPServer.begin(UDPPort); 

  digitalWrite(LED_OUT, 0);
  setLock(PMW_VAL);

  randomSeed(millis());

  memset(packetBuffer, 0, PACKAGE_SIZE);

#ifdef DEBUG
  testAES();
#endif

  state == "Unlocked";
  Serial.println("Server Start\n");
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    if (ledState == LOW)
      ledState = HIGH;  // Note that this switches the LED *off*
    else
      ledState = LOW;   // Note that this switches the LED *on*
    digitalWrite(LED_OUT, ledState);
  }
  processUDPData();
}


