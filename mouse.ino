/*  
  Raspberry Pi Pico (RP2040) - Arduino core

  - Move o cursor enquanto o joystick ultrapassar thresholds extremos:
      * Y controla esquerda/direita (horizontal).
      * X controla cima/baixo (vertical).
      * Thresholds: < LOW_THRESH ou > HIGH_THRESH disparam movimento.
  - Tratamento de botões A e B (clique press/release):
      * A = clique esquerdo (press enquanto pressionado)
      * B = clique direito
      * A+B = clique do meio (mantém enquanto ambos pressionados)
  - Velocidade aumentada e correção do sentido vertical (cima/baixo).
  - Abra Serial (115200) para debug (opcional).
*/

#include <Arduino.h>
#include <Mouse.h>

// pinagem (ajuste se necessário)
const uint8_t JOY_X_PIN = 26; // eixo X (cima/baixo)
const uint8_t JOY_Y_PIN = 27; // eixo Y (esquerda/direita)

const uint8_t BUTTON_A_PIN = 5;  // botão A -> clique esquerdo (press/release)
const uint8_t BUTTON_B_PIN = 6;  // botão B -> clique direito (press/release)

// thresholds fornecidos: < LOW_THRESH e > HIGH_THRESH
const int LOW_THRESH  = 350;
const int HIGH_THRESH = 600;

// movimento — velocidade aumentada conforme solicitado
const int MOVE_STEP = 20;               // pixels por iteração (aumentado)
const unsigned long LOOP_DELAY_MS = 10; // intervalo entre movimentos (ms) — menor = mais rápido

// Inversão caso queira (não é preciso alterar para corrigir o problema atual)
const bool INVERT_X = true; // true para inverter cima/baixo
const bool INVERT_Y = false; // true para inverter esquerda/direita

// Debounce botões
const unsigned long DEBOUNCE_MS = 30;
bool btnA_state = HIGH, btnB_state = HIGH;           // leitura temporária
bool btnA_lastStable = HIGH, btnB_lastStable = HIGH; // estado debounced
unsigned long btnA_lastChange = 0, btnB_lastChange = 0;

void handleButton(uint8_t pin, bool &lastStable, bool &curState, unsigned long &lastChange, uint8_t buttonMask) {
  bool reading = digitalRead(pin);
  if (reading != curState) {
    curState = reading;
    lastChange = millis();
  } else {
    if ((millis() - lastChange) > DEBOUNCE_MS) {
      if (curState != lastStable) {
        lastStable = curState;
        // curState == LOW significa pressionado (INPUT_PULLUP)
        if (lastStable == LOW) {
          if (buttonMask == 1) Mouse.press(MOUSE_LEFT);
          else if (buttonMask == 2) Mouse.press(MOUSE_RIGHT);
        } else {
          if (buttonMask == 1) Mouse.release(MOUSE_LEFT);
          else if (buttonMask == 2) Mouse.release(MOUSE_RIGHT);
        }
      }
    }
  }
}

void setup() {
  pinMode(BUTTON_A_PIN, INPUT_PULLUP);
  pinMode(BUTTON_B_PIN, INPUT_PULLUP);

  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && (millis() - t0) < 300) {}
  Serial.println(F("JoystickMouse_Thresholds_withButtons iniciado"));
  Serial.print(F("LOW_THRESH=")); Serial.print(LOW_THRESH);
  Serial.print(F(" HIGH_THRESH=")); Serial.println(HIGH_THRESH);

  Mouse.begin();
}

int readAxisRaw(uint8_t pin) {
  int v = analogRead(pin);
  if (v < 0) v = 0;
  return v;
}

void loop() {
  int rawX = readAxisRaw(JOY_X_PIN); // vertical: cima/baixo
  int rawY = readAxisRaw(JOY_Y_PIN); // horizontal: esquerda/direita

  // Determina estados de extremo
  bool isLeft  = (rawY < LOW_THRESH);
  bool isRight = (rawY > HIGH_THRESH);

  // isUp = stick para cima (rawX pequeno), isDown = stick para baixo (rawX grande)
  bool isUp    = (rawX < LOW_THRESH);
  bool isDown  = (rawX > HIGH_THRESH);

  // Ajusta sinais
  int dx = 0;
  int dy = 0;

  if (isLeft)  dx = -MOVE_STEP;
  if (isRight) dx =  MOVE_STEP;

  // Correção do sentido vertical:
  // isUp -> mover cursor para cima (dy negativo)
  // isDown -> mover cursor para baixo (dy positivo)
  if (isUp)    dy = -MOVE_STEP;
  if (isDown)  dy =  MOVE_STEP;

  if (INVERT_X) dy = -dy;
  if (INVERT_Y) dx = -dx;

  // Envia movimento se algum extremo estiver ativo
  if (dx != 0 || dy != 0) {
    Mouse.move(dx, dy);
  }

  // Tratamento dos botões A / B com debounce e suporte A+B -> clique do meio
  bool rawA = (digitalRead(BUTTON_A_PIN) == LOW);
  bool rawB = (digitalRead(BUTTON_B_PIN) == LOW);

  static bool middlePressed = false;

  if (rawA && rawB) {
    if (!middlePressed) {
      // libera possíveis left/right presos e press do meio
      Mouse.release(MOUSE_LEFT);
      Mouse.release(MOUSE_RIGHT);
      Mouse.press(MOUSE_MIDDLE);
      middlePressed = true;
    }
    // mantém middle pressionado enquanto A+B
  } else {
    if (middlePressed) {
      Mouse.release(MOUSE_MIDDLE);
      middlePressed = false;
    }
    // trata A e B individualmente (press/release)
    handleButton(BUTTON_A_PIN, btnA_lastStable, btnA_state, btnA_lastChange, 1);
    handleButton(BUTTON_B_PIN, btnB_lastStable, btnB_state, btnB_lastChange, 2);
  }

  // Debug opcional no Serial (comente se não quiser)
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 200) {
    lastDebug = millis();
    Serial.print(F("rawX=")); Serial.print(rawX);
    Serial.print(F(" rawY=")); Serial.print(rawY);
    Serial.print(F("  -> "));
    if (isLeft)  Serial.print(F("LEFT "));
    if (isRight) Serial.print(F("RIGHT "));
    if (isUp)    Serial.print(F("UP "));
    if (isDown)  Serial.print(F("DOWN "));
    if (!isLeft && !isRight && !isUp && !isDown) Serial.print(F("CENTER"));
    Serial.print(F("  dx=")); Serial.print(dx);
    Serial.print(F(" dy=")); Serial.println(dy);
  }

  delay(LOOP_DELAY_MS);
}