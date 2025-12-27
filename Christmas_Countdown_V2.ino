// CrowPanel ESP32 V2.2 3.5" LCD – Event Countdown
// Orientation: Vertical (320x480)

#include <WiFi.h>
#include <DNSServer.h>  //https://github.com/esp8266/Arduino/tree/master/libraries/DNSServer
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>  //https://github.com/tzapu/WiFiManager
#include <time.h>
#include <SPI.h>
#include <SD.h>
#include <time.h>
#include <Timezone.h>
#include <TFT_eSPI.h>
#include <math.h>

#define VERSION "V2.2"

// ---------------- DISPLAY ----------------
#define SCREEN_W 320
#define SCREEN_H 480
bool showHeading = true;  // we only need draw the heading once

TFT_eSPI tft = TFT_eSPI();

// ---------------- WIFI + EVENT ----------------
// HOSTNAME for OTA update
#define eventName "Christmas"       // this will be the AP name you set to setup wifi
uint8_t targetMonth = 12;           // month from 1 to 12
uint8_t targetDay = 25;             // Christmas day
#define LEADING_IMAGE "/Tree_2.bmp"    // graphics MUST be in 24bit format 192 x 128 (H x W)
#define DAY_IMAGE "/Christmas.bmp"  // main graphic 300 x 300 (H x W)
uint16_t epochOffset = 28800;       // users time zone offset for the trigger date, in seconds

// ---------------- TIME HELPERS ----------------

//Edit These Lines According To Your Timezone and Daylight Saving Time
//TimeZone Settings Details https://github.com/JChristensen/Timezone
//US Pacific Time Zone
TimeChangeRule usPDT = { "PDT", Second, dowSunday, Mar, 2, -420 };  // 7 hour offset
TimeChangeRule usPST = { "PST", First, dowSunday, Nov, 2, -480 };   // 8 hour offset
Timezone usPT(usPDT, usPST);
//Pointer To The Time Change Rule, Use to Get The TZ Abbrev
TimeChangeRule *tcr;
time_t utc;
bool didTargetCalc = false;  // we only need to calcuate the target once.
time_t targetTimestamp;

// ----------- CLOCK --------------
bool clock24 = false;  // not implemented yet
uint8_t lastSecond = 99;
uint8_t lastMinute;
uint8_t lastHour;
uint8_t lastDay;
uint8_t lastMonth;
uint16_t lastYear, myYear;
uint8_t myHour, myMinute, mySecond, my24Hour, myDay, myMonth, myWeekDay, tempMonth;
bool onTargetDay = false;    // tells us we are ON the trigger day
bool didTargetDraw = false;  // we only draw the image once
bool colonBlink = false;     // we want the colon to blink
const String monthNames[12] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
// ---------------- TIME ----------------
#define NTP_SERVER "pool.ntp.org"
#define NTP_RESYNC_INTERVAL (4 * 3600)
unsigned long lastNtpSync = 0;

static const char ntpServerName[] = "pool.ntp.org";
uint16_t localPort;
uint8_t ntpUpdateFrequency = 123;  // update the time every x minutes
WiFiUDP Udp;

// ---------------- FILES ----------------
#define SD_MOSI 23  // pin numbers for the SDcard on V2.2 CrowPanel
#define SD_MISO 19
#define SD_SCK 18
#define SD_CS 5

int sd_init_flag = 0;

SPIClass SD_SPI;  // for the graphics on the SDcard

File root;
// ---------------- BMP DRAW ----------------

void printDirectory(File dir, int numTabs) {
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}
//
void drawBmp(fs::FS &fs, const char *filename, int16_t x, int16_t y) {

  if ((x >= tft.width()) || (y >= tft.height())) return;

  // Open requested file on SD card
  File bmpFS = fs.open(filename, "r");

  if (!bmpFS) {
    Serial.print("File not found");
    return;
  }

  uint32_t seekOffset;
  uint16_t w, h, row;  //, col;
  uint8_t r, g, b;

  // uint32_t startTime = millis();

  if (read16(bmpFS) == 0x4D42) {
    read32(bmpFS);
    read32(bmpFS);
    seekOffset = read32(bmpFS);
    read32(bmpFS);
    w = read32(bmpFS);
    h = read32(bmpFS);

    if ((read16(bmpFS) == 1) && (read16(bmpFS) == 24) && (read32(bmpFS) == 0)) {
      y += h - 1;

      bool oldSwapBytes = tft.getSwapBytes();
      tft.setSwapBytes(true);
      bmpFS.seek(seekOffset);

      uint16_t padding = (4 - ((w * 3) & 3)) & 3;
      uint8_t lineBuffer[w * 3 + padding];

      for (row = 0; row < h; row++) {

        bmpFS.read(lineBuffer, sizeof(lineBuffer));
        uint8_t *bptr = lineBuffer;
        uint16_t *tptr = (uint16_t *)lineBuffer;
        // Convert 24 to 16-bit colours
        for (uint16_t col = 0; col < w; col++) {
          b = *bptr++;
          g = *bptr++;
          r = *bptr++;
          *tptr++ = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        }

        // Push the pixel row to screen, pushImage will crop the line if needed
        // y is decremented as the BMP image is drawn bottom up
        tft.pushImage(x, y--, w, 1, (uint16_t *)lineBuffer);
      }
      tft.setSwapBytes(oldSwapBytes);
      // Serial.print("Loaded in ");
      // Serial.print(millis() - startTime);
      // Serial.println(" ms");
    } else Serial.println("BMP format not recognized.");
  }
  bmpFS.close();
}

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(fs::File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read();  // LSB
  ((uint8_t *)&result)[1] = f.read();  // MSB
  return result;
}

uint32_t read32(fs::File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read();  // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read();  // MSB
  return result;
}
//
int sd_init() {
  SD_SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
  if (!SD.begin(SD_CS, SD_SPI, 40000000)) {
    Serial.println("Card Mount Failed");
    return 1;
  } else {
    Serial.println("Card Mount Successed");
  }
  // listDir(SD, "/", 0);
  // Serial.println("TF Card init over.");
  sd_init_flag = 1;
  return 0;
}

// ---------------- UI HELPERS ----------------
//
void handleGRID() {
  for (uint32_t myX = 0; myX < SCREEN_W; myX = myX + 32) {
    tft.drawFastVLine(myX, 0, SCREEN_H - 3, TFT_RED);  // Draw a horizontal red line from (10, 20) to (110, 20)
  }
  for (uint32_t myY = 0; myY < SCREEN_H; myY = myY + 32) {
    tft.drawFastHLine(0, myY, SCREEN_W - 3, TFT_RED);  // Draw a horizontal red line from (10, 20) to (110, 20)
  }
}
//
void handleLabels() {
  uint8_t xpos = 100;
  tft.setTextColor(TFT_WHITE, TFT_BLACK);  //
  tft.fillRoundRect(16, 176, SCREEN_W - 32, 64, 10, TFT_BLUE);
  tft.setTextDatum(MC_DATUM);             //  center justified
  tft.setTextColor(TFT_GREEN, TFT_BLUE);  //
  //tft.drawString("Christmas Counter", 160, 208, 4);
  tft.drawString(String(eventName) + " Counter", 160, 208, 4);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);  //
  tft.setTextDatum(BR_DATUM);              // right justified
  tft.drawString("DAYS:", xpos, 288, 4);   // labels
  tft.drawString("HOURS:", xpos, 320, 4);  //
  tft.drawString("MINS:", xpos, 352, 4);   //
  tft.drawString("SECS:", xpos, 384, 4);   // seconds left
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);  //
  tft.drawString(String(VERSION) + " @2025 - Wabbit Wanch Design", 160, 464, 2);
  tft.setTextDatum(TL_DATUM);
  // Serial.println("File_Listing");
  // Serial.println("");
  // printDirectory(root, 0);
  drawBmp(SD, LEADING_IMAGE, 175, 262);  //display image1
}
//
void handleCountDown(uint16_t theDays, uint8_t theHours, uint8_t theMinutes, uint8_t theSeconds) {
  uint8_t xpos = 160;
  char buffer[16];                         // holds the formated time
  tft.setTextColor(TFT_WHITE, TFT_BLACK);  // Leave a 7 segment ghost image, comment out next line!
  tft.setTextDatum(BR_DATUM);              // formatted bottom right
  sprintf(buffer, "   %3u", theDays);      // kill the leading 0 on the hours
  tft.drawString(buffer, xpos, 288, 4);    // labels
  sprintf(buffer, "  %2u", theHours);      // kill the leading 0 on the hours
  tft.drawString(buffer, xpos, 320, 4);    //
  sprintf(buffer, "  %2u", theMinutes);    // kill the leading 0 on the hours
  tft.drawString(buffer, xpos, 352, 4);    //
  sprintf(buffer, "  %2u", theSeconds);    // kill the leading 0 on the hours
  tft.drawString(buffer, xpos, 384, 4);    // seconds left
  tft.setTextDatum(TL_DATUM);
}
//
void handle_ClockDisplay() {
  tmElements_t tm;
  breakTime(now(), tm);
  char buffer[24];  // holds the formated time/day/date
  //
  myHour = hourFormat12(usPT.toLocal(utc, &tcr));
  myDay = day(usPT.toLocal(utc, &tcr));
  my24Hour = hour(usPT.toLocal(utc, &tcr));  // we need the 24 hour clock time
  myMinute = minute();
  mySecond = second();
  myYear = year();
  myMonth = month();
  //
  // now we draw the time and the date
  uint8_t xpos = (SCREEN_W / 2) - 1;
  uint8_t ypos = 96;
  tft.setTextColor(TFT_GREEN, TFT_BLACK);  // Now show the clock time
  tft.setTextDatum(BC_DATUM);
  if (colonBlink == false) {
    sprintf(buffer, " %2u:%02u ", myHour, myMinute);  // kill the leading 0 on the hours
  } else {
    sprintf(buffer, " %2u %02u ", myHour, myMinute);  // kill the leading 0 on the hours
  }
  colonBlink = !colonBlink;               // toggle the value for next pass
  tft.drawString(buffer, xpos, ypos, 7);  // Overwrite the text to clear it
  tft.setTextPadding(0);                  // Reset padding width to none

  if (myDay != lastDay) {  // if the day changes, we update it, day of the week, and day
    String monthStuff;
    tft.setTextDatum(BC_DATUM);
    tft.setTextColor(TFT_BLACK, TFT_BLACK);                          // we're going to print with black to erase the last month, day
    monthStuff = monthNames[lastMonth - 1] + " " + String(lastDay);  // this removes previous month/day
    tft.drawString(monthStuff, 160, 136, 4);                         // display the day of the week
    tft.setTextColor(TFT_WHITE, TFT_BLACK);                          // now we display the current month/day
    monthStuff = monthNames[myMonth - 1] + " " + String(myDay);
    tft.drawString(monthStuff, 160, 136, 4);  // display the day of the week
  }
  //
  if (mySecond == 15) {
    drawWiFiQuality();  // go update the wifi strength graphic once a minute
  }
  // --- now we work on the count down portion of the time display
  // --- target date is defined in the globals
  // if (myMonth != targetMonth && myDay != targetDay) {
  //   onTargetDay = false;
  //
  if (myDay == targetDay && myMonth == targetMonth) {  // okay today is the day...
    if (didTargetDraw == false) {
      drawBmp(SD, DAY_IMAGE, 10, 150);  //display the TRIGGER DAY image 300 x 300, 24bit
      didTargetDraw = true;             // we only draw the image once
      didTargetCalc = false;            // we want to calculate count down after today
    }
  } else {
    if (myMonth == targetMonth && myDay > targetDay) {  // same year, but past the trigger date
      if (didTargetCalc == false) {                     // only need this calc once so we set a flag
        tft.fillRect(1, 150, 319, 302, TFT_BLACK);      // erase the graphic that shows now
        struct tm targetDateInfo;
        targetDateInfo.tm_year = (myYear + 1) - 1900;  // current years since 1900
        targetDateInfo.tm_mon = targetMonth - 1;       // Months since Jan (0-11)
        targetDateInfo.tm_mday = targetDay;
        targetDateInfo.tm_hour = 0;
        targetDateInfo.tm_min = 0;
        targetDateInfo.tm_sec = 0;
        targetDateInfo.tm_isdst = 1;                // +1 in DST, 0 not DST, -1 Info not available
        targetTimestamp = mktime(&targetDateInfo);  // this will return XMAS as GMT epoch
        didTargetCalc = true;                       // set flag so we don't bother with it again
        handleLabels();                             // go draw the text labels on the display (only need to do them once)
      }
    } else {                     // we are currently counting up to the trigger date
      if (myYear != lastYear) {  // we are now in a new year
        didTargetCalc = false;   // so we need to start counting down in the new year
      }
      if (didTargetCalc == false) {  // only need this calc once so we set a flag
        struct tm targetDateInfo;
        targetDateInfo.tm_year = myYear - 1900;   // current years since 1900
        targetDateInfo.tm_mon = targetMonth - 1;  // Months since Jan
        targetDateInfo.tm_mday = targetDay;
        targetDateInfo.tm_hour = 0;
        targetDateInfo.tm_min = 0;
        targetDateInfo.tm_sec = 0;
        targetDateInfo.tm_isdst = 1;                // +1 in DST, 0 not DST, -1 Info not available
        targetTimestamp = mktime(&targetDateInfo);  // this will return XMAS as GMT epoch
        didTargetCalc = true;                       // set flag so we don't bother with it again
        handleLabels();                             // go draw the text labels on the display (only need to do them once)
      }
    }
    uint32_t diff = difftime(targetTimestamp, (now() - epochOffset));  // figures out the seconds remaining ((currentEpoch - 28800))
    if (diff <= 0) diff = 0;
    uint16_t days = diff / 86400;
    diff %= 86400;
    uint8_t hrs = diff / 3600;
    diff %= 3600;
    uint8_t mins = diff / 60;
    uint8_t secs = diff % 60;
    handleCountDown(days, hrs, mins, secs);  // go update the remaining time on the display
  }
  //handleGRID();    // just a grid to find spacing if you need it

  lastSecond = mySecond;  // save it for next time
  lastMinute = myMinute;  // save this for the next pass
  lastHour = my24Hour;    // save for the next pass
  lastDay = myDay;        // save the day...
  lastYear = myYear;      // save the last year we checked
  lastMonth = myMonth;    // save it
}
// -------- WIFI Signal Strength ---------
//
void drawWiFiQuality() {
  const byte numBars = 5;            // set the number of total bars to display
  const byte barWidth = 3;           // set bar width, height in pixels
  const byte barHeight = 20;         // should be multiple of numBars, or to indicate zero value
  const byte barSpace = 1;           // set number of pixels between bars
  const uint16_t barXPosBase = 295;  // set the baseline X-pos for drawing the bars
  const byte barYPosBase = 20;       // set the baseline Y-pos for drawing the bars
  const uint16_t barColor = TFT_YELLOW;
  const uint16_t barBackColor = TFT_DARKGREY;

  int8_t quality = getWifiQuality();

  for (int8_t i = 0; i < numBars; i++) {  // current bar loop
    byte barSpacer = i * barSpace;
    byte tempBarHeight = (barHeight / numBars) * (i + 1);
    for (int8_t j = 0; j < tempBarHeight; j++) {  // draw bar height loop
      for (byte ii = 0; ii < barWidth; ii++) {    // draw bar width loop
        byte nextBarThreshold = (i + 1) * (100 / numBars);
        byte currentBarThreshold = i * (100 / numBars);
        byte currentBarIncrements = (barHeight / numBars) * (i + 1);
        float rangePerBar = (100 / numBars);
        float currentBarStrength;
        if ((quality > currentBarThreshold) && (quality < nextBarThreshold)) {
          currentBarStrength = ((quality - currentBarThreshold) / rangePerBar) * currentBarIncrements;
        } else if (quality >= nextBarThreshold) {
          currentBarStrength = currentBarIncrements;
        } else {
          currentBarStrength = 0;
        }
        if (j < currentBarStrength) {
          tft.drawPixel((barXPosBase + barSpacer + ii) + (barWidth * i), barYPosBase - j, barColor);
        } else {
          tft.drawPixel((barXPosBase + barSpacer + ii) + (barWidth * i), barYPosBase - j, barBackColor);
        }
      }
    }
  }
}
// converts the dBm to a range between 0 and 100%
int8_t getWifiQuality() {
  int32_t dbm = WiFi.RSSI();
  if (dbm <= -100) {
    return 0;
  } else if (dbm >= -50) {
    return 100;
  } else {
    return 2 * (dbm + 100);
  }
}
/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48;      // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE];  //buffer to hold incoming & outgoing packets

time_t getNtpTime() {
  IPAddress timeServerIP;  // time.nist.gov NTP server address

  while (Udp.parsePacket() > 0)
    ;  // discard any previously received packets
  //  Serial.print(F("Transmit NTP Request "));
  //get a random server from the pool
  WiFi.hostByName(ntpServerName, timeServerIP);
  //  Serial.println(timeServerIP);

  sendNTPpacket(timeServerIP);
  uint32_t beginWait = millis();
  while ((millis() - beginWait) < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 = (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL;
    }
  }
  return 0;  // return 0 if unable to get the time
}
//
// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address) {

  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;  // LI, Version, Mode
  packetBuffer[1] = 0;           // Stratum, or type of clock
  packetBuffer[2] = 6;           // Polling Interval
  packetBuffer[3] = 0xEC;        // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123);  //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
//
// ---------------- SETUP ----------------

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(0);
  //
  SD.begin();
  root = SD.open("/");
  //
  //WiFiManager
  //Local intialization.
  WiFiManager wifiManager;
  //AP Configuration
  wifiManager.setAPCallback(configModeCallback);
  //Exit After Config Instead of connecting
  wifiManager.setBreakAfterConfig(true);
  // --- now connect to the last Wifi point
  if (!wifiManager.autoConnect(eventName)) {
    delay(3000);
    ESP.restart();
    delay(5000);
  }
  {
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
    }

    // Seed Random With values Unique To This Device
    uint8_t macAddr[6];
    WiFi.macAddress(macAddr);
    String ipaddress = WiFi.localIP().toString();
    Udp.begin(localPort);  // port to use
    setSyncProvider(getNtpTime);
    //Set Sync Intervals
    setSyncInterval(ntpUpdateFrequency * 60);
  }

  // OTA Setup
  WiFi.hostname(eventName);
  //
  MDNS.begin(eventName);  // start the MDNS server...
                          //
  ArduinoOTA.setHostname(eventName);
  // ArduinoOTA.setPassword((const char *)"12345");
  ArduinoOTA.begin();

  tft.fillScreen(TFT_BLACK);  // erase the TFT
}
// ---------------- LOOP ----------------
void loop() {

  static time_t prevDisplay = 0;
  timeStatus_t ts = timeStatus();
  utc = now();
  switch (ts) {
    case timeNeedsSync:
    case timeSet:
      //update the schedule checking only if time has changed
      if (now() != prevDisplay) {
        prevDisplay = now();
        handle_ClockDisplay();  // go handle the schedule if it's on
        tmElements_t tm;
        breakTime(now(), tm);
      }
      break;
    case timeNotSet:
      now();
      delay(3000);
  }

  ArduinoOTA.handle();
}
//
//To Display <Setup> if not connected to AP
void configModeCallback(WiFiManager *myWiFiManager) {
  //Serial.println("Setup");
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString(String(eventName), SCREEN_W / 2, SCREEN_H / 2, 4);
  tft.drawString("Access Point", SCREEN_W / 2, (SCREEN_H / 2) + 40, 4);
  tft.drawString("Active", SCREEN_W / 2, (SCREEN_H / 2) + 80, 4);
  delay(2000);
}
//
