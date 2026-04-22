// WORK IN PROGRESS!!!!!!!!!
// At the top, add this enum value to your existing GameState enum:
/*
 * Battleship ESP32 Main
 * 
 * Handles:
 *   - Bluepad32 dual PS4 controllers
 *   - 2 servos per player (L stick = tilt up/down, R stick = pan left/right)
 *   - ST7735 LCD display
 *   - Game state machine (PLACEMENT → PLAYING → GAME_OVER)
 *   - Shot clock (30 seconds per turn)
 *   - Serial2 communication with Arduino Mega (hit/miss/confirm/cancel/prompts)
 * 
 * WIRING:
 *   ST7735:
 *     CS  → 14
 *     RST → 15
 *     DC  → 32
 *   Servos:
 *     P1 Tilt  (L stick Y) → GPIO 19
 *     P1 Pan   (R stick X) → GPIO 18
 *     P2 Tilt  (L stick Y) → GPIO 25
 *     P2 Pan   (R stick X) → GPIO 26
 *   Mega UART:
 *     ESP32 RX2 (GPIO 16) ← Mega TX1
 *     ESP32 TX2 (GPIO 17) → Mega RX1
 *     Shared GND
 */

#include <Bluepad32.h>
#include <ESP32Servo.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// ─── TFT Pins ─────────────────────────────────────────────────────────────────
#define TFT_CS   14
#define TFT_RST  15
#define TFT_DC   32
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// ─── Servo Pins ───────────────────────────────────────────────────────────────
#define P1_TILT_PIN   19   // P1 L stick Y → tilt up/down
#define P1_PAN_PIN    18   // P1 R stick X → pan left/right
#define P2_TILT_PIN   25   // P2 L stick Y → tilt up/down
#define P2_PAN_PIN    26   // P2 R stick X → pan left/right

#define SERVO_MIN_US  900
#define SERVO_MAX_US  2100
#define DEADZONE      15
#define JOY_MIN      -511
#define JOY_MAX       511

Servo p1Tilt, p1Pan, p2Tilt, p2Pan;

// ─── Mega UART ────────────────────────────────────────────────────────────────
#define MEGA_RX  16
#define MEGA_TX  17

// ─── Game Config ──────────────────────────────────────────────────────────────
#define SHOT_CLOCK_SECONDS  15
#define GAME_TIME_SECONDS   600   // 10 minutes
#define MAX_HITS            4     // 4 ships per player

// ─── Game State ───────────────────────────────────────────────────────────────
enum GameState {
  PLACEMENT,
  WAITING_START,
  AIMING,
  SHOW_RESULT,
  BETWEEN_TURNS,
  GAME_OVER
};
GameState gameState = PLACEMENT;

int  currentPlayer   = 1;
int  p1Hits          = 0;
int  p2Hits          = 0;
long gameTimeLeft    = GAME_TIME_SECONDS;
int  shotClock       = SHOT_CLOCK_SECONDS;
bool resultShowing   = false;
bool lastResult      = false;  // true = hit, false = miss

unsigned long lastGameTick  = 0;
unsigned long lastShotTick  = 0;
unsigned long resultShownAt = 0;



// ─── Controller State ─────────────────────────────────────────────────────────
ControllerPtr controllers[BP32_MAX_GAMEPADS];

bool lastCrossP1 = false;
bool lastCrossP2 = false;
bool lastCircleP1 = false;
bool lastCircleP2 = false;

// ─── Colors ───────────────────────────────────────────────────────────────────
#define C_BG       ST77XX_BLACK
#define C_WHITE    ST77XX_WHITE
#define C_YELLOW   ST77XX_YELLOW
#define C_RED      ST77XX_RED
#define C_BLUE     ST77XX_BLUE
#define C_GREEN    ST77XX_GREEN
#define C_CYAN     ST77XX_CYAN
#define C_MAGENTA  ST77XX_MAGENTA
#define C_P1       ST77XX_CYAN
#define C_P2       ST77XX_MAGENTA

// ─────────────────────────────────────────────────────────────────────────────
//  BLUEPAD32 CALLBACKS
// ─────────────────────────────────────────────────────────────────────────────

void onConnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (controllers[i] == nullptr) {
      controllers[i] = ctl;
      if (i == 0) ctl->setColorLED(0, 255, 255);    // cyan  = P1
      if (i == 1) ctl->setColorLED(255, 0, 255);    // magenta = P2
      Serial.printf("Controller %d connected.\n", i + 1);
      return;
    }
  }
}

void onDisconnectedController(ControllerPtr ctl) {
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (controllers[i] == ctl) {
      controllers[i] = nullptr;
      Serial.printf("Controller %d disconnected.\n", i + 1);
      centreServos(i);
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

    // Only the active player can aim
    bool isActivePlayer = (gameState == AIMING) &&
                          ((i == 0 && currentPlayer == 1) ||
                           (i == 1 && currentPlayer == 2));

    // ── Placement phase: both controllers can confirm/cancel ──
    if (gameState == PLACEMENT) {
      bool crossNow  = ctl->a();   // Cross in Bluepad32 = a()
      bool circleNow = ctl->b();   // Circle = b()

      if (i == 0) {
        if (crossNow  && !lastCrossP1)  { Serial2.println("CONFIRM"); }
        if (circleNow && !lastCircleP1) { Serial2.println("CANCEL");  }
        lastCrossP1  = crossNow;
        lastCircleP1 = circleNow;
      }
      if (i == 1) {
        if (crossNow  && !lastCrossP2)  { Serial2.println("CONFIRM"); }
        if (circleNow && !lastCircleP2) { Serial2.println("CANCEL");  }
        lastCrossP2  = crossNow;
        lastCircleP2 = circleNow;
      }
      continue;
    }

    // ── Waiting to start: Cross on either controller starts game ──
    if (gameState == WAITING_START) {
      bool crossNow = ctl->a();
      if (i == 0 && crossNow && !lastCrossP1) { startGame(); }
      if (i == 1 && crossNow && !lastCrossP2) { startGame(); }
      if (i == 0) lastCrossP1 = crossNow;
      if (i == 1) lastCrossP2 = crossNow;
      continue;
    }

    // ── Aiming phase: only active player moves servos and fires ──
    if (isActivePlayer) {
      int tiltAxis = ctl->axisY();   // L stick Y = tilt
      int panAxis  = ctl->axisRX();  // R stick X = pan

      int tiltPulse = axisToPulse(tiltAxis);
      int panPulse  = axisToPulse(panAxis);

      if (i == 0) {
        p1Tilt.writeMicroseconds(tiltPulse);
        p1Pan.writeMicroseconds(panPulse);
      } else {
        p2Tilt.writeMicroseconds(tiltPulse);
        p2Pan.writeMicroseconds(panPulse);
      }

      // R2 fires the shot
      // Bluepad32: throttle() maps R2 (0-1023), treat >500 as pressed
     
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
//  GAME LOGIC
// ─────────────────────────────────────────────────────────────────────────────

void startGame() {
  gameState      = AIMING;
  currentPlayer  = 1;
  p1Hits         = 0;
  p2Hits         = 0;
  gameTimeLeft   = GAME_TIME_SECONDS;
  shotClock      = SHOT_CLOCK_SECONDS;
  lastGameTick   = millis();
  lastShotTick   = millis();
  shotFired      = false;
  waitingForHit  = false;
  centreAllServos();
  drawHUD();
  Serial.println("Game started!");
  Serial2.println("GAME START");
  Serial2.print("TURN,");
  Serial2.println(currentPlayer);
}


void registerResult(bool isHit, int row, int col) {
  waitingForHit = false;
  shotFired     = false;
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
  // Format: RESULT,<player>,<hit=1/miss=0>,<row>,<col>
  Serial2.print("RESULT,");
  Serial2.print(currentPlayer);
  Serial2.print(",");
  Serial2.print(isHit ? "1" : "0");
  Serial2.print(",");
  Serial2.print(row);
  Serial2.print(",");
  Serial2.println(col);

  // Check win
  if (p1Hits >= MAX_HITS || p2Hits >= MAX_HITS) {
    endGame(currentPlayer, false);
    return;
  }
}

void registerTimeout() {
  // Shot clock ran out or no beam break detected in time
  waitingForHit = false;
  shotFired     = false;
  lastResult    = false;
  gameState     = SHOW_RESULT;
  resultShownAt = millis();

  showMissGraphic();

  Serial2.print("RESULT,");
  Serial2.print(currentPlayer);
  Serial2.println(",0,-1,-1");  // -1,-1 = timeout miss, no cell

  Serial.printf("Player %d timed out - automatic miss.\n", currentPlayer);
}

void nextTurn() {
  currentPlayer = (currentPlayer == 1) ? 2 : 1;
  shotClock     = SHOT_CLOCK_SECONDS;
  lastShotTick  = millis();
  gameState     = AIMING;
  centreServos(currentPlayer - 1);

  // Tell Mega whose turn it is now
  Serial2.print("TURN,");
  Serial2.println(currentPlayer);

  drawHUD();
}

void endGame(int winner, bool timeUp) {
  gameState = GAME_OVER;
  centreAllServos();

  if (timeUp) {
    winner = (p1Hits >= p2Hits) ? 1 : 2;
  }

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

  Serial2.println("GAME OVER");
}

void resetGame() {
  gameState     = PLACEMENT;
  p1Hits        = 0;
  p2Hits        = 0;
  currentPlayer = 1;
  gameTimeLeft  = GAME_TIME_SECONDS;
  shotClock     = SHOT_CLOCK_SECONDS;
  shotFired     = false;
  waitingForHit = false;
  centreAllServos();
  drawPlacementScreen();
  Serial2.println("RESET");
}

// ─────────────────────────────────────────────────────────────────────────────
//  TIMERS
// ─────────────────────────────────────────────────────────────────────────────

void updateTimers() {
  unsigned long now = millis();

  // Game clock - counts down during AIMING only
  if (gameState == AIMING && now - lastGameTick >= 1000) {
    lastGameTick = now;
    gameTimeLeft--;
    drawGameClock();
    if (gameTimeLeft <= 0) {
      endGame(0, true);
      return;
    }
  }

  // Shot clock - counts down during AIMING
  if (gameState == AIMING && now - lastShotTick >= 1000) {
    lastShotTick = now;
    shotClock--;
    drawShotClock();
    if (shotClock <= 0) {
      // Times up - counts as a miss, move to next player
      registerTimeout();
      return;
    }
  }

  // Auto-advance from result screen after 2 seconds
  if (gameState == SHOW_RESULT && now - resultShownAt > 2000) {
    nextTurn();
  }
}

// ─────────────────────────────────────────────────────────────────────────────
//  SERIAL FROM MEGA
// ─────────────────────────────────────────────────────────────────────────────

#include "CommProtocol.h"

// ─── Send functions ───────────────────────────────────────────────────────────

void sendConfirm(int player) {
  uint8_t p[] = {(uint8_t)player};
  sendMsg(Serial2, MSG_CONFIRM, p, 1);
}

void sendCancel(int player) {
  uint8_t p[] = {(uint8_t)player};
  sendMsg(Serial2, MSG_CANCEL, p, 1);
}

void sendGameStart() {
  sendMsgEmpty(Serial2, MSG_GAME_START);
}

void sendTurn(int player) {
  uint8_t p[] = {(uint8_t)player};
  sendMsg(Serial2, MSG_TURN, p, 1);
}

void sendResult(int player, bool isHit, int row, int col) {
  uint8_t p[] = {
    (uint8_t)player,
    (uint8_t)(isHit ? 1 : 0),
    (uint8_t)row,
    (uint8_t)col
  };
  sendMsg(Serial2, MSG_RESULT, p, 4);
}

void sendGameOver() {
  sendMsgEmpty(Serial2, MSG_GAME_OVER);
}

void sendReset() {
  sendMsgEmpty(Serial2, MSG_RESET);
}

// ─── Receive handler ──────────────────────────────────────────────────────────

void handleMegaSerial() {
  ParsedMsg msg = receiveMsg(Serial2);
  if (!msg.valid) return;

  switch (msg.type) {

    case MSG_HIT: {
      int row = msg.payload[0];
      int col = msg.payload[1];
      registerResult(true, row, col);
      Serial.print("HIT R");
      Serial.print(row);
      Serial.print("C");
      Serial.println(col);
      break;
    }

    case MSG_MISS: {
      int row = msg.payload[0];
      int col = msg.payload[1];
      registerResult(false, row, col);
      Serial.print("MISS R");
      Serial.print(row);
      Serial.print("C");
      Serial.println(col);
      break;
    }

    case MSG_PROMPT: {
      uint8_t promptCode = msg.payload[0];
      handlePromptCode(promptCode);
      break;
    }

    case MSG_BOARD:
      // Optional: use board state for LCD display
      break;

    default:
      Serial.print("Unknown msg: 0x");
      Serial.println(msg.type, HEX);
      break;
  }
}

// ─── Prompt code handler (replaces drawPlacementPrompt string version) ────────

void handlePromptCode(uint8_t code) {
  switch (code) {
    case PROMPT_P1_PLACE:
      drawPlacementPrompt("P1 PLACE SHIPS");
      break;
    case PROMPT_P2_PLACE:
      drawPlacementPrompt("P2 PLACE SHIPS");
      break;
    case PROMPT_P1_CONFIRM:
      drawPlacementPrompt("P1 X=OK  O=CANCEL");
      break;
    case PROMPT_P2_CONFIRM:
      drawPlacementPrompt("P2 X=OK  O=CANCEL");
      break;
    case PROMPT_P1_CANCELLED:
      drawPlacementPrompt("P1 CANCELLED");
      break;
    case PROMPT_P2_CANCELLED:
      drawPlacementPrompt("P2 CANCELLED");
      break;
    case PROMPT_P1_DONE:
      drawPlacementPrompt("P1 ALL PLACED");
      break;
    case PROMPT_P2_DONE:
      drawPlacementPrompt("P2 ALL PLACED");
      break;
    case PROMPT_GAME_STARTING:
      gameState = WAITING_START;
      drawWaitingStart();
      break;
    case PROMPT_CELL_TAKEN:
      drawPlacementPrompt("CELL TAKEN!");
      break;
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
  tft.fillRect(0, 19, 160, 12, C_BG);
  long mins = gameTimeLeft / 60;
  long secs = gameTimeLeft % 60;

  uint16_t col = C_GREEN;
  if (gameTimeLeft <= 60)  col = C_RED;
  else if (gameTimeLeft <= 180) col = C_YELLOW;

  tft.setTextColor(col);
  tft.setTextSize(1);
  tft.setCursor(5, 21);
  tft.print("GAME: ");
  if (mins < 10) tft.print("0");
  tft.print(mins);
  tft.print(":");
  if (secs < 10) tft.print("0");
  tft.print(secs);
}

void drawShotClock() {
  tft.fillRect(90, 19, 70, 12, C_BG);

  uint16_t col = C_GREEN;
  if (shotClock <= 5)  col = C_RED;
  else if (shotClock <= 10) col = C_YELLOW;

  tft.setTextColor(col);
  tft.setTextSize(1);
  tft.setCursor(95, 21);
  tft.print("SHOT:");
  tft.print(shotClock);
  tft.print("s");
}

void drawPlayerTurn() {
  tft.fillRect(0, 33, 160, 40, C_BG);

  uint16_t pColor = (currentPlayer == 1) ? C_P1 : C_P2;
  tft.setTextColor(pColor);
  tft.setTextSize(2);
  tft.setCursor(20, 38);
  tft.print("PLAYER ");
  tft.print(currentPlayer);

  tft.setTextColor(C_WHITE);
  tft.setTextSize(1);
  tft.setCursor(35, 58);
  tft.print("AIM AND FIRE!");

  tft.setTextColor(0x7BEF);
  tft.setTextSize(1);
  tft.setCursor(20, 70);
  tft.print(L=TILT  R=PAN");
}


void showHitGraphic() {
  tft.fillRect(0, 33, 160, 95, C_BG);

  // Explosion rings
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

  // Splash rings
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

void drawPlacementPrompt(String prompt) {
  tft.fillRect(0, 40, 160, 90, C_BG);
  tft.setTextColor(C_WHITE);
  tft.setTextSize(1);

  // Word wrap manually for long prompts
  tft.setCursor(5, 55);
  if (prompt.length() > 20) {
    tft.println(prompt.substring(0, 20));
    tft.setCursor(5, 68);
    tft.println(prompt.substring(20));
  } else {
    tft.println(prompt);
  }

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

  // Servos
  p1Tilt.attach(P1_TILT_PIN, SERVO_MIN_US, SERVO_MAX_US);
  p1Pan.attach(P1_PAN_PIN,   SERVO_MIN_US, SERVO_MAX_US);
  p2Tilt.attach(P2_TILT_PIN, SERVO_MIN_US, SERVO_MAX_US);
  p2Pan.attach(P2_PAN_PIN,   SERVO_MIN_US, SERVO_MAX_US);
  centreAllServos();

  // Display
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(C_BG);

  // Bluepad32
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) controllers[i] = nullptr;
  BP32.setup(&onConnectedController, &onDisconnectedController);
  BP32.forgetBluetoothKeys();

  drawPlacementScreen();
  Serial.println("ESP32 Battleship ready.");
}

void loop() {
  bool updated = BP32.update();
  if (updated) processControllers();

  handleMegaSerial();
  updateTimers();

  vTaskDelay(1);
}
