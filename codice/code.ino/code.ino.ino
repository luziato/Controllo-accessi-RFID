
#include "Adafruit_VL53L0X.h"
#include <ArduinoBLE.h>

#define SENSOR_UPDATE_INTERVALL 1000  //notify change to the app every 5 seconds

int aleatorio = 0; //inizializza stato abito
String misura1 = "0"; //inizializza stato sensorA
String misura2 = "0"; //inizializza stato sensorB
String Level_String;  // simulazione codice RFID
int sensorValueA = 0;
int sensorValueB = 0;
unsigned long timeSensorA = 0;
unsigned long timeSensorB = 0;
int count = 0;
int personDetected = false;
#define DISTANCE_THRESHOLD 700 // Distanza sotto la quale viene attivato il sensore ed indicato il passaggio della persona.
unsigned long lastNotify = 0;
unsigned long TRIGGER_DELAY_MS = 100;

//BLE.setAdvertisedService( Antenna_RFID );  Name of service
BLEService Antenna_RFID( "1101" ); //Declare service for Antenna_RFID
BLEStringCharacteristic dressValuesChar( "2101", BLERead | BLENotify, 100 ); // Dress value: 0 NO Tags; 1-6 = tag value
BLEStringCharacteristic statusValuesChar( "3101", BLERead | BLENotify, 100 ); // sensorA status: 0 = NOBODY; 1 = PEOPLE
BLEStringCharacteristic statusValuesChar2( "4101", BLERead | BLENotify, 100 ); // sensorB status: 0 = NOBODY; 1 = PEOPLE
BLEStringCharacteristic roomPeopleValuesChar( "5101", BLERead | BLENotify, 100 ); // Person room direction : 0 = Leave Room; 1 = Enter in room

// address we will assign if dual sensor is present
#define LOX1_ADDRESS 0x30
#define LOX2_ADDRESS 0x31

// set the pins to shutdown
#define SHT_LOX1 3
#define SHT_LOX2 2

#define STX   2
#define ETX   3

#define MAX_LEN_RX_RFID 30


char RxRFID_Buffer[MAX_LEN_RX_RFID];
int iRXBuffer = 0;


// objects for the vl53l0x
Adafruit_VL53L0X lox1 = Adafruit_VL53L0X();
Adafruit_VL53L0X lox2 = Adafruit_VL53L0X();

// this holds the measurement
VL53L0X_RangingMeasurementData_t measure1;
VL53L0X_RangingMeasurementData_t measure2;

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
  delay(10);
  // all unreset
  digitalWrite(SHT_LOX1, HIGH);
  digitalWrite(SHT_LOX2, HIGH);
  delay(10);

  // activating LOX1 and resetting LOX2
  digitalWrite(SHT_LOX1, HIGH);
  digitalWrite(SHT_LOX2, LOW);

  // initing LOX1
  if (!lox1.begin(LOX1_ADDRESS)) {
    Serial.println(F("Failed to boot first VL53L0X"));
    //while(1);
  }
  delay(10);

  // activating LOX2
  digitalWrite(SHT_LOX2, HIGH);
  delay(10);

  //initing LOX2
  if (!lox2.begin(LOX2_ADDRESS)) {
    Serial.println(F("Failed to boot second VL53L0X"));
    //while(1);
  }
}

void read_dual_sensors() {

  lox1.rangingTest(&measure1, false); // pass in 'true' to get debug data printout!
  lox2.rangingTest(&measure2, false); // pass in 'true' to get debug data printout!

  // print sensor one reading
  Serial.print(F("1: "));
  if (measure1.RangeStatus != 4) {    // if not out of range
    sensorValueA = measure1.RangeMilliMeter; // set dinstance triggered in millimeters
    if (sensorValueA < DISTANCE_THRESHOLD) {
      misura1 = "A: " + String(sensorValueA) + " " + String(millis());
    } else {
      misura1 = "A: NONE";
    }
    //    Serial.print(sensorValueA);
    Serial.print(misura1);
  } else {
    //    sensorValueA = 20000;
    misura1 = "A: NONE";
    Serial.print(misura1);
  }

  Serial.print(F(" "));


  // print sensor two reading
  Serial.print(F("2: "));
  if (measure2.RangeStatus != 4) {    // if not out of range
    if (measure2.RangeMilliMeter == 0) {
      measure2.RangeMilliMeter = 20000;
    }
    sensorValueB = measure2.RangeMilliMeter; // set dinstance triggered in millimeters
    if (sensorValueB < DISTANCE_THRESHOLD) {
      misura2 = "B: " + String(sensorValueB) + " " + String(millis());
    } else {
      misura2 = "B: NONE";

    }
    //    Serial.print(sensorValueB);
    Serial.print(misura2);
  } else {
    //    sensorValueB = 20000;
    misura2 = "B: NONE";
    Serial.print(F("20000"));
  }


  Serial.println();
}

void setup()
{
  Serial.begin(9600);
  Serial1.begin(9600);

  // wait until serial port opens for native USB devices
  //  while (! Serial) { delay(1); }
  while (! Serial1) {
    delay(1);
  }

  pinMode(SHT_LOX1, OUTPUT);
  pinMode(SHT_LOX2, OUTPUT);

  Serial.println(F("Shutdown pins inited..."));

  digitalWrite(SHT_LOX1, LOW);
  digitalWrite(SHT_LOX2, LOW);

  Serial.println(F("Both in reset mode...(pins are low)"));


  Serial.println(F("Starting..."));
  setID();

  pinMode( LED_BUILTIN, OUTPUT ); //Intializes the built in LED to indicate when a central device has connected

  if ( !BLE.begin() )
  {
    //   Serial.println( "BLE failed" );
    while ( 1 );
  }

  BLE.setLocalName( "Antenna_RFID" );
  BLE.setAdvertisedService( Antenna_RFID );
  Antenna_RFID.addCharacteristic( dressValuesChar ); //Adds the dress value characteristics  0 NO Tags; 1-6 = tag value output from RFID Reader
  Antenna_RFID.addCharacteristic( statusValuesChar ); //Adds the Lidar A Status characteristics
  Antenna_RFID.addCharacteristic( statusValuesChar2 ); //Adds the Lidar B Status characteristics
  Antenna_RFID.addCharacteristic( roomPeopleValuesChar ); //Adds the count of people in room characteristics
  BLE.addService( Antenna_RFID );                                   //Adds the Antenna_RFID service
  BLE.setAppearance(384);                      // BLE_APPEARANCE_Antenna_RFID
  BLE.advertise();                                      //Starts advertising the peripheral device over bluetooth
  // Serial.println( "Waiting for connection.." );
  // PIR setup
  Serial.begin(9600);            //initializa al comunicazione seriale a 9600 bps
  // pinMode(pirPin, INPUT);        //imposta pirPin come INPUT

  // digitalWrite(pirPin, LOW);     //per default, nessun movimento

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

    if (Cret)
      RxRFID_Buffer[iRXBuffer] = '\0';      // terminatore

    if (iRXBuffer > MAX_LEN_RX_RFID)
      iRXBuffer = 0;
  }
  return Cret;
}
void loop()
{
  static unsigned long previousMillis = 0;

  BLEDevice central = BLE.central();                    //Waits for BLE Central device to connect


  if ( central )
  {
    //    Serial.print( "Connected to central: " );
    //    Serial.println( central.address() );
    digitalWrite( LED_BUILTIN, HIGH );                  //Turn on peripheral LED to indicate valid connection with Central Device
    delay(100);
    digitalWrite( LED_BUILTIN, LOW );

    while ( central.connected() )                       //While the Peripheral Device is connected to the Central Device
    {
      // gestione codice ricevuto in RxRFID_Buffer verso il BT formato <STX>["4000000000"]<CR> <LF> <ETX>
      if (RFID_RX_Message())
      {
        Serial.print("RX RFID : ");
        Serial.print((char*) RxRFID_Buffer);
      }

      unsigned long currentMillis = millis();

      if ( currentMillis - previousMillis > SENSOR_UPDATE_INTERVALL )
      {
        previousMillis = currentMillis;
        int aleatorio = random(0, 6); // Crea un numero aleatorio da 0 a 6.
        //       updateValues();
        Level_String = String(aleatorio);
        dressValuesChar.writeValue( RxRFID_Buffer );
        //        misura1 = String(measure1.RangeMilliMeter);
        read_dual_sensors();
        statusValuesChar.writeValue( misura1);  //Notifica stato PIR : 0 = no presence 1 = Presence
        //        statusValuesChar.writeValue( Status_String );  //Notifca stato PIR : 0 = no presence 1 = Presence
        //       misura2 = String(measure2.RangeMilliMeter);
        statusValuesChar2.writeValue( misura2);  //Notifica stato PIR : 0 = no presence 1 = Presence

        //    insert logic here

        if (!personDetected) {
          // Check if person is entering the room
          if (measure1.RangeMilliMeter < DISTANCE_THRESHOLD) {
            if (millis() - lastNotify >= TRIGGER_DELAY_MS) {
                personDetected = personDetected;
                // Trigger action for person entering the room
              lastNotify = millis();
            }
          }
        } else {
          // Check if person is leaving the room
          if (measure2.RangeMilliMeter < DISTANCE_THRESHOLD) {
            if (millis() - lastNotify >= TRIGGER_DELAY_MS) {
              personDetected = -1;
              // Trigger action for person leaving the room
              lastNotify = millis();
            }
          }
        }
        roomPeopleValuesChar.writeValue( String(personDetected));  //Notifica stato presenza: 1=è entrata una persona, -1= è uscita una persona



        // Limit of internal loop


      }
    }
  }

  digitalWrite( LED_BUILTIN, LOW );
  //  Serial.print( "Disconnected from central: " );
  //  Serial.println( central.address() );
}
