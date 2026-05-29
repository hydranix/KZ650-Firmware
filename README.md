# KZ650 Firmware

Relay-control firmware for a Kawasaki KZ650, running on an Arduino Nano
(ATmega328P). It reads the bike's handlebar/control switches and drives a
bank of relays for the starter, ignition, lights, horn, and turn signals.

> [!WARNING]
> This code is **not tested** and **not considered safe** for use on public
> roads. It may not be safe in any context. Use entirely at your own risk —
> the author takes no responsibility for any use of this code on any vehicle,
> anywhere.

## Hardware

- **Board:** Arduino Nano, ATmega328P, 16 MHz (new / Optiboot bootloader)
- **Relays:** 8-channel, **active-LOW** (a `LOW` output energizes the relay)
- **Switch inputs:** wired to ground, using the MCU's internal pull-ups
  (a pressed/closed switch reads `LOW`)

### Pin map

| Function       | Pin | Direction | Notes                                   |
|----------------|-----|-----------|-----------------------------------------|
| Brake switch   | D2  | input     |                                         |
| Horn button    | D3  | input     |                                         |
| Left turn      | D4  | input     |                                         |
| Right turn     | D5  | input     |                                         |
| High beam      | D6  | input     |                                         |
| Clutch         | D7  | input     | starter interlock                       |
| Start button   | D8  | input     | starter interlock                       |
| Starter        | D9  | output    | Relay 1 — starter solenoid              |
| Ignition       | D10 | output    | Relay 2 — power to the Dyna and coils   |
| Brake light    | D11 | output    | Relay 3                                 |
| Horn           | D12 | output    | Relay 4                                 |
| Left signal    | A0  | output    | Relay 5                                 |
| Right signal   | A1  | output    | Relay 6                                 |
| Headlight low  | A2  | output    | Relay 7 — also rear running light       |
| Headlight high | A3  | output    | Relay 8                                 |

`D0`/`D1` are left free for the serial console. `A6`/`A7` are unused — on the
Nano they are analog-input only and cannot drive a relay.

## Behavior

- **Brake / horn / high beam** — pass the (debounced) switch straight through
  to their relay.
- **Turn signals / hazards** — blink at `BLINK_INTERVAL_MS` (50% duty) while
  the switch is held.
- **Headlight low beam** — driven on at boot and left on at all times.
- **Electric start** — the starter engages **only** while *both* the start
  button **and** the clutch are held; releasing either one cuts the starter.
- **Watchdog** — a 2 s hardware watchdog resets the MCU to its safe boot state
  (all relays off, low beam on) if `loop()` ever stalls.
- **Debounce** — every input must hold a new level for `DEBOUNCE_MS` before it
  is accepted, rejecting mechanical switch chatter.

## Configuration

Compile-time options at the top of `src/main.cpp`:

| Macro               | Default  | Purpose                                       |
|---------------------|----------|-----------------------------------------------|
| `DEBUG`             | off      | Uncomment to emit setup/debug logs to serial  |
| `BLINK_INTERVAL_MS` | `500`    | Turn-signal/hazard half-period (ms)           |
| `DEBOUNCE_MS`       | `20`     | Input debounce hold time (ms)                 |
| `SERIAL_BAUD_RATE`  | `115200` | Serial console baud rate                       |

## Build & flash

This is a [PlatformIO](https://platformio.org/) project.

```sh
pio run                 # build
pio run --target upload # build and flash a connected Nano
pio device monitor      # open the serial console (enable DEBUG first)
```

## License

Copyright Nik Hamilton &lt;NikHamiltonSr@gmail.com&gt;.
