# Arduino Nano MIDI Master Clock

## Overview

This project is a modified version of the **Arduino MIDI Master Clock v0.2** originally created by Eunjae Im.

The original project provides MIDI clock, start/stop, sync, and clock-divider functionality for MIDI equipment such as:

* Synthesizers
* Drum machines
* Korg Volca devices
* Teenage Engineering Pocket Operators
* Other equipment that accepts MIDI Clock or analogue sync pulses

This version was adapted to use electronic components that were already available and to run on an **Arduino Nano fitted with the smaller ATmega168 microcontroller**.

The original OLED display has been removed and replaced with five seven-segment displays controlled by CD4511 BCD-to-seven-segment driver ICs.

The software was also reorganised and simplified to reduce unnecessary functionality and make the project suitable for the more limited program memory available on the ATmega168 Nano.

---

## Main Features

The clock provides:

* Adjustable MIDI clock from 20 to 300 BPM
* MIDI Start and Stop messages
* Standard 24 pulses-per-quarter-note MIDI Clock output
* Two analogue sync pulse outputs
* Adjustable divider value for the second sync output
* Rotary encoder BPM control
* Rotary encoder push-button mode selection
* Dedicated start/stop button
* Long-press BPM reset
* EEPROM storage for BPM and divider settings
* Three-digit BPM display
* Two-digit divider display
* Software-configurable BCD wiring order

The sketch retains the original MIDI clock functionality while replacing the OLED-based interface with CD4511-controlled seven-segment displays.

---

## Hardware Platform

### Supported Board

The project is designed for:

* Arduino Nano
* ATmega168 or ATmega328P
* 5 V operating voltage
* 16 MHz clock speed

The primary target is an older or lower-cost Arduino Nano using the **ATmega168**.

### ATmega168 Considerations

The ATmega168 has less memory than the ATmega328P commonly fitted to modern Arduino Nano boards.

Typical memory capacities are:

| Microcontroller | Flash Memory | SRAM |    EEPROM |
| --------------- | -----------: | ---: | --------: |
| ATmega168       |        16 KB | 1 KB | 512 bytes |
| ATmega328P      |        32 KB | 2 KB |      1 KB |

To account for the smaller ATmega168:

* The OLED display code was removed.
* No display graphics library is required.
* Display output is handled using basic `digitalWrite()` operations.
* The interface uses simple numeric seven-segment displays.
* Dynamic memory allocation is avoided.
* No large text buffers or graphical framebuffers are used.
* Settings are stored as small integer values in EEPROM.

This makes the design considerably lighter than an OLED-based implementation.

---

## Modifications from the Original Project

This version was modified to suit the parts already available rather than requiring the exact hardware used in the original design.

The main changes are:

* Removed the OLED display.
* Added five CD4511-controlled seven-segment displays.
* Added three digits for displaying BPM.
* Added two digits for displaying the second sync divider.
* Added shared BCD data wiring between the CD4511 chips.
* Added an individual latch connection for each display digit.
* Added software-remappable BCD bit order.
* Simplified encoder handling.
* Reversed the encoder direction to match the physical control.
* Added long-press BPM reset.
* Adapted the design for an ATmega168 Arduino Nano.
* Reused available components and pin assignments where practical.

The intention was to retain the useful MIDI clock functionality while redesigning the user interface around components that were already on hand.

---

## Required Libraries

The following Arduino libraries are used:

```cpp
#include <TimerOne.h>
#include <EEPROM.h>
#include <Encoder.h>
#include <MIDI.h>
```

### TimerOne

Used to generate accurately timed MIDI clock interrupts.

### EEPROM

Used to save the BPM and sync-divider values so that they are restored after power is removed.

### Encoder

Used to read the rotary encoder connected to digital pins D2 and D3.

### MIDI Library

Used to send MIDI Clock, MIDI Start, and MIDI Stop messages.

The sketch creates the default MIDI interface using:

```cpp
MIDI_CREATE_DEFAULT_INSTANCE();
```

---

## Pin Assignments

## Shared BCD Data Bus

All CD4511 driver chips share the same four BCD data connections.

| BCD Signal | Arduino Nano Pin | Binary Weight |
| ---------- | ---------------: | ------------: |
| BCD A      |               A0 |             1 |
| BCD B      |               A1 |             2 |
| BCD C      |               A2 |             4 |
| BCD D      |               A3 |             8 |

These pins are defined as:

```cpp
#define BCD_A A0
#define BCD_B A1
#define BCD_C A2
#define BCD_D A3
```

The shared BCD bus reduces the number of microcontroller pins required. Only the latch input of each CD4511 needs a separate Arduino pin.

---

## Display Latch Pins

Each display digit has its own CD4511 driver and latch-enable connection.

| Display Digit | Arduino Nano Pin |
| ------------- | ---------------: |
| BPM hundreds  |               A4 |
| BPM tens      |               A5 |
| BPM ones      |              D10 |
| Divider tens  |              D11 |
| Divider ones  |              D12 |

The three BPM displays can show values from `000` to `999`.

The project limits BPM to a range of 20 to 300.

The two divider displays can show values from `00` to `99`, although the software limits the divider to a range of 2 to 64.

---

## Main Control and Output Pins

| Function                 |                   Arduino Nano Pin |
| ------------------------ | ---------------------------------: |
| Rotary encoder channel A |                                 D2 |
| Rotary encoder channel B |                                 D3 |
| Start/stop button        |                                 D4 |
| Rotary encoder button    |                                 D5 |
| Primary sync output      |                                 D6 |
| Beat indicator LED       |                                 D7 |
| Secondary sync output    |                                 D8 |
| MIDI output              | Library/default serial MIDI output |
| BPM ones latch           |                                D10 |
| Divider tens latch       |                                D11 |
| Divider ones latch       |                                D12 |

The rotary encoder is created with:

```cpp
Encoder myEnc(2, 3);
```

Pins D2 and D3 are well suited to encoder input because they are the Arduino Nano’s external interrupt pins.

---

## CD4511 Display System

## Why CD4511 Drivers Are Used

A CD4511 converts a four-bit BCD value into the seven control signals required by a common-cathode seven-segment display.

Using one CD4511 per display digit provides several advantages:

* The Arduino only needs four shared digit-data pins.
* Each digit holds its value after being latched.
* The displays do not need to be continuously multiplexed.
* Display refresh timing does not interfere with MIDI clock timing.
* The implementation uses very little program memory.
* The displays remain stable while the Arduino performs other work.

This approach is particularly useful on an ATmega168 because it avoids the memory and processing overhead of an OLED display.

---

## Shared BCD Bus and Latching

All five CD4511 chips receive the same BCD data lines.

To update one digit, the software:

1. Pulls that digit’s latch-enable pin LOW.
2. Writes the desired BCD number to A0–A3.
3. Pulls the latch-enable pin HIGH.
4. Leaves that CD4511 holding the new value.
5. Repeats the process for the remaining digits.

The relevant function is:

```cpp
void latchDigit(byte latchPin, byte digit) {
    digitalWrite(latchPin, LOW);
    delayMicroseconds(5);

    writeBCD(digit);
    delayMicroseconds(5);

    digitalWrite(latchPin, HIGH);
    delayMicroseconds(5);
}
```

For the CD4511 latch-enable input:

* LOW means the output follows the current BCD inputs.
* HIGH means the current digit is stored and held.

The sketch configures every latch HIGH after initialisation so that each display retains its most recently written value.

---

## BCD Bit Remapping

Different wiring layouts may connect the Arduino BCD outputs to the CD4511 inputs in a different order.

Rather than physically rewiring the circuit, the bit order can be changed in software using:

```cpp
byte bcdMap[4] = {0, 1, 2, 3};
```

The normal mapping is:

| Arduino Output | BCD Bit |
| -------------- | ------: |
| BCD A / A0     |   Bit 0 |
| BCD B / A1     |   Bit 1 |
| BCD C / A2     |   Bit 2 |
| BCD D / A3     |   Bit 3 |

Several example mappings are included in the source code:

```cpp
byte bcdMap[4] = {0, 1, 2, 3}; // Normal
byte bcdMap[4] = {3, 2, 1, 0}; // Reversed
byte bcdMap[4] = {1, 0, 2, 3}; // A/B swapped
byte bcdMap[4] = {0, 1, 3, 2}; // C/D swapped
```

Only one `bcdMap` definition should be active at a time.

This feature was added because it allows the program to accommodate existing wiring, PCB layouts, or salvaged components without requiring the BCD connections to be rebuilt.

---

## User Controls

## Rotary Encoder

When the device is in normal mode, rotating the encoder changes the BPM.

The BPM range is:

```cpp
#define MINIMUM_BPM 20
#define MAXIMUM_BPM 300
#define DEFAULT_BPM 120
```

The encoder is configured to require two internal encoder counts per physical click:

```cpp
#define ENCODER_STEPS_PER_CLICK 2
```

This may need changing depending on the encoder model.

Possible values include:

```cpp
#define ENCODER_STEPS_PER_CLICK 1
#define ENCODER_STEPS_PER_CLICK 2
#define ENCODER_STEPS_PER_CLICK 4
```

If one physical click causes multiple BPM changes, increase the value.

If several clicks are needed before the BPM changes, decrease the value.

---

## Encoder Direction

The encoder direction has been deliberately reversed in the software.

In the current configuration:

* One direction decreases the selected value.
* The opposite direction increases the selected value.

The direction was changed to match the physical orientation of the encoder in the assembled device.

To reverse it again, swap the values assigned to `i` in the encoder-reading section:

```cpp
if (newPosition >= oldPosition + ENCODER_STEPS_PER_CLICK) {
    i = 1;
} else if (newPosition <= oldPosition - ENCODER_STEPS_PER_CLICK) {
    i = 2;
}
```

Alternatively, swap the two encoder signal wires connected to D2 and D3.

---

## Rotary Encoder Button

Pressing the rotary encoder button switches between two editing modes.

### BPM Editing Mode

In this mode:

* Rotating the encoder changes the BPM.
* The three-digit display shows the BPM.
* Pressing the encoder button enters divider editing mode.

### Divider Editing Mode

In this mode:

* Rotating the encoder changes the second sync-output divider.
* The two-digit divider display updates.
* Pressing the encoder button returns to BPM editing mode.

The BPM and divider values remain visible on their respective displays at all times.

The divider range is:

```text
2 to 64 MIDI clock pulses
```

---

## Start/Stop Button

A short press of the start/stop button toggles playback.

When starting, the device sends:

```cpp
MIDI.sendRealTime(midi::Start);
```

When stopping, the device sends:

```cpp
MIDI.sendRealTime(midi::Stop);
```

The analogue sync outputs and beat LED are also switched off when playback stops.

---

## Long-Press BPM Reset

Holding the start/stop button for at least one second resets the BPM to 120.

The hold time is configured by:

```cpp
#define LONG_PRESS_TIME 1000
```

The value is measured in milliseconds.

After resetting:

* BPM is set to 120.
* The timer interval is recalculated.
* The new BPM is stored in EEPROM.
* The displays are updated.
* The LED flashes briefly as confirmation.

A long press does not trigger the normal start/stop action when the button is released.

---

## MIDI Clock Operation

MIDI Clock uses 24 clock pulses per quarter note.

This is defined by:

```cpp
#define CLOCKS_PER_BEAT 24
```

The timer interval is calculated with:

```cpp
long interval = 60L * 1000 * 1000 / bpm / CLOCKS_PER_BEAT;
```

This calculation converts BPM into the number of microseconds between MIDI clock pulses.

For example, at 120 BPM:

```text
60,000,000 microseconds / 120 BPM / 24 pulses
= 20,833 microseconds per MIDI clock pulse
```

TimerOne generates an interrupt at this interval and calls:

```cpp
sendClockPulse();
```

Each interrupt sends:

```cpp
MIDI.sendRealTime(midi::Clock);
```

The timer period is recalculated whenever the BPM changes.

---

## Analogue Sync Outputs

The device provides two pulse outputs.

## Primary Sync Output

The primary output uses:

```cpp
#define AUDIO_SYNC 12
```

This produces a pulse every 12 MIDI clock pulses.

Because MIDI clock sends 24 pulses per quarter note, a setting of 12 produces two sync pulses per quarter note.

The primary sync output is connected to:

```text
Arduino Nano D6
```

---

## Secondary Sync Output

The second sync output uses the adjustable `audio_sync2` value.

It is connected to:

```text
Arduino Nano D8
```

The divider can be adjusted between 2 and 64.

A smaller value produces faster pulses.

A larger value produces slower pulses.

Examples:

| Divider | Result relative to MIDI clock     |
| ------: | --------------------------------- |
|       2 | Very fast pulse stream            |
|       6 | Four pulses per quarter note      |
|      12 | Two pulses per quarter note       |
|      24 | One pulse per quarter note        |
|      48 | One pulse every two quarter notes |
|      64 | Slower custom division            |

The precise musical division depends on the MIDI clock count and the receiving device’s expected sync format.

---

## Pulse Width

Each sync output is set HIGH when its counter reaches zero.

It is set LOW on the following MIDI clock interrupt.

This means the pulse width is equal to one MIDI clock interval.

As BPM increases, the pulse width becomes shorter.

As BPM decreases, the pulse width becomes longer.

---

## Beat Indicator LED

The LED on D7 flashes at the start of each beat.

The clock counts from 0 to 23 because there are 24 MIDI clock pulses per quarter note.

The LED switches on when the beat count reaches zero and switches off after the number of pulses configured by:

```cpp
#define BLINK_TIME 4
```

This creates a short visible flash at the beginning of each beat.

---

## EEPROM Storage

The device saves two settings:

| EEPROM Address | Stored Value           |
| -------------: | ---------------------- |
|            0–1 | BPM                    |
|            3–4 | Secondary sync divider |

Each value is stored as a two-byte integer.

On startup, the values are read and validated.

If the stored BPM is outside the valid range, it is replaced with 120.

If the stored divider is outside the valid range, it is replaced with 12.

This prevents uninitialised or corrupted EEPROM values from producing invalid clock settings.

### EEPROM Wear

The current program writes the value whenever BPM or divider settings are changed.

EEPROM has a limited number of write cycles. For normal manual use this is unlikely to cause an immediate problem, but rapidly and continuously rotating the encoder for long periods will create additional EEPROM writes.

A future improvement could delay EEPROM saving until the encoder has not moved for several seconds.

---

## Initialisation Process

When the Arduino starts, the sketch performs the following steps:

1. Starts the MIDI interface.
2. Disables MIDI Thru.
3. Reads BPM from EEPROM.
4. Validates the BPM.
5. Reads the sync-divider value from EEPROM.
6. Validates the divider.
7. Calculates the initial timer interval.
8. Starts TimerOne.
9. Attaches the MIDI clock interrupt.
10. Configures the buttons with internal pull-up resistors.
11. Configures the LED and sync outputs.
12. Configures the shared BCD and latch pins.
13. Writes the initial values to the displays.
14. Resets the encoder count.
15. Switches all pulse outputs off.

---

## Button Wiring

Both buttons use the Arduino’s internal pull-up resistors:

```cpp
pinMode(BUTTON_START, INPUT_PULLUP);
pinMode(BUTTON_ROTARY, INPUT_PULLUP);
```

Each button should therefore be wired between:

* Its assigned Arduino input pin
* Ground

The input reads:

* HIGH when the button is released
* LOW when the button is pressed

No external pull-up resistor is required.

---

## Rotary Encoder Wiring

A typical mechanical rotary encoder should be connected as follows:

| Encoder Connection | Arduino Nano |
| ------------------ | ------------ |
| Channel A          | D2           |
| Channel B          | D3           |
| Common             | Ground       |
| Push-button signal | D5           |
| Push-button common | Ground       |

Some encoder modules contain external pull-up resistors, while bare encoders may require pull-ups depending on the encoder library and hardware configuration.

The push-button input uses the Nano’s internal pull-up.

---

## CD4511 Wiring Notes

Each display digit requires one CD4511.

Typical CD4511 connections include:

* A, B, C, and D connected to the shared Arduino BCD bus
* Latch Enable connected to the digit’s individual Arduino latch pin
* Lamp Test held inactive
* Blanking Input held inactive
* Outputs connected to the corresponding seven-segment display segments
* Correct current-limiting resistors fitted
* Common-cathode display common connected to ground
* CD4511 power connected to 5 V and ground

Consult the datasheet for the exact pin numbers of the CD4511 version being used.

### Important Display Type

The CD4511 is intended for **common-cathode** seven-segment displays.

A common-anode display will not operate correctly with the CD4511 without additional inversion or driver circuitry.

---

## Display Resistors

Each display segment should have a current-limiting resistor.

A typical value is:

```text
220 Ω to 1 kΩ
```

The correct value depends on:

* LED colour
* Forward voltage
* Required brightness
* Display specifications
* Supply voltage
* CD4511 output-current limits

Using one resistor per segment is preferred.

Do not connect LED display segments directly without current limiting.

---

## MIDI Output Hardware

The sketch sends MIDI data using the default MIDI library serial interface.

A standard five-pin DIN MIDI output normally requires:

* Two current-limiting resistors
* A DIN socket
* Correct connection to the hardware serial transmit pin
* Grounding according to the MIDI electrical specification

On an Arduino Nano, hardware serial transmit is normally:

```text
D1 / TX
```

The exact circuit should follow a recognised MIDI OUT schematic.

Do not connect the Arduino TX pin directly to external MIDI equipment without the required output components.

---

## ATmega168 Upload Configuration

When using an ATmega168 Nano, select the appropriate processor option in the Arduino IDE.

Depending on the board package, this may appear as:

```text
Tools → Board → Arduino Nano
Tools → Processor → ATmega168
```

Some clone boards may require an older bootloader option or a third-party board definition.

If uploading fails:

* Confirm that the board actually contains an ATmega168.
* Check the USB-to-serial driver.
* Try the appropriate processor or bootloader option.
* Confirm the correct COM port.
* Disconnect hardware from D0 and D1 during upload if it interferes with serial communication.

---

## Memory Usage

Because this project targets the ATmega168, compiled memory usage should be checked after uploading.

The Arduino IDE reports:

* Program storage usage
* Dynamic memory usage

The program must remain below the board’s available Flash and SRAM limits.

If the compiled sketch is too large:

* Confirm that OLED libraries are no longer included.
* Disable unused MIDI features where supported by the library.
* Avoid adding large text strings.
* Avoid using the Arduino `String` class.
* Remove unused debug or serial-print code.
* Use smaller or more specialised libraries where possible.

---

## Operating Instructions

### Starting the Clock

1. Power on the device.
2. Confirm the BPM display shows the stored BPM.
3. Confirm the divider display shows the stored divider.
4. Press the start/stop button.
5. The device sends a MIDI Start message.
6. MIDI Clock and analogue sync pulses begin.
7. The beat LED flashes.

### Stopping the Clock

1. Press the start/stop button.
2. The device sends a MIDI Stop message.
3. Both sync outputs are set LOW.
4. The beat LED is switched off.

### Changing BPM

1. Ensure the device is in BPM editing mode.
2. Rotate the encoder.
3. The three-digit BPM display updates.
4. The timer period is updated immediately.
5. The new value is saved to EEPROM.

### Changing the Divider

1. Press the rotary encoder button.
2. Rotate the encoder.
3. The two-digit divider display updates.
4. Press the encoder button again to return to BPM mode.

### Resetting BPM

1. Hold the start/stop button for at least one second.
2. The BPM resets to 120.
3. The LED flashes as confirmation.
4. Release the button.
5. The start/stop state is not toggled by that release.

---

## Troubleshooting

## Display Shows Incorrect Numbers

Check the following:

* Confirm the display is common cathode.
* Confirm BCD A, B, C, and D are wired correctly.
* Confirm each latch pin goes to the correct CD4511.
* Confirm Lamp Test is held inactive.
* Confirm Blanking Input is held inactive.
* Confirm each segment is connected to the correct CD4511 output.
* Try changing the `bcdMap` order.
* Check that the latch-enable polarity matches the CD4511 being used.

If every digit is consistently wrong in the same way, the likely problem is the BCD bit order.

If only one digit is wrong, the likely problem is that digit’s latch, CD4511, display, or segment wiring.

---

## Display Only Updates After Pressing the Encoder

This may indicate:

* Encoder movement is not being detected reliably.
* `ENCODER_STEPS_PER_CLICK` is too high.
* The encoder pins are reversed or floating.
* The encoder common is not connected correctly.
* The button delay is masking encoder behaviour.
* The encoder library is accumulating counts but not reaching the configured threshold.

Try temporarily changing:

```cpp
#define ENCODER_STEPS_PER_CLICK 1
```

Also print `myEnc.read()` to the serial monitor during testing, provided serial MIDI output is temporarily disabled or moved to another interface.

---

## Encoder Changes Multiple Values Per Click

Increase:

```cpp
#define ENCODER_STEPS_PER_CLICK
```

For example, change it from 2 to 4.

Mechanical encoders commonly produce one, two, or four counts per detent depending on their internal construction.

---

## Encoder Direction Is Backwards

Either:

* Swap the D2 and D3 encoder wires, or
* Reverse the increase/decrease assignments in software.

---

## Clock Runs at the Wrong Speed

Check:

* The Arduino is running at 16 MHz.
* The correct Nano processor and bootloader are selected.
* `CLOCKS_PER_BEAT` is set to 24.
* The BPM display matches the actual `bpm` value.
* TimerOne supports the selected board configuration.
* The board is not using an unexpected 8 MHz clock source.

---

## No MIDI Output

Check:

* The MIDI output circuit.
* DIN socket pin numbering.
* D1/TX connection.
* Baud rate and MIDI library configuration.
* The receiving device’s MIDI sync settings.
* That MIDI Thru has not been accidentally enabled.
* That the receiving device responds to external MIDI Clock.

MIDI Clock messages are generated continuously by the interrupt in the current sketch, while the `playing` state controls the analogue sync outputs and beat LED.

Some equipment may ignore MIDI Clock until it receives a MIDI Start message.

---

## No Analogue Sync Output

Check:

* Playback has been started.
* D6 and D8 are connected to the expected sockets.
* The receiving device shares an appropriate ground reference.
* The pulse voltage is suitable for the receiving equipment.
* The selected divider is compatible with the receiving device.
* The receiving device expects positive-going 5 V pulses.

Use an LED with a suitable resistor, logic probe, oscilloscope, or multimeter with frequency mode to test the outputs.

---

## Possible Future Improvements

Potential improvements include:

* Proper non-blocking rotary-button debounce
* Delayed EEPROM saving to reduce write cycles
* Separate display-mode indicator LEDs
* MIDI Continue support
* Tap tempo
* Multiple selectable primary sync divisions
* Swing or shuffle clock
* External clock input
* MIDI input synchronisation
* Configurable pulse width
* Display blanking for leading zeroes
* Start/stop state display
* PCB design for the shared BCD bus
* Compile-time options for ATmega168 and ATmega328P
* Direct port writes to reduce interrupt overhead

---

## Credits

Original project:

**Arduino MIDI Master Clock v0.2**
Created by Eunjae Im
EJ Labs

Modified version:

* Adapted for an Arduino Nano
* Designed to support the smaller ATmega168 microcontroller
* Modified to use components already available
* OLED interface replaced with CD4511-driven seven-segment displays
* Encoder and control behaviour adjusted for the physical build


