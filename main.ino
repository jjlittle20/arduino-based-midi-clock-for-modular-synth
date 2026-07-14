/*
 * Arduino Midi Master Clock v0.2
 * MIDI master clock/sync/divider for MIDI instruments, Pocket Operators and Korg Volca.
 * by Eunjae Im https://ejlabs.net/arduino-midi-master-clock
 *
 * Modified:
 * - OLED display removed
 * - Encoder handling cleaned up
 * - Encoder direction flipped
 * - Long press start/stop button resets BPM to 120
 * - Added 4511 BCD display output:
 *   - 3 digits for BPM
 *   - 2 digits for division
 * - Added software-remappable BCD bit order
 */

#include <TimerOne.h>
#include <EEPROM.h>
#include <Encoder.h>
#include <MIDI.h>

// -------------------- 4511 display pins --------------------

// Shared BCD data lines to all 4511 chips
#define BCD_A A0
#define BCD_B A1
#define BCD_C A2
#define BCD_D A3

// Individual latch pins for each 4511
#define LATCH_BPM_HUNDREDS A4
#define LATCH_BPM_TENS     A5
#define LATCH_BPM_ONES     10

#define LATCH_DIV_TENS     11
#define LATCH_DIV_ONES     12

/*
 * BCD bit remap.
 *
 * Normal BCD:
 * bit 0 = 1s
 * bit 1 = 2s
 * bit 2 = 4s
 * bit 3 = 8s
 *
 * This array controls which BCD bit goes to each Arduino BCD output:
 *
 * bcdMap[0] -> BCD_A / Arduino A0
 * bcdMap[1] -> BCD_B / Arduino A1
 * bcdMap[2] -> BCD_C / Arduino A2
 * bcdMap[3] -> BCD_D / Arduino A3
 *
 * Try these if the display is jumbled:
 *
 * byte bcdMap[4] = {0, 1, 2, 3}; // normal
 * byte bcdMap[4] = {3, 2, 1, 0}; // reversed
 * byte bcdMap[4] = {1, 0, 2, 3}; // A/B swapped
 * byte bcdMap[4] = {0, 1, 3, 2}; // C/D swapped
 * byte bcdMap[4] = {1, 2, 3, 0};
 * byte bcdMap[4] = {2, 3, 0, 1};
 */
byte bcdMap[4] = {0, 1, 2, 3};

// -------------------- Main pins --------------------

#define LED_PIN1 7
#define SYNC_OUTPUT_PIN 6
#define SYNC_OUTPUT_PIN2 8
#define BUTTON_START 4
#define BUTTON_ROTARY 5

// -------------------- Clock settings --------------------

#define CLOCKS_PER_BEAT 24
#define AUDIO_SYNC 12
#define AUDIO_SYNC2 12

#define MINIMUM_BPM 20
#define MAXIMUM_BPM 300
#define DEFAULT_BPM 120

#define BLINK_TIME 4

#define ENCODER_STEPS_PER_CLICK 2
#define LONG_PRESS_TIME 1000

// -------------------- State --------------------

volatile int blinkCount = 0;
volatile int blinkCount2 = 0;
volatile int AudioSyncCount = 0;
volatile int AudioSyncCount2 = 0;

long bpm;
long audio_sync2;

boolean playing = false;
boolean sync_editing = false;
boolean display_update = false;

// Encoder pins: D2, D3
Encoder myEnc(2, 3);

long oldPosition = 0;

MIDI_CREATE_DEFAULT_INSTANCE();

// -------------------- Setup --------------------

void setup(void) {
  MIDI.begin();
  MIDI.turnThruOff();

  bpm = EEPROMReadInt(0);
  if (bpm > MAXIMUM_BPM || bpm < MINIMUM_BPM) {
    bpm = DEFAULT_BPM;
  }

  audio_sync2 = EEPROMReadInt(3);
  if (audio_sync2 > 64 || audio_sync2 < 2) {
    audio_sync2 = 12;
  }

  long initialInterval = 60L * 1000 * 1000 / bpm / CLOCKS_PER_BEAT;

  Timer1.initialize(initialInterval);
  Timer1.setPeriod(initialInterval);
  Timer1.attachInterrupt(sendClockPulse);

  pinMode(BUTTON_START, INPUT_PULLUP);
  pinMode(BUTTON_ROTARY, INPUT_PULLUP);

  pinMode(LED_PIN1, OUTPUT);
  pinMode(SYNC_OUTPUT_PIN, OUTPUT);
  pinMode(SYNC_OUTPUT_PIN2, OUTPUT);

  setupDisplays();
  updateDisplays();

  myEnc.write(0);
  oldPosition = 0;

  all_off();
}

// -------------------- 4511 display functions --------------------

void setupDisplays() {
  pinMode(BCD_A, OUTPUT);
  pinMode(BCD_B, OUTPUT);
  pinMode(BCD_C, OUTPUT);
  pinMode(BCD_D, OUTPUT);

  pinMode(LATCH_BPM_HUNDREDS, OUTPUT);
  pinMode(LATCH_BPM_TENS, OUTPUT);
  pinMode(LATCH_BPM_ONES, OUTPUT);

  pinMode(LATCH_DIV_TENS, OUTPUT);
  pinMode(LATCH_DIV_ONES, OUTPUT);

  digitalWrite(BCD_A, LOW);
  digitalWrite(BCD_B, LOW);
  digitalWrite(BCD_C, LOW);
  digitalWrite(BCD_D, LOW);

  /*
   * CD4511 latch enable:
   *
   * LE LOW  = transparent / accepts current BCD input
   * LE HIGH = holds latched value
   */
  digitalWrite(LATCH_BPM_HUNDREDS, HIGH);
  digitalWrite(LATCH_BPM_TENS, HIGH);
  digitalWrite(LATCH_BPM_ONES, HIGH);

  digitalWrite(LATCH_DIV_TENS, HIGH);
  digitalWrite(LATCH_DIV_ONES, HIGH);
}

void writeBCD(byte digit) {
  digit = digit % 10;

  digitalWrite(BCD_A, bitRead(digit, bcdMap[0]));
  digitalWrite(BCD_B, bitRead(digit, bcdMap[1]));
  digitalWrite(BCD_C, bitRead(digit, bcdMap[2]));
  digitalWrite(BCD_D, bitRead(digit, bcdMap[3]));
}

void latchDigit(byte latchPin, byte digit) {
  /*
   * For CD4511:
   * Pull LE LOW so this chip follows the BCD inputs,
   * put the digit on the shared BCD bus,
   * then set LE HIGH so the chip holds that digit.
   */

  digitalWrite(latchPin, LOW);
  delayMicroseconds(5);

  writeBCD(digit);
  delayMicroseconds(5);

  digitalWrite(latchPin, HIGH);
  delayMicroseconds(5);
}

void displayBpmNumber(int value) {
  if (value < 0) {
    value = 0;
  }

  if (value > 999) {
    value = 999;
  }

  byte hundreds = value / 100;
  byte tens = (value / 10) % 10;
  byte ones = value % 10;

  latchDigit(LATCH_BPM_HUNDREDS, hundreds);
  latchDigit(LATCH_BPM_TENS, tens);
  latchDigit(LATCH_BPM_ONES, ones);
}

void displayDivisionNumber(int value) {
  if (value < 0) {
    value = 0;
  }

  if (value > 99) {
    value = 99;
  }

  byte tens = value / 10;
  byte ones = value % 10;

  latchDigit(LATCH_DIV_TENS, tens);
  latchDigit(LATCH_DIV_ONES, ones);
}

void updateDisplays() {
  displayBpmNumber((int)bpm);
  displayDivisionNumber((int)audio_sync2);
}

// -------------------- EEPROM helpers --------------------

void EEPROMWriteInt(int p_address, int p_value) {
  byte lowByte = ((p_value >> 0) & 0xFF);
  byte highByte = ((p_value >> 8) & 0xFF);

  EEPROM.write(p_address, lowByte);
  EEPROM.write(p_address + 1, highByte);
}

unsigned int EEPROMReadInt(int p_address) {
  byte lowByte = EEPROM.read(p_address);
  byte highByte = EEPROM.read(p_address + 1);

  return ((lowByte << 0) & 0xFF) + ((highByte << 8) & 0xFF00);
}

// -------------------- BPM / division update functions --------------------

void resetBpm() {
  bpm = DEFAULT_BPM;
  updateBpm();
  EEPROMWriteInt(0, bpm);
  updateDisplays();

  // Small visual confirmation.
  digitalWrite(LED_PIN1, HIGH);
  delay(120);
  digitalWrite(LED_PIN1, LOW);
  delay(120);
  digitalWrite(LED_PIN1, HIGH);
  delay(120);
  digitalWrite(LED_PIN1, LOW);
}

void bpm_display() {
  updateBpm();
  EEPROMWriteInt(0, bpm);
  updateDisplays();

  display_update = false;
}

void sync_display() {
  EEPROMWriteInt(3, audio_sync2);
  updateDisplays();
}

// -------------------- Start / stop controls --------------------

void startOrStop() {
  if (!playing) {
    MIDI.sendRealTime(midi::Start);
  } else {
    all_off();
    MIDI.sendRealTime(midi::Stop);
  }

  playing = !playing;
}

void handleStartButton() {
  static boolean buttonWasDown = false;
  static boolean longPressDone = false;
  static unsigned long buttonDownTime = 0;

  boolean buttonDown = digitalRead(BUTTON_START) == LOW;
  unsigned long now = millis();

  if (buttonDown && !buttonWasDown) {
    buttonWasDown = true;
    longPressDone = false;
    buttonDownTime = now;
  }

  if (buttonDown && buttonWasDown && !longPressDone) {
    if (now - buttonDownTime >= LONG_PRESS_TIME) {
      resetBpm();
      longPressDone = true;
    }
  }

  if (!buttonDown && buttonWasDown) {
    buttonWasDown = false;

    // Only start/stop if it was not a long press.
    if (!longPressDone) {
      startOrStop();
    }
  }
}

// -------------------- Main loop --------------------

void loop(void) {
  byte i = 0;
  byte p = 0;

  handleStartButton();

  if (digitalRead(BUTTON_ROTARY) == LOW) {
    p = 1;
    delay(200);
  }

  /*
   * Encoder handling.
   *
   * i = 2 means increase
   * i = 1 means decrease
   *
   * Direction has been flipped here.
   */
  long newPosition = myEnc.read();

  if (newPosition >= oldPosition + ENCODER_STEPS_PER_CLICK) {
    i = 1;  // flipped: this direction now decreases
    oldPosition += ENCODER_STEPS_PER_CLICK;
  } else if (newPosition <= oldPosition - ENCODER_STEPS_PER_CLICK) {
    i = 2;  // flipped: this direction now increases
    oldPosition -= ENCODER_STEPS_PER_CLICK;
  }

  if (!sync_editing) {
    // BPM edit mode
    if (i == 2) {
      bpm++;

      if (bpm > MAXIMUM_BPM) {
        bpm = MAXIMUM_BPM;
      }

      bpm_display();

    } else if (i == 1) {
      bpm--;

      if (bpm < MINIMUM_BPM) {
        bpm = MINIMUM_BPM;
      }

      bpm_display();

    } else if (p == 1) {
      sync_display();
      sync_editing = true;
    }

  } else {
    // 2nd jack audio sync speed edit mode
    if (p == 1) {
      bpm_display();
      sync_editing = false;

    } else if (i == 1) {
      audio_sync2++;

      if (audio_sync2 > 64) {
        audio_sync2 = 64;
      }

      sync_display();

    } else if (i == 2) {
      audio_sync2--;

      if (audio_sync2 < 2) {
        audio_sync2 = 2;
      }

      sync_display();
    }
  }
}

// -------------------- Output helpers --------------------

void all_off() {
  digitalWrite(SYNC_OUTPUT_PIN, LOW);
  digitalWrite(SYNC_OUTPUT_PIN2, LOW);
  digitalWrite(LED_PIN1, LOW);
}

// -------------------- Clock interrupt --------------------

void sendClockPulse() {
  MIDI.sendRealTime(midi::Clock);

  if (playing) {
    blinkCount = (blinkCount + 1) % CLOCKS_PER_BEAT;
    blinkCount2 = (blinkCount2 + 1) % (CLOCKS_PER_BEAT / 2);
    AudioSyncCount = (AudioSyncCount + 1) % AUDIO_SYNC;
    AudioSyncCount2 = (AudioSyncCount2 + 1) % audio_sync2;

    if (AudioSyncCount == 0) {
      digitalWrite(SYNC_OUTPUT_PIN, HIGH);
    } else {
      if (AudioSyncCount == 1) {
        digitalWrite(SYNC_OUTPUT_PIN, LOW);
      }
    }

    if (AudioSyncCount2 == 0) {
      digitalWrite(SYNC_OUTPUT_PIN2, HIGH);
    } else {
      if (AudioSyncCount2 == 1) {
        digitalWrite(SYNC_OUTPUT_PIN2, LOW);
      }
    }

    if (blinkCount == 0) {
      digitalWrite(LED_PIN1, HIGH);
    } else {
      if (blinkCount == BLINK_TIME) {
        digitalWrite(LED_PIN1, LOW);
      }
    }
  }
}

// -------------------- Timer update --------------------

void updateBpm() {
  long interval = 60L * 1000 * 1000 / bpm / CLOCKS_PER_BEAT;
  Timer1.setPeriod(interval);
}
