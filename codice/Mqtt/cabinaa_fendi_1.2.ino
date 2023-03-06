#include "Adafruit_VL53L0X.h"
#include <Ethernet.h>
#include <MQTT.h>
#include <SPI.h>

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(192, 168, 1, 10);

#define SENSOR_UPDATE_INTERVALL 90

unsigned long pre = 0 ;
unsigned long dif = 0 ;


String dressTagValue ="";
String misura1 = "9999"; //inizializza stato sensorA
String misura2 = "9999"; //inizializza stato sensorB
int sensorValueA = 9999;
int sensorValueB = 9999;
unsigned long timeSensorA = 0;
unsigned long timeSensorB = 0;
unsigned long timeLastTag = 0;

int count = 0;
bool personDetected = false;
#define DISTANCE_THRESHOLD 1200
unsigned long lastNotify = 0;
unsigned long TRIGGER_DELAY_MS = 5;

// address we will assign if dual sensor is present
#define LOX1_ADDRESS 0x30
#define LOX2_ADDRESS 0x31

// set the pins to shutdown
#define SHT_LOX1 3
#define SHT_LOX2 2

#define STX   2
#define ETX   3

#define MAX_LEN_RX_RFID 30


String RFIDTagValue =""; // variabile globale che include il valore dell'ultimo tag letto e dei millis

char RxRFID_Buffer[MAX_LEN_RX_RFID];
int iRXBuffer = 0;


// objects for the vl53l0x
Adafruit_VL53L0X lox1 = Adafruit_VL53L0X();
Adafruit_VL53L0X lox2 = Adafruit_VL53L0X();

// this holds the measurement
VL53L0X_RangingMeasurementData_t measure1;
VL53L0X_RangingMeasurementData_t measure2;


bool verbose = false;
EthernetClient net;
MQTTClient client;

/*
    Reset all sensors by setting all of their XSHUT pins low for delay(10), then set all XSHUT high to bring out of reset
    Keep sensor #1 awake by keeping XSHUT pin high
    Put all other sensors into shutdown by pulling XSHUT pins low
    Initialize sensor #1 with lox.begin(new_i2c_address) Pick any number but 0x29 and it must be under 0x7F. Going with 0x30 to 0x3F is probably OK.
    Keep sensor #1 awake, and now bring sensor #2 out of reset by setting its XSHUT pin high.
    Initialize sensor #2 with lox.begin(new_i2c_address) Pick any number but 0x29 and whatever you set the first sensor to
*/
void setID() {
  // all reset
  digitalWrite(SHT_LOX1, LOW);
  digitalWrite(SHT_LOX2, LOW);
  delay(5);
  // all unreset
  digitalWrite(SHT_LOX1, HIGH);
  digitalWrite(SHT_LOX2, HIGH);
  delay(5);

  // activating LOX1 and resetting LOX2
  digitalWrite(SHT_LOX1, HIGH);
  digitalWrite(SHT_LOX2, LOW);

  // initing LOX1
  if (!lox1.begin(LOX1_ADDRESS)) {
    if(verbose)Serial.println(F("Failed to boot first VL53L0X"));
    //while(1);
  }
  delay(5);

  // activating LOX2
  digitalWrite(SHT_LOX2, HIGH);
  delay(5);

  //initing LOX2
  if (!lox2.begin(LOX2_ADDRESS)) {
    if(verbose)Serial.println(F("Failed to boot second VL53L0X"));
    //while(1);
  }
}

void read_dual_sensors() {

  lox1.rangingTest(&measure1, false); // pass in 'true' to get debug data printout!
  lox2.rangingTest(&measure2, false); // pass in 'true' to get debug data printout!

  // print sensor one reading
  if(verbose)Serial.print(F("1: "));
  if (measure1.RangeStatus != 4 ) {    // if not out of range
    sensorValueA = measure1.RangeMilliMeter; // set dinstance triggered in millimeters
    if (sensorValueA == 0 ) {
      sensorValueA = 9999;  // imposto la distanza a "9999" quando il sensore restituisce la lettura 0
    }
    if (sensorValueA < DISTANCE_THRESHOLD) {
      misura1 = String(sensorValueA);
    } else {
      misura1 = "9999";

    }
    if(verbose)Serial.print(misura1);
  } else {
    sensorValueA = 9999;
    misura1 = "9999";
    misura1 = String(sensorValueA); // Indica comunque i millis anche se vaolore 9999
    if(verbose)Serial.print(F("9999"));
  }

  if(verbose)Serial.print(F(" "));


  // print sensor two reading
  if(verbose)Serial.print(F("2: "));
  if (measure2.RangeStatus != 4 ) {    // if not out of range
    sensorValueB = measure2.RangeMilliMeter; // set dinstance triggered in millimeters
    if (sensorValueB == 0 ) {
      sensorValueB = 9999;  // imposto la distanza a "9999" quando il sensore restituisce la lettura 0
    }
    if (sensorValueB < DISTANCE_THRESHOLD) {
      misura2 = String(sensorValueB);
    } else {
      misura2 = "9999";

    }
    if(verbose)Serial.print(misura2);
  } else {
    sensorValueB = 9999;
    misura2 = "9999";
    misura2 = String(sensorValueB); // Indica comunque i millis anche se vaolore 9999
    if(verbose)Serial.print(F("9999"));
  }


  if(verbose)Serial.println();
  String out = misura1 + "x" + misura2 + ";" + String(millis());
  //Serial.println("**** AB > " + out);
  client.publish("/AB", out);
}

void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);

  // wait until serial port opens for native USB devices
  //  while (! Serial) { delay(1); }
  while (! Serial1) {
    delay(1);
  }
  /*
  while (! Serial) {
    delay(1);
  }
  */
  pinMode(SHT_LOX1, OUTPUT);
  pinMode(SHT_LOX2, OUTPUT);

  if(verbose)Serial.println(F("Shutdown pins inited..."));

  digitalWrite(SHT_LOX1, LOW);
  digitalWrite(SHT_LOX2, LOW);

  if(verbose)Serial.println(F("Both in reset mode...(pins are low)"));


  if(verbose)Serial.println(F("Starting..."));
  setID();

  if(verbose)Serial.println("Ethernet WebServer Example");

  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);

  // Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    if(verbose)Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
    while (true) {
      delay(1); // do nothing, no point running without Ethernet hardware
    }
  }
  if (Ethernet.linkStatus() == LinkOFF) {
    if(verbose)Serial.println("Ethernet cable is not connected.");
  // Initialize Ethernet libary
  //if (!Ethernet.begin(mac)) {
  //  if(verbose)Serial.println(F("Failed to initialize Ethernet library"));
  //  return;
  }

  
  client.begin("192.168.1.11", net);
  // client.onMessage(messageReceived);
  connect();
  
  if(verbose)Serial.println("Done");
}

void connect() {
  if(verbose)Serial.print("\nConnecting...");
  while (!client.connect("arduino", "public", "public")) {
    if(verbose)Serial.print(".");
    delay(1000);
  }
  if(verbose)Serial.println("\nconnected!");
  client.subscribe("/hello");
  // client.unsubscribe("/hello");
}
//
// Ricezione da ANTENNA SERIALE-1 Record RFID
//  <STX>["4000000000"]<CR> <LF> <ETX>
//
bool RFID_RX_Message()
{
  char rx;
  bool Cret = false;

  if (Serial1.available() > 0)
  {
    rx = Serial1.read();
    switch (rx)
    {
      case STX:
        iRXBuffer = 0;
        Cret = false;
        break;

      case ETX:
        Cret = true;
        break;

      default:
        break;
    }

    RxRFID_Buffer[iRXBuffer++] = rx;

    if (Cret )
      {
            // Concatena il codice del tag letto con il vaolore dei millis attuale, per consentire di far valutare se la lettura indicata nel json è recente o obsoleta
        RFIDTagValue = RxRFID_Buffer; //BL
        RFIDTagValue = RFIDTagValue + ";" + String(millis()); //BL
        //if(verbose)Serial.print(RFIDTagValue); //BL
  
        RxRFID_Buffer[iRXBuffer] = '\0';      // terminatore
      }

    if (iRXBuffer > MAX_LEN_RX_RFID)
      iRXBuffer = 0;
  }
 
  return Cret;
}


void loop() {
    static unsigned long previousMillis = 0;

    client.loop();

  if (!client.connected()) {
    connect();
    delay(1000);
  }
      // gestione codice ricevuto in RxRFID_Buffer verso il BT formato <STX>["4000000000"]<CR> <LF> <ETX>
      if (RFID_RX_Message())
      {
        if(verbose)Serial.print(RFIDTagValue); //BL
        //if(verbose)Serial.print("RX RFID : ");
        //if(verbose)Serial.print((char*) RxRFID_Buffer);
        client.publish("/TAG", RFIDTagValue);
        if(verbose)Serial.println("**** TAG > " + RFIDTagValue);
      }
      unsigned long currentMillis = millis();

      if ( currentMillis - previousMillis > SENSOR_UPDATE_INTERVALL )
      {
        previousMillis = currentMillis;
        //       updateValues();


        //dressTagValue = RxRFID_Buffer;
        dressTagValue = RFIDTagValue; //BL Valorizza la variabile per il json con il valore del tag e dei millis quando viene letto 

        //        misura1 = String(measure1.RangeMilliMeter);
        read_dual_sensors();
        //statusValuesChar.writeValue( misura1);  //Notifica stato PIR : 0 = no presence 1 = Presence
        //        statusValuesChar.writeValue( Status_String );  //Notifca stato PIR : 0 = no presence 1 = Presence
        //       misura2 = String(measure2.RangeMilliMeter);
        //statusValuesChar2.writeValue( misura2);  //Notifica stato PIR : 0 = no presence 1 = Presence
    }
}
