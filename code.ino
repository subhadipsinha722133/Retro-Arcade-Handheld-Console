#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Preferences.h>

// Display SPI Pins (Tested Hardware)
#define TFT_CS    7
#define TFT_DC    5
#define TFT_MOSI  6
#define TFT_SCK   4
#define TFT_RST   -1  // RST 3.3V se juda hai

// Control & Sound Pins
#define PIN_JOY_X  0
#define PIN_JOY_Y  1
#define PIN_BUZZER 2
#define PIN_BTN_A  3   // Jump / Action / Shoot / Select
#define PIN_BTN_B  8   // Exit to Main Menu
#define PIN_BTN_P  9   // Dedicated Pause / Resume Button

// Basic Color Constants
#define C_BLACK   ST77XX_BLACK
#define C_WHITE   ST77XX_WHITE
#define C_RED     ST77XX_RED
#define C_GREEN   ST77XX_GREEN
#define C_BLUE    ST77XX_BLUE
#define C_YELLOW  ST77XX_YELLOW
#define C_MAGENTA ST77XX_MAGENTA
#define C_CYAN    ST77XX_CYAN
#define C_ORANGE  0xFD20
#define C_NAVY    0x000F
#define C_DGREEN  0x03E0
#define C_DGREY   0x39E7
#define C_LGREY   0xBDF7
#define C_BROWN   0x9A60

SPIClass hspi(FSPI);
Adafruit_ST7789 tft = Adafruit_ST7789(&hspi, TFT_CS, TFT_DC, TFT_RST);
Preferences preferences;

enum GameState { 
  SPLASH,
  MENU, 
  GAME_FLAPPY, 
  GAME_SPACE, 
  GAME_SNAKE, 
  GAME_BRICK, 
  GAME_MARIO,
  GAME_DINO,
  GAME_RACING,
  GAME_PONG,
  GAME_METEOR,
  GAME_SHOOTER
};

GameState currentState = SPLASH;
bool isPaused = false;
int selectedGame = 0;
const int TOTAL_GAMES = 10;

const char* gameTitles[] = { 
  "1. FLAPPY BIRD PRO", 
  "2. SPACE SHOOTER", 
  "3. RETRO SNAKE", 
  "4. BRICK BREAKER", 
  "5. MARIO RUNNER",
  "6. CHROME DINO",
  "7. RETRO RACING",
  "8. PONG VS AI",
  "9. METEOR DODGE",
  "10. TARGET SHOOT"
};

unsigned long lastDebounceTime = 0;
int score = 0;
int bestScoreFlappy = 0;

// Dynamic Background Shift Variables
uint16_t bgCycleColors[] = { 0x0000, 0x000A, 0x0841, 0x1082, 0x0013, 0x2004, 0x0120, 0x0028 };
int bgHueIndex = 0;
unsigned long lastBgShift = 0;

bool isPressed(int pin) {
  return digitalRead(pin) == LOW;
}

// -------------------------------------------------------------
// HARDWARE VERIFIED JOYSTICK MAPPING (No Drift, Right Inverted)
// -------------------------------------------------------------
bool getJoyUp() {
  return analogRead(PIN_JOY_X) > 3100;
}

bool getJoyDown() {
  return analogRead(PIN_JOY_X) < 900;
}

bool getJoyLeft() {
  return analogRead(PIN_JOY_Y) < 900;
}

bool getJoyRight() {
  return analogRead(PIN_JOY_Y) > 3100;
}

// -------------------------------------------------------------
// SOFTWARE AUDIO ENGINE (Unique 8-Bit SFX Engine)
// -------------------------------------------------------------
void playToneDirect(int freq, int durationMs) {
  if (freq <= 0) {
    digitalWrite(PIN_BUZZER, LOW);
    delay(durationMs);
    return;
  }
  int period = 1000000 / freq;
  long cycles = ((long)freq * durationMs) / 1000;
  for (long i = 0; i < cycles; i++) {
    digitalWrite(PIN_BUZZER, HIGH);
    delayMicroseconds(period / 2);
    digitalWrite(PIN_BUZZER, LOW);
    delayMicroseconds(period / 2);
  }
}

void sndMenuTick()   { playToneDirect(1700, 10); }
void sndMenuSelect() { playToneDirect(900, 30); playToneDirect(1400, 50); playToneDirect(2100, 70); }
void sndFlappyWing() { playToneDirect(700, 15); playToneDirect(950, 20); }
void sndStarBonus()  { playToneDirect(1318, 30); playToneDirect(1567, 30); playToneDirect(2093, 60); }
void sndLaser()      { for(int f=2400; f>600; f-=120) playToneDirect(f, 3); }
void sndAlienBoom()  { for(int f=500; f>150; f-=40) playToneDirect(f, 8); }
void sndSnakeEat()   { playToneDirect(1200, 20); playToneDirect(1800, 35); }
void sndPaddleHit()  { playToneDirect(800, 15); }
void sndBrickSmash() { playToneDirect(1600, 20); playToneDirect(1100, 25); }
void sndMarioJump()  { for(int f=600; f<1400; f+=80) playToneDirect(f, 3); }
void sndDinoHop()    { playToneDirect(1050, 18); }
void sndCarSteer()   { playToneDirect(350, 15); }
void sndCarCrash()   { for(int f=300; f>80; f-=20) playToneDirect(f, 15); }
void sndPongPing()   { playToneDirect(900, 12); }
void sndPongPong()   { playToneDirect(1250, 12); }
void sndMeteorBoom() { for(int f=700; f>100; f-=50) playToneDirect(f, 12); }
void sndSniper()     { playToneDirect(2200, 10); playToneDirect(1500, 25); }
void sndTargetDing() { playToneDirect(2400, 60); }

void sndGameOver()   { 
  playToneDirect(800, 80); delay(40);
  playToneDirect(600, 90); delay(40);
  playToneDirect(400, 120); delay(50);
  playToneDirect(200, 250);
}

// Forward Declarations
void drawSplashScreen();
void drawMenu();
void updateMenuList();
void initFlappy();  void loopFlappy();
void initSpace();   void loopSpace();
void initSnake();   void loopSnake();
void initBrick();   void loopBrick();
void initMario();   void loopMario();
void initDino();    void loopDino();
void initRacing();  void loopRacing();
void initPong();    void loopPong();
void initMeteor();  void loopMeteor();
void initShooter(); void loopShooter();

// -------------------------------------------------------------
// UNIVERSAL CONTROLS
// -------------------------------------------------------------
bool checkUniversalControls() {
  if (isPressed(PIN_BTN_B)) {
    playToneDirect(500, 50);
    delay(200);
    currentState = MENU;
    drawMenu();
    return true;
  }

  if (isPressed(PIN_BTN_P)) {
    playToneDirect(1000, 40);
    delay(250);
    isPaused = !isPaused;
    if (isPaused) {
      tft.fillRect(75, 90, 170, 55, C_RED);
      tft.drawRect(75, 90, 170, 55, C_WHITE);
      tft.setTextColor(C_WHITE);
      tft.setTextSize(2);
      tft.setCursor(125, 100);
      tft.print("PAUSED");
      tft.setTextSize(1);
      tft.setCursor(95, 124);
      tft.print("MADE BY SUBHADIP");
    } else {
      tft.fillRect(75, 90, 170, 55, C_BLACK);
    }
  }
  return false;
}

// -------------------------------------------------------------
// SPLASH INTRO SCREEN
// -------------------------------------------------------------
void drawSplashScreen() {
  tft.fillScreen(C_BLACK);

  tft.drawRect(5, 5, 310, 230, C_CYAN);
  tft.drawRect(7, 7, 306, 226, C_NAVY);

  tft.setTextColor(C_YELLOW);
  tft.setTextSize(3);
  tft.setCursor(45, 40);
  tft.print("RETRO ARCADE");

  tft.fillRoundRect(30, 90, 260, 50, 8, C_NAVY);
  tft.drawRoundRect(30, 90, 260, 50, 8, C_ORANGE);
  
  tft.setTextColor(C_WHITE);
  tft.setTextSize(2);
  tft.setCursor(55, 106);
  tft.print("MADE BY SUBHADIP");

  tft.setTextColor(C_GREEN);
  tft.setTextSize(2);
  tft.setCursor(48, 175);
  tft.print(">> PRESS [A] PLAY <<");

  playToneDirect(440, 70); delay(30);
  playToneDirect(554, 70); delay(30);
  playToneDirect(659, 70); delay(30);
  playToneDirect(880, 160);
}

void handleSplash() {
  if (isPressed(PIN_BTN_A)) {
    sndMenuSelect();
    delay(250);
    currentState = MENU;
    drawMenu();
  }
}

// -------------------------------------------------------------
// MENU SYSTEM
// -------------------------------------------------------------
void drawMenu() {
  tft.fillScreen(C_BLACK);
  
  tft.fillRect(0, 0, 320, 38, C_NAVY);
  tft.drawFastHLine(0, 38, 320, C_ORANGE);
  
  tft.setTextColor(C_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(12, 10);
  tft.print("ARCADE");

  tft.setTextColor(C_CYAN);
  tft.setTextSize(1);
  tft.setCursor(200, 14);
  tft.print("BY SUBHADIP");

  tft.setTextColor(C_LGREY);
  tft.setTextSize(1);
  tft.setCursor(18, 222);
  tft.print("Joy: Select | A: Play | P: Pause | B: Menu");

  updateMenuList();
}

void updateMenuList() {
  int pageStart = (selectedGame / 5) * 5;
  for (int i = 0; i < 5; i++) {
    int gIdx = pageStart + i;
    int y = 46 + (i * 34);
    if (gIdx < TOTAL_GAMES) {
      if (gIdx == selectedGame) {
        tft.fillRoundRect(20, y, 280, 28, 6, C_ORANGE);
        tft.setTextColor(C_BLACK);
      } else {
        tft.fillRoundRect(20, y, 280, 28, 6, C_DGREY);
        tft.setTextColor(C_WHITE);
      }
      tft.setTextSize(2);
      tft.setCursor(32, y + 6);
      tft.print(gameTitles[gIdx]);
    } else {
      tft.fillRect(20, y, 280, 28, C_BLACK);
    }
  }
}

void handleMenu() {
  if (millis() - lastDebounceTime > 260) {
    if (getJoyUp()) {
      selectedGame = (selectedGame - 1 + TOTAL_GAMES) % TOTAL_GAMES;
      sndMenuTick();
      updateMenuList();
      lastDebounceTime = millis();
    } else if (getJoyDown()) {
      selectedGame = (selectedGame + 1) % TOTAL_GAMES;
      sndMenuTick();
      updateMenuList();
      lastDebounceTime = millis();
    }
  }

  if (isPressed(PIN_BTN_A)) {
    sndMenuSelect();
    delay(200);
    tft.fillScreen(C_BLACK);
    score = 0;
    isPaused = false;
    bgHueIndex = 0;
    
    switch (selectedGame) {
      case 0: currentState = GAME_FLAPPY;  initFlappy();  break;
      case 1: currentState = GAME_SPACE;   initSpace();   break;
      case 2: currentState = GAME_SNAKE;   initSnake();   break;
      case 3: currentState = GAME_BRICK;   initBrick();   break;
      case 4: currentState = GAME_MARIO;   initMario();   break;
      case 5: currentState = GAME_DINO;    initDino();    break;
      case 6: currentState = GAME_RACING;  initRacing();  break;
      case 7: currentState = GAME_PONG;    initPong();    break;
      case 8: currentState = GAME_METEOR;  initMeteor();  break;
      case 9: currentState = GAME_SHOOTER; initShooter(); break;
    }
  }
}

// -------------------------------------------------------------
// GAME 1: FLAPPY BIRD PRO
// -------------------------------------------------------------
const int FB_BIRD_X = 55;
const int FB_BIRD_R = 7;
const int FB_PIPE_W = 26;
const int FB_GAP_H = 75;

float fbBirdY, fbVelocity;
int fbPipeX, fbGapY;
int fbLevel = 1;
int fbPipeSpeed = 3;
bool fbPipeScored = false;
bool fbStarActive = false;
int fbStarX, fbStarY;

void initFlappy() {
  fbBirdY = 110; fbVelocity = 0;
  fbPipeX = 320; fbGapY = random(40, 120);
  fbLevel = 1; fbPipeSpeed = 3;
  fbPipeScored = false; fbStarActive = false;
  score = 0;
  bestScoreFlappy = preferences.getInt("bestFlappy", 0);

  tft.fillRect(0, 0, 320, 24, C_NAVY);
  tft.setTextColor(C_WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 8);
  tft.print("BY SUBHADIP");
}

void loopFlappy() {
  if (checkUniversalControls()) return;
  if (isPaused) return;

  if (isPressed(PIN_BTN_A) || getJoyUp()) {
    fbVelocity = -4.8;
    sndFlappyWing();
  }

  tft.fillCircle(FB_BIRD_X, (int)fbBirdY, FB_BIRD_R, C_BLACK);
  tft.fillRect(FB_BIRD_X, (int)fbBirdY - 2, FB_BIRD_R + 5, 4, C_BLACK);

  fbVelocity += 0.42;
  fbBirdY += fbVelocity;

  tft.fillRect(fbPipeX + fbPipeSpeed, 25, FB_PIPE_W + 2, fbGapY - 25, C_BLACK);
  tft.fillRect(fbPipeX + fbPipeSpeed, fbGapY + FB_GAP_H, FB_PIPE_W + 2, 240 - (fbGapY + FB_GAP_H), C_BLACK);

  fbPipeX -= fbPipeSpeed;

  if (fbStarActive) {
    tft.fillCircle(fbStarX, fbStarY, 5, C_BLACK);
    fbStarX -= fbPipeSpeed;
    
    int dx = fbStarX - FB_BIRD_X;
    int dy = fbStarY - (int)fbBirdY;
    if (dx * dx + dy * dy <= 100) {
      fbStarActive = false;
      score += 2;
      sndStarBonus();
    } else if (fbStarX < -10) {
      fbStarActive = false;
    } else {
      tft.fillCircle(fbStarX, fbStarY, 5, C_YELLOW);
    }
  }

  if (!fbPipeScored && fbPipeX + FB_PIPE_W < FB_BIRD_X) {
    score++;
    fbPipeScored = true;
    playToneDirect(1800, 25);

    int newLevel = 1 + score / 8;
    if (newLevel > 5) newLevel = 5;
    if (newLevel > fbLevel) {
      fbLevel = newLevel;
      fbPipeSpeed = 2 + fbLevel;
      sndStarBonus();
    }
  }

  if (fbPipeX + FB_PIPE_W < 0) {
    fbPipeX = 320;
    fbGapY = random(35, 125);
    fbPipeScored = false;

    if (!fbStarActive && random(0, 100) < 45) {
      fbStarActive = true;
      fbStarX = 310;
      fbStarY = random(fbGapY + 12, fbGapY + FB_GAP_H - 12);
    }
  }

  uint16_t pipeColor = (fbLevel % 2 == 0) ? C_CYAN : C_GREEN;
  tft.fillRect(fbPipeX, 25, FB_PIPE_W, fbGapY - 25, pipeColor);
  tft.fillRect(fbPipeX - 2, fbGapY - 6, FB_PIPE_W + 4, 6, C_WHITE);
  tft.fillRect(fbPipeX, fbGapY + FB_GAP_H, FB_PIPE_W, 240 - (fbGapY + FB_GAP_H), pipeColor);
  tft.fillRect(fbPipeX - 2, fbGapY + FB_GAP_H, FB_PIPE_W + 4, 6, C_WHITE);

  tft.fillCircle(FB_BIRD_X, (int)fbBirdY, FB_BIRD_R, C_YELLOW);
  tft.drawPixel(FB_BIRD_X + 2, (int)fbBirdY - 2, C_BLACK);
  tft.drawLine(FB_BIRD_X + 3, (int)fbBirdY, FB_BIRD_X + 7, (int)fbBirdY, C_ORANGE);

  bool collision = false;
  if (fbBirdY - FB_BIRD_R <= 25 || fbBirdY + FB_BIRD_R >= 238) collision = true;
  if (FB_BIRD_X + FB_BIRD_R >= fbPipeX && FB_BIRD_X - FB_BIRD_R <= fbPipeX + FB_PIPE_W) {
    if (fbBirdY - FB_BIRD_R < fbGapY || fbBirdY + FB_BIRD_R > fbGapY + FB_GAP_H) collision = true;
  }

  if (collision) {
    sndGameOver();
    if (score > bestScoreFlappy) {
      bestScoreFlappy = score;
      preferences.putInt("bestFlappy", bestScoreFlappy);
    }
    tft.fillScreen(C_RED);
    tft.setTextColor(C_WHITE);
    tft.setTextSize(3);
    tft.setCursor(65, 80);
    tft.print("GAME OVER");
    tft.setTextSize(2);
    tft.setCursor(60, 130);
    tft.printf("SCORE: %d   BEST: %d", score, bestScoreFlappy);
    delay(1800);
    currentState = MENU;
    drawMenu();
    return;
  }

  tft.fillRect(100, 4, 215, 18, C_NAVY);
  tft.setTextColor(C_WHITE);
  tft.setTextSize(1);
  tft.setCursor(105, 8);
  tft.printf("SCORE: %d   LV: %d   BEST: %d", score, fbLevel, bestScoreFlappy);
  delay(24);
}

// -------------------------------------------------------------
// GAME 2: SPACE SHOOTER
// -------------------------------------------------------------
int shipX, shipY, oldShipX;
int bulletX, bulletY;
bool bulletActive;
int alienX, alienY, alienSpeed;
int spaceStarsX[10], spaceStarsY[10];

void initSpace() {
  shipX = 150; shipY = 200; oldShipX = shipX;
  bulletActive = false;
  alienX = random(20, 290); alienY = 25; alienSpeed = 3;
  for(int i=0; i<10; i++) {
    spaceStarsX[i] = random(10, 310);
    spaceStarsY[i] = random(25, 230);
  }
}

void loopSpace() {
  if (checkUniversalControls()) return;
  if (isPaused) return;

  for(int i=0; i<10; i++) {
    tft.drawPixel(spaceStarsX[i], spaceStarsY[i], C_BLACK);
    spaceStarsY[i] += 1;
    if(spaceStarsY[i] > 235) { spaceStarsY[i] = 25; spaceStarsX[i] = random(10, 310); }
    tft.drawPixel(spaceStarsX[i], spaceStarsY[i], (i % 2 == 0) ? C_CYAN : C_MAGENTA);
  }

  oldShipX = shipX;
  if (getJoyLeft() && shipX > 10) shipX -= 6;
  if (getJoyRight() && shipX < 290) shipX += 6;

  if (oldShipX != shipX) {
    tft.fillTriangle(oldShipX, shipY + 20, oldShipX + 10, shipY, oldShipX + 20, shipY + 20, C_BLACK);
  }
  tft.fillTriangle(shipX, shipY + 20, shipX + 10, shipY, shipX + 20, shipY + 20, C_YELLOW);

  if (isPressed(PIN_BTN_A) && !bulletActive) {
    bulletActive = true;
    bulletX = shipX + 9;
    bulletY = shipY - 5;
    sndLaser();
  }

  if (bulletActive) {
    tft.drawFastVLine(bulletX, bulletY, 6, C_BLACK);
    bulletY -= 11;
    if (bulletY < 20) bulletActive = false;
    else tft.drawFastVLine(bulletX, bulletY, 6, C_CYAN);
  }

  tft.fillRect(alienX, alienY, 20, 14, C_BLACK);
  alienY += alienSpeed;
  if (alienY > 220) { alienY = 25; alienX = random(20, 290); }
  tft.fillRect(alienX, alienY, 20, 14, C_RED);

  if (bulletActive && bulletX >= alienX && bulletX <= alienX + 20 && bulletY <= alienY + 14) {
    tft.fillRect(alienX, alienY, 20, 14, C_BLACK);
    tft.drawFastVLine(bulletX, bulletY, 6, C_BLACK);
    bulletActive = false;
    alienY = 25; alienX = random(20, 290);
    score += 10;
    sndAlienBoom();
  }

  tft.fillRect(10, 6, 120, 12, C_BLACK);
  tft.setTextColor(C_GREEN);
  tft.setTextSize(1);
  tft.setCursor(10, 8);
  tft.printf("SCORE: %d", score);
  delay(25);
}

// -------------------------------------------------------------
// GAME 3: RETRO SNAKE
// -------------------------------------------------------------
#define MAX_SNAKE 60
int snakeX[MAX_SNAKE], snakeY[MAX_SNAKE];
int snakeLen, sDirX, sDirY, foodX, foodY;
uint16_t snakeBorderColor = C_DGREY;

void initSnake() {
  snakeLen = 4; sDirX = 1; sDirY = 0;
  for (int i = 0; i < snakeLen; i++) {
    snakeX[i] = 120 - (i * 8);
    snakeY[i] = 100;
  }
  foodX = 180; foodY = 100;
}

void loopSnake() {
  if (checkUniversalControls()) return;
  if (isPaused) return;

  if (getJoyLeft() && sDirX == 0)       { sDirX = -1; sDirY = 0; }
  else if (getJoyRight() && sDirX == 0) { sDirX = 1;  sDirY = 0; }
  else if (getJoyUp() && sDirY == 0)    { sDirX = 0;  sDirY = -1; }
  else if (getJoyDown() && sDirY == 0)  { sDirX = 0;  sDirY = 1; }

  if (millis() - lastBgShift > 2000) {
    bgHueIndex = (bgHueIndex + 1) % 8;
    snakeBorderColor = (bgHueIndex % 2 == 0) ? C_NAVY : C_DGREEN;
    tft.drawRect(0, 20, 320, 220, snakeBorderColor);
    lastBgShift = millis();
  }

  tft.fillRect(snakeX[snakeLen - 1], snakeY[snakeLen - 1], 8, 8, C_BLACK);

  for (int i = snakeLen - 1; i > 0; i--) {
    snakeX[i] = snakeX[i - 1];
    snakeY[i] = snakeY[i - 1];
  }

  snakeX[0] += sDirX * 8;
  snakeY[0] += sDirY * 8;

  if (snakeX[0] < 8) snakeX[0] = 304;
  if (snakeX[0] > 304) snakeX[0] = 8;
  if (snakeY[0] < 24) snakeY[0] = 224;
  if (snakeY[0] > 224) snakeY[0] = 24;

  tft.fillRect(foodX, foodY, 8, 8, C_RED);

  if (abs(snakeX[0] - foodX) < 8 && abs(snakeY[0] - foodY) < 8) {
    if (snakeLen < MAX_SNAKE) snakeLen++;
    foodX = (random(2, 38)) * 8;
    foodY = (random(4, 28)) * 8;
    score += 5;
    sndSnakeEat();
  }

  tft.fillRect(snakeX[0], snakeY[0], 8, 8, C_GREEN);
  for (int i = 1; i < snakeLen; i++) {
    tft.fillRect(snakeX[i], snakeY[i], 8, 8, C_DGREEN);
  }
  delay(55);
}

// -------------------------------------------------------------
// GAME 4: BRICK BREAKER
// -------------------------------------------------------------
int padX, oldPadX;
float bX, bY, bVx, bVy;
#define BRICK_ROWS 4
#define BRICK_COLS 8
bool bricks[BRICK_ROWS][BRICK_COLS];

void initBrick() {
  padX = 130; oldPadX = 130;
  bX = 160; bY = 160;
  bVx = 3.0; bVy = -3.2;
  uint16_t rowColors[] = { C_RED, C_ORANGE, C_YELLOW, C_GREEN };
  for (int r = 0; r < BRICK_ROWS; r++) {
    for (int c = 0; c < BRICK_COLS; c++) {
      bricks[r][c] = true;
      tft.fillRect(12 + c * 37, 30 + r * 14, 33, 10, rowColors[r]);
    }
  }
}

void loopBrick() {
  if (checkUniversalControls()) return;
  if (isPaused) return;

  oldPadX = padX;
  if (getJoyLeft() && padX > 8) padX -= 6;
  if (getJoyRight() && padX < 252) padX += 6;

  if (oldPadX != padX) tft.fillRect(oldPadX, 215, 60, 8, C_BLACK);
  tft.fillRect(padX, 215, 60, 8, C_CYAN);

  tft.fillCircle((int)bX, (int)bY, 4, C_BLACK);
  bX += bVx; bY += bVy;

  if (bX <= 4 || bX >= 316) { bVx = -bVx; sndPaddleHit(); }
  if (bY <= 15) { bVy = -bVy; sndPaddleHit(); }

  if (bY >= 211 && bY <= 218 && bX >= padX && bX <= padX + 60) {
    bVy = -abs(bVy);
    sndPaddleHit();
  }

  int col = (bX - 12) / 37;
  int row = (bY - 30) / 14;
  if (row >= 0 && row < BRICK_ROWS && col >= 0 && col < BRICK_COLS) {
    if (bricks[row][col]) {
      bricks[row][col] = false;
      tft.fillRect(12 + col * 37, 30 + row * 14, 33, 10, C_BLACK);
      bVy = -bVy;
      score += 10;
      sndBrickSmash();
    }
  }

  if (bY > 235) {
    sndGameOver();
    tft.fillScreen(C_RED);
    tft.setTextColor(C_WHITE);
    tft.setTextSize(2);
    tft.setCursor(90, 100);
    tft.print("OUT OF BALLS");
    delay(1500);
    currentState = MENU;
    drawMenu();
    return;
  }

  tft.fillCircle((int)bX, (int)bY, 4, C_WHITE);
  tft.fillRect(10, 8, 90, 10, C_BLACK);
  tft.setTextColor(C_GREEN);
  tft.setTextSize(1);
  tft.setCursor(10, 8);
  tft.printf("SCORE: %d", score);
  delay(24);
}

// -------------------------------------------------------------
// GAME 5: MARIO PLATFORM RUNNER
// -------------------------------------------------------------
float marioY, marioVelY;
bool isGrounded;
int goombaX;
const int GROUND_Y = 190;
uint16_t skyColors[] = { 0x000F, 0x0017, 0x187F, 0x0011 };
int skyIndex = 0;

void initMario() {
  marioY = GROUND_Y - 20; marioVelY = 0; isGrounded = true; goombaX = 310;
  tft.fillRect(0, GROUND_Y, 320, 50, C_BROWN);
  tft.fillRect(0, GROUND_Y, 320, 6, C_GREEN);
}

void loopMario() {
  if (checkUniversalControls()) return;
  if (isPaused) return;

  if ((isPressed(PIN_BTN_A) || getJoyUp()) && isGrounded) {
    marioVelY = -8.5;
    isGrounded = false;
    sndMarioJump();
  }

  tft.fillRect(50, (int)marioY, 16, 20, C_BLACK);
  marioVelY += 0.65;
  marioY += marioVelY;

  if (marioY >= GROUND_Y - 20) {
    marioY = GROUND_Y - 20; marioVelY = 0; isGrounded = true;
  }

  tft.fillRect(50, (int)marioY, 16, 10, C_RED);
  tft.fillRect(50, (int)marioY + 10, 16, 10, C_BLUE);

  tft.fillRect(goombaX + 4, GROUND_Y - 14, 14, 14, C_BLACK);
  goombaX -= 5;
  if (goombaX < -15) { 
    goombaX = 320; 
    score += 10; 
    playToneDirect(1700, 20);
    skyIndex = (skyIndex + 1) % 4;
    tft.drawFastHLine(0, 189, 320, skyColors[skyIndex]);
  }
  tft.fillRect(goombaX, GROUND_Y - 14, 14, 14, C_MAGENTA);

  if (goombaX >= 42 && goombaX <= 64 && marioY + 20 >= GROUND_Y - 14) {
    sndGameOver();
    tft.fillScreen(C_RED);
    tft.setTextColor(C_WHITE);
    tft.setTextSize(2);
    tft.setCursor(100, 100);
    tft.print("MARIO DIED");
    delay(1500);
    currentState = MENU;
    drawMenu();
    return;
  }

  tft.fillRect(10, 8, 90, 10, C_BLACK);
  tft.setTextColor(C_YELLOW);
  tft.setTextSize(1);
  tft.setCursor(10, 8);
  tft.printf("SCORE: %d", score);
  delay(25);
}

// -------------------------------------------------------------
// GAME 6: CHROME DINO
// -------------------------------------------------------------
float dinoY, dinoVelY;
bool dinoGrounded;
int cactusX;
const int DINO_GROUND = 200;

void initDino() {
  dinoY = DINO_GROUND - 18; dinoVelY = 0; dinoGrounded = true; cactusX = 320;
  tft.drawFastHLine(0, DINO_GROUND, 320, C_WHITE);
}

void loopDino() {
  if (checkUniversalControls()) return;
  if (isPaused) return;

  if ((isPressed(PIN_BTN_A) || getJoyUp()) && dinoGrounded) {
    dinoVelY = -8.2;
    dinoGrounded = false;
    sndDinoHop();
  }

  tft.fillRect(45, (int)dinoY, 14, 18, C_BLACK);
  dinoVelY += 0.65;
  dinoY += dinoVelY;

  if (dinoY >= DINO_GROUND - 18) {
    dinoY = DINO_GROUND - 18; dinoVelY = 0; dinoGrounded = true;
  }

  tft.fillRect(45, (int)dinoY, 14, 18, C_LGREY);

  tft.fillRect(cactusX + 5, DINO_GROUND - 16, 8, 16, C_BLACK);
  cactusX -= 6;
  if (cactusX < -10) { 
    cactusX = 320; 
    score += 5; 
    playToneDirect(1900, 15);
    tft.drawFastHLine(0, DINO_GROUND, 320, (score % 20 == 0) ? C_ORANGE : C_WHITE);
  }
  tft.fillRect(cactusX, DINO_GROUND - 16, 8, 16, C_GREEN);

  if (cactusX >= 40 && cactusX <= 58 && dinoY + 18 >= DINO_GROUND - 16) {
    sndGameOver();
    tft.fillScreen(C_RED);
    tft.setTextColor(C_WHITE);
    tft.setTextSize(2);
    tft.setCursor(88, 100);
    tft.print("DINO CRASHED");
    delay(1500);
    currentState = MENU;
    drawMenu();
    return;
  }

  tft.fillRect(10, 8, 90, 10, C_BLACK);
  tft.setTextColor(C_WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 8);
  tft.printf("SCORE: %d", score);
  delay(25);
}

// -------------------------------------------------------------
// GAME 7: RETRO HIGHWAY RACING
// -------------------------------------------------------------
int carLane, enemyLane, enemyY, oldCarLane;
const int lanes[3] = { 75, 150, 225 };
int roadDashY = 0;
unsigned long lastSteerTime = 0;

void initRacing() {
  carLane = 1; oldCarLane = 1;
  enemyLane = 0; enemyY = 20;
}

void loopRacing() {
  if (checkUniversalControls()) return;
  if (isPaused) return;

  roadDashY = (roadDashY + 7) % 30;
  for(int y = -30; y < 240; y += 30) {
    tft.drawFastVLine(120, y + roadDashY, 15, C_YELLOW);
    tft.drawFastVLine(195, y + roadDashY, 15, C_YELLOW);
    tft.drawFastVLine(120, y + roadDashY - 15, 15, C_BLACK);
    tft.drawFastVLine(195, y + roadDashY - 15, 15, C_BLACK);
  }

  oldCarLane = carLane;
  if (millis() - lastSteerTime > 190) {
    if (getJoyLeft() && carLane > 0) {
      carLane--;
      sndCarSteer();
      lastSteerTime = millis();
    } else if (getJoyRight() && carLane < 2) {
      carLane++;
      sndCarSteer();
      lastSteerTime = millis();
    }
  }

  if (oldCarLane != carLane) {
    tft.fillRect(lanes[oldCarLane], 185, 20, 35, C_BLACK);
  }
  tft.fillRect(lanes[carLane], 185, 20, 35, C_CYAN);

  tft.fillRect(lanes[enemyLane], enemyY - 6, 20, 35, C_BLACK);
  enemyY += 6;
  if (enemyY > 235) {
    enemyY = 20;
    enemyLane = random(0, 3);
    score += 10;
    playToneDirect(1600, 25);
  }
  tft.fillRect(lanes[enemyLane], enemyY, 20, 35, C_RED);

  if (enemyLane == carLane && enemyY + 35 >= 185 && enemyY <= 220) {
    sndCarCrash();
    tft.fillScreen(C_RED);
    tft.setTextColor(C_WHITE);
    tft.setTextSize(2);
    tft.setCursor(80, 100);
    tft.print("HIGHWAY CRASH");
    delay(1500);
    currentState = MENU;
    drawMenu();
    return;
  }

  tft.fillRect(10, 8, 90, 10, C_BLACK);
  tft.setTextColor(C_YELLOW);
  tft.setTextSize(1);
  tft.setCursor(10, 8);
  tft.printf("SCORE: %d", score);
  delay(25);
}

// -------------------------------------------------------------
// GAME 8: PONG VS AI (Fixed Arguments)
// -------------------------------------------------------------
int pPaddleY, oldPPaddleY, aiPaddleY, oldAiPaddleY;
float pongBX, pongBY, pongVx, pongVy;

void initPong() {
  pPaddleY = 95; oldPPaddleY = 95;
  aiPaddleY = 95; oldAiPaddleY = 95;
  pongBX = 160; pongBY = 120;
  pongVx = 4.0; pongVy = 2.4;
  for (int y = 0; y < 240; y += 12) tft.drawFastVLine(160, y, 6, C_DGREY);
}

void loopPong() {
  if (checkUniversalControls()) return;
  if (isPaused) return;

  oldPPaddleY = pPaddleY;
  if (getJoyUp() && pPaddleY > 10) pPaddleY -= 6;
  if (getJoyDown() && pPaddleY < 185) pPaddleY += 6;

  if (oldPPaddleY != pPaddleY) {
    tft.fillRect(15, oldPPaddleY, 6, 42, C_BLACK);
  }
  tft.fillRect(15, pPaddleY, 6, 42, C_GREEN);

  oldAiPaddleY = aiPaddleY;
  if (aiPaddleY + 21 < pongBY && aiPaddleY < 185) aiPaddleY += 3;
  if (aiPaddleY + 21 > pongBY && aiPaddleY > 10) aiPaddleY -= 3;

  if (oldAiPaddleY != aiPaddleY) {
    tft.fillRect(299, oldAiPaddleY, 6, 42, C_BLACK);
  }
  tft.fillRect(299, aiPaddleY, 6, 42, C_RED);

  tft.fillCircle((int)pongBX, (int)pongBY, 4, C_BLACK);
  pongBX += pongVx; pongBY += pongVy;

  if (pongBY <= 10 || pongBY >= 230) { pongVy = -pongVy; sndPongPing(); }

  if (pongBX <= 25 && pongBY >= pPaddleY && pongBY <= pPaddleY + 42) {
    pongVx = abs(pongVx);
    score++;
    sndPongPong();
  }

  if (pongBX >= 295 && pongBY >= aiPaddleY && pongBY <= aiPaddleY + 42) {
    pongVx = -abs(pongVx);
    sndPongPing();
  }

  if (pongBX < 6) {
    sndGameOver();
    tft.fillScreen(C_RED);
    tft.setTextColor(C_WHITE);
    tft.setTextSize(2);
    tft.setCursor(120, 100);
    tft.print("AI WON!");
    delay(1500);
    currentState = MENU;
    drawMenu();
    return;
  }
  if (pongBX > 314) {
    score += 50;
    playToneDirect(2200, 40);
    pongBX = 160; pongBY = 120;
    pongVx = -4.0;
  }

  tft.fillCircle((int)pongBX, (int)pongBY, 4, C_WHITE);
  tft.fillRect(130, 8, 90, 10, C_BLACK);
  tft.setTextColor(C_WHITE);
  tft.setTextSize(1);
  tft.setCursor(135, 8);
  tft.printf("SCORE: %d", score);
  delay(24);
}

// -------------------------------------------------------------
// GAME 9: METEOR DODGE
// -------------------------------------------------------------
int playerX, playerY, oldPX, oldPY;
int mX[3], mY[3], mSpd[3];
uint16_t meteorColor[] = { C_RED, C_ORANGE, C_YELLOW };

void initMeteor() {
  playerX = 155; playerY = 205; oldPX = 155; oldPY = 205;
  for (int i = 0; i < 3; i++) {
    mX[i] = random(20, 300);
    mY[i] = random(-80, -20);
    mSpd[i] = random(3, 7);
  }
}

void loopMeteor() {
  if (checkUniversalControls()) return;
  if (isPaused) return;

  oldPX = playerX;
  oldPY = playerY;

  if (getJoyLeft() && playerX > 15) playerX -= 6;
  if (getJoyRight() && playerX < 305) playerX += 6;
  if (getJoyUp() && playerY > 20) playerY -= 6;
  if (getJoyDown() && playerY < 225) playerY += 6;

  if (oldPX != playerX || oldPY != playerY) {
    tft.fillCircle(oldPX, oldPY, 7, C_BLACK);
  }
  tft.fillCircle(playerX, playerY, 7, C_CYAN);

  for (int i = 0; i < 3; i++) {
    tft.fillCircle(mX[i], mY[i], 9, C_BLACK);
    mY[i] += mSpd[i];
    if (mY[i] > 240) {
      mY[i] = -20;
      mX[i] = random(20, 300);
      mSpd[i] = random(3, 7);
      score += 5;
      playToneDirect(1400, 15);
    }
    tft.fillCircle(mX[i], mY[i], 9, meteorColor[i]);

    if (abs(playerX - mX[i]) < 14 && abs(playerY - mY[i]) < 14) {
      sndMeteorBoom();
      tft.fillScreen(C_RED);
      tft.setTextColor(C_WHITE);
      tft.setTextSize(2);
      tft.setCursor(80, 100);
      tft.print("METEOR IMPACT");
      delay(1500);
      currentState = MENU;
      drawMenu();
      return;
    }
  }

  tft.fillRect(10, 8, 90, 10, C_BLACK);
  tft.setTextColor(C_GREEN);
  tft.setTextSize(1);
  tft.setCursor(10, 8);
  tft.printf("SCORE: %d", score);
  delay(25);
}

// -------------------------------------------------------------
// GAME 10: TARGET SHOOTING GALLERY
// -------------------------------------------------------------
int crossX, crossY, oldCX, oldCY;
int tgtX, tgtY, tgtVx;

void initShooter() {
  crossX = 160; crossY = 120; oldCX = 160; oldCY = 120;
  tgtX = 40; tgtY = 60; tgtVx = 5;
}

void loopShooter() {
  if (checkUniversalControls()) return;
  if (isPaused) return;

  oldCX = crossX;
  oldCY = crossY;

  if (getJoyLeft() && crossX > 15) crossX -= 6;
  if (getJoyRight() && crossX < 305) crossX += 6;
  if (getJoyUp() && crossY > 25) crossY -= 6;
  if (getJoyDown() && crossY < 225) crossY += 6;

  if (oldCX != crossX || oldCY != crossY) {
    tft.drawCircle(oldCX, oldCY, 8, C_BLACK);
    tft.drawFastHLine(oldCX - 10, oldCY, 20, C_BLACK);
    tft.drawFastVLine(oldCX, oldCY - 10, 20, C_BLACK);
  }

  tft.fillCircle(tgtX, tgtY, 14, C_BLACK);
  tgtX += tgtVx;
  if (tgtX < 30 || tgtX > 290) tgtVx = -tgtVx;
  tft.fillCircle(tgtX, tgtY, 14, C_RED);
  tft.fillCircle(tgtX, tgtY, 7, C_WHITE);

  if (isPressed(PIN_BTN_A)) {
    sndSniper();
    if (abs(crossX - tgtX) < 14 && abs(crossY - tgtY) < 14) {
      score += 20;
      sndTargetDing();
      tft.fillCircle(tgtX, tgtY, 14, C_YELLOW);
      delay(50);
      tft.fillCircle(tgtX, tgtY, 14, C_BLACK);
      tgtX = random(40, 280);
      tgtY = random(40, 150);
    }
  }

  tft.drawCircle(crossX, crossY, 8, C_GREEN);
  tft.drawFastHLine(crossX - 10, crossY, 20, C_GREEN);
  tft.drawFastVLine(crossX, crossY - 10, 20, C_GREEN);

  tft.fillRect(10, 8, 90, 10, C_BLACK);
  tft.setTextColor(C_WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 8);
  tft.printf("SCORE: %d", score);
  delay(25);
}

// -------------------------------------------------------------
// ARDUINO SETUP & MAIN LOOP
// -------------------------------------------------------------
void setup() {
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  pinMode(PIN_BTN_A, INPUT_PULLUP);
  pinMode(PIN_BTN_B, INPUT_PULLUP);
  pinMode(PIN_BTN_P, INPUT_PULLUP);

  preferences.begin("flappy_subhadip", false);

  hspi.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);
  tft.init(240, 320);
  tft.setRotation(3);

  drawSplashScreen();
}

void loop() {
  switch (currentState) {
    case SPLASH:       handleSplash(); break;
    case MENU:         handleMenu();   break;
    case GAME_FLAPPY:  loopFlappy();   break;
    case GAME_SPACE:   loopSpace();    break;
    case GAME_SNAKE:   loopSnake();    break;
    case GAME_BRICK:   loopBrick();    break;
    case GAME_MARIO:   loopMario();    break;
    case GAME_DINO:    loopDino();     break;
    case GAME_RACING:  loopRacing();   break;
    case GAME_PONG:    loopPong();     break;
    case GAME_METEOR:  loopMeteor();   break;
    case GAME_SHOOTER: loopShooter();  break;
  }
}
