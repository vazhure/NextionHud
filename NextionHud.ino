#include <FastLED.h>
#include <SoftwareSerial.h>

#define LED_PIN 7
#define NUM_LEDS 8
#define BAUD_RATE 57600
#define LED_BLINK_INTERVAL 250
#define BRIGHTNESS 20
#define DEF_LED_COLOR CRGB(255, 0, 0)
#define DEF_LIM_COLOR CRGB(0, 0, 255)
#define RX_PIN 8
#define TX_PIN 9
#define TIMEOUT 1000

#define CMD_ID 0

#define SERIAL_TX_BUFFER_SIZE 128
#define SERIAL_RX_BUFFER_SIZE 128

CRGB leds[NUM_LEDS];
SoftwareSerial nextion(RX_PIN, TX_PIN);

#define RAW_DATA_LEN ((NUM_LEDS + 1) * sizeof(CRGB) + 4)

struct LED_DATA {
  union {
    struct {
      CRGB color[NUM_LEDS];  // Rev. indicator colors
      CRGB limitator[1];     // Lim. Indicator colors
      uint8_t rev_mask;      // Rev. indicator leds mask
      uint8_t lim_mask;      // Lim. indicator leds mask
      uint8_t brightness;    // Leds brightness
      uint8_t flags;         // flags: 0 - none, 0x01 - limitator mode, 0xff - power off
    };
    uint8_t raw[RAW_DATA_LEN];
  };
} led_data;

uint8_t buf[RAW_DATA_LEN];

typedef enum LED_FLAGS { LEDS_FLAG_NONE = 0,
                         LED_FLAG_LIM = 0x01,
                         LED_FLAG_POWER_OFF = 0xff };

void UpdateLeds() {
  if (led_data.flags == LED_FLAG_POWER_OFF) {
    FastLED.setBrightness(0);
  } else {
    FastLED.setBrightness(led_data.brightness);

    uint8_t blinkMask = (byte)((millis() % (LED_BLINK_INTERVAL * 2) > LED_BLINK_INTERVAL) ? 0x00 : 0xFF);

    if (led_data.flags == LED_FLAG_LIM) {
      blinkMask &= led_data.lim_mask;
      for (int t = 0; t < NUM_LEDS; ++t)
        leds[t] = (blinkMask & (1 << t)) > 0 ? led_data.limitator[0] : CRGB(0, 0, 0);
    } else {
      //blinkMask = led_data.rev_mask = 0xff ? led_data.rev_mask & blinkMask : 0xff;
      for (int t = 0; t < NUM_LEDS; ++t)
        leds[t] = (led_data.rev_mask & (1 << t)) > 0 ? led_data.color[t] : CRGB(0, 0, 0);
    }

    FastLED.show();
  }
}

// Initialization
void setup() {
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);

  pinMode(RX_PIN, INPUT);
  pinMode(TX_PIN, OUTPUT);

  Serial.begin(BAUD_RATE);
  nextion.begin(BAUD_RATE);

  // Initializing LED_DATA struct
  for (int t = 0; t < NUM_LEDS; ++t)
    led_data.color[t] = DEF_LED_COLOR;
  led_data.limitator[0] = DEF_LIM_COLOR;
  led_data.limitator[7] = DEF_LIM_COLOR;
  led_data.brightness = BRIGHTNESS;
  led_data.flags = LED_FLAG_LIM;
  led_data.rev_mask = 0x00;
  led_data.lim_mask = 0b10000001;
  UpdateLeds();
  led_data.limitator[0] = CRGB(0,0,0);
  led_data.limitator[7] = CRGB(0,0,0);
  led_data.flags = LEDS_FLAG_NONE;
  delay(1000);
}

// Main loop
void loop() {
  UpdateLeds();
  int data_cnt = Serial.available();
  for (int t = 0; t < data_cnt; ++t) {
    if (Serial.available()) {
      int byte = Serial.read();
      if (byte == CMD_ID) {
        int cnt = 0;
        unsigned long cur = millis();
        while (cnt < RAW_DATA_LEN) {
          if (Serial.available())
            buf[cnt++] = Serial.read();

          if (millis() - cur > TIMEOUT)
            break;
        }
        if (cnt == RAW_DATA_LEN) {
          memcpy(led_data.raw, buf, RAW_DATA_LEN);
          UpdateLeds();
        }
      } else
        nextion.write(byte);
    }
  }
}