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

inline void sendMsg(HardwareSerial &serial, uint8_t msgType,
                    uint8_t* payload, uint8_t payloadLen) {
  serial.write(msgType);
  serial.write(payloadLen);
  for (uint8_t i = 0; i < payloadLen; i++)
    serial.write(payload[i]);
}

inline void sendMsgEmpty(HardwareSerial &serial, uint8_t msgType) {
  serial.write(msgType);
  serial.write((uint8_t)0);
}

inline void sendPrompt(HardwareSerial &serial, uint8_t promptCode) {
  uint8_t p[] = {promptCode};
  sendMsg(serial, MSG_PROMPT, p, 1);
}

inline void sendHit(HardwareSerial &serial, int row, int col) {
  uint8_t p[] = {(uint8_t)row, (uint8_t)col};
  sendMsg(serial, MSG_HIT, p, 2);
}

inline void sendMiss(HardwareSerial &serial, int row, int col) {
  uint8_t p[] = {(uint8_t)row, (uint8_t)col};
  sendMsg(serial, MSG_MISS, p, 2);
}

inline void sendBoard(HardwareSerial &serial, int player, bool ships[4][4]) {
  uint8_t p[17];
  p[0] = (uint8_t)player;
  int idx = 1;
  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 4; c++)
      p[idx++] = ships[r][c] ? 1 : 0;
  sendMsg(serial, MSG_BOARD, p, 17);
}

// ─── Receive ──────────────────────────────────────────────────────────────────

struct ParsedMsg {
  bool    valid;
  uint8_t type;
  uint8_t len;
  uint8_t payload[20];
};

inline ParsedMsg receiveMsg(HardwareSerial &serial, uint8_t* buf, uint8_t &bufLen) {
  ParsedMsg msg = {false, 0, 0, {}};

  while (serial.available() && bufLen < 22)
    buf[bufLen++] = serial.read();

  if (bufLen < 2) return msg;

  uint8_t payLen = buf[1];
  if (bufLen < (uint8_t)(2 + payLen)) return msg;

  msg.valid = true;
  msg.type  = buf[0];
  msg.len   = payLen;
  for (uint8_t i = 0; i < payLen; i++)
    msg.payload[i] = buf[2 + i];

  uint8_t consumed = 2 + payLen;
  bufLen -= consumed;
  memmove(buf, buf + consumed, bufLen);
  return msg;
}

#endif
