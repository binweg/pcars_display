#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266HTTPClient.h>
#include <SPI.h>
#include <SD.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_PWMServoDriver.h>

const char *ssid = "your_ssid";
const char *password = "your_password";

LiquidCrystal_I2C lcd(0x3F, 16, 2);

const int chipSelect = 15;  // SD card module Chip Select pin, D8 on NodeMCU

const int NEOPIN = 2; // D4 on NodeMCU
Adafruit_NeoPixel strip(8, NEOPIN, NEO_GRB + NEO_KHZ800);

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

// Depending on your servo make, the pulse width min and max may vary, you
// want these to be as small/large as possible without hitting the hard stop
// for max range. You'll have to tweak them as necessary to match the servos you
// have!
#define SERVOMIN 170 // this is the 'minimum' pulse length count (out of 4096)
#define SERVOMAX 580 // this is the 'maximum' pulse length count (out of 4096)

float floatmap(float x, float in_min, float in_max, float out_min, float out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

class Motor {
    int _servoMin, _servoMax, _driverIndex;
    Adafruit_PWMServoDriver _driver;

    public:
        Motor(Adafruit_PWMServoDriver driver, int driverIndex);
        void setPosition(float pos);
};

Motor::Motor(Adafruit_PWMServoDriver driver, int driverIndex) {
    _servoMin = SERVOMIN;
    _servoMax = SERVOMAX;
    _driver = driver;
    _driverIndex = driverIndex;
}

void Motor::setPosition(float pos) {
    pos = constrain(pos, 0, 1);
    int pulselen = (int) floatmap(pos, 0, 1, _servoMin, _servoMax);
    _driver.setPWM(_driverIndex, 0, pulselen);
}

Motor motRPM = Motor(pwm, 0);
Motor motFuel = Motor(pwm, 1);
Motor motSpeed = Motor(pwm, 2);

WiFiUDP Udp;
const unsigned int localUdpPort = 5606; // local port to listen on
char packet[2048];                      // buffer for incoming packets

HTTPClient http;
int httpCode;
String payload;

char hueURL[] = "http://ip_address/api/api_username/lights/light_id/state";
char hueCommandField[68];
/*
{"on":true,"sat":254,"bri":254,"transitiontime":0,"xy":[0.12,0.12]}0
*/
const byte HUE_OFF = 0;
const byte HUE_GREEN = 1;
const byte HUE_YELLOW = 2;
const byte HUE_RED = 3;
byte last_hue_color;

byte raceState;
bool lapInvalid;
char lapInvalidField;
bool isNewLap;

char carName[64];
char carClassName[64];
char trackLocation[64];
char trackVariation[64];

//   (64+2) * 4
// + (1+2)
// + (9)
// + 1 null byte
char logString[277];

byte currentLap;
byte oldCurrentLap;
char currentLapField[3];
byte sector;

char tempChar[2]; // Temporary array for u16 conversion

u16 currentLapDistance;

u16 joyPad;
byte dPad;

byte gear;
char gearField;

u16 RPM;
u16 maxRPM;
char rpmField[6];

float rpmRatio;
byte fuelCapacity;
float fuelLevel;
float fuelAmount;
float lastLapFuelAmount;
float currentLapFuelAmount;
float fuelConsumption;
float fuelLapsRemaining;
float odometer;
float spd;
byte minutes;
float bestLapTime;
float lastLapTime;
float currentTime;
float oldCurrentTime;

char fuelAmountField[5];
char fuelConsumptionField[5];
char fuelLapsRemainingField[5];
char fuelLap;
char odometerField[7];
char spdField[4];
char lastLapField[10];

u16 tyreTreadTempLF;
u16 tyreTreadTempRF;
u16 tyreTreadTempLR;
u16 tyreTreadTempRR;
char tyreTreadTempLFField[5];
char tyreTreadTempRFField[5];
char tyreTreadTempLRField[5];
char tyreTreadTempRRField[5];

s16 brakeTempLF;
s16 brakeTempRF;
s16 brakeTempLR;
s16 brakeTempRR;
char brakeTempLFField[5];
char brakeTempRFField[5];
char brakeTempLRField[5];
char brakeTempRRField[5];

bool buttonStatus;
bool lastButtonStatus;
byte pageToDisplay;
char line_1[17];
char line_2[17];
char line_3[17];
char line_4[17];
char line_5[17];
char line_6[17];
char line_7[17];

byte num_pages = 4;

void setHueCommand(byte bri, float x, float y)
{
    sprintf(hueCommandField,
            "{\"on\":true,\"sat\":254,\"bri\":%3u,\"transitiontime\":0,\"xy\":[%4.2f,%4.2f]}",
            bri,
            x,
            y);
}

void setHue(float ratio)
{
    byte color = HUE_OFF;
    if (ratio > .88) color = HUE_GREEN;
    if (ratio > .92) color = HUE_YELLOW;
    if (ratio > .98) color = HUE_RED;

    if (last_hue_color != color)
    {
        switch (color)
        {
            case HUE_OFF:
                setHueCommand(0, 0.2, 0.7);
                break;
            case HUE_GREEN:
                setHueCommand(128, 0.2, 0.7);
                break;
            case HUE_YELLOW:
                setHueCommand(191, 0.53, 0.45);
                break;
            case HUE_RED:
                setHueCommand(254, 0.7, 0.3);
                break;
        }
        http.begin(hueURL);
        httpCode = http.PUT(hueCommandField);
        payload = http.getString();
        http.end();
        Serial.println(payload);
        last_hue_color = color;
    }
}

void setStrip(float ratio)
{
    for (int i = 0; i < strip.numPixels(); i++)
    {
        strip.setPixelColor(i, 0);
    }
    if (ratio > .88)
    {
        strip.setPixelColor(7, 0, 31, 0);
    }
    if (ratio > .9)
    {
        strip.setPixelColor(6, 0, 31, 0);
    }
    if (ratio > .92)
    {
        strip.setPixelColor(5, 63, 63, 0);
    }
    if (ratio > .94)
    {
        strip.setPixelColor(4, 63, 63, 0);
    }
    if (ratio > .955)
    {
        strip.setPixelColor(3, 63, 63, 0);
    }
    if (ratio > .97)
    {
        strip.setPixelColor(2, 63, 63, 0);
    }
    if (ratio > .98)
    {
        strip.setPixelColor(1, 127, 0, 0);
    }
    if (ratio > .985)
    {
        strip.setPixelColor(0, 127, 0, 0);
    }
    if (ratio > .99)
    {
        for (int i = 0; i < strip.numPixels(); i++)
        {
            strip.setPixelColor(i, 96, 0, 0);
        }
    }
    strip.show();
}

void setup()
{
    Serial.begin(115200);
    pwm.begin();
    pwm.setPWMFreq(60);  // Analog servos run at ~60 Hz updates
    strip.begin();
    strip.show();
    lcd.begin();
    lcd.backlight();
    lcd.clear();
    while (!Serial)
    {
        ; // wait for serial port to connect. Needed for native USB port only
    }
    Serial.print("Initializing SD card...");
    // see if the card is present and can be initialized:
    if (!SD.begin(chipSelect))
    {
        Serial.println("Card failed, or not present");
        lcd.print("card failed");
        // don't do anything more:
        while (1);
    }
    lcd.clear();
    lcd.print("card init.");
    lcd.setCursor(0, 1);
    lcd.print("connecting WiFi");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
    }
    Udp.begin(localUdpPort);
    lcd.clear();
    lcd.print("connected");
    delay(1000);
    lcd.clear();
}

void loop()
{
    int packetSize = Udp.parsePacket();
    if (packetSize)
    {
        int len = Udp.read(packet, packetSize);
        if (len > 0)
        {
            packet[len] = 0;
        }

        if (len == 1347) // ParticipantInfoStrings
        {
            memcpy(&carName, &packet[3], 64);
            memcpy(&carClassName, &packet[67], 64);
            memcpy(&trackLocation, &packet[131], 64);
            memcpy(&trackVariation, &packet[195], 64);
        }
        else if (len == 1367) // Telemetry
        {
            // game session state
            memcpy(&raceState, &packet[10], 1);

            // gear
            memcpy(&gear, &packet[128], 1);
            gear = gear & 0x0f;
            if (gear == 0xf)
            {
                gearField = 'R';
            }
            else if (gear == 0)
            {
                gearField = 'N';
            }
            else
            {
                gearField = (char) gear + '0';
            }

            // current RPM
            memcpy(&tempChar, &packet[124], 2);
            RPM = (tempChar[1] << 8) + tempChar[0];
            sprintf(rpmField, "%5u", RPM);

            // max RPM
            memcpy(&tempChar, &packet[126], 2);
            maxRPM = (tempChar[1] << 8) + tempChar[0];

            // speed
            memcpy(&spd, &packet[120], 4);
            spd *= 3.6; // telemetry speed is in meter per second
            sprintf(spdField, "%3u", (int)spd);

            // fuel
            memcpy(&fuelCapacity, &packet[111], 1);
            memcpy(&fuelLevel, &packet[116], 4);
            fuelAmount = fuelLevel * (float)fuelCapacity;
            if (fuelAmount >= 100)
            {
                sprintf(fuelAmountField, "%4u", (int) fuelAmount);
            }
            else
            {
                sprintf(fuelAmountField, "%4.1f", fuelAmount);
            }

            // odometer
            memcpy(&odometer, &packet[132], 4);
            if (odometer < 10000)
            {
                sprintf(odometerField, "%6.1f", odometer);
            }
            else
            {
                sprintf(odometerField, "%6u", (int) odometer);
            }

            // lap time
            memcpy(&bestLapTime, &packet[12], 4);
            memcpy(&lastLapTime, &packet[16], 4);
            memcpy(&currentTime, &packet[20], 4);
            if (lastLapTime >= 60.)
            {
                minutes = (byte) (lastLapTime / 60);
                sprintf(lastLapField, "%2u:%06.3f", minutes, lastLapTime - 60 * (float)minutes);
            }
            else if (lastLapTime > 0.)
            {
                sprintf(lastLapField, "   %06.3f", lastLapTime);
            }
            else
            {
                sprintf(lastLapField, "%9s", "   n/v   ");
            }

            // current lap
            memcpy(&currentLap, &packet[464 + 10], 1);
            sprintf(currentLapField, "%2u", currentLap);

            // sector
            memcpy(&sector, &packet[464 + 11], 1);

            // current lap distance
            memcpy(&tempChar, &packet[464 + 6], 2);
            currentLapDistance = (tempChar[1] << 8) + tempChar[0];

            // joypad + dpad
            memcpy(&tempChar, &packet[96], 2);
            joyPad = (tempChar[1] << 8) + tempChar[0];
            buttonStatus = (joyPad & 4096) | (joyPad & 4);  // X button on PS4, A on PC and XBox

            // tyres
            // Order of tyres is LF, RF, LR, RR
            memcpy(&tempChar, &packet[336], 2);
            tyreTreadTempLF = (tempChar[1] << 8) + tempChar[0] - 273;
            if (tyreTreadTempLF > 9999) tyreTreadTempLF = 0;
            sprintf(tyreTreadTempLFField, "%4d", tyreTreadTempLF);
            memcpy(&tempChar, &packet[338], 2);
            tyreTreadTempRF = (tempChar[1] << 8) + tempChar[0] - 273;
            if (tyreTreadTempRF > 9999) tyreTreadTempRF = 0;
            sprintf(tyreTreadTempRFField, "%4d", tyreTreadTempRF);
            memcpy(&tempChar, &packet[340], 2);
            tyreTreadTempLR = (tempChar[1] << 8) + tempChar[0] - 273;
            if (tyreTreadTempLR > 9999) tyreTreadTempLR = 0;
            sprintf(tyreTreadTempLRField, "%4d", tyreTreadTempLR);
            memcpy(&tempChar, &packet[342], 2);
            tyreTreadTempRR = (tempChar[1] << 8) + tempChar[0] - 273;
            if (tyreTreadTempRR > 9999) tyreTreadTempRR = 0;
            sprintf(tyreTreadTempRRField, "%4d", tyreTreadTempRR);

            // brakes
            memcpy(&tempChar, &packet[328], 2);
            brakeTempLF = (tempChar[1] << 8) + tempChar[0];
            if (brakeTempLF > 9999) brakeTempLF = 0;
            sprintf(brakeTempLFField, "%4d", brakeTempLF);
            memcpy(&tempChar, &packet[330], 2);
            brakeTempRF = (tempChar[1] << 8) + tempChar[0];
            if (brakeTempRF > 9999) brakeTempRF = 0;
            sprintf(brakeTempRFField, "%4d", brakeTempRF);
            memcpy(&tempChar, &packet[332], 2);
            brakeTempLR = (tempChar[1] << 8) + tempChar[0];
            if (brakeTempLR > 9999) brakeTempLR = 0;
            sprintf(brakeTempLRField, "%4d", brakeTempLR);
            memcpy(&tempChar, &packet[334], 2);
            brakeTempRR = (tempChar[1] << 8) + tempChar[0];
            if (brakeTempRR > 9999) brakeTempRR = 0;
            sprintf(brakeTempRRField, "%4d", brakeTempRR);

            if (maxRPM)
            {
                rpmRatio = (float)RPM / (float)maxRPM;
            }
            else
            {
                rpmRatio = 0.0;
            }
            setStrip(rpmRatio);
            
            // uncomment this line to enable Hue lights
            // setHue(rpmRatio);

            motRPM.setPosition(1.0 - rpmRatio);
            motFuel.setPosition(1.0 - fuelLevel);
            motSpeed.setPosition(1.0 - spd/350.); // assume top speed of 350 km/h

            // new lap test
            isNewLap = (oldCurrentTime == -1.0 && currentTime > 0) ||
                       (currentLap == oldCurrentLap + 1);
            if (isNewLap && oldCurrentTime > 0)
            {
                sprintf(logString, "%s, %s, %s, %s, %c, %s\0",
                        carName,
                        carClassName,
                        trackLocation,
                        trackVariation,
                        lapInvalidField,
                        lastLapField);
                File lapFile = SD.open("lapdata.txt", FILE_WRITE);
                if (lapFile)
                {
                    lapFile.println(logString);
                    lapFile.close();
                }
                else
                {
                    Serial.println("error opening lapdata.txt");
                }
                Serial.println(logString);

                lastLapFuelAmount = currentLapFuelAmount;
                currentLapFuelAmount = fuelAmount;
                fuelConsumption = lastLapFuelAmount - currentLapFuelAmount;
            }
            else if (isNewLap)
            {
                currentLapFuelAmount = fuelAmount;
                fuelConsumption = 0.;
            }

            oldCurrentTime = currentTime;
            oldCurrentLap = currentLap;

            if (fuelConsumption > 0)
            {
                fuelLapsRemaining = fuelAmount / fuelConsumption;
                sprintf(fuelConsumptionField, "%4.1f", fuelConsumption);
                if (fuelLapsRemaining >= 100)
                {
                    sprintf(fuelLapsRemainingField, "%4u", (int) fuelLapsRemaining);
                }
                else
                {
                    sprintf(fuelLapsRemainingField, "%4.1f", fuelLapsRemaining);
                }
            }
            else
            {
                sprintf(fuelConsumptionField, "%4s", "n/v ");
                sprintf(fuelLapsRemainingField, "%4s", "n/v ");
            }

            // need old state for log write
            lapInvalid = raceState & 8;
            if (lapInvalid)
            {
                lapInvalidField = 'i';
            }
            else
            {
                lapInvalidField = 'v';
            }
        }


        if (buttonStatus && !lastButtonStatus)
        {
            pageToDisplay = (pageToDisplay+1) % num_pages;
        }
        lastButtonStatus = buttonStatus;

        sprintf(line_1, "%6s  %c %c  %3s\0", odometerField, gearField, lapInvalidField, spdField);
        sprintf(line_2, "%2s     %9s\0", currentLapField, lastLapField);
        sprintf(line_3, "%4s  %4s  %4s\0", fuelAmountField, fuelConsumptionField, fuelLapsRemainingField);
        sprintf(line_4, "%4s  Temp  %4s\0", tyreTreadTempLFField, tyreTreadTempRFField);
        sprintf(line_5, "%4s  tyre  %4s\0", tyreTreadTempLRField, tyreTreadTempRRField);
        sprintf(line_6, "%4s  Temp  %4s\0", brakeTempLFField, brakeTempRFField);
        sprintf(line_7, "%4s  brake %4s\0", brakeTempLRField, brakeTempRRField);

        if (pageToDisplay % num_pages == 0)
        {
            lcd.setCursor(0, 0);
            lcd.print(line_1);
            lcd.setCursor(0, 1);
            lcd.print(line_2);
        }
        else if (pageToDisplay % num_pages == 1)
        {
            lcd.setCursor(0, 0);
            lcd.print(line_1);
            lcd.setCursor(0, 1);
            lcd.print(line_3);
        }
        else if (pageToDisplay % num_pages == 2)
        {
            lcd.setCursor(0, 0);
            lcd.print(line_4);
            lcd.setCursor(0, 1);
            lcd.print(line_5);
        }
        else
        {
            lcd.setCursor(0, 0);
            lcd.print(line_6);
            lcd.setCursor(0, 1);
            lcd.print(line_7);
        }
    }
}
