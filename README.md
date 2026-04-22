PLAN:
- game logic fully completed after easter weekend
- wiring diagram
status as of 4/21, 8pm

TESTED:
- switches UPDATE: board does not work, directly to pins 22-54 in code
- shift registers (follow instructions in code to daisy chain - battleship_test.ino) <-- one is kind of buggy, need to ensure proper connection before soldering _ UPDATE no more see above
- LEDs
- beam breaks (no shift registers) - UPDATE CODE TO PULL FIRST READING, NOT ALL ROWS
- PS4 controller connected to ESP32 
- PS4 controller + Servos + ESP32
- ESP32 and Mega Comms - UPDATE: where is voltage divider circuit? UPDATE: found, needs to be added to defender board that holds esp32
- LCDs (see code for wiring information)

TO BE TESTED: 
- full code together (waiting to have mechanical set for half of board) - trial from just ai is in together 1
- working now on final, making sure will work

TO DO:
- get mechanical assembly working
