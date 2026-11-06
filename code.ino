#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Preferences.h>

// Display SPI Pins
#define TFT_CS    7
#define TFT_DC    5
#define TFT_MOSI  6
#define TFT_SCK   4
#define TFT_RST   -1  // RST 3.3V se connected hai

// Control & Sound Pins
#define PIN_JOY_X  0
#define PIN_JOY_Y  1
#define PIN_BUZZER 2
#define PIN_BTN_A  3   // Jump / Action / Shoot / Select
#define PIN_BTN_B  8   // Exit to Main Menu
#define PIN_BTN_P  9   // Dedicated Pause / Resume Button

// Colors
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
  GAME_MARIO 
};

GameState currentState = SPLASH;
bool isPaused = false;
int selectedGame = 0;
const int TOTAL_GAMES = 5;

const char* gameTitles[] = { 
  "1. FLAPPY BIRD PRO", 
  "2. SPACE SHOOTER", 
  "3. RETRO SNAKE", 
  "4. BRICK BREAKER", 
  "5. MARIO RUNNER" 
};

unsigned long lastDebounceTime = 0;
int score = 0;
int bestScoreFlappy = 0;

bool isPressed(int pin) {
  return digitalRead(pin) == LOW;
}

// -------------------------------------------------------------
// JOYSTICK CALIBRATION
// -------------------------------------------------------------
bool getJoyUp() {
  return analogRead(PIN_JOY_X) > 3100;
}

bool getJoyDown() {
  return analogRead(PIN_JOY_X) < 900;
}

bool getJoyLeft() {
  return analogRead(PIN_JOY_Y) > 3100;
}

bool getJoyRight() {
  return analogRead(PIN_JOY_Y) < 900;
}

// -------------------------------------------------------------
// SAFE SOFTWARE AUDIO QUEUE (No Hardware Timers / Crash-Proof)
// -------------------------------------------------------------
struct SoundNote {
  int frequency;
  unsigned int duration;
};

const int SOUND_QUEUE_SIZE = 32;
SoundNote soundQueue[SOUND_QUEUE_SIZE];
int soundRead = 0;
int soundWrite = 0;

void addSound(int frequency, unsigned int duration) {
  int next = (soundWrite + 1) % SOUND_QUEUE_SIZE;
  if (next == soundRead) return;
  soundQueue[soundWrite].frequency = frequency;
  soundQueue[soundWrite].duration = duration;
  soundWrite = next;
}

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

void processSoundQueue() {
  while (soundRead != soundWrite) {
    SoundNote note = soundQueue[soundRead];
    soundRead = (soundRead + 1) % SOUND_QUEUE_SIZE;
    playToneDirect(note.frequency, note.duration);
  }
}

void sndMenuTick()   { playToneDirect(1600, 12); }
void sndMenuSelect() { playToneDirect(1200, 30); delay(35); playToneDirect(1800, 60); }
void sndShoot()      { playToneDirect(1300, 15); }
void sndJump()       { playToneDirect(850, 20); }
void sndScore()      { playToneDirect(1800, 30); }
void sndHit()        { playToneDirect(350, 35); }
void sndBonus()      { playToneDirect(1200, 40); playToneDirect(1600, 50); }
void sndLevel()      { playToneDirect(700, 50); playToneDirect(1000, 50); playToneDirect(1400, 80); }
void sndGameOver()   { playToneDirect(880, 80); playToneDirect(660, 100); playToneDirect(440, 130); playToneDirect(250, 180); }

// Forward Declarations
void drawSplashScreen();
void drawMenu();
void updateMenuList();
void initFlappy(); void loopFlappy();
void initSpace();  void loopSpace();
void initSnake();  void loopSnake();
void initBrick();  void loopBrick();
void initMario();  void loopMario();

// -------------------------------------------------------------
// UNIVERSAL CONTROLS (Pause & Menu Exit)
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

  playToneDirect(440, 80); delay(40);
  playToneDirect(554, 80); delay(40);
  playToneDirect(659, 80); delay(40);
  playToneDirect(880, 180);
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
  for (int i = 0; i < TOTAL_GAMES; i++) {
    int y = 46 + (i * 34);
    if (i == selectedGame) {
      tft.fillRoundRect(20, y, 280, 28, 6, C_ORANGE);
      tft.setTextColor(C_BLACK);
    } else {
      tft.fillRoundRect(20, y, 280, 28, 6, C_DGREY);
      tft.setTextColor(C_WHITE);
    }
    tft.setTextSize(2);
    tft.setCursor(32, y + 6);
    tft.print(gameTitles[i]);
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
    
    switch (selectedGame) {
      case 0: currentState = GAME_FLAPPY; initFlappy(); break;
      case 1: currentState = GAME_SPACE;  initSpace();  break;
      case 2: currentState = GAME_SNAKE;  initSnake();  break;
      case 3: currentState = GAME_BRICK;  initBrick();  break;
      case 4: currentState = GAME_MARIO;  initMario();  break;
    }
  }
}

// -------------------------------------------------------------
// GAME 1: FLAPPY BIRD PRO (Advanced Features with Color Display)
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
  fbBirdY = 110;
  fbVelocity = 0;
  fbPipeX = 320;
  fbGapY = random(40, 120);
  fbLevel = 1;
  fbPipeSpeed = 3;
  fbPipeScored = false;
  fbStarActive = false;
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
    sndJump();
  }

  // Clear previous bird
  tft.fillCircle(FB_BIRD_X, (int)fbBirdY, FB_BIRD_R, C_BLACK);
  tft.fillRect(FB_BIRD_X, (int)fbBirdY - 2, FB_BIRD_R + 5, 4, C_BLACK);

  fbVelocity += 0.42;
  fbBirdY += fbVelocity;

  // Clear previous pipe column
  tft.fillRect(fbPipeX + fbPipeSpeed, 25, FB_PIPE_W + 2, fbGapY - 25, C_BLACK);
  tft.fillRect(fbPipeX + fbPipeSpeed, fbGapY + FB_GAP_H, FB_PIPE_W + 2, 240 - (fbGapY + FB_GAP_H), C_BLACK);

  fbPipeX -= fbPipeSpeed;

  // Star handling
  if (fbStarActive) {
    tft.fillCircle(fbStarX, fbStarY, 5, C_BLACK);
    fbStarX -= fbPipeSpeed;
    
    int dx = fbStarX - FB_BIRD_X;
    int dy = fbStarY - (int)fbBirdY;
    if (dx * dx + dy * dy <= 100) {
      fbStarActive = false;
      score += 2;
      sndBonus();
    } else if (fbStarX < -10) {
      fbStarActive = false;
    } else {
      tft.fillCircle(fbStarX, fbStarY, 5, C_YELLOW);
    }
  }

  // Score pipe
  if (!fbPipeScored && fbPipeX + FB_PIPE_W < FB_BIRD_X) {
    score++;
    fbPipeScored = true;
    sndScore();

    // Level progression
    int newLevel = 1 + score / 8;
    if (newLevel > 5) newLevel = 5;
    if (newLevel > fbLevel) {
      fbLevel = newLevel;
      fbPipeSpeed = 2 + fbLevel;
      sndLevel();
    }
  }

  // Spawn new pipe & occasional star
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

  // Draw Pipes
  tft.fillRect(fbPipeX, 25, FB_PIPE_W, fbGapY - 25, C_GREEN);
  tft.fillRect(fbPipeX - 2, fbGapY - 6, FB_PIPE_W + 4, 6, C_DGREEN);
  tft.fillRect(fbPipeX, fbGapY + FB_GAP_H, FB_PIPE_W, 240 - (fbGapY + FB_GAP_H), C_GREEN);
  tft.fillRect(fbPipeX - 2, fbGapY + FB_GAP_H, FB_PIPE_W + 4, 6, C_DGREEN);

  // Draw Bird
  tft.fillCircle(FB_BIRD_X, (int)fbBirdY, FB_BIRD_R, C_YELLOW);
  tft.drawPixel(FB_BIRD_X + 2, (int)fbBirdY - 2, C_BLACK);
  tft.drawLine(FB_BIRD_X + 3, (int)fbBirdY, FB_BIRD_X + 7, (int)fbBirdY, C_ORANGE);

  // Collision Detection
  bool collision = false;
  if (fbBirdY - FB_BIRD_R <= 25 || fbBirdY + FB_BIRD_R >= 238) collision = true;
  if (FB_BIRD_X + FB_BIRD_R >= fbPipeX && FB_BIRD_X - FB_BIRD_R <= fbPipeX + FB_PIPE_W) {
    if (fbBirdY - FB_BIRD_R < fbGapY || fbBirdY + FB_BIRD_R > fbGapY + FB_GAP_H) {
      collision = true;
    }
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
    tft.printf("SCORE: %d", score);
    tft.setCursor(180, 130);
    tft.printf("BEST: %d", bestScoreFlappy);

    delay(1800);
    currentState = MENU;
    drawMenu();
    return;
  }

  // Top Status Bar
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

void initSpace() {
  shipX = 150; shipY = 200; oldShipX = shipX;
  bulletActive = false;
  alienX = random(20, 290); alienY = 25; alienSpeed = 3;
}

void loopSpace() {
  if (checkUniversalControls()) return;
  if (isPaused) return;

  oldShipX = shipX;
  if (getJoyLeft() && shipX > 10) shipX -= 6;
  if (getJoyRight() && shipX < 290) shipX += 6;

  if (oldShipX != shipX) {
    tft.fillTriangle(oldShipX, shipY + 20, oldShipX + 10, shipY, oldShipX + 20, shipY + 20, C_BLACK);
  }
  tft.fillTriangle(shipX, shipY + 20, shipX + 10, shipY, shipX + 20, shipY + 20, C_CYAN);

  if (isPressed(PIN_BTN_A) && !bulletActive) {
    bulletActive = true;
    bulletX = shipX + 9;
    bulletY = shipY - 5;
    sndShoot();
  }

  if (bulletActive) {
    tft.drawFastVLine(bulletX, bulletY, 6, C_BLACK);
    bulletY -= 10;
    if (bulletY < 10) bulletActive = false;
    else tft.drawFastVLine(bulletX, bulletY, 6, C_YELLOW);
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
    sndScore();
  }

  tft.fillRect(10, 8, 120, 10, C_BLACK);
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

  tft.fillRect(snakeX[snakeLen - 1], snakeY[snakeLen - 1], 8, 8, C_BLACK);

  for (int i = snakeLen - 1; i > 0; i--) {
    snakeX[i] = snakeX[i - 1];
    snakeY[i] = snakeY[i - 1];
  }

  snakeX[0] += sDirX * 8;
  snakeY[0] += sDirY * 8;

  if (snakeX[0] < 0) snakeX[0] = 312;
  if (snakeX[0] > 312) snakeX[0] = 0;
  if (snakeY[0] < 0) snakeY[0] = 232;
  if (snakeY[0] > 232) snakeY[0] = 0;

  tft.fillRect(foodX, foodY, 8, 8, C_RED);

  if (abs(snakeX[0] - foodX) < 8 && abs(snakeY[0] - foodY) < 8) {
    if (snakeLen < MAX_SNAKE) snakeLen++;
    foodX = (random(2, 38)) * 8;
    foodY = (random(3, 28)) * 8;
    score += 5;
    sndScore();
  }

  tft.fillRect(snakeX[0], snakeY[0], 8, 8, C_GREEN);
  for (int i = 1; i < snakeLen; i++) {
    tft.fillRect(snakeX[i], snakeY[i], 8, 8, C_DGREEN);
  }
  delay(60);
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
  for (int r = 0; r < BRICK_ROWS; r++) {
    for (int c = 0; c < BRICK_COLS; c++) {
      bricks[r][c] = true;
      tft.fillRect(12 + c * 37, 30 + r * 14, 33, 10, C_ORANGE);
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

  if (bX <= 4 || bX >= 316) { bVx = -bVx; sndHit(); }
  if (bY <= 15) { bVy = -bVy; sndHit(); }

  if (bY >= 211 && bY <= 218 && bX >= padX && bX <= padX + 60) {
    bVy = -abs(bVy);
    sndHit();
  }

  int col = (bX - 12) / 37;
  int row = (bY - 30) / 14;
  if (row >= 0 && row < BRICK_ROWS && col >= 0 && col < BRICK_COLS) {
    if (bricks[row][col]) {
      bricks[row][col] = false;
      tft.fillRect(12 + col * 37, 30 + row * 14, 33, 10, C_BLACK);
      bVy = -bVy;
      score += 10;
      sndScore();
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
  delay(25);
}

// -------------------------------------------------------------
// GAME 5: MARIO PLATFORM RUNNER
// -------------------------------------------------------------
float marioY, marioVelY;
bool isGrounded;
int goombaX;
const int GROUND_Y = 190;

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
    sndJump();
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
  if (goombaX < -15) { goombaX = 320; score += 10; sndScore(); }
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
  tft.setRotation(3); // Horizontal orientation

  drawSplashScreen();
}

void loop() {
  switch (currentState) {
    case SPLASH:      handleSplash(); break;
    case MENU:        handleMenu();   break;
    case GAME_FLAPPY: loopFlappy();   break;
    case GAME_SPACE:  loopSpace();    break;
    case GAME_SNAKE:  loopSnake();    break;
    case GAME_BRICK:  loopBrick();    break;
    case GAME_MARIO:  loopMario();    break;
  }
}
