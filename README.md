# Dashboard Display for Project CARS

This sketch is for a dashboard display with shifter lights and lap time logger for Project CARS or Project CARS 2 assembled out of a NodeMCU dev board.

The code quality is probably pretty low and you might find some bugs. But I'm not an Arduino person and it works for me, so… ¯\\\_(ツ)_/¯

### Multi-page dashboard display

The board listents for UDP packets with telemetry data sent by Project CARS on the local network and displays the data on a LC display categorized in multiple pages that you can toggle between.

#### Page 1

     383.9  5 v  245
     2      8:38.121

Top: odometer (km), current gear, current lap valid (v) or invalid (i), speed (km/h)  
Bottom: current lap number, last lap time

#### Page 2

      27.5  2 v   56
    42.1   1.2  35.1

Top: as before  
Bottom: current fuel, fuel consumption per lap, laps left with fuel

#### Page 3

      81  Temp    80
      89  tyre    87

Tyre temperatures in Celsius

#### Page 4

     590  Temp   585
     434  brake  422

Brake temperatures in Celsius

### Lap Time Logger

The SD card reader will log every completed lap in a `.csv` file on the card in the format `car, class, track location, track variation, valid/invalid, time`, e.g.

    Porsche 911 GT3 R, GT3, Brands Hatch, Grand Prix, i,  1:31.520
    Porsche 911 GT3 R, GT3, Brands Hatch, Grand Prix, v,  1:31.528

The values are taken from the top of the list of participants, which works fine for free practice and time trial, with only one participant, but doesn't necessarily produce the correct values in races. If you care about race lap times: Sorry, but no.

### Shifter Lights

An RGB LED strip or a Philips Hue lamp will act as shifter lights based on the ratio of current to max RPM. The hardcoded mapping from ratio to which lights to switch on isn't ideal for all cars as the best time to shift isn't the same, but for GT, Touring and so it should do.

### Servos for Gauges

A servo driver board can control some servos so they can be used to drive the needles of an analogue gauge. The servo driver that I use provides quite a number of connectors for multiple servos (and you could even chain them), but each of them needs power and in sum they will probably make a significant amount of noise.

There are three servos in use in this script: `RPM ratio, fuel level` and `speed` for motors zero to two. There's no `maxSpeed` or something similar in the telemetry, so one has to hardcode the maximum speed or remember the fastest speed so far.

## Parts and Libraries

I use the following parts for the display:

- NodeMCU development board with ESP8266 chip
- MicroSD reader with SDHC MicroSD card  
  Library already part of Arduino IDE
- 16x2 LC Display with I2C backpack  
  [Library used](https://github.com/fdebrabander/Arduino-LiquidCrystal-I2C-library)
- PCA9685 Servo Driver  
  [Library used](https://github.com/adafruit/Adafruit-PWM-Servo-Driver-Library)
- WS2812 LED strip with 8 LEDs for shifter lights  
  [Library used](https://github.com/adafruit/Adafruit_NeoPixel), or install Neopixel library with Arduino IDE Library Manager

Instead of (or in combination to) the LED strip shifter lights you can configure a color lamp on a Philips Hue to act as a shifter light. In principle it would be possible to set up and use a lamp group instead of a single lamp, but I haven't checked the API for that and the suggested API call rate is different: 10 commands per second for a single light vs one command per second for a group.

## Wiring

### MicroSD

    MicroSD  -  NodeMCU

    CS       -  D8 (GPIO 16)
    SCK      -  D5 (GPIO 14)
    MISO     -  D6 (GPIO 12)
    MOSI     -  D7 (GPIO 13)
    VCC      -  Vin, +5V
    GND      -  GND

The MicroSD breakout board that I use requires 5V power but accepts 3.3V for the logic signals, so there's no need for level shifting. Your MicroSD board might differ.

### LC Display

    LCD  -  NodeMCU

    GND  -  GND
    VCC  -  Vin, +5V
    SDA  -  D2 (GPIO 4)
    SCL  -  D1 (GPIO 5)

### PCA9685 Servo Driver

    PCA  -  NodeMCU

    GND  -  GND
    SCL  -  D1 (GPIO 5)
    SDA  -  D2 (GPIO 4)
    VCC  -  Vin, +5V
    V+   -  Vin, +5V

Instead powering the servos with *V+*, can also power them through the terminal block. And you might want to supply the power from a different power source. Otherwise moving the servos might cause the LCD to flicker.

### WS2812 LED strip

    LED  -  NodeMCU

    GND  -  GND
    DI   -  D4 (GPIO 2)
    +5V  -  Vin, +5V

## Setup

### Network access

The display works by listening for UDP packets sent over WiFi and has to be connected to the WiFi network. You need to assign your *SSID* and *password* to the corresponding constants in the code.

### I2C, LC Display and PCA

The LC Display (with an I2C backpack) and the PCA9685 Servo Driver are controlled over I2C bus. You have to find out the I2C address for any of these parts that you use. Either you're able to read the address off of the parts by looking at the soldered bridges or you run the sketch [I2C scanner](https://playground.arduino.cc/Main/I2cScanner) on the assembled device which should report all addresses of connected I2C modules. My display with three soldering bridges `A0` to `A2` to change the address has the default address `0x3F`. I2C backpacks without these soldering bridges should have a default address of `0x27`. My servo driver has `0x40` by default.

### WS2812 LED strip

The constructor for the LED strip needs the number of LEDs on the strip. I use a strip with 8 LEDs. Depending on the type of your LEDs you might have to adjust the third paramter of the constructor according to the [documentation of the library](https://learn.adafruit.com/adafruit-neopixel-uberguide/arduino-library-use). You can also connect strips of different length, but that will require more changes to the code than to simply replace the number in the constructor.

The LEDs will draw some current. Even without servos, eight rather bright LEDs can cause loss of contrast on the LCD. If there's too much flickering, you might want to further reduce the brightness of the LEDs in the `setPixelColor` call or connect a different power source than USB.

If the LEDs light up the wrong way, instead of rotating the stip you could change the indices – the first parameter of the `setPixelColor` call – in the `setStrip` function.

### Philips Hue control

To set up Philips Hue, you have to create an API key for your Hue bridge, and enter the key and the bridge's IP address into the code.

The steps to create the API key are taken from the [Hue documentation for developers](https://developers.meethue.com/documentation/getting-started). The IP address can be aquired with a [service set up by Philips](https://discovery.meethue.com/), but I assume it's best to assign a static IP address from the router.

1. Go to `http://<bridge ip address>/debug/clip.html`

2. Press the link button on your Hue bridge.

3. Within 30 seconds, I suppose, send a `POST` request to URL `/api` with a body like `{"devicetype":"pcars_display#ESP8266"}` and copy the `username` of the response, which should be a long hex string.

4. Send a `GET` request to `/api/<username>/lights`, look for the correct lamp and note its ID.

5. Set the `hueURL` in the script to the URL for the command to `"http://<bridge ip address>/api/<username>/lights/<lamp id>/state"`;

6. You might want to adjust the `brightness`, `x` and `y` values in the `setHue` function according to [this page](https://developers.meethue.com/documentation/core-concepts). Newer lamps should be Gammut A, but if you use an older bulb, you might need different `x` and `y`.

### Page switching

The LC Display supports multiple pages that can be switched between by pressing a button on the controller. The button has to be set by hardcoding its bit value in the assignment to the variable `buttonStatus` according to the list mentioned in [this page](http://forum.projectcarsgame.com/showthread.php?40113-HowTo-Companion-App-UDP-Streaming). At the moment this is bit `4096` (*A* on XBox/Pc, *X* on PS4) or `4` (which doesn't appear in the list, but is *X* on a G29)
