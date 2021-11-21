#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <PubSubClient.h>
#include <ESP8266HTTPClient.h>

#include <Adafruit_GFX.h>       
#include <Adafruit_ST7735.h> 
#include <SPI.h>

#include <SD.h>

#include <ArduinoJson.h>

// TFT
#define TFT_CS         D8 //15      
#define TFT_RST        D3 //0
#define TFT_DC         D4 //2

// SD
#define SD_CS D2

// BMP
#define BUFFPIXEL 20

// SENSOR
#define VIB_PIN D1

// WiFi
const char *ssid = "MYWIFI"; 
const char *password = "*********";

// MQTT Broker
const char *mqtt_broker = "******";
const char *topic = "ads";
const char *mqtt_username = "******";
const char *mqtt_password = "******";
const int mqtt_port = 15686;

// Data
String json_payload = "";
String url = "";
String type = "";
String title = "";
String author = "";
String creation_date = "";
char *image_name = "img.bmp";

// Sensor
int senseVib;
unsigned long lastDetection = 0;
unsigned long debounceTime = 2000;
unsigned long shakeCount = 0;
unsigned long shakeTH = 10000;

uint16_t identifier;

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
ESP8266WiFiMulti WiFiMulti;
WiFiClient espClientMqtt;
WiFiClient espClientWS;
PubSubClient client(espClientMqtt);
DynamicJsonDocument content(1024);
HTTPClient http;
File image_file;

void setup() { 
  Serial.begin(9600);
  Serial.println("LOADING");
  
  tft.initR(INITR_GREENTAB);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);
  
  print_msg("LOADING");
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(ssid, password);
  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(500);
  }
  print_msg("WIFI OK");
  delay(500);

  if (!SD.begin(SD_CS)) {
    print_msg("SD CARD FAILED");
  }
  print_msg("SD CARD OK");
  delay(500);

  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);
  while (!client.connected()) {
    String client_id = "esp8266-client-";
    client_id += String(WiFi.macAddress());
    if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      print_msg("MQTT BROKER OK");
    } else {
      print_msg("MQTT BROKER FAILED");
      delay(2000);
    }
  }
  client.subscribe(topic);

  pinMode(VIB_PIN, INPUT);
}

void loop() {
  client.loop();

  senseVib = digitalRead(VIB_PIN);
  
  if (senseVib == 1) {
    
    if(millis() - lastDetection > debounceTime){
      shakeCount = 0;
      lastDetection = millis();
    } else {
      shakeCount++;
    }

    if (shakeCount > shakeTH) {
      Serial.println("SHAKING");
      shakeCount = 0;
      client.publish(topic, "{\"type\": \"COMMAND\", \"value\": \"NEXT\"}");
    }
  }
  
}

void callback(char *topic, byte *payload, unsigned int length) {   
  json_payload = "";
  for (int i = 0; i < length; i++) {
    json_payload.concat((char) payload[i]);
  }

  deserializeJson(content, json_payload);
  type = (String)content["type"];
  if (type == "IMAGE") {
    print_msg("LOADING IMAGE");
    url = (String)content["url"];
    author = (String)content["author"];
    title = (String)content["title"];
    creation_date = (String)content["creation_date"];
  
    save_image();
    print_info();
  
    delay(1000);
    tft.fillScreen(ST77XX_BLACK);
    bmpDraw(image_name, 0, 0);
  }
}

void save_image() {
  boolean image_saved = false;
  if (http.begin(espClientWS, url)) {
    SD.remove(image_name);
    image_file = SD.open(image_name, FILE_WRITE);
    
    int httpCode = http.GET();
    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        int len = http.getSize();
        uint8_t buff[128] = { 0 };
        WiFiClient * stream = http.getStreamPtr();
        while(http.connected() && (len > 0 || len == -1)) {          
          size_t size = stream->available();
          
          if(size) {
            int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
            image_file.write(buff, c);

            if(len > 0) {
              len -= c;
            }
          }
          delay(1);
        }
        image_saved = true;        
      }
    } else {
      print_msg("DOWNLOAD FAILED");
    }
    http.end();
    image_file.close();
  } else {
    print_msg("CONNECTION FAILED");
  }
  delay(1000);
  if (image_saved) {
    print_msg("IMAGE OK");
  } else {
    print_msg("IMAGE FAILED");
  }
}

void print_info() {
  tft.fillScreen(ST77XX_BLACK);  
  tft.setTextWrap(true);
  
  tft.fillRect(0, 3 , 68, 10, ST77XX_WHITE); 
  tft.setCursor(7, 5); 
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_BLACK);
  tft.print("title");

  tft.setCursor(5, 25);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_BLUE);
  tft.println(title);

  tft.fillRect(0, 83 , 68, 10, ST77XX_WHITE); 
  tft.setCursor(7, 85); 
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_BLACK);
  tft.print("author");

  tft.setCursor(5, 105);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_BLUE);
  tft.println(author);
}

void print_msg(char *msg) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(5, 5);    //Horiz/Vertic
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.print(msg);
}

void bmpDraw(char *filename, uint8_t x, uint16_t y) {
 
  File     bmpFile;
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t  sdbuffer[3*BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col;
  uint8_t  r, g, b;
  uint32_t pos = 0, startTime = millis();
 
  if((x >= tft.width()) || (y >= tft.height())) return;
 
  Serial.println();
  Serial.print(F("Loading image '"));
  Serial.print(filename);
  Serial.println('\'');
 
  // Open requested file on SD card
  if ((bmpFile = SD.open(filename)) == NULL) {
    Serial.print(F("File not found"));
    return;
  }
 
  // Parse BMP header
  if(read16(bmpFile) == 0x4D42) { // BMP signature
    Serial.print(F("File size: ")); Serial.println(read32(bmpFile));
    (void)read32(bmpFile); // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    Serial.print(F("Image Offset: ")); Serial.println(bmpImageoffset, DEC);
    // Read DIB header
    Serial.print(F("Header size: ")); Serial.println(read32(bmpFile));
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if(read16(bmpFile) == 1) { // # planes -- must be '1'
      bmpDepth = read16(bmpFile); // bits per pixel
      Serial.print(F("Bit Depth: ")); Serial.println(bmpDepth);
      if((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed
 
        goodBmp = true; // Supported BMP format -- proceed!
        Serial.print(F("Image size: "));
        Serial.print(bmpWidth);
        Serial.print('x');
        Serial.println(bmpHeight);
 
        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;
 
        // If bmpHeight is negative, image is in top-down order.
        // This is not canon but has been observed in the wild.
        if(bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip      = false;
        }
 
        // Crop area to be loaded
        w = bmpWidth;
        h = bmpHeight;
        if((x+w-1) >= tft.width())  w = tft.width()  - x;
        if((y+h-1) >= tft.height()) h = tft.height() - y;
 
        // Set TFT address window to clipped image bounds
        tft.startWrite();
        tft.setAddrWindow(x, y, w, h);
 
        for (row=0; row<h; row++) { // For each scanline...
 
          // Seek to start of scan line.  It might seem labor-
          // intensive to be doing this on every line, but this
          // method covers a lot of gritty details like cropping
          // and scanline padding.  Also, the seek only takes
          // place if the file position actually needs to change
          // (avoids a lot of cluster math in SD library).
          if(flip) // Bitmap is stored bottom-to-top order (normal BMP)
            pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else     // Bitmap is stored top-to-bottom
            pos = bmpImageoffset + row * rowSize;
          if(bmpFile.position() != pos) { // Need seek?
            tft.endWrite();
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer); // Force buffer reload
          }
 
          for (col=0; col<w; col++) { // For each pixel...
            // Time to read more pixel data?
            if (buffidx >= sizeof(sdbuffer)) { // Indeed
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0; // Set index to beginning
              tft.startWrite();
            }
 
            // Convert pixel from BMP to TFT format, push to display
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];
            tft.pushColor(tft.color565(r,g,b));
          } // end pixel
        } // end scanline
        tft.endWrite();
        Serial.print(F("Loaded in "));
        Serial.print(millis() - startTime);
        Serial.println(" ms");
      } // end goodBmp
    }
  }
 
  bmpFile.close();
  if(!goodBmp) Serial.println(F("BMP format not recognized."));
}
 
 
// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.
 
uint16_t read16(File f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}
 
uint32_t read32(File f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}
