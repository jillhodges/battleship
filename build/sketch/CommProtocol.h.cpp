#line 1 "h:\\Shared drives\\AME40463_Spring2026_Group08\\Electrical\\Jason_Git_Repository\\battleship\\lib\\CommProtocol.h"
#ifndef COMM_PROTOCOL_H
#define COMM_PROTOCOL_H

#include <Arduino.h>

// ─── Message Types ────────────────────────────────────────────────────────────
#define MSG_CONFIRM     0x01
#define MSG_CANCEL      0x02
#define MSG_GAME_START  0x03
#define MSG_TURN        0x04
#define MSG_RESULT      0x05
#define MSG_GAME_OVER   0x06
#define MSG_RESET       0x07
#define MSG_HIT         0x10
#define MSG_MISS        0x11
#define MSG_PROMPT      0x12
#define MSG_BOARD       0x13

// ─── Prompt Codes ─────────────────────────────────────────────────────────────
#define PROMPT_P1_PLACE      0x01
#define PROMPT_P2_PLACE      0x02
#define PROMPT_P1_CONFIRM    0x03
#define PROMPT_P2_CONFIRM    0x04
#define PROMPT_P1_CANCELLED  0x05
#define PROMPT_P2_CANCELLED  0x06
#define PROMPT_P1_DONE       0x07
#define PROMPT_P2_DONE       0x08
#define PROMPT_GAME_STARTING 0x09
#define PROMPT_CELL_TAKEN    0x0A

// ─── Send helpers ─────────────────────────────────────────────────────────────
// ─── Data Structure (MUST BE ABOVE FUNCTIONS) ─────────────────────────────────
struct ParsedMsg {
  bool    valid;
  uint8_t type;
  uint8_t len;
  uint8_t payload[20];
};

// ─── Send helpers ─────────────────────────────────────────────────────────────
inline void sendMsg(HardwareSerial &serial, uint8_t msgType, uint8_t* payload, uint8_t payloadLen) {
  serial.write(msgType);
  serial.write(payloadLen);
  for (uint8_t i = 0; i < payloadLen; i++) serial.write(payload[i]);
}

inline void sendMsgEmpty(HardwareSerial &serial, uint8_t msgType) {
  serial.write(msgType);
  serial.write((uint8_t)0);
}

inline void sendPrompt(HardwareSerial &serial, uint8_t promptCode) {
  uint8_t p[] = {promptCode};
  sendMsg(serial, MSG_PROMPT, p, 1);
}

// ─── Receive ──────────────────────────────────────────────────────────────────
inline ParsedMsg receiveMsg(HardwareSerial &serial, uint8_t* buf, uint8_t &bufLen) {
  ParsedMsg msg = {false, 0, 0, {}};
  while (serial.available() && bufLen < 22) buf[bufLen++] = serial.read();
  if (bufLen < 2) return msg;

  uint8_t payLen = buf[1];
  if (bufLen < (uint8_t)(2 + payLen)) return msg;

  msg.valid = true;
  msg.type  = buf[0];
  msg.len   = payLen;
  for (uint8_t i = 0; i < payLen; i++) msg.payload[i] = buf[2 + i];

  uint8_t consumed = 2 + payLen;
  bufLen -= consumed;
  memmove(buf, buf + consumed, bufLen);
  return msg;
}

#endif
#line 1 "h:\\Shared drives\\AME40463_Spring2026_Group08\\Electrical\\Jason_Git_Repository\\battleship\\lib\\new_esp32_code.ino"
#include <Bluepad32.h>
#include <constants.h>
#include <Controller.h>
#include <ControllerData.h>
#include <ControllerProperties.h>
#include <Gamepad.h>
#include <GamepadProperties.h>


/*
 * Battleship ESP32 - DUAL SCREEN REMOTE HUB
 * Features: 
 * - Dual ST7735 Screens on a shared SPI bus
 * - 4 Servos (Pan/Tilt for P1 and P2)
 * - PS4 Controller mapping via Bluepad32
 * - No game logic (Logic is on the Mega)
 */

#include <Bluepad32.h>
#include <ESP32Servo.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include "CommProtocol.h" 

// ADD THESE 3 LINES HERE:
struct ParsedMsg; 
void onConnectedController(ControllerPtr ctl);
void onDisconnectedController(ControllerPtr ctl);
//

// ─── UART CONFIG (To Mega) ───────────────────────────────────────────────────
uint8_t esp32SerialBuf[22];
uint8_t esp32SerialBufLen = 0;
#define MEGA_RX 16
#define MEGA_TX 17

// ─── DUAL TFT CONFIG ─────────────────────────────────────────────────────────
#define TFT_RST  15  // Shared
#define TFT_DC   32  // Shared
#define TFT_CS1  14  // Unique to P1
#define TFT_CS2  27  // Unique to P2

Adafruit_ST7735 tft1 = Adafruit_ST7735(TFT_CS1, TFT_DC, TFT_RST);
Adafruit_ST7735 tft2 = Adafruit_ST7735(TFT_CS2, TFT_DC, TFT_RST);

// ─── SERVO CONFIG ────────────────────────────────────────────────────────────
#define P1_TILT_PIN  19
#define P1_PAN_PIN   18
#define P2_TILT_PIN  25
#define P2_PAN_PIN   26
#define SERVO_MIN_US 900
#define SERVO_MAX_US 2100
#define DEADZONE     20

Servo p1Tilt, p1Pan, p2Tilt, p2Pan;

// ─── STATE VARIABLES ─────────────────────────────────────────────────────────
ControllerPtr controllers[BP32_MAX_GAMEPADS];
int activePlayer = 1; // Updated by Mega via MSG_TURN
bool lastCross[2] = {false, false}; 

// ─────────────────────────────────────────────────────────────────────────────
//  INPUT PROCESSING (Joysticks & Buttons)
// ─────────────────────────────────────────────────────────────────────────────

int axisToPulse(int axisValue) {
    if (abs(axisValue) < DEADZONE) return 1500;
    return map(axisValue, -511, 511, SERVO_MIN_US, SERVO_MAX_US);
}

void processInputs() {
    for (int i = 0; i < 2; i++) {
        ControllerPtr ctl = controllers[i];
        if (ctl == nullptr || !ctl->isConnected()) continue;

        // ONLY the active player can move their servos
        if ((i + 1) == activePlayer) {
            if (activePlayer == 1) {
                p1Tilt.writeMicroseconds(axisToPulse(ctl->axisY()));
                p1Pan.writeMicroseconds(axisToPulse(ctl->axisRX()));
            } else {
                p2Tilt.writeMicroseconds(axisToPulse(ctl->axisY()));
                p2Pan.writeMicroseconds(axisToPulse(ctl->axisRX()));
            }

            // Fire Button Logic
            bool crossNow = ctl->a();
            if (crossNow && !lastCross[i]) {
                uint8_t p[] = {(uint8_t)activePlayer};
                sendMsg(Serial2, MSG_CONFIRM, p, 1);
                Serial.printf("P%d SHOT FIRED\n", activePlayer);
            }
            lastCross[i] = crossNow;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  SCREEN UPDATES
// ─────────────────────────────────────────────────────────────────────────────

void clearScreens() {
    tft1.fillScreen(ST77XX_BLACK);
    tft2.fillScreen(ST77XX_BLACK);
}

void showStatus(const char* msg, uint16_t color) {
    clearScreens();
    // Update P1 Screen
    tft1.setCursor(10, 50);
    tft1.setTextColor(activePlayer == 1 ? color : ST77XX_WHITE);
    tft1.print(activePlayer == 1 ? msg : "WAITING...");
    
    // Update P2 Screen
    tft2.setCursor(10, 50);
    tft2.setTextColor(activePlayer == 2 ? color : ST77XX_WHITE);
    tft2.print(activePlayer == 2 ? msg : "WAITING...");
}

// ─────────────────────────────────────────────────────────────────────────────
//  COMMUNICATION (Handling Mega Commands)
// ─────────────────────────────────────────────────────────────────────────────

void handleMegaComm() {
    ParsedMsg msg = receiveMsg(Serial2, esp32SerialBuf, esp32SerialBufLen);
    if (!msg.valid) return;

    switch (msg.type) {
        case MSG_TURN:
            activePlayer = msg.payload[0];
            showStatus("YOUR TURN", ST77XX_GREEN);
            break;

        case MSG_HIT:
            showStatus("HIT!", ST77XX_RED);
            break;

        case MSG_MISS:
            showStatus("MISS", ST77XX_BLUE);
            break;
            
        case MSG_RESET:
            activePlayer = 1;
            showStatus("RESETTING", ST77XX_YELLOW);
            break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  BLUEPAD32 CALLBACKS
// ─────────────────────────────────────────────────────────────────────────────

// turns controller colors to red or blue based on player 1/2
void onConnectedController(ControllerPtr ctl) {
    for (int i = 0; i < 2; i++) {
        if (controllers[i] == nullptr) {
            controllers[i] = ctl;
            Serial.printf("Controller %d connected\n", i + 1);

            if (i == 0) {
                // Player 1: ND GOLD
                // (High Red + Medium Green = Golden Yellow)
                ctl->setColorLED(255, 215, 0); 
            } else {
                // Player 2: ND GREEN
                // (No Red, Full Green, No Blue)
                ctl->setColorLED(0, 255, 0);
            }
            break;
        }
    }
}

void onDisconnectedController(ControllerPtr ctl) {
    for (int i = 0; i < 2; i++) {
        if (controllers[i] == ctl) controllers[i] = nullptr;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  SETUP & LOOP
// ─────────────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    Serial2.begin(9600, SERIAL_8N1, MEGA_RX, MEGA_TX);

    // Init Servos
    p1Tilt.attach(P1_TILT_PIN, SERVO_MIN_US, SERVO_MAX_US);
    p1Pan.attach(P1_PAN_PIN, SERVO_MIN_US, SERVO_MAX_US);
    p2Tilt.attach(P2_TILT_PIN, SERVO_MIN_US, SERVO_MAX_US);
    p2Pan.attach(P2_PAN_PIN, SERVO_MIN_US, SERVO_MAX_US);
    
    // Init Screens
    tft1.initR(INITR_BLACKTAB);
    tft2.initR(INITR_BLACKTAB);
    tft1.setRotation(1);
    tft2.setRotation(1);
    showStatus("READY", ST77XX_CYAN);

    BP32.setup(&onConnectedController, &onDisconnectedController);
}

void loop() {
    BP32.update();
    processInputs();
    handleMegaComm();
    vTaskDelay(1);
}
