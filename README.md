# Event CountDown
 Yearly Custom Event Count Down

 Application was built with the Arduino IDE 2.3.7

This is a customizable bit of code for an ESP32 based Crowpanel 2.2. It uses the standard WIFI libraries and TFT_eSPI (there's a user setup file available that works with the 2.2 Crowpanel (User_Setup_Select.h). There are also two bitmaps (24 bit BMP format) that are shown leading up to the event date and then one for the actual event day. Time is obtained via the NTP servers ever couple of hours.

The custom section in the code is where you would define your own event like a birthday, anniversary, etc. The graphics go in the root of the SDcard and are defined in the code so it loads them:

```
#define eventName "Christmas"       // this will be the AP name you set to setup wifi<br/>
uint8_t targetMonth = 12;           // month from 1 to 12<br/>
uint8_t targetDay = 25;             // Christmas day<br/>
#define LEADING_IMAGE "/Tree_2.bmp"    // graphics MUST be in 24bit format 192 x 128 (H x W)<br/>
#define DAY_IMAGE "/Christmas.bmp"  // main graphic 300 x 300 (H x W)<br/>
uint16_t epochOffset = 28800;       // users time zone offset for the trigger date, in seconds<br/>
```

Depending on your Time Zone, you may need to adjust the calculation for that with these lines (Pacific Daylight Savings time setting shown:

```
TimeChangeRule usPDT = { "PDT", Second, dowSunday, Mar, 2, -420 };  // 7 hour offset<br/>
TimeChangeRule usPST = { "PST", First, dowSunday, Nov, 2, -480 };   // 8 hour offset<br/>
```

This is what the code does on the day after the event:

<img width="240" height="383" alt="Christmas_Event_Countdown" src="https://github.com/user-attachments/assets/1cb96447-2335-4950-a94c-555cfcf5775f" />

On the day of the event, the second larger graphic is display all day:

<img width="240" height="346" alt="Christmas_Event_Day" src="https://github.com/user-attachments/assets/8cb18a7c-9c9b-4047-9b8e-eabd713eaf3c" />

Graphics:

There are two images used. One is leading up to the event day defined as "LEADING_IMAGE", the one shown on the actual event day is the DAY_IMAGE. These are 24bit BMP colour files. Note the specific sizes previously if/when you create your own.

Supplied is a custom TFT_eSPI file if you're using a Crowpanel 2.2. If you're not sure which version of the Crowpanel you have, it's clearly labeled on the back side:

<img width="240" height="201" alt="CrowPanel" src="https://github.com/user-attachments/assets/ff715cc9-6ca4-471f-9fe9-7a682ddd132a" />

When I originally wrote the firmware for count down, my intent was to use it strictly for Christmas. However I soon realized that with a few changes it could be used for any yearly event. Thus as the code stands at the moment, that's what you have. In the Arduino IDE the board selected is the "ESP32 Wrover Kit (all versions)" like this:

<img width="320" height="214" alt="Settings" src="https://github.com/user-attachments/assets/fbea0a43-145b-4ae2-ad0a-d0012471e9be" />

Have fun and enjoy.
