# Arduino-S88
Control a S88 feedback bus with arduino

Default pinout for the S88 bus

| Arduino pin | Atmega32u4 pin | Atmega328p pin | S88 name | S88 flat pin | S88n pin |
|-------------|----------------|----------------|----------|--------------|----------|
| D11         | PB7            | PB3            | Data     | 1            | 2        |
|             |                |                | Gnd      | 2            | 3/5      |
| D10         | PB6            | PB2            | Clk      | 3            | 4        |
| D9          | PB5            | PB1            | Load     | 4            | 6        |
| D8          | PB4            | PB0            | Reset    | 5            | 7        |
| D7          | PE6            |                | Pwr      | 6            |          |

## Clock speed

The clock speed of the S88 bus is determined by the AVR Clock, usually 8 or 16 MHz, the prescaler, set to /64, and the user configurable number of clock ticks, set with setClock(), divided by two:

F_S88 = F_CPU/128/ticks

On a 16 MHz cpu that results in a range of 125 kHz (ticks = 1) to 2 Hz (ticks = 65535);
