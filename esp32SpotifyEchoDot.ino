#include <FS.h>
#include <Arduino.h>
// #include <WiFi.h>
#include "SpotifyClient.h"

#include <SPI.h>
#include "MFRC522.h"

#include <WiFiManager.h>

#include "settings.h"

#ifdef ESP32
  #include <SPIFFS.h>
#endif

#include <ArduinoJson.h> 

#define RST_PIN 22 // Configurable, see typical pin layout above
#define SS_PIN 21  // Configurable, see typical pin layout above

// select which pin will trigger the configuration portal when set to LOW
#define TRIGGER_PIN 0

MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance

byte const BUFFERSiZE = 176;

SpotifyClient spotify = SpotifyClient();

WiFiManager wifiManager;
bool portalRunning = false;

char refreshToken[150] = "AQDcqxZ34YlfsQUggQvHkOJqI3OLnQyYa1KQYFqzfy7C23Tk3NCHtEg909DAL_yhIgLhtUqoOhts0-isbxnb3M8qaC7WoZo_LrSx3vAnkBTSW1w66vWSc6wOmUM0-19CBD4";
char deviceName[10] = "Roman";

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Setup started");

  pinMode(TRIGGER_PIN, INPUT_PULLUP);  

    //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
/*
     //Format File System
  if(SPIFFS.format())
  {
    Serial.println("Datei-System formatiert");
  }
  else
  {
    Serial.println("Datei-System formatiert Error");
  }
  */


    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);

#ifdef ARDUINOJSON_VERSION_MAJOR >= 6
        DynamicJsonDocument json(1024);
        auto deserializeError = deserializeJson(json, buf.get());
        serializeJson(json, Serial);
        if ( ! deserializeError ) {
#else
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
#endif
          Serial.println("\nparsed json");
          strcpy(refreshToken, json["refreshToken"]);
          strcpy(deviceName, json["deviceName"]);
        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

  WiFiManagerParameter custom_field_token("refreshTokenId", "Refresh Token", refreshToken, 150);
  WiFiManagerParameter custom_field_name("deviceNameId", "Device Name", deviceName, 10);



  wifiManager.setSaveConfigCallback(saveConfigCallback);

  wifiManager.addParameter(&custom_field_token);
  wifiManager.addParameter(&custom_field_name);

  wifiManager.setMinimumSignalQuality(10);
  
  if(!wifiManager.autoConnect("MUSIC-BOX")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");


  strcpy(refreshToken, custom_field_token.getValue());
  strcpy(deviceName, custom_field_name.getValue());
  Serial.println("The values in the file are: ");
  Serial.println("refreshToken : " + String(refreshToken));
  Serial.println("deviceName : " + String(deviceName));

    //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
        //read updated parameters


#ifdef ARDUINOJSON_VERSION_MAJOR >= 6
    DynamicJsonDocument json(1024);
#else
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
#endif
    json["refreshToken"] = refreshToken;
    json["deviceName"] = deviceName;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

#ifdef ARDUINOJSON_VERSION_MAJOR >= 6
    serializeJson(json, Serial);
    serializeJson(json, configFile);
#else
    json.printTo(Serial);
    json.printTo(configFile);
#endif
    configFile.close();
    //end save
  }

  Serial.println("local ip:");
  Serial.println(WiFi.localIP());
  Serial.println("Init Spotify");
  spotify.Init(clientId, clientSecret, deviceName, refreshToken);
  Serial.println("Init Spotify done!");
  //connectWifi();

  // Init SPI bus and MFRC522 for NFC reader


  //SPI.begin();        
  //mfrc522.PCD_Init(); 

  // Refresh Spotify Auth token and Deivce ID
  Serial.println("Get Spotify access_token");
  spotify.FetchToken();
  Serial.println("Get Spotify access_token...done!");
  Serial.println("Get Spotify devices");
  spotify.GetDevices();
  Serial.println("Get Spotify devices done !");

  String context_uri = "spotify:playlist:1UWJY6Ql9JsqGaor6ifZe7";
  Serial.println("Play test: spotify:playlist:1UWJY6Ql9JsqGaor6ifZe7");
  playSpotifyUri(context_uri);
}
/*
String getParam(String name){
  //read parameter from server, for customhmtl input
  String value;
  if(wifiManager.server->hasArg(name)) {
    value = wifiManager.server->arg(name);
  }
  return value;
}
*/

void loop()
{
  checkButton();
  /*
  if (mfrc522.PICC_IsNewCardPresent())
  {
    Serial.println("NFC tag present");
    readNFCTag();
  }
  */
}

void checkButton(){
  // is auto timeout portal running
  if(portalRunning){
    wifiManager.process();
  }

  // is configuration portal requested?
  if(digitalRead(TRIGGER_PIN) == LOW) {
    delay(50);
    if( digitalRead(TRIGGER_PIN) == LOW ){
      Serial.println("Button Pressed");
      // still holding button for 3000 ms, reset settings, code not ideaa for production
      delay(3000); // reset delay hold
      if( digitalRead(TRIGGER_PIN) == LOW ){
        if(!portalRunning){
          Serial.println("Button Held, Starting Portal");
          wifiManager.startWebPortal();
          portalRunning = true;
        }
        else{
          Serial.println("Button Pressed, Stopping Portal");
          wifiManager.stopWebPortal();
          portalRunning = false;
        }
      }
    }
  }
}

void readNFCTag()
{
  if (mfrc522.PICC_ReadCardSerial())
  {
    byte dataBuffer[BUFFERSiZE];
    readNFCTagData(dataBuffer);
    mfrc522.PICC_HaltA();

    //hexDump(dataBuffer);
    Serial.print("Read NFC tag: ");
    String context_uri = parseNFCTagData(dataBuffer);
    Serial.println(context_uri);
    playSpotifyUri(context_uri);
  }
}

void playSpotifyUri(String context_uri)
{
  int code = spotify.Play(context_uri);
  switch (code)
  {
    case 404:
    {
      // device id changed, get new one
      spotify.GetDevices();
      spotify.Play(context_uri);
      //spotify.Shuffle();
      break;
    }
    case 401:
    {
      // auth token expired, get new one
      spotify.FetchToken();
      spotify.Play(context_uri);
      //spotify.Shuffle();
      break;
    }
    default:
    {
      // spotify.Shuffle();
      break;
    }
  }
}

bool readNFCTagData(byte *dataBuffer)
{
  MFRC522::StatusCode status;
  byte byteCount;
  byte buffer[18];
  byte x = 0;

  int totalBytesRead = 0;

  // reset the dataBuffer
  for (byte i = 0; i < BUFFERSiZE; i++)
  {
    dataBuffer[i] = 0;
  }

  for (byte page = 0; page < BUFFERSiZE / 4; page += 4)
  {
    // Read pages
    byteCount = sizeof(buffer);
    status = mfrc522.MIFARE_Read(page, buffer, &byteCount);
    if (status == mfrc522.STATUS_OK)
    {
      totalBytesRead += byteCount - 2;

      for (byte i = 0; i < byteCount - 2; i++)
      {
        dataBuffer[x++] = buffer[i]; // add data output buffer
      }
    }
    else
    {
      break;
    }
  }
}

/*
  Parse the Spotify link from the NFC tag data
  The first 28 bytes from the tag is a header info for the tag
  Spotify link starts at position 29


  Parse a link
  open.spotify.com/album/3JfSxDfmwS5OeHPwLSkrfr
  open.spotify.com/playlist/69pYSDt6QWuBMtIWSZ8uQb
  open.spotify.com/artist/53XhwfbYqKCa1cC15pYq2q


  Return a uri
  spotify:album:3JfSxDfmwS5OeHPwLSkrfr
  spotify:playlist:69pYSDt6QWuBMtIWSZ8uQb
  spotify:artist:53XhwfbYqKCa1cC15pYq2q

*/

String parseNFCTagData(byte *dataBuffer)
{
  // first 28 bytes is header info
  // data ends with 0xFE
  //String retVal = "spotify:";
  String retVal = "";
  //for (int i = 28 + 17; i < BUFFERSiZE; i++)
  for (int i = 28; i < BUFFERSiZE; i++)
  {
    if (dataBuffer[i] == 0xFE || dataBuffer[i] == 0x00)
    {
      break;
    }
    if (dataBuffer[i] == '/')
    {
      retVal += ':';
    }
    else
    {
      retVal += (char)dataBuffer[i];
    }
  }
  return retVal;
}

void hexDump(byte *dataBuffer)
{
  Serial.print(F("hexDump: "));
  Serial.println();
  for (byte index = 0; index < BUFFERSiZE; index++)
  {
    if (index % 4 == 0)
    {
      Serial.println();
    }
    Serial.print(dataBuffer[index] < 0x10 ? F(" 0") : F(" "));
    Serial.print(dataBuffer[index], HEX);
  }
}
/*
void connectWifi()
{
  WiFi.begin(ssid, pass);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}
*/
