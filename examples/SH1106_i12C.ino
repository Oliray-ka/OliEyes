/*
  OliEyes - expressive animated OLED robot eyes
  Display: 1.3" OLED, SH1106 driver, I2C
  ------------------------------------------------------------------
  BOARD COMPATIBILITY: this sketch needs more RAM than Arduino Uno,
  Nano, or Pro Mini have (they only have 2KB - the display's frame
  buffer alone needs 1KB, so display.begin() will fail with "SH1106
  init failed"). Confirmed working: Arduino Mega/Mega2560, ESP32
  (classic and C3 confirmed; other ESP32 variants expected to work,
  same abundant-RAM architecture).

  Wiring (SDA / SCL - VCC to 3V3 or 5V per your module, GND to GND):
    Arduino Uno / Nano        -> A4 / A5   (compiles but won't run - see above)
    Arduino Mega / Mega2560   -> 20 / 21
    ESP32 (classic DevKit)    -> GPIO21 / GPIO22
    ESP32-C3 (varies by board, check silkscreen) -> often GPIO4/5 or GPIO8/9
    ESP8266 (NodeMCU)         -> D2 / D1  (GPIO4 / GPIO5)

  This sketch uses Wire.begin() with no arguments, so it automatically
  uses whichever of the pins above matches the board you've selected
  in the Arduino IDE - no editing needed for standard boards. If your
  OLED is wired to different pins than your board's default (common on
  some ESP32-C3 clones), uncomment the Wire.begin(SDA, SCL) line below
  and set your own pin numbers.

  IMPORTANT: most cheap 1.3" I2C OLED modules use the SH1106 driver,
  NOT SSD1306, even though they look identical. Running SH1106 glass
  with SSD1306 code causes a shifted/striped image. If you get that
  symptom, you have the other driver - use the matching sketch instead.

  Libraries required (install via Library Manager):
    Adafruit GFX Library
    Adafruit SH110X

  Serial Monitor commands (115200 baud, line ending: Newline):
    <mood name>  - switch directly to that mood (see 'list')
    list         - print every available mood name
    demo         - resume automatic mood cycling
    blink        - trigger a normal two-eye blink right now
    wink         - trigger a right-eye wink (alias for winkright)
    winkleft     - trigger a left-eye wink
    winkright    - trigger a right-eye wink
    doubletake   - quick startled glance + blink gesture
    interval <ms> - set base time between moods in demo mode
    speed <1-100> - set how fast eye shapes ease into a new mood
    help         - print this command list
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// ====================== USER CONFIG ======================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET   -1
#define OLED_I2C_ADDRESS 0x3C

// Uncomment and edit if your OLED is wired to non-default I2C pins:
// #define CUSTOM_I2C_PINS
// #define SDA_PIN 4
// #define SCL_PIN 5

const unsigned long FRAME_INTERVAL_MS   = 25;   // ~40 FPS
const float EYE_EASE_SPEED_DEFAULT      = 0.12; // how fast eye shapes morph between moods
const unsigned long MOOD_DWELL_MS       = 3500; // base time per mood in demo mode
const int WINK_INSTEAD_OF_BLINK_PERCENT = 18;   // chance a scheduled blink becomes a one-eye wink
const int DOUBLE_BLINK_PERCENT          = 18;   // chance of a quick second blink right after one
// ===========================================================

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---------- Mood definitions ----------
enum Mood {
  NEUTRAL, HAPPY, SAD, ANGRY, SURPRISED, SLEEPY, LOVE, SUSPICIOUS,
  CONFUSED, SCARED, LAUGH, BORED, EXCITED, SHY, ANNOYED, DIZZY, SICK,
  PROUD, FRUSTRATED, CURIOUS,
  MOOD_COUNT
};

const char* moodNames[MOOD_COUNT] = {
  "neutral","happy","sad","angry","surprised","sleepy","love","suspicious",
  "confused","scared","laugh","bored","excited","shy","annoyed","dizzy","sick",
  "proud","frustrated","curious"
};

Mood currentMood = NEUTRAL;
bool demoMode = true;
unsigned long lastMoodSwitch = 0;
unsigned long moodSwitchBase = MOOD_DWELL_MS;

// ---------- Eye shape target parameters ----------
struct EyeTarget {
  float w, h, radius;
  float slantInner;
  float slantOuter;
  float squint;
  float yOff;
};

void getMoodTarget(Mood m, bool isRight, EyeTarget &t) {
  switch (m) {
    case HAPPY:
      t = {40, 40, 10, 0, 0, 16, 0};
      break;
    case SAD:
      t = {36, 34, 10, 0, 14, 0, 4};
      break;
    case ANGRY:
      t = {36, 30, 8, 16, 0, 0, 2};
      break;
    case SURPRISED:
      t = {46, 50, 20, 0, 0, 0, -2};
      break;
    case SLEEPY:
      t = {40, 14, 6, 0, 0, 0, 10};
      break;
    case LOVE:
      t = {40, 40, 10, 0, 0, 19, -2};
      break;
    case SUSPICIOUS:
      t = {40, 20, 6, 0, (isRight ? 10.0f : 0.0f), 0, 2};
      break;
    case CONFUSED:
      t = {40, 40, 10, 0, 0, 0, (isRight ? -8.0f : 8.0f)};
      break;
    case SCARED:
      t = {44, 48, 18, 0, 0, 0, -4};
      break;
    case LAUGH:
      t = {42, 42, 10, 0, 0, 26, -2};
      break;
    case BORED:
      t = {40, 16, 8, 0, 6, 0, 8};
      break;
    case EXCITED:
      t = {44, 46, 18, 0, 0, 0, -3};
      break;
    case SHY:
      t = {32, 24, 10, 0, 0, 8, 8};
      break;
    case ANNOYED:
      t = {38, 22, 8, 10, 0, 0, 2};
      break;
    case DIZZY:
      t = {38, 38, 14, 0, 0, 0, 0};
      break;
    case SICK:
      t = {38, (isRight ? 24.0f : 34.0f), 10, 0, 6, 0, 6};
      break;
    case PROUD:
      t = {42, 44, 14, 0, 0, 10, -3};
      break;
    case FRUSTRATED:
      t = {34, 26, 8, 20, 0, 0, 4};
      break;
    case CURIOUS:
      t = {(isRight ? 46.0f : 34.0f), (isRight ? 46.0f : 34.0f), 14, 0, 0, 0, (isRight ? -2.0f : 2.0f)};
      break;
    case NEUTRAL:
    default:
      t = {40, 40, 10, 0, 0, 0, 0};
      break;
  }
}

// ---------- Live eye state ----------
struct EyeState {
  float w, h, radius, slantInner, slantOuter, squint, yOff;
};
EyeState leftEye, rightEye;

void drawEye(EyeState &e, int cx, int cy, bool isRight);
void getMoodTarget(Mood m, bool isRight, EyeTarget &t);

float easeSpeed = EYE_EASE_SPEED_DEFAULT;

float easeTo(float current, float target, float speed) {
  return current + (target - current) * speed;
}

// ---------- Look state ----------
float lookX = 0, lookY = 0;
float lookTargetX = 0, lookTargetY = 0;
unsigned long lastLookChange = 0;
unsigned long nextLookInterval = 2000;

// Scripted "quick startled glance + blink" gesture (command: doubletake)
bool doubleTakeMode = false;
unsigned long doubleTakeStart = 0;
int doubleTakeDir = 1; // 1 = glance right first, -1 = glance left first

// ---------- Blink state ----------
float blinkAmount = 0;
int blinkPhase = 0;
unsigned long lastBlinkTime = 0;
unsigned long nextBlinkInterval = 3000;
unsigned long blinkPhaseStart = 0;
bool doubleBlinkArmed = false;
bool blinkRequested = false;

// Wink state - each eye animates smoothly and independently, using the
// same close/open timing as a normal blink (not an instant snap).
float winkRightAmount = 0;
int winkRightPhase = 0; // 0 idle, 1 closing, 2 opening
unsigned long winkRightPhaseStart = 0;

float winkLeftAmount = 0;
int winkLeftPhase = 0;
unsigned long winkLeftPhaseStart = 0;

void triggerWinkRight(unsigned long now) {
  if (winkRightPhase == 0) { winkRightPhase = 1; winkRightPhaseStart = now; }
}
void triggerWinkLeft(unsigned long now) {
  if (winkLeftPhase == 0) { winkLeftPhase = 1; winkLeftPhaseStart = now; }
}

float bounceY = 0;

unsigned long lastFrame = 0;

void printHelp() {
  Serial.println(F("OliEyes commands:"));
  Serial.println(F("  <mood name>     - switch to that mood, e.g. happy"));
  Serial.println(F("  list            - list all mood names"));
  Serial.println(F("  demo            - resume automatic mood cycling"));
  Serial.println(F("  blink           - trigger a normal blink"));
  Serial.println(F("  wink / winkright- trigger a right-eye wink"));
  Serial.println(F("  winkleft        - trigger a left-eye wink"));
  Serial.println(F("  doubletake      - startled glance + blink gesture"));
  Serial.println(F("  interval <ms>   - base time between moods in demo"));
  Serial.println(F("  speed <1-100>   - eye morph speed"));
  Serial.println(F("  help            - show this list"));
}

void setup() {
  Serial.begin(115200);

#if defined(CUSTOM_I2C_PINS)
  Wire.begin(SDA_PIN, SCL_PIN);
#else
  Wire.begin();
#endif

  if (!display.begin(OLED_I2C_ADDRESS, true)) {
    Serial.println(F("SH1106 init failed"));
    while (true) delay(1000);
  }
  display.clearDisplay();
  display.display();

  leftEye = {40, 40, 10, 0, 0, 0, 0};
  rightEye = {40, 40, 10, 0, 0, 0, 0};

  Serial.println(F("OliEyes ready."));
  printHelp();
  randomSeed(analogRead(0));
}

void loop() {
  handleSerial();

  unsigned long now = millis();
  if (now - lastFrame < FRAME_INTERVAL_MS) return;
  lastFrame = now;

  if (demoMode && now - lastMoodSwitch > moodSwitchBase) {
    lastMoodSwitch = now;
    moodSwitchBase = MOOD_DWELL_MS + random(-800, 1200);
    currentMood = (Mood)((currentMood + 1) % MOOD_COUNT);
    Serial.print(F("Mood: "));
    Serial.println(moodNames[currentMood]);
  }

  updateLook(now);
  updateBlink(now);
  updateWinks(now);
  updateEyeEasing();

  drawEyes();
}

void handleSerial() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toLowerCase();
  if (cmd.length() == 0) return;

  if (cmd == "help") {
    printHelp();
    return;
  }
  if (cmd == "demo") {
    demoMode = true;
    Serial.println(F("Demo mode ON"));
    return;
  }
  if (cmd == "wink" || cmd == "winkright") {
    triggerWinkRight(millis());
    Serial.println(F("Wink right!"));
    return;
  }
  if (cmd == "winkleft") {
    triggerWinkLeft(millis());
    Serial.println(F("Wink left!"));
    return;
  }
  if (cmd == "blink") {
    blinkRequested = true;
    Serial.println(F("Blink!"));
    return;
  }
  if (cmd == "doubletake") {
    doubleTakeMode = true;
    doubleTakeStart = millis();
    doubleTakeDir = random(0, 2) ? 1 : -1;
    Serial.println(F("Double take!"));
    return;
  }
  if (cmd == "list") {
    Serial.println(F("Moods:"));
    for (int i = 0; i < MOOD_COUNT; i++) Serial.println(moodNames[i]);
    return;
  }
  if (cmd.startsWith("interval ")) {
    long v = cmd.substring(9).toInt();
    if (v > 300) {
      moodSwitchBase = v;
      Serial.print(F("Interval set to "));
      Serial.println(v);
    }
    return;
  }
  if (cmd.startsWith("speed ")) {
    int v = cmd.substring(6).toInt();
    v = constrain(v, 1, 100);
    easeSpeed = v / 100.0 * 0.4 + 0.02;
    Serial.print(F("Speed set to "));
    Serial.println(v);
    return;
  }

  for (int i = 0; i < MOOD_COUNT; i++) {
    if (cmd == moodNames[i]) {
      demoMode = false;
      currentMood = (Mood)i;
      Serial.print(F("Mood: "));
      Serial.println(moodNames[i]);
      return;
    }
  }
  Serial.println(F("Unknown command. Type 'help' for the command list."));
}

void updateLook(unsigned long now) {
  if (doubleTakeMode) {
    unsigned long elapsed = now - doubleTakeStart;
    if (elapsed < 120) {
      lookTargetX = 16 * doubleTakeDir;
      lookTargetY = 0;
      lookX = easeTo(lookX, lookTargetX, 0.6);
      lookY = easeTo(lookY, lookTargetY, 0.6);
    } else if (elapsed < 220) {
      blinkRequested = true;
      lookTargetX = 0;
      lookX = easeTo(lookX, lookTargetX, 0.5);
      lookY = easeTo(lookY, 0, 0.5);
    } else if (elapsed < 500) {
      lookX = easeTo(lookX, 0, 0.3);
      lookY = easeTo(lookY, 0, 0.3);
    } else {
      doubleTakeMode = false;
    }
    return;
  }

  if (currentMood == DIZZY) {
    float angle = now / 260.0;
    lookTargetX = cos(angle) * 15;
    lookTargetY = sin(angle) * 6;
    lookX = easeTo(lookX, lookTargetX, 0.25);
    lookY = easeTo(lookY, lookTargetY, 0.25);
    return;
  }

  if (now - lastLookChange > nextLookInterval) {
    lastLookChange = now;
    nextLookInterval = 1500 + random(0, 2500);
    if (currentMood == SHY) {
      lookTargetX = random(-14, -4);
      lookTargetY = random(4, 10);
    } else if (random(0, 100) < 70) {
      lookTargetX = random(-16, 17);
      lookTargetY = random(-6, 7);
    } else {
      lookTargetX = 0;
      lookTargetY = 0;
    }
  }
  lookX = easeTo(lookX, lookTargetX, 0.08);
  lookY = easeTo(lookY, lookTargetY, 0.08);

  if (currentMood == LAUGH) {
    bounceY = sin(now / 65.0) * 3.0;
  } else {
    bounceY = easeTo(bounceY, 0, 0.2);
  }
}

void updateWinks(unsigned long now) {
  if (winkRightPhase == 1) {
    float t = (now - winkRightPhaseStart) / 90.0;
    winkRightAmount = min(1.0f, t);
    if (t >= 1.0) { winkRightPhase = 2; winkRightPhaseStart = now; }
  } else if (winkRightPhase == 2) {
    float t = (now - winkRightPhaseStart) / 140.0;
    winkRightAmount = max(0.0f, 1.0f - t);
    if (t >= 1.0) { winkRightPhase = 0; winkRightAmount = 0; }
  }

  if (winkLeftPhase == 1) {
    float t = (now - winkLeftPhaseStart) / 90.0;
    winkLeftAmount = min(1.0f, t);
    if (t >= 1.0) { winkLeftPhase = 2; winkLeftPhaseStart = now; }
  } else if (winkLeftPhase == 2) {
    float t = (now - winkLeftPhaseStart) / 140.0;
    winkLeftAmount = max(0.0f, 1.0f - t);
    if (t >= 1.0) { winkLeftPhase = 0; winkLeftAmount = 0; }
  }
}

void updateBlink(unsigned long now) {
  unsigned long minInt = 2200, maxInt = 5500;
  if (currentMood == SLEEPY || currentMood == BORED) { minInt = 1500; maxInt = 3200; }
  if (currentMood == SURPRISED || currentMood == SCARED) { minInt = 4200; maxInt = 8500; }
  if (currentMood == EXCITED) { minInt = 900; maxInt = 2000; }

  if (blinkRequested && blinkPhase == 0) {
    blinkRequested = false;
    blinkPhase = 1;
    blinkPhaseStart = now;
  }

  if (blinkPhase == 0 && now - lastBlinkTime > nextBlinkInterval) {
    if (random(0, 100) < WINK_INSTEAD_OF_BLINK_PERCENT) {
      if (random(0, 2)) triggerWinkRight(now); else triggerWinkLeft(now);
      lastBlinkTime = now;
      nextBlinkInterval = random(minInt, maxInt);
    } else {
      blinkPhase = 1;
      blinkPhaseStart = now;
    }
  }

  if (blinkPhase == 1) {
    float t = (now - blinkPhaseStart) / 90.0;
    blinkAmount = min(1.0f, t);
    if (t >= 1.0) { blinkPhase = 2; blinkPhaseStart = now; }
  } else if (blinkPhase == 2) {
    float t = (now - blinkPhaseStart) / 140.0;
    blinkAmount = max(0.0f, 1.0f - t);
    if (t >= 1.0) {
      blinkPhase = 0;
      blinkAmount = 0;
      lastBlinkTime = now;
      if (!doubleBlinkArmed && random(0, 100) < DOUBLE_BLINK_PERCENT) {
        doubleBlinkArmed = true;
        nextBlinkInterval = random(120, 260);
      } else {
        doubleBlinkArmed = false;
        nextBlinkInterval = random(minInt, maxInt);
      }
    }
  }
}

void updateEyeEasing() {
  EyeTarget tl, tr;
  getMoodTarget(currentMood, false, tl);
  getMoodTarget(currentMood, true, tr);

  leftEye.w = easeTo(leftEye.w, tl.w, easeSpeed);
  leftEye.h = easeTo(leftEye.h, tl.h, easeSpeed);
  leftEye.radius = easeTo(leftEye.radius, tl.radius, easeSpeed);
  leftEye.slantInner = easeTo(leftEye.slantInner, tl.slantInner, easeSpeed);
  leftEye.slantOuter = easeTo(leftEye.slantOuter, tl.slantOuter, easeSpeed);
  leftEye.squint = easeTo(leftEye.squint, tl.squint, easeSpeed);
  leftEye.yOff = easeTo(leftEye.yOff, tl.yOff, easeSpeed);

  rightEye.w = easeTo(rightEye.w, tr.w, easeSpeed);
  rightEye.h = easeTo(rightEye.h, tr.h, easeSpeed);
  rightEye.radius = easeTo(rightEye.radius, tr.radius, easeSpeed);
  rightEye.slantInner = easeTo(rightEye.slantInner, tr.slantInner, easeSpeed);
  rightEye.slantOuter = easeTo(rightEye.slantOuter, tr.slantOuter, easeSpeed);
  rightEye.squint = easeTo(rightEye.squint, tr.squint, easeSpeed);
  rightEye.yOff = easeTo(rightEye.yOff, tr.yOff, easeSpeed);
}

void drawEye(EyeState &e, int cx, int cy, bool isRight) {
  int w = (int)e.w;

  if (lookX > 6 && isRight) w += (int)min((lookX - 6) * 0.6f, 6.0f);
  if (lookX < -6 && !isRight) w += (int)min((-lookX - 6) * 0.6f, 6.0f);

  int h = (int)e.h;
  int r = (int)e.radius;
  int x = cx - w / 2;
  int y = cy - h / 2 + (int)e.yOff + (int)lookY + (int)bounceY;

  float blink = blinkAmount;
  if (isRight) blink = max(blink, winkRightAmount);
  if (!isRight) blink = max(blink, winkLeftAmount);

  int hh = max(2, (int)(h * (1.0 - blink)));
  int yy = y + (h - hh);

  display.fillRoundRect(x, yy, w, hh, r, SH110X_WHITE);

  if (e.squint > 1) {
    float halfW = w / 2.0;
    float sq = min(e.squint, (float)(hh - 3));
    if (sq > 1) {
      float R = (sq * sq + halfW * halfW) / (2.0 * sq);
      int bottom = yy + hh;
      int ycenter = bottom + (int)(R - sq);
      display.fillCircle(cx, ycenter, (int)R, SH110X_BLACK);
    }
  }

  if (e.slantInner > 1) {
    int cutW = w / 2;
    int cutH = (int)e.slantInner;
    if (isRight) {
      display.fillTriangle(x, yy, x + cutW, yy, x, yy + cutH, SH110X_BLACK);
    } else {
      display.fillTriangle(x + w, yy, x + w - cutW, yy, x + w, yy + cutH, SH110X_BLACK);
    }
  }

  if (e.slantOuter > 1) {
    int cutW = w / 2;
    int cutH = (int)e.slantOuter;
    if (isRight) {
      display.fillTriangle(x + w, yy, x + w - cutW, yy, x + w, yy + cutH, SH110X_BLACK);
    } else {
      display.fillTriangle(x, yy, x + cutW, yy, x, yy + cutH, SH110X_BLACK);
    }
  }
}

void drawEyes() {
  display.clearDisplay();

  int eyeGap = 16;
  int baseLeftX = SCREEN_WIDTH / 2 - (eyeGap / 2 + (int)leftEye.w / 2);
  int baseRightX = SCREEN_WIDTH / 2 + (eyeGap / 2 + (int)rightEye.w / 2);
  int baseY = SCREEN_HEIGHT / 2;

  int lx = baseLeftX + (int)lookX;
  int rx = baseRightX + (int)lookX;

  drawEye(leftEye, lx, baseY, false);
  drawEye(rightEye, rx, baseY, true);

  if (currentMood == SLEEPY || currentMood == BORED) {
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(SCREEN_WIDTH - 22, 4);
    display.print("z");
    display.setCursor(SCREEN_WIDTH - 16, 0);
    display.print("Z");
  }

  if (currentMood == SICK) {
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(SCREEN_WIDTH - 20, SCREEN_HEIGHT - 12);
    display.print("~");
  }

  display.display();
}
