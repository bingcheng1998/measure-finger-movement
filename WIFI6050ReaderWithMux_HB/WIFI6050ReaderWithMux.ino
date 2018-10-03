
#include <ESPWiFi.h>
#include <WiFiUdp.h>
#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps20.h"
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
#include "Wire.h"
#include "WiFi_setup.h"
#endif

#define MUXUSED                //Muxing Around With The CD74HC4067

//Choose one of these three MODES
//#define MODE_SERIAL            //Serial debug mode
//#define MODE_PROCESSING        //Processing mode without host
#define MODE_PROCESSING_HOST   //Processing mode with host
//#define MODE_SERIAL_TEAPOT


//#define WiFiHOTSPOTS                            //是否自身发送热点
//#define OUTPUT_SERIALR                          //串口调试输出
//#define DEBUG                                       //DEBUG output
//#define OUTPUT_PROCESSING                            //UDP高级防损失打包发送
//#define WiFiHOTSPOTS                            //是否自身发送热点
//#define TIME_COUNTER                           //Count the times of reading from all the mpu6050s from Serial
//#define SERIAL_START                           //正式使用UDP需要注释掉这条, 仅限调试使用
#define SerialPort 115200
#define mpuNum 2  //mpuNum is the number of elements inside of bootPin[mpuNum]
#define mpuNum1 13
#define teapot_num 7
#ifndef MUXUSED
static int bootPin[mpuNum] = {13, 15, 14, 10, 5, 4};  //Available ports are 4 ,5, 9, 10, 12, 13, 14, 15.
#else
//Mux control pins
static int s0 = 5;
static int s1 = 4;
static int s2 = 14;
static int s3 = 12;
static int En = 13;
#endif
//Run IMU_Zero at Example-MPU6050 and you can get these numbers:
const int M13 [6] = {-1227, -3541, 1357, 21, -67, 57}; //假的，还没测
const int M12 [6] = {-1227, -3541, 1357, 21, -67, 57}; //假的，还没测
const int M11 [6] = {-1227, -3541, 1357, 21, -67, 57}; //假的，还没测
const int M10 [6] = {1139, 105, 1786, 54, 21, 7};
const int M9 [6] = {-5003, 1581, 399, 94, 133, -29};
const int M8 [6] = {-799, 1317, 1829, 157, 60, -40};
const int M7 [6] = {1045, 1115, 1561, 3, 48, 12};
const int M6 [6] = {-2103, 1965, 1437, 123, 29, -4};
const int M5 [6] = {-2183, -1303, 1273, -112, 81, -1};
const int M4 [6] = {-3139, -3599, 2903, 26, -58, -2};
const int M3 [6] = {-1227, -3541, 1357, 21, -67, 57};
const int M2 [6] = {-1729, -101, 1143, -156, 143, -32};
const int M1 [6] = {-1931, -1475, 1161, -48, 52, 45};
//                                 M1   M2   M3   M4   M5   M6   M7   M8   M9   M10   M11
static int XGyroOffset[mpuNum1] = {M5[3], M6[3], M3[3], M4[3], M7[3], M8[3], M9[3], M10[3], M1[3], M2[3], M11[3], M12[3], M13[3]};
static int YGyroOffset[mpuNum1] = {M5[4], M6[4], M3[4], M4[4], M7[4], M6[4], M9[4], M10[4], M1[4], M2[4], M11[4], M12[4], M13[4]};
static int ZGyroOffset[mpuNum1] = {M5[5], M6[5], M3[5], M4[5], M7[5], M6[5], M9[5], M10[5], M1[5], M2[5], M11[5], M12[5], M13[5]};
static int ZAccelOffset[mpuNum1] ={M5[2], M6[2], M3[2], M4[2], M7[2], M6[2], M9[2], M10[2], M1[2], M2[2], M11[2], M12[2], M13[2]};
unsigned int localPort = 8266;                  //The port you used in processing
static char WiFiAPPSW[] = "12345678";            // Your password of Wifi hotspots
static char* ssid     = "bingcheng";             // The SSID of your router
static char* password = "963852741";         // The passWd of your router

#ifdef MUXUSED
void setupMux();
#endif


#ifdef MODE_SERIAL
  #define OUTPUT_SERIALR
  #define DEBUG                                       //DEBUG output
  #define TIME_COUNTER                           //Count the times of reading from all the mpu6050s from Serial
  #define SERIAL_START                           //
#endif

#ifdef MODE_PROCESSING
  #define OUTPUT_PROCESSING                            //UDP高级防损失打包发送
  #undef OUTPUT_SERIALR
#endif

#ifdef MODE_PROCESSING_HOST
  #define OUTPUT_PROCESSING                            //UDP高级防损失打包发送
  #define WiFiHOTSPOTS                                 //是否自身发送热点
#endif

#ifdef MODE_SERIAL_TEAPOT
  #define SERIAL_TEAPOT
  #define SERIAL_START
  #define QUANTANION_OUT
#endif

WiFiUDP Udp;
char packetBuffer[255]; //buffer to hold incoming packet
char  ReplyBuffer[] = "I received n";
//                                                                    循环  MPU序号
#ifndef SERIAL_TEAPOT
uint8_t teapotPacket[15] = { '$', 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0x00, 0x00, 0x00, '\r', '\n' };
#endif
#ifdef SERIAL_TEAPOT
uint8_t teapotPacket[14] = { '$', 0x02, 0,0, 0,0, 0,0, 0,0, 0x00, 0x00, '\r', '\n' };
#endif
IPAddress remoteIp;
int remotePORT;
// AD0 low = 0x69 (default and active)
// AD0 high = 0x68
MPU6050 mpu[mpuNum]; // default with 0x69 AD0 low will boot
//bool mpuInitSuccess[mpuNum];
#define LED_PIN 16
bool blinkState = false;

// MPU control/status vars
bool dmpReady[mpuNum];  // set true if DMP init was successful
uint8_t mpuIntStatus[mpuNum];   // holds actual interrupt status byte from MPU
uint8_t devStatus[mpuNum];      // return status after each device operation (0 = success, !0 = error)
uint16_t packetSize[mpuNum];    // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount[mpuNum];     // count of all bytes currently in FIFO
uint8_t fifoBuffer[mpuNum][64]; // FIFO storage buffer

// orientation/motion vars
Quaternion q;           // [w, x, y, z]         quaternion container
VectorInt16 aa;         // [x, y, z]            accel sensor measurements
VectorInt16 aaReal;     // [x, y, z]            gravity-free accel sensor measurements
VectorInt16 aaWorld;    // [x, y, z]            world-frame accel sensor measurements
VectorFloat gravity;    // [x, y, z]            gravity vector
float euler[3];         // [psi, theta, phi]    Euler angle container
float ypr[3];           // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector

// ================================================================
// ===                      BOOT                                ===
// ================================================================
#ifndef MUXUSED
//boot with bootPins
void boot(int pin) {
  for (int i = 0; i < mpuNum; ++i)
  {
    digitalWrite(bootPin[i], HIGH);
  }
  digitalWrite(pin, LOW);
}
#endif

#ifdef MUXUSED
//boot with CD74HC4067 http://bildr.org/2011/02/cd74hc4067-arduino/
int setMux(int channel) {
  int controlPin[] = {s0, s1, s2, s3};
  digitalWrite(En, HIGH);
  int muxChannel[16][4] = {
    {0, 0, 0, 0}, //channel 0
    {1, 0, 0, 0}, //channel 1
    {0, 1, 0, 0}, //channel 2
    {1, 1, 0, 0}, //channel 3
    {0, 0, 1, 0}, //channel 4
    {1, 0, 1, 0}, //channel 5
    {0, 1, 1, 0}, //channel 6
    {1, 1, 1, 0}, //channel 7
    {0, 0, 0, 1}, //channel 8
    {1, 0, 0, 1}, //channel 9
    {0, 1, 0, 1}, //channel 10
    {1, 1, 0, 1}, //channel 11
    {0, 0, 1, 1}, //channel 12
    {1, 0, 1, 1}, //channel 13
    {0, 1, 1, 1}, //channel 14
    {1, 1, 1, 1} //channel 15
  };
  for (int i = 0; i < 4; i ++) {
    digitalWrite(controlPin[i], muxChannel[channel][i]);
  };
  digitalWrite(En, LOW);
}
#endif

#ifdef TIME_COUNTER
int iCount = 0;
#endif

// ================================================================
// ===                      INITIAL SETUP                       ===
// ================================================================

void setup() {
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
  Wire.begin(0, 2);
  Wire.setClock(400000); // 400kHz I2C clock. Comment this line if having compilation difficulties
#elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
  Fastwire::setup(400, true);
#endif
  // configure LED for output
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, false);
#ifndef MUXUSED
  for (int i = 0; i < mpuNum; ++i)
  {
    pinMode(bootPin[i], OUTPUT);
  }
#endif
#ifdef MUXUSED
  setupMux();
#endif
  int bootSuccess = 0;
  Serial.begin(SerialPort);
  while (!Serial);
  delay(500);
  Serial.println();
  for (int i = 0; i < mpuNum; i++) {
#ifndef MUXUSED
    boot(bootPin[i]);
#endif

#ifdef MUXUSED
    setMux(i);
#endif
    delay(200);
    // initialize device
    Serial.print(F("Initializing MPU6050 ic = "));
    Serial.println(i);
    mpu[i].initialize();

    // verify connection
    Serial.println(F("Testing device connections..."));
    Serial.println(mpu[i].testConnection() ? F("MPU6050 connection --------successful") : F("MPU6050 connection --------failed"));
    Serial.println(F("Initializing DMP..."));
    devStatus[i] = mpu[i].dmpInitialize();

    mpu[i].setXGyroOffset(XGyroOffset[i]);
    mpu[i].setYGyroOffset(YGyroOffset[i]);
    mpu[i].setZGyroOffset(ZGyroOffset[i]);
    mpu[i].setZAccelOffset(ZAccelOffset[i]);
    if (devStatus[i] == 0) {
      // turn on the DMP, now that it's ready
      Serial.println(F("Enabling DMP..."));
      mpu[i].setDMPEnabled(true);
      mpuIntStatus[i] = mpu[i].getIntStatus();
      dmpReady[i] = true;
      packetSize[i] = mpu[i].dmpGetFIFOPacketSize();
      bootSuccess++;
    } else {
      // ERROR!
      // 1 = initial memory load failed
      // 2 = DMP configuration updates failed
      // (if it's going to break, usually the code will be 1)
      Serial.print(F("DMP Initialization failed (code "));
      Serial.print(devStatus[i]);
      Serial.println(F(")"));
    }
  }
  //————————————————————————————Test the MPU 6050 is connected successfully————————————————————————————//
#ifdef OUTPUT_SERIALR
  Serial.print(F("["));
  for (int i = 0; i < mpuNum; ++i)
  {
    Serial.print("\tP-");
    Serial.print(i);
  }
  Serial.println(F("\t]"));

  Serial.print(F("[\t"));
  for (int i = 0; i < mpuNum; ++i)
  {
    if (devStatus[i] == 0) {
      Serial.print(" o\t");
    } else {
      Serial.print(" x\t");
    }
  }
  Serial.println(F("]"));

  if (bootSuccess == mpuNum) {
    Serial.println(F("Congratulations!\tAll of the MPU6050 chips are successfully connected and work well"));
  } else {
    Serial.println(String("Initialization error!\t" + String(mpuNum - bootSuccess) + " of the " + mpuNum + " MPU6050 chips are disconnected or broken"));
  }
#endif
  if (bootSuccess == mpuNum) {
    for (int j = 0; j < 6; j++) {
      blinkState = !blinkState;
      digitalWrite(LED_PIN, blinkState);
      delay(60);
    }
  } else {
    while (1) {
      blinkState = !blinkState;
      digitalWrite(LED_PIN, blinkState);
      delay(1000);
    }
  }
  //————————————————————-----————————init the WiFi connection-----————————————————————————————//
#ifndef  OUTPUT_SERIALR
#ifndef SERIAL_TEAPOT
  delay(100);

  Udp.begin(localPort);
  #ifdef WiFiHOTSPOTS
    setupHotspots();
  #else
    WiFiSetup();
  #endif
  setupUdp();
#endif
#endif

  delay(100);

#ifdef SERIAL_START
  Serial.println(F("\nSend any character to begin DMP: "));
  while (Serial.available() && Serial.read()); // empty buffer
  while (!Serial.available());                 // wait for data
  while (Serial.available() && Serial.read()); // empty buffer again
#endif
}



// ================================================================
// ===                    MAIN PROGRAM LOOP                     ===
// ================================================================

void loop() {

//  for (int i = 0; i < mpuNum; ++i)
//  {
//    if (!dmpReady[i]) return;
//    // if programming failed, don't try to do anything
//  }

  for (int i = 0; i < mpuNum; ++i)
  {
#ifndef MUXUSED
    boot(bootPin[i]);
#endif
#ifdef MUXUSED
    setMux(i);
#endif
   //if (!dmpReady[i]) return; 
    mpuIntStatus[i] = mpu[i].getIntStatus();
 
    fifoCount[i] = mpu[i].getFIFOCount();

    // check for overflow (this should never happen unless our code is too inefficient)
    if ((mpuIntStatus[i] & 0x10) || fifoCount[i] == 1024) {
      // reset so we can continue cleanly
      
      mpu[i].resetFIFO();
      //Serial.println(F("FIFO0 overflow!"));
#ifdef OUTPUT_SERIALR
      Serial.print("ypr");
      Serial.print(i);
      Serial.print("\tFIFO\tover\tflow");
#endif
      // otherwise, check for DMP data ready interrupt (this should happen frequently)
    } else if (mpuIntStatus[i] & 0x02) {
      // wait for correct available data length, should be a VERY short wait

      while (fifoCount[i] < packetSize[i]) fifoCount[i] = mpu[i].getFIFOCount();

      // read a packet from FIFO
      mpu[i].getFIFOBytes(fifoBuffer[i], packetSize[i]);

      // track FIFO count here in case there is > 1 packet available
      // (this lets us immediately read more without waiting for an interrupt)
      fifoCount[i] -= packetSize[i];

#ifdef OUTPUT_SERIALR
      // display Euler angles in degrees
      mpu[i].dmpGetQuaternion(&q, fifoBuffer[i]);
      mpu[i].dmpGetGravity(&gravity, &q);
      mpu[i].dmpGetYawPitchRoll(ypr, &q, &gravity);
      Serial.print("ypr");
      Serial.print(i);
      Serial.print("\t");
      Serial.print(ypr[0] * 180 / M_PI);
      Serial.print("\t");
      Serial.print(ypr[1] * 180 / M_PI);
      Serial.print("\t");
      Serial.print(ypr[2] * 180 / M_PI);
#endif

#ifdef OUTPUT_PROCESSING
      Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
      // display quaternion values in InvenSense Teapot demo format:
      teapotPacket[2] = fifoBuffer[i][0];
      teapotPacket[3] = fifoBuffer[i][1];
      teapotPacket[4] = fifoBuffer[i][4];
      teapotPacket[5] = fifoBuffer[i][5];
      teapotPacket[6] = fifoBuffer[i][8];
      teapotPacket[7] = fifoBuffer[i][9];
      teapotPacket[8] = fifoBuffer[i][12];
      teapotPacket[9] = fifoBuffer[i][13];
      teapotPacket[12] = i;
      //Serial.write(teapotPacket, 14);
      for (int j = 0; j < 15; j++) {
        Udp.write(teapotPacket[j]);
      }
      teapotPacket[11]++; // packetCount, loops at 0xFF on purpose
      Udp.endPacket();
#endif
#ifdef QUANTANION_OUT
if(i == teapot_num){
      teapotPacket[2] = fifoBuffer[i][0];
      teapotPacket[3] = fifoBuffer[i][1];
      teapotPacket[4] = fifoBuffer[i][4];
      teapotPacket[5] = fifoBuffer[i][5];
      teapotPacket[6] = fifoBuffer[i][8];
      teapotPacket[7] = fifoBuffer[i][9];
      teapotPacket[8] = fifoBuffer[i][12];
      teapotPacket[9] = fifoBuffer[i][13];
            Serial.write(teapotPacket, 14);
            teapotPacket[11]++;
}
#endif
      // blink LED to indicate activity
      blinkState = !blinkState;
      digitalWrite(LED_PIN, blinkState);

    } else {
 #ifdef OUTPUT_SERIALR
      Serial.print("ypr");
      Serial.print(i);
      Serial.print("\t    \t    \t    ");
#endif
    }
 #ifdef OUTPUT_SERIALR
    Serial.print("\t");
  #ifdef TIME_COUNTER
      iCount ++;
  #endif
#endif
  }
#ifdef OUTPUT_SERIALR
  #ifdef TIME_COUNTER
    int millisCounter = millis();
    Serial.println(String("Count:\t" + String(iCount/11) + "\tmillis:\t" + String(millisCounter)));
  #endif
  #ifndef TIME_COUNTER
    Serial.println();
  #endif
#endif
}


// ================================================================
// ===                     setup Hotspots                       ===
// ================================================================


#ifdef WiFiHOTSPOTS
void setupHotspots() {
  WiFi.mode(WIFI_AP);
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.softAPmacAddress(mac);

  String macID = String(mac[WL_MAC_ADDR_LENGTH - 3], HEX) +
                 String(mac[WL_MAC_ADDR_LENGTH - 2], HEX) +
                 String(mac[WL_MAC_ADDR_LENGTH - 1], HEX);
  macID.toUpperCase();

  String AP_NameString = "MPU6050" + macID;

  char AP_NameChar[AP_NameString.length() + 1];
  memset(AP_NameChar, AP_NameString.length() + 1, 0);

  for (int i = 0; i < AP_NameString.length(); i++)
    AP_NameChar[i] = AP_NameString.charAt(i);

  WiFi.softAP(AP_NameChar, WiFiAPPSW);

  Serial.println();
  Serial.print ( "IP address: " );
  Serial.println ( WiFi.softAPIP() );
}
#endif





// ================================================================
// ===                       setup MUX                          ===
// ================================================================
#ifdef MUXUSED
void setupMux() {
  pinMode(s0, OUTPUT);
  pinMode(s1, OUTPUT);
  pinMode(s2, OUTPUT);
  pinMode(s3, OUTPUT);
  pinMode(En, OUTPUT);

  digitalWrite(s0, LOW);
  digitalWrite(s1, LOW);
  digitalWrite(s2, LOW);
  digitalWrite(s3, LOW);
  digitalWrite(En, LOW);
}
#endif

