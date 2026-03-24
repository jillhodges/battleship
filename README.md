PLAN:
- wednesday 3/25: ESP32 and Mega Comms work, LCDs tested
- thursday 3/26: main code running
- friday 3/27: begin making neat, hardware integration and wire routing

status as of 3/24, 6pm

TESTED:
- switches (connect shift register to C, NC to GND) - need to solder second group <--- waiting on mechanical switch placement to solder board, protoype still together
- shift registers (follow instructions in code to daisy chain - battleship_test.ino) <-- one is kind of buggy, need to ensure proper connection before soldering
- LEDs
- beam breaks (no shift registers yet)
- PS4 controller connected to ESP32
- PS4 controller + Servos + ESP32

IN PROGRESS:
- ESP32 and Mega Comms (still not getting ps4 controller data from the esp32 to mega using RX/TX and voltage divider), voltage divider circuit still set up, will do more research tonight

TO BE TESTED:
- LCDs (need to update code, no longer i2c communication)

TO BE ORDERED:
- logic level shifter (maybe) <- still having problems with communication between ESP32 and MEGA
