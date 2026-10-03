/*
  Standalone Uno/Nano test for STM32 FOC + SPD_MODE + TANK_STEERING.
  Open this file in Arduino IDE. No ROS, Pi client, or Arduino PID required.

  D2 <- hoverboard TX, D3 -> hoverboard RX; retain the proven USART3 wiring.
  USB Serial Monitor: 115200 baud, Newline (Both NL & CR also works).
  Enter: 10,0   0,10   10,10   10,20   -10,-10   or S to stop.
  Commands are RAW normalized left/right inputs, limited here to +/-30.
  With effective N_MOT_MAX=1000 and identity input calibration, raw 10 ~10 RPM.
  Saved STM32 calibration/limits can change this relationship.

  Each command runs for at most 5 seconds. Repeat it to restart the timer.
  Missing feedback for 200 ms stops motion. No automatic restart on recovery.
  Test with wheels raised. Flash matching STM32 firmware before using this sketch.
*/
#include <Arduino.h>
#include <SoftwareSerial.h>
#include <stdlib.h>
#include <string.h>

// true requires ENABLE_ODOMETRY in the FLASHED STM32 image (22-byte feedback).
// false accepts the 18-byte RPM-only format and prints NA for encoder counts.
const bool ENCODERS_IN_FEEDBACK = true;
const uint8_t FRAME_BYTES = ENCODERS_IN_FEEDBACK ? 22 : 18;
const uint32_t RUN_MS = 5000;
const uint32_t FEEDBACK_TIMEOUT_MS = 200;
const int16_t MAX_COMMAND = 30;
SoftwareSerial hover(2, 3); // Arduino RX, TX

uint8_t frame[22], frameUsed = 0;
char line[32];
uint8_t lineUsed = 0;
bool discardLine = false, haveFeedback = false, running = false;
uint32_t lastFeedback = 0, lastSend = 0, lastPrint = 0, started = 0;
int16_t targetL = 0, targetR = 0;
int16_t echoL = 0, echoR = 0, rpmL = 0, rpmR = 0;
int16_t encL = 0, encR = 0, battery = 0;

uint16_t readWord(const uint8_t *p) {
  return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}
void writeWord(uint8_t *p, uint16_t value) {
  p[0] = value; p[1] = value >> 8;
}
void stopWheels() {
  targetL = targetR = 0; running = false;
}
bool feedbackHealthy(uint32_t now) {
  return haveFeedback && uint32_t(now - lastFeedback) <= FEEDBACK_TIMEOUT_MS;
}
void sendWheels() {
  uint8_t packet[8];
  writeWord(packet, 0xABCD);
  writeWord(packet + 2, uint16_t(targetL));
  writeWord(packet + 4, uint16_t(targetR));
  writeWord(packet + 6, 0xABCD ^ uint16_t(targetL) ^ uint16_t(targetR));
  hover.write(packet, sizeof(packet));
}
void readFeedback() {
  for (uint8_t budget = 0; budget < 64 && hover.available(); ++budget) {
    frame[frameUsed++] = uint8_t(hover.read());
    if (frameUsed < FRAME_BYTES) continue;
    uint16_t sum = 0;
    for (uint8_t i = 0; i < FRAME_BYTES - 2; i += 2) sum ^= readWord(frame + i);
    if (readWord(frame) == 0xABCD && sum == readWord(frame + FRAME_BYTES - 2)) {
      echoL = int16_t(readWord(frame + 2));
      echoR = int16_t(readWord(frame + 4));
      rpmL = int16_t(readWord(frame + 8));
      // Match the forward-positive sign convention of working serial_drive.
      rpmR = -int16_t(readWord(frame + 6));
      if (ENCODERS_IN_FEEDBACK) {
        encR = int16_t(readWord(frame + 10));
        encL = int16_t(readWord(frame + 12));
      }
      battery = int16_t(readWord(frame + (ENCODERS_IN_FEEDBACK ? 14 : 10)));
      lastFeedback = millis(); haveFeedback = true; frameUsed = 0;
    } else {
      memmove(frame, frame + 1, FRAME_BYTES - 1);
      frameUsed = FRAME_BYTES - 1;
    }
  }
}
bool parseValue(char *&p, int16_t &out) {
  while (*p == ' ' || *p == '\t') ++p;
  bool negative = false;
  if (*p == '-' || *p == '+') { negative = *p == '-'; ++p; }
  if (*p < '0' || *p > '9') return false;
  int value = 0;
  while (*p >= '0' && *p <= '9') {
    value = value * 10 + (*p++ - '0');
    if (value > MAX_COMMAND) return false;
  }
  while (*p == ' ' || *p == '\t') ++p;
  out = negative ? -value : value;
  return true;
}
void executeLine() {
  line[lineUsed] = '\0';
  if (!strcmp(line, "S") || !strcmp(line, "s")) {
    stopWheels(); sendWheels(); Serial.println(F("STOP")); return;
  }
  char *p = line;
  int16_t left, right;
  if (!parseValue(p, left) || *p++ != ',' || !parseValue(p, right) || *p) {
    stopWheels(); sendWheels();
    Serial.println(F("ERROR: enter two integers within -30..30, e.g. 10,0; or S."));
    return;
  }
  if (!feedbackHealthy(millis())) {
    stopWheels(); sendWheels(); Serial.println(F("REJECTED: no fresh feedback.")); return;
  }
  targetL = left; targetR = right;
  started = millis(); running = left != 0 || right != 0;
  sendWheels();
  Serial.println(F("ACCEPTED: five-second test started (0,0 stops)."));
}
void readMonitor() {
  for (uint8_t budget = 0; budget < 32 && Serial.available(); ++budget) {
    char c = char(Serial.read());
    if (c == '\n' || c == '\r') {
      if (discardLine) {
        stopWheels(); sendWheels(); Serial.println(F("ERROR: invalid/oversized line."));
      } else if (lineUsed) executeLine();
      lineUsed = 0; discardLine = false;
    } else if (!discardLine) {
      if (c == '\0' || lineUsed >= sizeof(line) - 1) {
        discardLine = true; stopWheels(); sendWheels();
      } else line[lineUsed++] = c;
    }
  }
}
void printFeedback(uint32_t now) {
  if (!feedbackHealthy(now)) {
    Serial.println(F("NO_FRESH_FEEDBACK: stopped; check power, wiring and 18/22-byte setting."));
    return;
  }
  Serial.print(F("cmd_raw=")); Serial.print(targetL); Serial.print(','); Serial.print(targetR);
  Serial.print(F(" echo=")); Serial.print(echoL); Serial.print(','); Serial.print(echoR);
  Serial.print(F(" rpm=")); Serial.print(rpmL); Serial.print(','); Serial.print(rpmR);
  Serial.print(F(" enc="));
  if (ENCODERS_IN_FEEDBACK) { Serial.print(encL); Serial.print(','); Serial.print(encR); }
  else Serial.print(F("NA,NA"));
  Serial.print(F(" battery=")); Serial.print(battery / 100.0, 2);
  Serial.print(F(" age_ms=")); Serial.println(uint32_t(now - lastFeedback));
}
void setup() {
  Serial.begin(115200); hover.begin(115200);
  for (uint8_t i = 0; i < 10; ++i) { sendWheels(); delay(20); }
  Serial.println(F("STM32 SPEED + TANK test; monitor 115200, Newline."));
  Serial.println(F("Raw L,R commands +/-30; five-second auto-stop; S stops now."));
  Serial.println(F("Expect ~1 RPM/raw unit ONLY with effective N_MOT_MAX=1000 and default calibration."));
}
void loop() {
  uint32_t now = millis();
  // Expire old motion before processing recovered feedback or a new command.
  if (running && (!feedbackHealthy(now) || uint32_t(now - started) >= RUN_MS)) {
    stopWheels(); sendWheels(); Serial.println(F("STOP: duration expired or feedback lost."));
  }
  readFeedback();
  readMonitor();
  now = millis();
  if (uint32_t(now - lastSend) >= 20) { lastSend = now; sendWheels(); }
  if (uint32_t(now - lastPrint) >= 200) { lastPrint = now; printFeedback(now); }
}
