#include <Arduino.h>
#include <SPI.h>
#include <ILI9341_t3.h>

// ======================================================
// TFT PIN MAP
// ======================================================
#define TFT_CS   10
#define TFT_DC    9
#define TFT_RST   8

ILI9341_t3 tft(TFT_CS, TFT_DC, TFT_RST);

// ======================================================
// BUTTON PIN
// ======================================================
#define TEST_BUTTON_PIN 2

bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;
unsigned long lastDebounceTime = 0;
constexpr unsigned long debounceDelayMs = 35;

// ======================================================
// RADAR UART SETTINGS
// ======================================================
#define RADAR_SERIAL Serial1
constexpr uint32_t RADAR_BAUD = 256000;

// ======================================================
// DISPLAY GEOMETRY
// ======================================================
constexpr int RADAR_CX = 160;
constexpr int RADAR_CY = 220;
constexpr int RADAR_R  = 180;

constexpr float THETA_MIN_DEG = -60.0f;
constexpr float THETA_MAX_DEG =  60.0f;
constexpr float DIST_MIN_M    =   0.20f;
constexpr float DIST_MAX_M    =   6.00f;

// ======================================================
// RD-03D SETTINGS
// ======================================================
constexpr uint8_t RD03D_FRAME_SIZE   = 30;
constexpr uint8_t RD03D_HEADER_SIZE  = 4;
constexpr uint8_t RD03D_TARGET_SIZE  = 8;
constexpr uint8_t RD03D_MAX_TARGETS  = 3;

constexpr bool RADAR_FLIP_LEFT_RIGHT = true;
constexpr unsigned long RADAR_DISPLAY_INTERVAL_MS = 100;
constexpr unsigned long RADAR_TARGET_STALE_MS = 800;
constexpr bool PRINT_RAW_RADAR_BYTES = false;

unsigned long lastRadarDisplayUpdateMs = 0;
unsigned long lastRadarFrameMs = 0;

struct RadarTarget {
  bool valid;
  uint8_t slot;
  float thetaDeg;
  float distanceM;
  int16_t x_mm;
  int16_t y_mm;
  int16_t speed_cm_s;
  uint16_t resolution_mm;
};

RadarTarget currentTargets[RD03D_MAX_TARGETS];
RadarTarget lastDrawnTargets[RD03D_MAX_TARGETS];
uint8_t currentTargetCount = 0;
uint8_t lastDrawnCount = 0;
bool currentDisplayFromRadar = false;
bool staticScreenDrawn = false;

const uint16_t TARGET_COLORS[RD03D_MAX_TARGETS] = {
  ILI9341_RED,
  ILI9341_YELLOW,
  ILI9341_CYAN
};

inline float clampFloat(float x, float lo, float hi) {
  return (x < lo) ? lo : (x > hi) ? hi : x;
}

inline float degToRad(float deg) {
  return deg * PI / 180.0f;
}

void clearTargetArray(RadarTarget *targets) {
  for (uint8_t i = 0; i < RD03D_MAX_TARGETS; i++) {
    targets[i] = {false, static_cast<uint8_t>(i + 1), 0.0f, 0.0f, 0, 0, 0, 0};
  }
}

bool sameTarget(const RadarTarget &a, const RadarTarget &b) {
  return a.valid == b.valid &&
         a.slot == b.slot &&
         fabsf(a.thetaDeg - b.thetaDeg) < 0.1f &&
         fabsf(a.distanceM - b.distanceM) < 0.05f &&
         a.speed_cm_s == b.speed_cm_s;
}

void radarToScreen(float thetaDeg, float distanceM, int &x, int &y) {
  thetaDeg = clampFloat(thetaDeg, THETA_MIN_DEG, THETA_MAX_DEG);
  distanceM = clampFloat(distanceM, DIST_MIN_M, DIST_MAX_M);

  const float r = (distanceM / DIST_MAX_M) * RADAR_R;
  const float theta = degToRad(thetaDeg);

  x = RADAR_CX + static_cast<int>(r * sinf(theta));
  y = RADAR_CY - static_cast<int>(r * cosf(theta));
}

void drawStaticRadarScreen() {
  tft.fillScreen(ILI9341_BLACK);

  for (float theta = THETA_MIN_DEG; theta <= THETA_MAX_DEG; theta += 60.0f) {
    const float rad = degToRad(theta);
    const int x2 = RADAR_CX + static_cast<int>(RADAR_R * sinf(rad));
    const int y2 = RADAR_CY - static_cast<int>(RADAR_R * cosf(rad));
    tft.drawLine(RADAR_CX, RADAR_CY, x2, y2, ILI9341_BLUE);
  }

  tft.drawLine(RADAR_CX, RADAR_CY, RADAR_CX, RADAR_CY - RADAR_R, ILI9341_GREEN);

  for (int arc = 1; arc <= 4; arc++) {
    const int r = (RADAR_R * arc) / 4;
    int prevX = -1, prevY = -1;

    for (float theta = THETA_MIN_DEG; theta <= THETA_MAX_DEG; theta += 2.0f) {
      const float rad = degToRad(theta);
      const int x = RADAR_CX + static_cast<int>(r * sinf(rad));
      const int y = RADAR_CY - static_cast<int>(r * cosf(rad));
      if (prevX >= 0) tft.drawLine(prevX, prevY, x, y, ILI9341_BLUE);
      prevX = x;
      prevY = y;
    }
  }

  tft.fillCircle(RADAR_CX, RADAR_CY, 4, ILI9341_WHITE);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(1);
  tft.setCursor(8, 215); tft.print("-60");
  tft.setCursor(150, 42); tft.print("0");
  tft.setCursor(288, 215); tft.print("+60");

  staticScreenDrawn = true;
}

void drawOneTarget(const RadarTarget &target, uint8_t drawIndex) {
  if (!target.valid) return;

  int x, y;
  radarToScreen(target.thetaDeg, target.distanceM, x, y);

  const uint16_t color = TARGET_COLORS[drawIndex % RD03D_MAX_TARGETS];
  tft.fillCircle(x, y, 6, color);
  tft.drawCircle(x, y, 10, ILI9341_WHITE);

  int labelX = x + 9;
  int labelY = y - 4;
  if (labelX > 306) labelX = x - 16;
  if (labelY < 2) labelY = y + 8;

  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(1);
  tft.setCursor(labelX, labelY);
  tft.print('T');
  tft.print(target.slot);
}

void drawReadout(const RadarTarget *targets, uint8_t count, const char *sourceName) {
  tft.fillRect(184, 3, 136, 86, ILI9341_BLACK);
  tft.drawRect(184, 3, 136, 86, ILI9341_WHITE);

  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(1);
  tft.setCursor(190, 10); tft.print("SRC: "); tft.print(sourceName);
  tft.setCursor(190, 22); tft.print("TARGETS: "); tft.print(count);

  int y = 38;
  for (uint8_t i = 0; i < RD03D_MAX_TARGETS; i++) {
    tft.setCursor(190, y);
    if (i < count && targets[i].valid) {
      tft.print('T'); tft.print(targets[i].slot); tft.print(' ');
      if (targets[i].thetaDeg >= 0) tft.print('+');
      tft.print(targets[i].thetaDeg, 0); tft.print("deg ");
      tft.print(targets[i].distanceM, 1); tft.print('m');
    } else {
      tft.print('T'); tft.print(i + 1); tft.print(" ----");
    }
    y += 14;
  }
}

void renderRadarDisplay(const char *sourceName) {
  bool changed = (currentTargetCount != lastDrawnCount);
  if (!changed) {
    for (uint8_t i = 0; i < currentTargetCount; i++) {
      if (!sameTarget(currentTargets[i], lastDrawnTargets[i])) {
        changed = true;
        break;
      }
    }
  }
  if (!changed) return;

  if (!staticScreenDrawn) drawStaticRadarScreen();
  else drawStaticRadarScreen(); // Simple full redraw only on changed frame.

  for (uint8_t i = 0; i < currentTargetCount; i++) drawOneTarget(currentTargets[i], i);
  drawReadout(currentTargets, currentTargetCount, sourceName);

  for (uint8_t i = 0; i < RD03D_MAX_TARGETS; i++) lastDrawnTargets[i] = currentTargets[i];
  lastDrawnCount = currentTargetCount;
}

// ... rest unchanged functional logic

int16_t decodeRD03DValue(uint8_t lowByte, uint8_t highByte) {
  int16_t value = ((highByte & 0x7F) << 8) | lowByte;
  if ((highByte & 0x80) == 0) value = -value;
  return value;
}

bool decodeRD03DTarget(const uint8_t *frame, uint8_t targetIndex, RadarTarget &target) {
  target.valid = false; target.slot = targetIndex + 1;
  if (targetIndex >= RD03D_MAX_TARGETS) return false;
  const uint8_t offset = RD03D_HEADER_SIZE + targetIndex * RD03D_TARGET_SIZE;

  const int16_t x_mm = decodeRD03DValue(frame[offset + 0], frame[offset + 1]);
  const int16_t y_mm = decodeRD03DValue(frame[offset + 2], frame[offset + 3]);
  const int16_t speed_cm_s = decodeRD03DValue(frame[offset + 4], frame[offset + 5]);
  const uint16_t resolution_mm = frame[offset + 6] | (static_cast<uint16_t>(frame[offset + 7]) << 8);

  if (x_mm == 0 && y_mm == 0 && speed_cm_s == 0 && resolution_mm == 0) return false;

  const float x = static_cast<float>(x_mm);
  const float y = static_cast<float>(y_mm);
  const float distanceM = sqrtf(x * x + y * y) / 1000.0f;
  float thetaDeg = atan2f(x, y) * 180.0f / PI;
  if (RADAR_FLIP_LEFT_RIGHT) thetaDeg = -thetaDeg;

  if (distanceM < DIST_MIN_M || distanceM > DIST_MAX_M) return false;
  if (thetaDeg < THETA_MIN_DEG || thetaDeg > THETA_MAX_DEG) return false;

  target = {true, static_cast<uint8_t>(targetIndex + 1), thetaDeg, distanceM, x_mm, y_mm, speed_cm_s, resolution_mm};
  return true;
}

uint8_t parseRD03DFrame(const uint8_t *frame, RadarTarget *targets) {
  clearTargetArray(targets);
  const bool headerOK = ((frame[0] == 0xAA || frame[0] == 0xAD) && frame[1] == 0xFF && frame[2] == 0x03 && frame[3] == 0x00);
  const bool footerOK = (frame[28] == 0x55 && frame[29] == 0xCC);
  if (!headerOK || !footerOK) return 0;

  uint8_t count = 0;
  for (uint8_t slot = 0; slot < RD03D_MAX_TARGETS; slot++) {
    RadarTarget decoded;
    if (decodeRD03DTarget(frame, slot, decoded)) targets[count++] = decoded;
  }
  return count;
}

void updateRadarTargets(const RadarTarget *targets, uint8_t count, const char *sourceName, bool fromRadar) {
  clearTargetArray(currentTargets);
  if (count > RD03D_MAX_TARGETS) count = RD03D_MAX_TARGETS;
  currentTargetCount = 0;
  for (uint8_t i = 0; i < count; i++) {
    if (targets[i].valid) currentTargets[currentTargetCount++] = targets[i];
  }
  currentDisplayFromRadar = fromRadar;
  renderRadarDisplay(sourceName);
}

void handleRadarSerial() {
  static uint8_t frame[RD03D_FRAME_SIZE];
  static uint8_t pos = 0;

  while (RADAR_SERIAL.available()) {
    const uint8_t b = RADAR_SERIAL.read();
    if (PRINT_RAW_RADAR_BYTES) { if (b < 0x10) Serial.print('0'); Serial.print(b, HEX); Serial.print(' '); }

    switch (pos) {
      case 0: if (b == 0xAA || b == 0xAD) frame[pos++] = b; break;
      case 1: pos = (b == 0xFF) ? (frame[pos++] = b, pos) : 0; break;
      case 2: pos = (b == 0x03) ? (frame[pos++] = b, pos) : 0; break;
      case 3: pos = (b == 0x00) ? (frame[pos++] = b, pos) : 0; break;
      default:
        frame[pos++] = b;
        if (pos >= RD03D_FRAME_SIZE) {
          lastRadarFrameMs = millis();
          RadarTarget parsedTargets[RD03D_MAX_TARGETS];
          const uint8_t parsedCount = parseRD03DFrame(frame, parsedTargets);
          const unsigned long now = millis();
          if (now - lastRadarDisplayUpdateMs >= RADAR_DISPLAY_INTERVAL_MS) {
            lastRadarDisplayUpdateMs = now;
            updateRadarTargets(parsedTargets, parsedCount, "RADAR", true);
          }
          pos = 0;
        }
    }
  }
}

void handleRadarStaleTimeout() {
  if (currentDisplayFromRadar && currentTargetCount > 0 && (millis() - lastRadarFrameMs > RADAR_TARGET_STALE_MS)) {
    RadarTarget emptyTargets[RD03D_MAX_TARGETS];
    clearTargetArray(emptyTargets);
    updateRadarTargets(emptyTargets, 0, "RADAR", true);
  }
}

void handleButton() {
  const bool reading = digitalRead(TEST_BUTTON_PIN);
  if (reading != lastButtonReading) lastDebounceTime = millis();
  if ((millis() - lastDebounceTime) > debounceDelayMs && reading != stableButtonState) {
    stableButtonState = reading;
    if (stableButtonState == LOW) {
      RadarTarget fake[RD03D_MAX_TARGETS];
      clearTargetArray(fake);
      const uint8_t fakeCount = random(1, RD03D_MAX_TARGETS + 1);
      for (uint8_t i = 0; i < fakeCount; i++) {
        const float theta = random(static_cast<long>(THETA_MIN_DEG * 10), static_cast<long>(THETA_MAX_DEG * 10 + 1)) / 10.0f;
        const float dist = random(static_cast<long>(DIST_MIN_M * 100), static_cast<long>(DIST_MAX_M * 100 + 1)) / 100.0f;
        const float thetaRad = degToRad(theta);
        const float distMm = dist * 1000.0f;
        fake[i] = {true, static_cast<uint8_t>(i + 1), theta, dist, static_cast<int16_t>(distMm * sinf(thetaRad)), static_cast<int16_t>(distMm * cosf(thetaRad)), static_cast<int16_t>(random(-80, 81)), 0};
      }
      updateRadarTargets(fake, fakeCount, "BUTTON", false);
    }
  }
  lastButtonReading = reading;
}

void setup() {
  Serial.begin(115200);
  delay(250);
  pinMode(TEST_BUTTON_PIN, INPUT_PULLUP);
  randomSeed(analogRead(A0) ^ micros());
  RADAR_SERIAL.begin(RADAR_BAUD);
  tft.begin();
  tft.setRotation(3);
  clearTargetArray(currentTargets);
  clearTargetArray(lastDrawnTargets);
  drawStaticRadarScreen();
}

void loop() {
  handleButton();
  handleRadarSerial();
  handleRadarStaleTimeout();
}

