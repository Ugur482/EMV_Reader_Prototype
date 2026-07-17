#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>

#define PN532_SCK  (D7)
#define PN532_MOSI (D5)
#define PN532_SS   (D1)
#define PN532_MISO (D6)
Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);

#define DEBUG_MODE 1   // set to 0 to compile out ALL debug prints entirely

#if DEBUG_MODE
  #define dbgPrint(...)   Serial.print(__VA_ARGS__)
  #define dbgPrintln(...) Serial.println(__VA_ARGS__)
#else
  #define dbgPrint(...)
  #define dbgPrintln(...)
#endif

#define BuiltIn_LED D4  //Visualization component for the PAN chechk 

#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];

enum { NONE, ISO14443A, EMV };
int lastType = NONE;

int PAN_ID = 0x5A;
int PAN_lenght;
int PAN_start_index;

int ExpDateID1 = 0x5F;
int ExpDateID2 = 0x24;
int ExpDateLenght;
int ExpDateStartIndex;

String PANasString;
String StoredPAN = "ENTER YOUR DEVICES PAN HERE";
String ExpDateAsString;
String PANandExpDate;

String UIDasString;
String SecretAsString;
String UIDandSecret;

bool SentMessage = false;

void setup() {
  //Initialize serial
  Serial.begin(9600);

  pinMode(BuiltIn_LED, OUTPUT);
  digitalWrite(BuiltIn_LED, HIGH); //Built in LED is active LOW so set high to turn it off.

  nfc.begin();

  //check if we can communicate to the PN532 board
  uint32_t versiondata = nfc.getFirmwareVersion();
  dbgPrintln(versiondata);
  if (! versiondata) {
    dbgPrintln("Didn't find PN532 board");
    while (1); // halt
  }

  nfc.setPassiveActivationRetries(1); //else, it will endlessly try to retrieve the tag

  nfc.SAMConfig(); //configure board to read NFC stuff
}

void loop() {

  delay(100);
  
  uint8_t uid[7];  // Buffer to store the returned UID
  uint8_t uidLength; // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
                 
  uint8_t response[255]; // Buffer to store the returned data
  uint8_t responseLength=sizeof(response); // Length of the response buffer  

  uint8_t apduOpenPay2Sys[] = {0x00, /* CLA (Class byte) */
                               0xA4, /* INS (Instruction byte; 0xA4:Select Command; 0xB2:Read Record Command */
                               0x04, /* P1  (Parameter 1 byte; The value and meaning depends on the instruction code [INS]) */ 
                               0x00, /* P2  (Parameter 2 byte; The value and meaning depends on the instruction code [INS]) */ 
                               0x0e, /* Lc  (Number of data bytes send to the card) */
                               /* The Data we send is just 2PAY.SYS.DDF01 ASCII encoded */                      
                               0x32, 0x50, 0x41, 0x59, 0x2e, 0x53, 0x59, 0x53, 0x2e, 0x44, 0x44, 0x46, 0x30, 0x31, /* Data  */
                               0x00  /* Le  (Number of data bytes expected in the response. If Le is 0x00, at maximum 256 bytes are expected) */ };  
  
  uint8_t apduOpenAid[] =     {0x00, /* CLA */
                               0xA4, /* INS */
                               0x04, /* P1  */ 
                               0x00, /* P2  */ 
                               0x07, /* Lc  */
                               /* AID for Master Card */
                               0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10, /* Data */
                               0x00  /* Le  */ };
                               
  uint8_t apduGetPdol[] =     {0x80, /* CLA */
                               0xA8, /* INS */
                               0x00, /* P1  */ 
                               0x00, /* P2  */ 
                               /* for Master Card */
                               0x02, /* Lc  */
                               0x83, 0x00, /* Data  */
                               /* for Visa it might be the following... */
                               //0x0D, /* Lc  */
                               //0x83, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Data  */                               
                               0x00 /* Le  */ };

                              //Maybee only valid for Master Cards in Apple Pay
  uint8_t apduGetPanExpdate[]={0x00, /* CLA */
                               0xB2, /* INS */
                               0x01, /* P1  */
                               0x0C, /* P2  */ 
                               0x00  /* Le  */};
  if(lastType==NONE){

    //Function returns true if any NFC capable object was detected by the reader
    if(nfc.inListPassiveTarget()){
      
      // Opens the Master Card AID (Application Identifier), if function returns false maybe a ISO14443A card was placed, checked in the else if
      if(nfc.inDataExchange(apduOpenAid, sizeof(apduOpenAid), response, &responseLength)){
        
        // setting last type, so reading the card is just done once
        lastType = EMV;

        //printing out what was returned by the card
        dbgPrintln("EMV placed");
        dbgPrintln("AID return:");
        nfc.PrintHexChar(response, responseLength);
        dbgPrintln();

        /*
        // if you don't have a Master Card you need to access the 2Pay.Sys file on the card to get the AID
        // put the output into the TLV decoder and get the AID of your card https://emvlab.org/tlvutils/
        // put the AID into the Data section of the apduOpenAid[] buffer and don't forget to edit the Lc byte if the length of the AID is NOT 7 bytes
        // after you got the AID disable the code again
        nfc.inDataExchange(apduOpenPay2Sys, sizeof(apduOpenPay2Sys), response, &responseLength);
        
        //printing out what was returnd by the card
        dbgPrintln("2Pay.Sys return:");
        nfc.PrintHexChar(response, responseLength);
        dbgPrintln();
        */

        /*
        //if the PAN and expiration date can't be read or do not look right, the application address is maybe wrong
        //the list of available applications can be accessed by the following code
        //if you are using VISA you may have to edit the apduGetPdol[] buffer and enable the VISA part
        //from the response you can calculate a new APDU for the apduGetPanExpdate[] buffer (use the TLV decoder to make sense of the data)
        //visit http://werner.rothschopf.net/201703_arduino_esp8266_nfc.htm for the how to
        nfc.inDataExchange(apduGetPdol, sizeof(apduGetPdol), response, &responseLength);
        
        //printing out what was returnd by the card
        dbgPrintln("PDOL return:");
        nfc.PrintHexChar(response, responseLength);
        dbgPrintln();
        */
             
        //reading the application data which is containing the PAN (Credit card number) and the expiration date
        nfc.inDataExchange(apduGetPanExpdate, sizeof(apduGetPanExpdate), response, &responseLength);
        
        dbgPrintln("PanExpdate return:");
        nfc.PrintHexChar(response, responseLength);
        dbgPrintln();

        //we try to find the position of the PAN and it's length in the response (the PAN is identified by 0x5A and the next byte is the length)
        for (unsigned int i = 0; i < sizeof(response); i++) {
          if (response[i] == PAN_ID) {
            PAN_lenght = response[i+1];
            PAN_start_index = i+2;
            break;
          }
        }       

        //we will write the PAN into a string
        for (int i = PAN_start_index; i < (PAN_start_index + PAN_lenght) ; i++) {
          if (response[i] < 10) {
            PANasString.concat("0");
            PANasString.concat(String(response[i],HEX));
          }
          else {
            PANasString.concat(String(response[i],HEX));
          }          
        }
        //print out the PAN
        dbgPrintln("PAN: "+PANasString);
        dbgPrintln();   

        if (PANasString == StoredPAN)
        {
          digitalWrite(BuiltIn_LED, LOW);
          delay(300);
        }

        //we try to find the position of the expiration and it's length in the response (the expiration date is identified by 0x5F 0x24 and the next byte is the length)
        for (unsigned int i = 0; i < sizeof(response); i++) {
          if (response[i] == ExpDateID1 and response[i+1] == ExpDateID2) {
            ExpDateLenght = response[i+2];
            ExpDateStartIndex = i+3;
            break;
          }
        }        

        //we will write the expiration date into a string
        for (int i = ExpDateStartIndex; i < (ExpDateStartIndex + ExpDateLenght) ; i++) {
          if (response[i] < 10) {
            ExpDateAsString.concat("0");
             ExpDateAsString.concat(String(response[i],HEX));
          }
          else {
            ExpDateAsString.concat(String(response[i],HEX));
          }        
        }
        //print out the expiration date
        dbgPrintln("ExpDate "+ExpDateAsString);
        dbgPrintln();

        //combining the PAN and expiration date so we can send it as one message via MQTT
        PANandExpDate = PANasString + " " + ExpDateAsString;

        //deleting the data, maybe just paranoia...
        PANasString = "";
        ExpDateAsString = "";

        if (PANasString == StoredPAN)
        {
          digitalWrite(BuiltIn_LED, LOW);
          delay(100);
          digitalWrite(BuiltIn_LED, HIGH);
          delay(100);
        }
        else
          digitalWrite(BuiltIn_LED, HIGH);

      } 
      //check if we have a ISO14443A card and can read the UID
      else if(nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength)){

        // setting last type, so reading the card is just done once
        lastType = ISO14443A;


        //printing out what UID returnd by the card and writing it into a string
        dbgPrintln("ISO14443A placed");   
        dbgPrint("UID Length: ");dbgPrint(uidLength, DEC);dbgPrintln(" bytes");
        dbgPrint("UID Value: ");
        for (uint8_t i=0; i < uidLength; i++) 
        {
          dbgPrint(" 0x");dbgPrint(uid[i], HEX);          
          UIDasString.concat(String(uid[i],HEX));
        }
        dbgPrintln();

        // i have stored a "secret" on the NFC tags which will be read here and saved to a string
        int8_t success;
        uint8_t data[32];
        for (uint8_t i = 7; i < 15; i++) {
          success = nfc.ntag2xx_ReadPage(i, data);
          if (success) {
            for (int k = 0; k < 4; k++) {
            SecretAsString.concat((char)data[k]);
            }
          }
        }
        dbgPrint("Secret: "+SecretAsString);

        //combining the UID and secret so we can send it as one message via MQTT
        UIDandSecret = UIDasString + " " + SecretAsString;

        //deleting the data, maybe just paranoia...
        UIDasString = "";
        SecretAsString = ""; 
      }
    }
  }
   
  else {
    //EMV REMOVED?
    if(lastType==EMV){

      //publishing the PAN and expiration date via MQTT
      if (SentMessage == false) {
        SentMessage = true;

        PANandExpDate.toCharArray(msg,MSG_BUFFER_SIZE);
        dbgPrint("Publish message: ");
        dbgPrintln(msg);
        //MQTT code here      
      }

      //when EMV was removed, reset some stuff          
      if(!nfc.inDataExchange(apduGetPanExpdate, sizeof(apduGetPanExpdate), response, &responseLength)){
        dbgPrintln("EMV Removed");
        lastType = NONE;
        SentMessage = false;
        PANandExpDate = "";
      }
    }
    //ISO14443A REMOVED?
    else if(lastType==ISO14443A){

      //publishing the UID and Secret via MQTT
      if (SentMessage == false) {
        SentMessage = true;

        UIDandSecret.toCharArray(msg,MSG_BUFFER_SIZE);
        dbgPrintln("");
        dbgPrint("Publish message: ");
        dbgPrintln(msg);   
        //MQTT code here 
      }

      //when ISO14443A was removed, reset some stuff 
      if(!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, &uid[0], &uidLength)){
        dbgPrintln("ISO14443A removed");
          lastType = NONE;
          SentMessage = false;
          UIDandSecret = "";
      }
    }
  }
 }