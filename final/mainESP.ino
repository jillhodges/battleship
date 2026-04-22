// WORK IN PROGRESS!!!!!!!!!
// At the top, add this enum value to your existing GameState enum:
enum GameState { PLACEMENT, MENU, WAITING_PLAYERS, PLAYING, SHOW_RESULT, GAME_OVER };

// ─── In setup() ───────────────────────────────────────────────────────────────
// Change starting state to PLACEMENT
gameState = PLACEMENT;

// ─── In loop(), add this call ─────────────────────────────────────────────────
handleMegaSerial();

// ─── Add this function ────────────────────────────────────────────────────────
void handleMegaSerial() {
  if (Serial2.available()) {
    String msg = Serial2.readStringUntil('\n');
    msg.trim();

    if (msg.startsWith("PROMPT,")) {
      String prompt = msg.substring(7);
      drawPlacementPrompt(prompt);

    } else if (msg.startsWith("BOARD,")) {
      // optional - update LCD grid display
      // int player = msg.charAt(6) - '0';
      // String bits = msg.substring(8);

    } else if (msg == "HIT") {
      registerResult(true);

    } else if (msg == "MISS") {
      registerResult(false);
    }
  }
}

// ─── Add this function to draw prompts on the LCD ─────────────────────────────
void drawPlacementPrompt(String prompt) {
  // Clear prompt area only
  tft.fillRect(0, 40, 160, 90, ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(5, 55);
  tft.print(prompt);

  // If it says GAME STARTING, transition state
  if (prompt == "GAME STARTING") {
    gameState = MENU;
    drawMenu();
  }
}

// ─── In your existing PS4 button handler, add placement confirm/cancel ────────
void handlePS4() {
  if (!PS4.isConnected()) return;

  bool crossNow  = PS4.Cross();
  bool circleNow = PS4.Circle();

  // Placement phase controls
  if (gameState == PLACEMENT) {
    if (crossNow && !lastCrossP1) {
      Serial2.println("CONFIRM");
    }
    if (circleNow && !lastCircleP1) {
      Serial2.println("CANCEL");
    }

  // Menu / game controls (your existing code)
  } else if (gameState == MENU) {
    if (crossNow && !lastCrossP1) {
      gameState = WAITING_PLAYERS;
      drawWaitingScreen();
    }
  } else if (gameState == WAITING_PLAYERS) {
    if (crossNow && !lastCrossP1) {
      startGame();
    }
  } else if (gameState == GAME_OVER) {
    if (crossNow && !lastCrossP1) {
      resetGame();
    }
  }

  if (PS4.Options()) resetGame();

  lastCrossP1  = crossNow;
  lastCircleP1 = circleNow;  // make sure this variable is declared globally
}
