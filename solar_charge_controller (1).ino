#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// --- These defines MUST come before #include <IRremote.hpp> ---
// Fix for a Tinkercad simulation compile issue; harmless on real hardware.
#define DECODE_NEC
#define IR_RECEIVE_PIN 3
#define EXCLUDE_UNIVERSAL_PROTOCOLS
#define EXCLUDE_EXOTIC_PROTOCOLS
#include <IRremote.hpp>

#define RELAY   12
#define PWM_PIN 5

LiquidCrystal_I2C lcd(0x27, 16, 2);

byte PC[8] = {0b10010, 0b10010, 0b10010, 0b10010, 0b10010, 0b10010, 0b10010, 0}; // parallel symbol
byte SC[8] = {0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001, 0b10001, 0}; // series symbol

const float maxV = 4.3;

unsigned long cS = 0, lL = 0, lP = 0, tE = 0, lastIRtime = 0;
uint16_t lastIRcmd = 0xFFFF;
const long sameCodeDebounce = 300;

int   pwm = 0;
bool  sU = true, charging = true, iS = false, tSet = false, sI = false;
float tV = 0.0;
int   d1 = -1;

bool needRedraw = false;
int  rdA = -1, rdB = -1;

// reads A0 / A1 through the high-voltage divider (R1=3.9k, R2=1k -> factor 4.9)
float rV(int p) {
  return analogRead(p) * (5.0 / 1023.0) * 4.9;
}

// A3 is wired directly (no divider) — output never exceeds 4.3V
float rO() {
  if (charging) return 0.0;

  long total = 0;
  const int samples = 20;

  for (int i = 0; i < samples; i++) {
    total += analogRead(A3);
    delayMicroseconds(100);
  }

  float average = total / float(samples);
  float voltage = average * (5.0 / 1023.0);

  if (voltage < 0.02) voltage = 0.0;

  return voltage;
}

// open-loop initial PWM estimate; controlOutput() refines it
int tP(float v) {
  v = constrain(v, 0.0, maxV);
  return (int)(v / maxV * 255);
}

// closed-loop feedback: nudges PWM based on measured A3 vs target
void controlOutput() {
  if (charging || !iS || !tSet) return;

  float measuredV = rO();
  float error = tV - measuredV;
  const float tolerance = 0.05;

  if (abs(error) > tolerance) {
    int step = constrain((int)(error * 20), -4, 4);
    if (step == 0) step = (error > 0) ? 1 : -1;
    pwm += step;
  }

  pwm = constrain(pwm, 0, 255);
  analogWrite(PWM_PIN, pwm);

  Serial.print("Target:");
  Serial.print(tV, 2);
  Serial.print(" Measured:");
  Serial.print(measuredV, 2);
  Serial.print(" Error:");
  Serial.print(error, 2);
  Serial.print(" PWM:");
  Serial.println(pwm);
}

int dB(uint16_t c) {
  uint16_t b[] = {0x0C, 0x10, 0x11, 0x12, 0x14, 0x15, 0x16, 0x18, 0x19, 0x1A};
  for (int i = 0; i < 10; i++) if (c == b[i]) return i;
  return -1;
}

void sN() {
  lcd.setCursor(0, 0);
  iS ? lcd.write(byte(1)) : lcd.write(byte(0));
  lcd.print(rO(), 2); lcd.print("V");
  lcd.setCursor(8, 0); lcd.print("Mashiach");
  lcd.setCursor(0, 1);
  lcd.print(rV(A0), 2); lcd.print("V ");
  lcd.print(rV(A1), 2); lcd.print("V ");
}

void showInput(int a, int b) {
  lcd.setCursor(0, 0);
  iS ? lcd.write(byte(1)) : lcd.write(byte(0));
  lcd.print(rO(), 2); lcd.print("V");
  lcd.setCursor(8, 0); lcd.print("Mashiach");
  lcd.setCursor(0, 1); lcd.print("Input=");
  lcd.print(a); lcd.print(".");
  if (b >= 0) { lcd.print(b); lcd.print("0V "); }
  else lcd.print("__V ");
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY, OUTPUT);
  pinMode(PWM_PIN, OUTPUT);
  digitalWrite(RELAY, LOW);
  analogWrite(PWM_PIN, 0);

  lcd.init();
  lcd.backlight();
  lcd.createChar(0, PC);
  lcd.createChar(1, SC);

  lcd.setCursor(0, 0); lcd.print("Solar Charge");
  lcd.setCursor(0, 1); lcd.print("Controller v1.0");
  delay(2000);
  lcd.clear();

  IrReceiver.begin(IR_RECEIVE_PIN, 0); // 0 = no LED feedback

  cS = millis();
  Serial.println("Ready - press two buttons");
}

void loop() {
  unsigned long n = millis();

  // --- IR input ---
  if (IrReceiver.decode()) {
    uint16_t cmd = IrReceiver.decodedIRData.command;
    bool repeat  = IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT;

    bool isSameCodeTooSoon = (cmd == lastIRcmd) && ((n - lastIRtime) < (unsigned long)sameCodeDebounce);

    if (cmd != 0 && !repeat && !isSameCodeTooSoon) {
      int b = dB(cmd);

      if (b >= 0) {
        lastIRcmd  = cmd;
        lastIRtime = n;

        if (d1 < 0) {
          d1 = b;
          needRedraw = true;
          rdA = d1; rdB = -1;
          Serial.print("D1="); Serial.println(d1);

        } else {
          tV   = constrain(d1 + b / 10.0, 0, maxV);
          tSet = true;
          sI   = true;
          tE   = n + 1000;
          pwm  = tP(tV); // initial estimate, feedback refines it
          analogWrite(PWM_PIN, pwm);

          needRedraw = true;
          rdA = d1; rdB = b;

          Serial.print("D2="); Serial.println(b);
          Serial.print("Target="); Serial.print(tV, 2);
          Serial.print("V InitialPWM="); Serial.println(pwm);
          d1 = -1;
        }
      }
    }
    IrReceiver.resume();
  }

  // deferred LCD redraw (kept out of the IR interrupt path)
  if (needRedraw) {
    needRedraw = false;
    showInput(rdA, rdB);
  }

  // return to normal screen after the 1-second input display
  if (sI && (long)(n - tE) >= 0) {
    sI = false;
    lL = n;
    sN();
  }
  if (!sI && d1 < 0 && n - lL >= 100) { lL = n; sN(); }

  // --- charging stage (parallel) ---
  if (charging) {
    digitalWrite(RELAY, LOW);
    analogWrite(PWM_PIN, 0);
    iS = false;

    if (n - cS >= 5000) {
      charging = false;
      iS = true;
      lP = n;
      digitalWrite(RELAY, HIGH);

      if (tSet) {
        pwm = tP(tV);
        analogWrite(PWM_PIN, pwm);
        Serial.print("SERIES locked at ");
        Serial.print(tV, 2); Serial.println("V");
      } else {
        pwm = 0; sU = true;
        analogWrite(PWM_PIN, 0);
        Serial.println("SERIES sweeping");
      }
    }
  }

  // --- discharging stage (series) ---
  else {

    if (tSet) {
      if (n - lP >= 80) {
        lP = n;
        controlOutput();
      }

    } else if (n - lP >= 20) {
      lP = n;
      if (sU) {
        pwm++;
        if (pwm >= 255) { pwm = 255; sU = false; }
      } else {
        pwm--;
        if (pwm <= 0) {
          pwm = 0; charging = true; iS = false; cS = n;
          digitalWrite(RELAY, LOW);
          analogWrite(PWM_PIN, 0);
          Serial.println("CHARGING");
        }
      }
      analogWrite(PWM_PIN, pwm);
    }
  }
}
