/*
 * Battleship ESP32 Main
 *
 * WIRING:
 *   ST7735:  CS=14  RST=15  DC=32
 *   Servos:  P1 Tilt=19  P1 Pan=18  P2 Tilt=25  P2 Pan=26
 *   Mega:    ESP32 RX2=16 ← Mega TX1 | ESP32 TX2=17 → Mega RX1
 */
void handlePromptCode(uint8_t code);
#include <Bluepad32.h>
#include <ESP32Servo.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include "CommProtocol.h"
uint8_t esp32SerialBuf[22];
uint8_t esp32SerialBufLen = 0;

// ─── TFT ──────────────────────────────────────────────────────────────────────
#define TFT_CS  14
#define TFT_RST 15
#define TFT_DC  32
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// ─── SERVOS ───────────────────────────────────────────────────────────────────
#define P1_TILT_PIN  19
#define P1_PAN_PIN   18
#define P2_TILT_PIN  25
#define P2_PAN_PIN   26
#define SERVO_MIN_US 900
#define SERVO_MAX_US 2100
#define DEADZONE     15
#define JOY_MIN     -511
#define JOY_MAX      511

Servo p1Tilt, p1Pan, p2Tilt, p2Pan;

// ─── MEGA UART ────────────────────────────────────────────────────────────────
#define MEGA_RX 16
#define MEGA_TX 17

// ─── GAME CONFIG ──────────────────────────────────────────────────────────────
#define SHOT_CLOCK_SECONDS 15
#define GAME_TIME_SECONDS  600
#define MAX_HITS           4

// ─── GAME STATE ───────────────────────────────────────────────────────────────
enum GameState {
  PLACEMENT,
  WAITING_START,
  AIMING,
  SHOW_RESULT,
  GAME_OVER
};
GameState gameState = PLACEMENT;

int  currentPlayer  = 1;
int  p1Hits         = 0;
int  p2Hits         = 0;
long gameTimeLeft   = GAME_TIME_SECONDS;
int  shotClock      = SHOT_CLOCK_SECONDS;
bool lastResult     = false;

unsigned long lastGameTick  = 0;
unsigned long lastShotTick  = 0;
unsigned long resultShownAt = 0;

// ─── CONTROLLER STATE ─────────────────────────────────────────────────────────
ControllerPtr controllers[BP32_MAX_GAMEPADS];
bool lastCrossP1  = false;
bool lastCrossP2  = false;
bool lastCircleP1 = false;
bool lastCircleP2 = false;

// ─── COLORS ───────────────────────────────────────────────────────────────────
#define C_BG      ST77XX_BLACK
#define C_WHITE   ST77XX_WHITE
#define C_YELLOW  ST77XX_YELLOW
#define C_RED     ST77XX_RED
#define C_BLUE    ST77XX_BLUE
#define C_GREEN   ST77XX_GREEN
#define C_CYAN    ST77XX_CYAN
#define C_MAGENTA ST77XX_MAGENTA
#define C_P1      ST77XX_CYAN
#define C_P2      ST77XX_MAGENTA

// ─────────────────────────────────────────────────────────────────────────────
//  BLUEPAD32 CALLBACKS
// ─────────────────────────────────────────────────────────────────────────────

void onConnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (controllers[i] == nullptr) {
      controllers[i] = ctl;
      if (i == 0) ctl->setColorLED(0, 255, 255);
      if (i == 1) ctl->setColorLED(255, 0, 255);
      Serial.print("Controller ");
      Serial.print(i + 1);
      Serial.println(" connected.");
      return;
    }
  }
}

void onDisconnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (controllers[i] == ctl) {
      controllers[i] = nullptr;
      centreServos(i);
      Serial.print("Controller ");
      Serial.print(i + 1);
      Serial.println(" disconnected.");
      return;
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
//  SERVO HELPERS
// ─────────────────────────────────────────────────────────────────────────────

int axisToPulse(int axisValue) {
  if (abs(axisValue) < DEADZONE) return 1500;
  return map(axisValue, JOY_MIN, JOY_MAX, SERVO_MIN_US, SERVO_MAX_US);
}

void centreServos(int playerIndex) {
  if (playerIndex == 0) {
    p1Tilt.writeMicroseconds(1500);
    p1Pan.writeMicroseconds(1500);
  } else {
    p2Tilt.writeMicroseconds(1500);
    p2Pan.writeMicroseconds(1500);
  }
}

void centreAllServos() {
  p1Tilt.writeMicroseconds(1500);
  p1Pan.writeMicroseconds(1500);
  p2Tilt.writeMicroseconds(1500);
  p2Pan.writeMicroseconds(1500);
}

// ─────────────────────────────────────────────────────────────────────────────
//  CONTROLLER PROCESSING
// ─────────────────────────────────────────────────────────────────────────────

void processControllers() {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    ControllerPtr ctl = controllers[i];
    if (ctl == nullptr || !ctl->isConnected()) continue;

    bool isActivePlayer = (gameState == AIMING) &&
                          ((i == 0 && currentPlayer == 1) ||
                           (i == 1 && currentPlayer == 2));

    // ── Placement: both controllers confirm/cancel ────────────────────────────
    if (gameState == PLACEMENT) {
      bool crossNow  = ctl->a();
      bool circleNow = ctl->b();

      if (i == 0) {
        if (crossNow  && !lastCrossP1)  {
          uint8_t p[] = {1};
          sendMsg(Serial2, MSG_CONFIRM, p, 1);
        }
        if (circleNow && !lastCircleP1) {
          uint8_t p[] = {1};
          sendMsg(Serial2, MSG_CANCEL, p, 1);
        }
        lastCrossP1  = crossNow;
        lastCircleP1 = circleNow;
      }
      if (i == 1) {
        if (crossNow  && !lastCrossP2)  {
          uint8_t p[] = {2};
          sendMsg(Serial2, MSG_CONFIRM, p, 1);
        }
        if (circleNow && !lastCircleP2) {
          uint8_t p[] = {2};
          sendMsg(Serial2, MSG_CANCEL, p, 1);
        }
        lastCrossP2  = crossNow;
        lastCircleP2 = circleNow;
      }
      continue;
    }

    // ── Waiting to start: either Cross starts game ────────────────────────────
    if (gameState == WAITING_START) {
      bool crossNow = ctl->a();
      if (i == 0 && crossNow && !lastCrossP1) startGame();
      if (i == 1 && crossNow && !lastCrossP2) startGame();
      if (i == 0) lastCrossP1 = crossNow;
      if (i == 1) lastCrossP2 = crossNow;
      continue;
    }

    // ── Game over: either Cross resets ────────────────────────────────────────
    if (gameState == GAME_OVER) {
      bool crossNow = ctl->a();
      if (i == 0 && crossNow && !lastCrossP1) resetGame();
      if (i == 1 && crossNow && !lastCrossP2) resetGame();
      if (i == 0) lastCrossP1 = crossNow;
      if (i == 1) lastCrossP2 = crossNow;
      continue;
    }

    // ── Aiming: only active player moves servos ───────────────────────────────
    if (isActivePlayer) {
      int tiltPulse = axisToPulse(ctl->axisY());
      int panPulse  = axisToPulse(ctl->axisRX());

      if (i == 0) {
        p1Tilt.writeMicroseconds(tiltPulse);
        p1Pan.writeMicroseconds(panPulse);
      } else {
        p2Tilt.writeMicroseconds(tiltPulse);
        p2Pan.writeMicroseconds(panPulse);
      }
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
//  GAME LOGIC
// ─────────────────────────────────────────────────────────────────────────────

void startGame() {
  gameState     = AIMING;
  currentPlayer = 1;
  p1Hits        = 0;
  p2Hits        = 0;
  gameTimeLeft  = GAME_TIME_SECONDS;
  shotClock     = SHOT_CLOCK_SECONDS;
  lastGameTick  = millis();
  lastShotTick  = millis();
  centreAllServos();
  drawHUD();

  sendMsgEmpty(Serial2, MSG_GAME_START);

  uint8_t t[] = {(uint8_t)currentPlayer};
  sendMsg(Serial2, MSG_TURN, t, 1);

  Serial.println("Game started!");
}

void registerResult(bool isHit, int row, int col) {
  lastResult    = isHit;
  gameState     = SHOW_RESULT;
  resultShownAt = millis();

  if (isHit) {
    if (currentPlayer == 1) p1Hits++;
    else                    p2Hits++;
    showHitGraphic();
  } else {
    showMissGraphic();
  }

  // Tell Mega to light LEDs
  uint8_t p[] = {
    (uint8_t)currentPlayer,
    (uint8_t)(isHit ? 1 : 0),
    (uint8_t)row,
    (uint8_t)col
  };
  sendMsg(Serial2, MSG_RESULT, p, 4);

  if (p1Hits >= MAX_HITS || p2Hits >= MAX_HITS) {
    endGame(currentPlayer, false);
  }
}

void registerTimeout() {
  lastResult    = false;
  gameState     = SHOW_RESULT;
  resultShownAt = millis();
  showMissGraphic();

  // Send miss with 0,0 to indicate timeout - no cell to light
  uint8_t p[] = {(uint8_t)currentPlayer, 0, 0, 0};
  sendMsg(Serial2, MSG_RESULT, p, 4);

  Serial.print("Player ");
  Serial.print(currentPlayer);
  Serial.println(" timed out.");
}

void nextTurn() {
  currentPlayer = (currentPlayer == 1) ? 2 : 1;
  shotClock     = SHOT_CLOCK_SECONDS;
  lastShotTick  = millis();
  gameState     = AIMING;
  centreServos(currentPlayer - 1);

  uint8_t t[] = {(uint8_t)currentPlayer};
  sendMsg(Serial2, MSG_TURN, t, 1);

  drawHUD();
  Serial.print("Player ");
  Serial.print(currentPlayer);
  Serial.println("'s turn.");
}

void endGame(int winner, bool timeUp) {
  gameState = GAME_OVER;
  centreAllServos();

  if (timeUp) winner = (p1Hits >= p2Hits) ? 1 : 2;

  sendMsgEmpty(Serial2, MSG_GAME_OVER);

  tft.fillScreen(C_BG);
  tft.setTextColor(C_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(20, 10);
  tft.print("GAME OVER!");

  if (timeUp) {
    tft.setTextColor(C_WHITE);
    tft.setTextSize(1);
    tft.setCursor(40, 35);
    tft.print("TIME'S UP!");
  }

  uint16_t wColor = (winner == 1) ? C_P1 : C_P2;
  tft.setTextColor(wColor);
  tft.setTextSize(2);
  tft.setCursor(15, 55);
  tft.print("P");
  tft.print(winner);
  tft.print(" WINS!");

  tft.setTextSize(1);
  tft.setTextColor(C_P1);
  tft.setCursor(5, 85);
  tft.print("P1 Hits: ");
  tft.print(p1Hits);

  tft.setTextColor(C_P2);
  tft.setCursor(85, 85);
  tft.print("P2 Hits: ");
  tft.print(p2Hits);

  tft.setTextColor(C_WHITE);
  tft.setCursor(20, 105);
  tft.print("Cross to play again");
}

void resetGame() {
  gameState     = PLACEMENT;
  p1Hits        = 0;
  p2Hits        = 0;
  currentPlayer = 1;
  gameTimeLeft  = GAME_TIME_SECONDS;
  shotClock     = SHOT_CLOCK_SECONDS;
  centreAllServos();
  sendMsgEmpty(Serial2, MSG_RESET);
  drawPlacementScreen();
}

// ─────────────────────────────────────────────────────────────────────────────
//  TIMERS
// ─────────────────────────────────────────────────────────────────────────────

void updateTimers() {
  unsigned long now = millis();

  if (gameState == AIMING && now - lastGameTick >= 1000) {
    lastGameTick = now;
    gameTimeLeft--;
    drawGameClock();
    if (gameTimeLeft <= 0) {
      endGame(0, true);
      return;
    }
  }

  if (gameState == AIMING && now - lastShotTick >= 1000) {
    lastShotTick = now;
    shotClock--;
    drawShotClock();
    if (shotClock <= 0) {
      registerTimeout();
      return;
    }
  }

  if (gameState == SHOW_RESULT && now - resultShownAt > 2000) {
  if (p1Hits < MAX_HITS && p2Hits < MAX_HITS) {
    nextTurn();
  }
}
  }
}

// ─────────────────────────────────────────────────────────────────────────────
//  SERIAL FROM MEGA
// ─────────────────────────────────────────────────────────────────────────────

void handleMegaSerial() {
  ParsedMsg msg = receiveMsg(Serial2, esp32SerialBuf, esp32SerialBufLen);
  if (!msg.valid) return;

  switch (msg.type) {

    case MSG_HIT:
      if (gameState == AIMING) {
        registerResult(true, msg.payload[0], msg.payload[1]);
      }
      break;

    case MSG_MISS:
      if (gameState == AIMING) {
        registerResult(false, msg.payload[0], msg.payload[1]);
      }
      break;

    case MSG_PROMPT:
      handlePromptCode(msg.payload[0]);
      break;

    case MSG_BOARD:
      // Reserved for optional LCD grid display
      break;

    default:
      Serial.print("Unknown msg: 0x");
      Serial.println(msg.type, HEX);
      break;
  }
}

void handlePromptCode(uint8_t code) {
  switch (code) {
    case PROMPT_P1_PLACE:
      drawPlacementPrompt("P1 PLACE SHIPS");      break;
    case PROMPT_P2_PLACE:
      drawPlacementPrompt("P2 PLACE SHIPS");      break;
    case PROMPT_P1_CONFIRM:
      drawPlacementPrompt("P1 X=OK  O=CANCEL");   break;
    case PROMPT_P2_CONFIRM:
      drawPlacementPrompt("P2 X=OK  O=CANCEL");   break;
    case PROMPT_P1_CANCELLED:
      drawPlacementPrompt("P1 CANCELLED");        break;
    case PROMPT_P2_CANCELLED:
      drawPlacementPrompt("P2 CANCELLED");        break;
    case PROMPT_P1_DONE:
      drawPlacementPrompt("P1 ALL PLACED");       break;
    case PROMPT_P2_DONE:
      drawPlacementPrompt("P2 ALL PLACED");       break;
    case PROMPT_GAME_STARTING:
      gameState = WAITING_START;
      drawWaitingStart();                         break;
    case PROMPT_CELL_TAKEN:
      drawPlacementPrompt("CELL TAKEN!");         break;
    default:
      break;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
//  DISPLAY
// ─────────────────────────────────────────────────────────────────────────────

void drawHUD() {
  tft.fillScreen(C_BG);
  drawScoreBar();
  drawGameClock();
  drawShotClock();
  drawPlayerTurn();
}

void drawScoreBar() {
  tft.fillRect(0, 0, 78, 18, 0x1082);
  tft.setTextColor(C_P1);
  tft.setTextSize(1);
  tft.setCursor(3, 5);
  tft.print("P1:");
  tft.setTextColor(C_WHITE);
  tft.print(p1Hits);

  tft.drawFastVLine(79, 0, 18, C_WHITE);

  tft.fillRect(80, 0, 80, 18, 0x1082);
  tft.setTextColor(C_P2);
  tft.setCursor(83, 5);
  tft.print("P2:");
  tft.setTextColor(C_WHITE);
  tft.print(p2Hits);
}

void drawGameClock() {
  tft.fillRect(0, 19, 85, 12, C_BG);
  long mins = gameTimeLeft / 60;
  long secs = gameTimeLeft % 60;

  uint16_t col = C_GREEN;
  if (gameTimeLeft <= 60)       col = C_RED;
  else if (gameTimeLeft <= 180) col = C_YELLOW;

  tft.setTextColor(col);
  tft.setTextSize(1);
  tft.setCursor(5, 21);
  tft.print("GAME:");
  if (mins < 10) tft.print("0");
  tft.print(mins);
  tft.print(":");
  if (secs < 10) tft.print("0");
  tft.print(secs);
}

void drawShotClock() {
  tft.fillRect(88, 19, 72, 12, C_BG);

  uint16_t col = C_GREEN;
  if (shotClock <= 5)        col = C_RED;
  else if (shotClock <= 10)  col = C_YELLOW;

  tft.setTextColor(col);
  tft.setTextSize(1);
  tft.setCursor(90, 21);
  tft.print("SHOT:");
  tft.print(shotClock);
  tft.print("s");
}

void drawPlayerTurn() {
  tft.fillRect(0, 33, 160, 50, C_BG);

  uint16_t pColor = (currentPlayer == 1) ? C_P1 : C_P2;
  tft.setTextColor(pColor);
  tft.setTextSize(2);
  tft.setCursor(20, 38);
  tft.print("PLAYER ");
  tft.print(currentPlayer);

  tft.setTextColor(C_WHITE);
  tft.setTextSize(1);
  tft.setCursor(35, 58);
  tft.print("AIM AND SHOOT!");

  tft.setTextColor(0x7BEF);
  tft.setCursor(25, 70);
  tft.print("L=TILT   R=PAN");
}

void showHitGraphic() {
  tft.fillRect(0, 33, 160, 95, C_BG);
  tft.fillCircle(80, 80, 38, C_RED);
  tft.fillCircle(80, 80, 26, C_YELLOW);
  tft.fillCircle(80, 80, 14, C_WHITE);
  tft.setTextColor(C_RED);
  tft.setTextSize(3);
  tft.setCursor(32, 70);
  tft.print("HIT!");
  tft.setTextSize(1);
  uint16_t pColor = (currentPlayer == 1) ? C_P1 : C_P2;
  tft.setTextColor(pColor);
  tft.setCursor(50, 112);
  tft.print("P");
  tft.print(currentPlayer);
  tft.print(" +1");
  drawScoreBar();
  drawGameClock();
}

void showMissGraphic() {
  tft.fillRect(0, 33, 160, 95, C_BG);
  tft.fillCircle(80, 80, 38, C_BLUE);
  tft.fillCircle(80, 80, 26, 0x02FF);
  tft.fillCircle(80, 80, 14, C_WHITE);
  tft.setTextColor(C_BLUE);
  tft.setTextSize(3);
  tft.setCursor(20, 70);
  tft.print("MISS!");
  tft.setTextSize(1);
  tft.setTextColor(C_WHITE);
  tft.setCursor(45, 112);
  tft.print("No points!");
  drawScoreBar();
  drawGameClock();
}

void drawPlacementScreen() {
  tft.fillScreen(C_BG);
  tft.setTextColor(C_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(18, 15);
  tft.print("BATTLESHIP");
  tft.drawFastHLine(0, 35, 160, C_WHITE);
  tft.setTextColor(C_WHITE);
  tft.setTextSize(1);
  tft.setCursor(15, 45);
  tft.print("Place your ships...");
}

void drawPlacementPrompt(const char* prompt) {
  tft.fillRect(0, 40, 160, 90, C_BG);
  tft.setTextColor(C_WHITE);
  tft.setTextSize(1);
  tft.setCursor(5, 55);
  tft.print(prompt);
  tft.setTextColor(0x7BEF);
  tft.setCursor(5, 100);
  tft.print("X=Confirm  O=Cancel");
}

void drawWaitingStart() {
  tft.fillScreen(C_BG);
  tft.setTextColor(C_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(18, 20);
  tft.print("BATTLESHIP");
  tft.setTextColor(C_GREEN);
  tft.setTextSize(1);
  tft.setCursor(20, 55);
  tft.print("All ships placed!");
  tft.setTextColor(C_WHITE);
  tft.setCursor(15, 75);
  tft.print("Press Cross to start");
}

// ─────────────────────────────────────────────────────────────────────────────
//  SETUP & LOOP
// ─────────────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, MEGA_RX, MEGA_TX);

  p1Tilt.attach(P1_TILT_PIN, SERVO_MIN_US, SERVO_MAX_US);
  p1Pan.attach(P1_PAN_PIN,   SERVO_MIN_US, SERVO_MAX_US);
  p2Tilt.attach(P2_TILT_PIN, SERVO_MIN_US, SERVO_MAX_US);
  p2Pan.attach(P2_PAN_PIN,   SERVO_MIN_US, SERVO_MAX_US);
  centreAllServos();

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(C_BG);

  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) controllers[i] = nullptr;
  BP32.setup(&onConnectedController, &onDisconnectedController);
  BP32.forgetBluetoothKeys();

  drawPlacementScreen();
  Serial.println("ESP32 ready.");
}

void loop() {
  bool updated = BP32.update();
  if (updated) processControllers();

  handleMegaSerial();
  updateTimers();

  vTaskDelay(1);
}
