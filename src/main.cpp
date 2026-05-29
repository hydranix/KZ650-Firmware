// Copyright Nik Hamilton <NikHamiltonSr@gmail.com>
//
// This code is not tested nor considered safe for
//   use on public motorways. I take no responsibility
//  for any use of this code on any vehicle in any way
//  anywhere.
//
//  This code may not be safe, use at your own risk.
//
//
//  Version 0.0.1.3

#include <Arduino.h>
#include <arduino-timer.h>
#include <avr/wdt.h>

#pragma region Config
//=================================// Config //===============================//

////////////////////////////////////////////////////////////////////////////////
// Debugging messages (optional)
// Enables debugging messages to be sent to serial console
//  otherwise that are completely excluded from the firmware
//
// #define DEBUG 1

////////////////////////////////////////////////////////////////////////////////
// Blinker blink rate in milliseconds (required)
//   The blinkers and hazards will cycle at this rate.
//  2 * BLINK_INTERVAL_MS = one cycle of the lights.
//   They blink with a 50% duty cycle
//  THis effects both front and rear signal lights
#define BLINK_INTERVAL_MS 500

////////////////////////////////////////////////////////////////////////////////
// Input debounce time in milliseconds (required)
//   An input must hold a new level this long before it is accepted,
//  rejecting mechanical switch chatter. Keep well below human reaction
//   time so response still feels instant.
#define DEBOUNCE_MS 20

////////////////////////////////////////////////////////////////////////////////
// Serial Baud rate (required)
// Example values: 9600, 19200, (default) 115200, 921600
#define SERIAL_BAUD_RATE 115200

/////////////////////////////////////////////////////////////////////////////////

#pragma endregion Config

////////////////////////////////////////////////////////////////////////////////
// For debug messaging to serial monitor
//  which wont be included in final build
#ifndef DEBUG
#define DBG(x)
#define DBGLN(x)
#else
#define DBG(x)       \
  do                 \
  {                  \
    Serial.print(x); \
  } while (0)
#define DBGLN(x)       \
  do                   \
  {                    \
    Serial.println(x); \
  } while (0)
#endif

// This is the type for pins
//   In the future we should make
//  this a class and use templates
//   for storing the physical pin
//  number
typedef const uint8_t Pin;

////////////////////////////////////////////////////////////////////////////////
// Inputs
//
Pin Brake{2};
Pin HornBtn{3};
Pin LeftTurn{4};
Pin RightTurn{5};
Pin HighBeam{6};
Pin Clutch{7};
Pin StartBtn{8};
Pin Inputs[]{Brake, HornBtn, LeftTurn, RightTurn, HighBeam, Clutch, StartBtn};

////////////////////////////////////////////////////////////////////////////////
// Debounced input state, indexed by pin number so it can be read by the
//   same names used everywhere else (e.g. input_state[Brake]). Only the
//  slots for the Inputs[] pins are ever touched.
byte input_state[NUM_DIGITAL_PINS];          // last accepted (debounced) level
byte input_last_raw[NUM_DIGITAL_PINS];        // last raw sample
unsigned long input_changed_at[NUM_DIGITAL_PINS]; // millis() of last raw change

// Re-sample every input and commit a new level only once it has been
//   stable for DEBOUNCE_MS. Call once per loop() before reading state.
void updateInputs()
{
  unsigned long now = millis();
  for (const auto &p : Inputs)
  {
    byte raw = digitalRead(p);
    if (raw != input_last_raw[p])
    {
      input_last_raw[p] = raw;
      input_changed_at[p] = now;
    }
    else if (now - input_changed_at[p] >= DEBOUNCE_MS)
    {
      input_state[p] = raw;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Outputs
//
Pin Starter{9};        // Relay 1 : starter solenoid
Pin Ignition{10};      // Relay 2 : power to the dyna and coils
Pin BrakeLight{11};    // Relay 3 : brake light
Pin HornOut{12};       // Relay 4 : horn
Pin LeftSignal{A0};    // Relay 5 : left turn signals
Pin RightSignal{A1};   // Relay 6 : right turn signals
Pin HeadLightLow{A2};  // Relay 7 : headlight and rear running lights
Pin HeadLightHigh{A3}; // Relay 8 : headlight high beam
Pin Outputs[]{Starter, Ignition, BrakeLight, HornOut, LeftSignal, RightSignal, HeadLightLow, HeadLightHigh};

////////////////////////////////////////////////////////////////////////////////
// Timer object which runs all of our tasks
Timer<2, millis> timer;

// function for timer tasks which need to toggle
//   an LED on/off. Pass pointer to LED value
bool onTimer_toggleLED(void *arg);

////////////////////////////////////////////////////////////////////////////////
// this value toggles at BLINK_INTERVAL_MS rate
byte blinker_value = 0;

////////////////////////////////////////////////////////////////////////////////
// indicator led
// blinks fast during setup() and then pulses* after setup
//
//   note*: pulse not yet implemented

//   TODO: use indicator led to communicate errors
//     when a certain pin is shorted to ground
// bool onTimer_indicator(void *arg);
byte indicator_led = 0;
void *indicator_task = nullptr;

void setup()
{
  // indicator led blinks fast while in setup
  indicator_task = timer.every(250UL, onTimer_toggleLED, &indicator_led);
  Serial.begin(SERIAL_BAUD_RATE);
#ifdef DEBUG
  // wait 4 seconds for a serial connection so
  //   we get all the debug messages in setup()
  {
    int32_t timeout = 4000;
    while (!Serial && timeout--)
      delay(1);
  }
#endif

  DBGLN("Serial up...");
  DBGLN("");
  DBGLN("Setting up GPIO pins...");
  DBGLN("");

  timer.cancel(indicator_task);
  indicator_task = timer.every(100, onTimer_toggleLED, &indicator_led);

  for (const auto &p : Inputs)
  {
    // preset the PORT latch (pullup) before switching to input
    //   so the pin is never left floating
    digitalWrite(p, HIGH);

    DBG("Pin ");
    DBG(p);
    DBG(" mode set to INPUT_PULLUP\n");
    pinMode(p, INPUT_PULLUP);

    // seed debounce state so we start from the real level, not 0
    input_state[p] = input_last_raw[p] = digitalRead(p);
    input_changed_at[p] = millis();
  }

  for (const auto &p : Outputs)
  {
    // preset the PORT latch HIGH before switching to output so the
    //   pin drives HIGH immediately (relays are active-LOW: no glitch)
    digitalWrite(p, HIGH);

    DBG("Pin ");
    DBG(p);
    DBG(" mode set to OUTPUT\n");
    pinMode(p, OUTPUT);
  }

  // for blinkers and hazards
  timer.every(BLINK_INTERVAL_MS, onTimer_toggleLED, &blinker_value);

  timer.cancel(indicator_task);
  indicator_task = nullptr;
  indicator_task = timer.every(1000, onTimer_toggleLED, &indicator_led);

  // turn on los beam headlight at all times
  digitalWrite(HeadLightLow, LOW);

  // arm the watchdog last, after all slow init is done. If loop()
  //   ever stalls for >2s the MCU resets to the safe boot state.
  //   (the new-bootloader Nano runs Optiboot, which clears the WDT
  //   on reset, so this can't latch into a reset loop)
  wdt_enable(WDTO_2S);
}

void loop()
{
  // pet the watchdog; if a pass ever hangs, the MCU resets
  wdt_reset();

  // required
  timer.tick();

  // refresh debounced input levels before acting on them
  updateInputs();

  //================================================
  //================ Brakes
  //================================================

  digitalWrite(BrakeLight, input_state[Brake]);

  //================================================
  //================ Horn
  //================================================

  digitalWrite(HornOut, input_state[HornBtn]);

  //================================================
  //================ Turn Signals/Hazards
  //================================================

  digitalWrite(LeftSignal, input_state[LeftTurn] ? HIGH : blinker_value);
  digitalWrite(RightSignal, input_state[RightTurn] ? HIGH : blinker_value);

  //================================================
  //================ Headlight
  //================================================

  digitalWrite(HeadLightHigh, input_state[HighBeam]);

  //================================================
  //================ Electric Start
  //================================================

  // Only allow the starter to actiavte if both the
  //   StartBtn and Clutch inputs are LOW
  //  This will cause the starter to stop (go HIGH)
  //   if either the start button or clutch is released
  digitalWrite(Starter, (input_state[StartBtn] || input_state[Clutch]) ? HIGH : LOW);
}

bool onTimer_toggleLED(void *arg)
{
  *static_cast<byte *>(arg) = !*static_cast<byte *>(arg);
  return true;
}
