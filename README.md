# Arduino-S88
Control a S88 feedback bus with arduino

Default pinout for the S88 bus

| Arduino pin | Attiny84 pin | Atmega328p pin | Atmega32u4 pin | S88 name | S88 flat pin | S88n pin |
|-------------|--------------|----------------|----------------|----------|--------------|----------|
| D11         | PA3          | PB4            | PB7            | Data     | 1            | 2        |
|             |              |                |                | Gnd      | 2            | 3/5      |
| D10         | PA2          | PB2            | PB6            | Clk      | 3            | 4        |
| D9          | PA1          | PB1            | PB5            | Load     | 4            | 6        |
| D8          | PA0          | PB0            | PB4            | Reset    | 5            | 7        |
| D7          | PA4          | PB5            | PE6            | Pwr      | 6            |          |

The S88 bus currently requires a 16 bit timer.
