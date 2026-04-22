#ifndef COMM_PROTOCOL_H
#define COMM_PROTOCOL_H

// ─── Message Types ────────────────────────────────────────────────────────────
// ESP32 → Mega
#define MSG_CONFIRM     0x01  // payload: [player]
#define MSG_CANCEL      0x02  // payload: [player]
#define MSG_GAME_START  0x03  // payload: none
#define MSG_TURN        0x04  // payload: [player]
#define MSG_RESULT      0x05  // payload: [player, isHit, row, col]
#define MSG_GAME_OVER   0x06  // payload: none
#define MSG_RESET       0x07  // payload: none

// Mega → ESP32
#define MSG_HIT         0x10  // payload: [row, col]
#define MSG_MISS        0x11  // payload: [row, col]
#define MSG_PROMPT      0x12  // payload: [promptCode]
#define MSG_BOARD       0x13  // payload: [player, b0, b1, ... b15] (17 bytes)

// ─── Prompt Codes (replaces strings) ─────────────────────────────────────────
#define PROMPT_P1_PLACE       0x01
#define PROMPT_P2_PLACE       0x02
#define PROMPT_P1_CONFIRM     0x03
#define PROMPT_P2_CONFIRM     0x04
#define PROMPT_P1_CANCELLED   0x05
#define PROMPT_P2_CANCELLED   0x06
#define PROMPT_P1_DONE        0x07
#define PROMPT_P2_DONE        0x08
#define PROMPT_GAME_STARTING  0x09
#define PROMPT_CELL_TAKEN     0x0A

// ─── Send helpers ─────────────────────────────────────────────────────────────

// Send a message with a fixed-size payload array
// Works on both HardwareSerial (Mega) and HardwareSerial (ESP32 Serial2)
template<typename T>
void sendMsg(T &serial, uint8_t msgType, uint8_t* payload, uint8_t payloadLen) {
  serial.write(msgType);
  serial.write(payloadLen);
  for (uint8_t i = 0; i < payloadLen; i++) {
    serial.write(payload[i]);
  }
}

// Convenience: send with no payload
template<typename T>
void sendMsgEmpty(T &serial, uint8_t msgType) {
  serial.write(msgType);
  serial.write((uint8_t)0);
}

// ─── Receive helper ───────────────────────────────────────────────────────────

struct ParsedMsg {
  bool     valid;
  uint8_t  type;
  uint8_t  len;
  uint8_t  payload[20];  // max payload size
};

// Call this every loop() - reads one complete message if available
// Returns valid=false if not enough bytes yet
template<typename T>
ParsedMsg receiveMsg(T &serial) {
  ParsedMsg msg = {false, 0, 0, {}};

  if (serial.available() < 2) return msg;  // need at least type + length

  // Peek at type and length without consuming
  // We need to check if full message has arrived
  // Use a static buffer to accumulate bytes
  static uint8_t buf[22];
  static uint8_t bufLen = 0;

  while (serial.available() && bufLen < sizeof(buf)) {
    buf[bufLen++] = serial.read();
  }

  if (bufLen < 2) return msg;  // still not enough

  uint8_t msgType = buf[0];
  uint8_t payLen  = buf[1];

  if (bufLen < (uint8_t)(2 + payLen)) return msg;  // payload not fully arrived yet

  // Full message received
  msg.valid = true;
  msg.type  = msgType;
  msg.len   = payLen;
  for (uint8_t i = 0; i < payLen; i++) {
    msg.payload[i] = buf[2 + i];
  }

  // Shift buffer left
  uint8_t consumed = 2 + payLen;
  bufLen -= consumed;
  memmove(buf, buf + consumed, bufLen);

  return msg;
}

#endif
