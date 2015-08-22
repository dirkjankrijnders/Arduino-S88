# Arduino-S88
Control a S88 feedback bus with arduino

Default pinout for the S88 bus

| Arduino pin | Atmega32u4 pin | S88 name | S88 flat pin | S88n pin |
|-------------|----------------|----------|--------------|----------|
| D11         | PB7            | Data     | 1            | 2        |
|             |                | Gnd      | 2            | 3/5      |
| D10         | PB6            | Clk      | 3            | 4        |
| D9          | PB5            | Load     | 4            | 6        |
| D8          | PB4            | Reset    | 5            | 7        |
| D7          | PE6            | Pwr      | 6            |          |
