# Arduino IDE: STM32 speed/tank test

Open `arduino_speed_test.ino` in Arduino IDE, select Arduino Uno and its USB port,
and upload. There are no dependencies beyond the AVR core's SoftwareSerial.
This sketch replaces the Arduino bridge temporarily; it is not an STM32 program.
Do not run the Pi serial client while Serial Monitor is open.

## Prerequisites

The STM32 must have been rebuilt and flashed with FOC_CTRL, SPD_MODE,
TANK_STEERING in VARIANT_USART, and signed centered inputs. Editing config.h alone
does not change the running STM32. Keep the known D2/RX <- hoverboard TX and
D3/TX -> hoverboard RX wiring, common ground, and USART3 baud 115200.

The sketch defaults to 22-byte feedback with encoders. Enable ENABLE_ODOMETRY
in the STM32 build for this format. If deliberately testing existing 18-byte
feedback instead, set ENCODERS_IN_FEEDBACK=false in the sketch; encoder output
will be NA. This selects an exact packet format, not automatic detection.

## Monitor and commands

Set Serial Monitor to 115200 baud and Newline (Both NL & CR also works).
Raise the driven wheels. After fresh feedback appears, enter one line at a time:

| Input | Intended test |
|---|---|
| `0,0` | Zero targets; observe feedback |
| `10,0` | Left wheel only |
| `0,10` | Right wheel only |
| `10,10` | Equal forward targets |
| `10,20` | Unequal forward targets |
| `-10,-10` | Reverse targets |
| `-10,10` | Opposite wheel directions |
| `S` | Stop immediately |

Each nonzero command lasts at most five seconds, then sends zero. Allow it to
stop before the next test. Limits are +/-30 raw units. Missing feedback for
200 ms or invalid input stops both targets. Commands sent before valid feedback
are rejected; feedback returning does not restart old commands.

Example output (illustrative, not a measured result):

```
cmd_raw=10,20 echo=10,20 rpm=10,20 enc=123,8999 battery=42.00 age_ms=2
```

The fields are:
- cmd_raw: requested left/right normalized inputs currently being transmitted.
- echo: STM32's reported input1/input2 (left/right in tank mode).
- rpm: measured left/right RPM, with the right sign normalized as in serial_drive.
- enc: left/right raw Hall counters, wrapping modulo 9000; NOT ticks per revolution.
- battery: volts; age_ms: time since the last valid feedback packet.

## What to measure and report

For every test, save several lines after startup acceleration and before auto-stop:
1. Confirm the intended physical wheel moves in the intended direction.
2. Check echoed inputs settle to the requested inputs.
3. Record measured RPM for both wheels: mean/range over the settled interval,
   persistent oscillation, and response to zero. Do not expect exact agreement
   during acceleration or from every individual low-speed Hall measurement.
4. Confirm encoders increase for forward and decrease for reverse modulo 9000.
   Stationary wheels may show slight tick changes while settling.
5. Compare `10,10` against `10,20`: does right-wheel RPM approximately double
   while left-wheel RPM stays similar? This is an initial scaling check, not
   a complete test of closed-loop performance under load.

Commands are RAW inputs for this first test, not a final rad/s API. Nominally:

```
RPM_target = raw_input * effective_N_MOT_MAX / 1000
```

With effective N_MOT_MAX=1000 and default input calibration, raw 10 corresponds
to approximately 10 RPM. Keeping FLASH_WRITE_KEY unchanged means previously
saved input calibration and current/speed limits may override compiled defaults.
That is why we measure RPM instead of assuming this scale. No sketch packet can
read the saved limit directly. A mismatch requires investigating saved settings
and the flashed firmware; do not immediately change PID gains.

No Arduino PID runs here. STM32 performs speed regulation. The test retains a
finite five-second motion even if the PC disconnects; it has no continuous host
heartbeat. Stop on feedback loss acts independently. Zero input requests zero
speed but cannot guarantee instant mechanical stopping or confirm motor mode.

## Development verification

Compiled for Uno using the Arduino AVR core through a temporary PlatformIO
project. Host checks covered packet encoding/decoding, sign handling, command
limits, malformed input, five-second expiry, feedback timeout, and no automatic
restart. Physical speed accuracy and the Arduino IDE upload need hardware tests.
