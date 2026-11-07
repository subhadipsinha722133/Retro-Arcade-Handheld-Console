#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Preferences.h>

// Hardware SPI Pins
#define TFT_CS    7
#define TFT_DC    5
#define TFT_MOSI  6
#define TFT_SCK   4
#define TFT_RST   -1

#define PIN_JOY_X  0
#define PIN_JOY_Y  1
#define PIN_BUZZER 2
#define PIN_BTN_A  3
#define PIN_BTN_B  8
#define PIN_BTN_P  9

#define SCREEN_W  320
#define SCREEN_H  240
#define HUD_H     28

// Colors (RGB565 Palette)
#define C_BG          0x0000
#define C_BLACK       0x0000
#define C_WHITE       0xFFFF
#define C_RED         0xF800
#define C_GREEN       0x07E0
#define C_BLUE        0x001F
#define C_YELLOW      0xFFE0
#define C_MAGENTA     0xF81F
#define C_CYAN        0x07FF
#define C_ORANGE      0xFD20
#define C_PINK        0xFC18
#define C_NAVY        0x0010
#define C_DGREEN      0x03E0
#define C_DGREY       0x3186
#define C_ROADGREY    0x2104
#define C_LGREY       0xBDF7
#define C_BROWN       0x9260
#define C_PEACH       0xFCE0

SPIClass hspi(FSPI);
Adafruit_ST7789 tft = Adafruit_ST7789(&hspi, TFT_CS, TFT_DC, TFT_RST);
Preferences preferences;

enum GameState { 
  SPLASH, MENU, 
  G_FLAPPY, G_SPACE, G_SNAKE, G_BRICK, G_MARIO,
  G_DINO, G_RACING, G_PONG, G_SHOOTER, G_RIVER,
  G_PACMAN, G_ASTEROIDS, G_GALAGA, G_COPTER, G_SUBMARINE
};

GameState currentState = SPLASH;
bool isPaused = false;
int selectedGame = 0;
const int TOTAL_GAMES = 15;

const char* gameTitles[] = { 
  "1. FLAPPY BIRD PRO",  "2. SPACE SHOOTER",  "3. RETRO SNAKE", 
  "4. BRICK BREAKER",    "5. MARIO RUNNER",   "6. CHROME DINO PRO",
  "7. RETRO RACING",     "8. PONG VS AI",     "9. TARGET SHOOT",
  "10. RIVER RAID JET",  "11. PAC-MAN ARCADE","12. ASTEROID FIELD",
  "13. GALAGA WAVES",    "14. DAN THE MAN", "15. COMBAT TANK 90"
};

unsigned long lastDebounceTime = 0;
long score = 0;
long lastRenderedScore = -1;

bool isPressed(int pin) {
  return digitalRead(pin) == LOW;
}

// Calibrated Joystick (Deadzone tuned)
bool getJoyUp()    { return analogRead(PIN_JOY_X) > 3150; }
bool getJoyDown()  { return analogRead(PIN_JOY_X) < 850;  }
bool getJoyLeft()  { return analogRead(PIN_JOY_Y) < 850;  }
bool getJoyRight() { return analogRead(PIN_JOY_Y) > 3150; }

// Safe Sound Generation Engine
void playSound(int freq, int durationMs) {
  if (freq <= 0) { delay(durationMs); return; }
  int period = 1000000 / freq;
  long cycles = ((long)freq * durationMs) / 1000;
  for (long i = 0; i < cycles; i++) {
    digitalWrite(PIN_BUZZER, HIGH);
    delayMicroseconds(period / 2);
    digitalWrite(PIN_BUZZER, LOW);
    delayMicroseconds(period / 2);
  }
}

void sfxMenuTick()   { playSound(1800, 8); }
void sfxMenuSelect() { playSound(900, 25); playSound(1500, 45); playSound(2200, 60); }
void sfxJump()       { for(int f=600; f<1350; f+=110) playSound(f, 3); }
void sfxLaser()      { for(int f=2400; f>600; f-=160) playSound(f, 2); }
void sfxExplosion()  { for(int f=550; f>80; f-=35) playSound(f, 8); }
void sfxScoreDing()  { playSound(1900, 20); playSound(2500, 40); }
void sfxBounce()     { playSound(1100, 10); }
void sfxWaka()       { playSound(450, 15); playSound(600, 15); }
void sfxGameOver()   { playSound(700, 70); delay(20); playSound(500, 80); delay(20); playSound(250, 180); }

// Unified HUD System
void drawGameHUD(const char* name, uint16_t themeColor) {
  tft.fillRect(0, 0, SCREEN_W, HUD_H, C_NAVY);
  tft.drawFastHLine(0, HUD_H - 1, SCREEN_W, themeColor);
  tft.setTextColor(themeColor);
  tft.setTextSize(1);
  tft.setCursor(8, 9);
  tft.print(name);
  tft.setTextColor(C_CYAN);
  tft.setCursor(130, 9);
  tft.print("BY SUBHADIP");
  lastRenderedScore = -1;
}

void updateHUDScore() {
  if (score == lastRenderedScore) return;
  lastRenderedScore = score;
  tft.fillRect(230, 4, 86, 20, C_NAVY);
  tft.setTextColor(C_YELLOW);
  tft.setTextSize(1);
  tft.setCursor(232, 9);
  tft.printf("SC:%05ld", score);
}

// Forward Declarations
void drawMenu();
void restartCurrentGame();

void initFlappy();    void loopFlappy();
void initSpace();     void loopSpace();
void initShooter();   void loopShooter();
void initSnake();     void loopSnake();
void initBrick();     void loopBrick();
void initMario();     void loopMario();
void initDino();      void loopDino();
void initRacing();    void loopRacing();
void initPong();      void loopPong();
void initDefender();  void loopDefender();
void initRiver();     void loopRiver();
void initPacmanPro(); void loopPacmanPro();
void initAsteroids(); void loopAsteroids();
void initGalaga();    void loopGalaga();
void initDan();       void loopDan();
void initFrogger();   void loopFrogger(); // Linker safety ke liye
void initTank();      void loopTank();


bool checkUniversalControls() {
  if (isPressed(PIN_BTN_B)) {
    playSound(400, 50);
    delay(200);
    currentState = MENU;
    drawMenu();
    return true;
  }
  if (isPressed(PIN_BTN_P)) {
    playSound(1000, 35);
    delay(250);
    isPaused = !isPaused;
    if (isPaused) {
      tft.fillRect(70, 90, 180, 56, C_RED);
      tft.drawRect(70, 90, 180, 56, C_WHITE);
      tft.setTextColor(C_WHITE);
      tft.setTextSize(2);
      tft.setCursor(120, 102);
      tft.print("PAUSED");
      tft.setTextSize(1);
      tft.setCursor(88, 126);
      tft.print("PRESS [P] TO RESUME");
    } else {
      // Resume hone par game arena clean refresh hoga
      if (currentState == G_COPTER) {
        initDan();
      } else {
        tft.fillRect(70, 90, 180, 56, C_BG);
      }
    }
  }
  return false;
}

void restartCurrentGame() {
  tft.fillScreen(C_BG);
  score = 0;
  isPaused = false;
  switch (currentState) {
    case G_FLAPPY:    initFlappy();    break;
    case G_SPACE:     initSpace();     break;
    case G_SNAKE:     initSnake();     break;
    case G_BRICK:     initBrick();     break;
    case G_MARIO:     initMario();     break;
    case G_DINO:      initDino();      break;
    case G_RACING:    initRacing();    break;
    case G_PONG:      initPong();      break;
    case G_SHOOTER:   initShooter();   break;
    case G_RIVER:     initRiver();     break;
    case G_PACMAN:    initPacmanPro(); break;
    case G_ASTEROIDS: initAsteroids(); break;
    case G_GALAGA:    initGalaga();    break;
    case G_COPTER:    initDan();       break; // Sirf ek baar initDan()
    case G_SUBMARINE: initTank();      break;
    default:
      currentState = MENU;
      drawMenu();
      break;
  }
}

void triggerGameOverScreen(const char* reason) {
  // Center Box: Width 280 (X: 20 to 300)
  tft.fillRoundRect(20, 50, 280, 140, 8, C_RED);
  tft.drawRoundRect(20, 50, 280, 140, 8, C_WHITE);
  tft.drawRoundRect(22, 52, 276, 136, 6, C_YELLOW);

  // "GAME OVER" (9 chars * 12 = 108px -> Center X = 106)
  tft.setTextSize(2);
  tft.setTextColor(C_WHITE);
  tft.setCursor(106, 64);
  tft.print("GAME OVER");

  // Dynamic Reason Text Centering
  int rLen = strlen(reason);
  int rWidth = rLen * 6; // TextSize 1 is 6px per char
  int rX = max(26, (SCREEN_W - rWidth) / 2);
  tft.setTextSize(1);
  tft.setTextColor(C_YELLOW);
  tft.setCursor(rX, 92);
  tft.print(reason);

  // Score Line Centering
  char scoreBuf[30];
  sprintf(scoreBuf, "FINAL SCORE: %ld", score);
  int sWidth = strlen(scoreBuf) * 6;
  int sX = (SCREEN_W - sWidth) / 2;
  tft.setTextColor(C_WHITE);
  tft.setCursor(sX, 112);
  tft.print(scoreBuf);

  // Symmetrical Option Buttons (Left: X=35, Right: X=165, Width=120 each)
  tft.fillRoundRect(35, 140, 120, 28, 5, C_GREEN);
  tft.drawRoundRect(35, 140, 120, 28, 5, C_WHITE);
  tft.setTextColor(C_BLACK);
  tft.setTextSize(1);
  // "[A] REPLAY" (10 chars * 6 = 60px -> Box Center = 35 + (120-60)/2 = 65)
  tft.setCursor(65, 150);
  tft.print("[A] REPLAY");

  tft.fillRoundRect(165, 140, 120, 28, 5, C_DGREY);
  tft.drawRoundRect(165, 140, 120, 28, 5, C_WHITE);
  tft.setTextColor(C_WHITE);
  // "[B] BACK MENU" (13 chars * 6 = 78px -> Box Center = 165 + (120-78)/2 = 186)
  tft.setCursor(186, 150);
  tft.print("[B] BACK MENU");

  delay(400);

  // Input Wait Loop
  while (true) {
    if (isPressed(PIN_BTN_A)) {
      sfxScoreDing();
      delay(200);
      restartCurrentGame();
      return;
    }

    if (isPressed(PIN_BTN_B)) {
      sfxBounce();
      score = 0;
      isPaused = false;
      delay(200);
      currentState = MENU;
      drawMenu();
      return;
    }

    delay(20);
  }
}
void drawSplashScreen() {
  tft.fillScreen(C_BG);
  tft.drawRect(4, 4, 312, 232, C_CYAN);
  tft.drawRect(6, 6, 308, 228, C_NAVY);

  // "RETRO STATION" (13 chars * 18 = 234px -> Center X = 43)
  tft.setTextColor(C_YELLOW);
  tft.setTextSize(3);
  tft.setCursor(43, 38);
  tft.print("RETRO STATION");

  // Center banner box (X = 25 se 270w -> Center X 160)
  tft.fillRoundRect(25, 88, 270, 50, 8, C_NAVY);
  tft.drawRoundRect(25, 88, 270, 50, 8, C_ORANGE);

  // "MADE BY SUBHADIP" (16 chars * 12 = 192px -> Center X = 64)
  tft.setTextColor(C_WHITE);
  tft.setTextSize(2);
  tft.setCursor(64, 105);
  tft.print("MADE BY SUBHADIP");

  // ">> PRESS [A] PLAY <<" (20 chars * 12 = 240px -> Center X = 40)
  tft.setTextColor(C_GREEN);
  tft.setTextSize(2);
  tft.setCursor(40, 172);
  tft.print(">> PRESS [A] PLAY <<");

  playSound(440, 60); delay(30);
  playSound(554, 60); delay(30);
  playSound(659, 60); delay(30);
  playSound(880, 140);
}


void handleSplash() {
  if (isPressed(PIN_BTN_A)) {
    sfxMenuSelect();
    delay(250);
    currentState = MENU;
    drawMenu();
  }
}

void updateMenuList() {
  int pageStart = (selectedGame / 5) * 5;
  for (int i = 0; i < 5; i++) {
    int gIdx = pageStart + i;
    int y = 46 + (i * 34);

    if (gIdx < TOTAL_GAMES) {
      if (gIdx == selectedGame) {
        tft.fillRoundRect(20, y, 280, 28, 6, C_ORANGE);
        tft.setTextColor(C_BG);
      } else {
        tft.fillRoundRect(20, y, 280, 28, 6, C_DGREY);
        tft.setTextColor(C_WHITE);
      }

      tft.setTextSize(2);
      
      // Fixed Left Alignment: Sabhi game titles X=35 se ek seedh mein shuru honge
      tft.setCursor(35, y + 6);
      tft.print(gameTitles[gIdx]);
    } else {
      tft.fillRect(20, y, 280, 28, C_BG);
    }
  }
}

void drawMenu() {
  // 1. Screen background clear
  tft.fillScreen(C_BG);

  // 2. Header Background Box (Color Block)
  // X: 0, Y: 0, Width: 320, Height: 44
  tft.fillRect(0, 0, 320, 44, C_NAVY); 
  tft.drawFastHLine(0, 43, 320, C_CYAN); // Bottom divider line

  // Header Title ("15-IN-1 ARCADE" -> 14 chars * 12px = 168px -> Center X = (320 - 168)/2 = 76)
  tft.setTextSize(2);
  tft.setTextColor(C_WHITE, C_NAVY);
  tft.setCursor(76, 8);
  tft.print("15-IN-1 ARCADE");

  // Subtitle ("BY SUBHADIP" -> 11 chars * 6px = 66px -> Center X = (320 - 66)/2 = 127)
  tft.setTextSize(1);
  tft.setTextColor(C_YELLOW, C_NAVY);
  tft.setCursor(127, 28);
  tft.print("BY SUBHADIP");

  // 3. Footer Bar with Color
  tft.fillRect(0, 220, 320, 20, C_NAVY);
  tft.drawFastHLine(0, 219, 320, C_CYAN);
  tft.setTextSize(1);
  tft.setTextColor(C_WHITE, C_NAVY);
  tft.setCursor(20, 226);
  tft.print("Joy: Select | A: Play | P: Pause | B: Menu");

  // 4. Menu list render
  updateMenuList();
}



void handleMenu() {
  if (millis() - lastDebounceTime > 260) {
    if (getJoyUp()) {
      selectedGame = (selectedGame - 1 + TOTAL_GAMES) % TOTAL_GAMES;
      sfxMenuTick();
      updateMenuList();
      lastDebounceTime = millis();
    } else if (getJoyDown()) {
      selectedGame = (selectedGame + 1) % TOTAL_GAMES;
      sfxMenuTick();
      updateMenuList();
      lastDebounceTime = millis();
    }
  }

  if (isPressed(PIN_BTN_A)) {
    sfxMenuSelect();
    delay(200);
    tft.fillScreen(C_BG);
    score = 0;
    isPaused = false;
    
    switch (selectedGame) {
      case 0:  currentState = G_FLAPPY;    initFlappy();    break;
      case 1:  currentState = G_SPACE;     initSpace();     break;
      case 2:  currentState = G_SNAKE;     initSnake();     break;
      case 3:  currentState = G_BRICK;     initBrick();     break;
      case 4:  currentState = G_MARIO;     initMario();     break;
      case 5:  currentState = G_DINO;      initDino();      break;
      case 6:  currentState = G_RACING;    initRacing();    break;
      case 7:  currentState = G_PONG;      initPong();      break;
      case 8:  currentState = G_SHOOTER;   initShooter();   break;
      case 9:  currentState = G_RIVER;     initRiver();     break;
      case 10: currentState = G_PACMAN;    initPacmanPro(); break;
      case 11: currentState = G_ASTEROIDS; initAsteroids(); break;
      case 12: currentState = G_GALAGA;    initGalaga();    break;
      case 13: currentState = G_COPTER;    initDan();       break; // <--- YAHAN LIKH DIYA
      case 14: currentState = G_SUBMARINE; initTank();      break;
    }
  }
}

// -------------------------------------------------------------
// GAME 1: FLAPPY BIRD PRO (Stable Memory + Sky Theme)
// -------------------------------------------------------------
#define FB_SKY        0x653F  // Retro Sky Blue
#define FB_X          55
#define FB_W          18
#define FB_H          14
#define PIPE_WIDTH    32
#define PIPE_GAP      78

float fbY, fbV, oldFbY;
int pipe1X, pipe1GapY;
int pipe2X, pipe2GapY;
uint16_t pipe1Color, pipe2Color;

// 5 Multi-Color Pipe Varieties
const uint16_t pipeColorPalette[5] = { C_GREEN, C_YELLOW, C_CYAN, C_RED, C_MAGENTA };

// Clouds & Distant Birds
int fbCloud1X, fbCloud2X;
int fbSkyBirdX, fbSkyBirdY;
int fbSkyBirdWing = 0;
int fbFrameCounter = 0;

void drawFlappyBirdSprite(int x, int y, float velocity, bool erase) {
  if (erase) {
    tft.fillRect(x - 2, y - 4, 30, 24, FB_SKY);
    return;
  }

  uint16_t cBody  = C_YELLOW;
  uint16_t cBelly = C_WHITE;
  uint16_t cBeak  = C_ORANGE;

  // Solid Oval Body
  tft.fillRoundRect(x, y, FB_W, FB_H, 5, cBody);
  // White Feather Belly
  tft.fillRoundRect(x + 2, y + 6, 7, 6, 2, cBelly);

  // Eye
  tft.fillCircle(x + 12, y + 4, 3, C_WHITE);
  tft.drawPixel(x + 13, y + 4, C_BLACK);

  // Sharp Orange Beak
  tft.fillTriangle(x + 16, y + 5, x + 23, y + 8, x + 16, y + 11, cBeak);
  tft.drawFastHLine(x + 16, y + 8, 5, C_BROWN);

  // Wing Flap
  if (velocity < -0.5) {
    tft.fillTriangle(x + 3, y + 6, x + 9, y - 1, x + 11, y + 6, C_WHITE);
    tft.drawTriangle(x + 3, y + 6, x + 9, y - 1, x + 11, y + 6, C_ORANGE);
  } else {
    tft.fillTriangle(x + 3, y + 6, x + 9, y + 12, x + 11, y + 6, C_WHITE);
    tft.drawTriangle(x + 3, y + 6, x + 9, y + 12, x + 11, y + 6, C_ORANGE);
  }
}

void drawFlappyCloud(int x, int y, bool erase) {
  uint16_t c = erase ? FB_SKY : C_WHITE;
  tft.fillRoundRect(x, y + 4, 32, 10, 4, c);
  tft.fillRoundRect(x + 6, y, 20, 10, 4, c);
}

void drawDistantSkyBird(int x, int y, int wingState, bool erase) {
  if (erase) {
    tft.fillRect(x - 2, y - 4, 14, 10, FB_SKY);
    return;
  }
  if (wingState % 2 == 0) {
    tft.drawLine(x, y, x + 4, y + 2, C_BLACK);
    tft.drawLine(x + 4, y + 2, x + 8, y, C_BLACK);
  } else {
    tft.drawLine(x, y + 2, x + 4, y, C_BLACK);
    tft.drawLine(x + 4, y, x + 8, y + 2, C_BLACK);
  }
}

void drawPipePair(int px, int gapY, uint16_t mainColor, bool erase) {
  uint16_t cFill = erase ? FB_SKY : mainColor;
  uint16_t cLip  = erase ? FB_SKY : C_WHITE;

  if (erase) {
    tft.fillRect(px + PIPE_WIDTH, HUD_H, 5, gapY, FB_SKY);
    tft.fillRect(px + PIPE_WIDTH, HUD_H + gapY + PIPE_GAP, 5, SCREEN_H - (HUD_H + gapY + PIPE_GAP), FB_SKY);
    return;
  }

  // Top Pipe Body
  tft.fillRect(px, HUD_H, PIPE_WIDTH, gapY - 6, cFill);
  tft.drawFastVLine(px, HUD_H, gapY - 6, cLip);

  // Top Pipe Lip Collar
  tft.fillRect(px - 2, HUD_H + gapY - 6, PIPE_WIDTH + 4, 6, cFill);
  tft.drawRect(px - 2, HUD_H + gapY - 6, PIPE_WIDTH + 4, 6, C_BLACK);

  // Bottom Pipe Lip Collar
  int botY = HUD_H + gapY + PIPE_GAP;
  tft.fillRect(px - 2, botY, PIPE_WIDTH + 4, 6, cFill);
  tft.drawRect(px - 2, botY, PIPE_WIDTH + 4, 6, C_BLACK);

  // Bottom Pipe Body
  tft.fillRect(px, botY + 6, PIPE_WIDTH, SCREEN_H - (botY + 6), cFill);
  tft.drawFastVLine(px, botY + 6, SCREEN_H - (botY + 6), cLip);
}

void moveSinglePipe(int &px, int &gapY, uint16_t &pColor) {
  drawPipePair(px, gapY, pColor, true);
  px -= 4;

  if (px < -PIPE_WIDTH) {
    px = 320;
    gapY = random(25, 100);
    pColor = pipeColorPalette[random(0, 5)];
    score += 5;
    sfxScoreDing();
  }

  drawPipePair(px, gapY, pColor, false);
}

void initFlappy() {
  drawGameHUD("FLAPPY BIRD PRO", C_GREEN);
  fbY = 110; oldFbY = 110; fbV = 0;
  fbFrameCounter = 0;

  pipe1X = 320; 
  pipe1GapY = 55;
  pipe1Color = pipeColorPalette[random(0, 5)];

  pipe2X = 320 + 175; 
  pipe2GapY = 85;
  pipe2Color = pipeColorPalette[random(0, 5)];

  fbCloud1X = 140;
  fbCloud2X = 280;
  fbSkyBirdX = 200;
  fbSkyBirdY = HUD_H + 20;
  fbSkyBirdWing = 0;

  tft.fillRect(0, HUD_H, SCREEN_W, SCREEN_H - HUD_H, FB_SKY);

  drawFlappyCloud(fbCloud1X, HUD_H + 16, false);
  drawFlappyCloud(fbCloud2X, HUD_H + 34, false);
  drawDistantSkyBird(fbSkyBirdX, fbSkyBirdY, fbSkyBirdWing, false);
  drawFlappyBirdSprite(FB_X, (int)fbY, fbV, false);
}

void loopFlappy() {
  if (checkUniversalControls()) return;
  if (isPaused) return;

  fbFrameCounter++;

  // 1. Distant Birds Animation
  if (fbFrameCounter % 2 == 0) {
    drawDistantSkyBird(fbSkyBirdX, fbSkyBirdY, fbSkyBirdWing, true);
    fbSkyBirdX -= 2;
    if (fbSkyBirdX < -20) {
      fbSkyBirdX = 320;
      fbSkyBirdY = random(HUD_H + 10, HUD_H + 45);
    }
    if (fbFrameCounter % 6 == 0) fbSkyBirdWing++;
    drawDistantSkyBird(fbSkyBirdX, fbSkyBirdY, fbSkyBirdWing, false);
  }

  // 2. Smooth Cloud Drift
  if (fbFrameCounter % 3 == 0) {
    tft.fillRect(fbCloud1X + 32, HUD_H + 16, 3, 14, FB_SKY);
    fbCloud1X -= 1;
    if (fbCloud1X < -34) {
      tft.fillRect(0, HUD_H + 16, 35, 14, FB_SKY);
      fbCloud1X = 320;
    }
    drawFlappyCloud(fbCloud1X, HUD_H + 16, false);

    tft.fillRect(fbCloud2X + 32, HUD_H + 34, 3, 14, FB_SKY);
    fbCloud2X -= 1;
    if (fbCloud2X < -34) {
      tft.fillRect(0, HUD_H + 34, 35, 14, FB_SKY);
      fbCloud2X = 320;
    }
    drawFlappyCloud(fbCloud2X, HUD_H + 34, false);
  }

  // 3. Flap Jump Physics
  if (isPressed(PIN_BTN_A) || getJoyUp()) {
    fbV = -4.8;
    playSound(1150, 15);
  }

  drawFlappyBirdSprite(FB_X, (int)oldFbY, fbV, true);

  fbV += 0.42;
  fbY += fbV;
  oldFbY = fbY;

  drawFlappyBirdSprite(FB_X, (int)fbY, fbV, false);

  // 4. Pipes Movement
  moveSinglePipe(pipe1X, pipe1GapY, pipe1Color);
  moveSinglePipe(pipe2X, pipe2GapY, pipe2Color);

  // 5. Collision Detection
  bool collided = false;
  if (fbY <= HUD_H + 2 || fbY + FB_H >= SCREEN_H - 2) collided = true;

  if (FB_X + FB_W >= pipe1X && FB_X <= pipe1X + PIPE_WIDTH) {
    if (fbY < HUD_H + pipe1GapY || fbY + FB_H > HUD_H + pipe1GapY + PIPE_GAP) {
      collided = true;
    }
  }

  if (FB_X + FB_W >= pipe2X && FB_X <= pipe2X + PIPE_WIDTH) {
    if (fbY < HUD_H + pipe2GapY || fbY + FB_H > HUD_H + pipe2GapY + PIPE_GAP) {
      collided = true;
    }
  }

  if (collided) {
    sfxExplosion();
    triggerGameOverScreen("BIRD HIT OBSTACLE");
    return;
  }

  tft.drawFastHLine(0, HUD_H - 1, SCREEN_W, C_GREEN);

  updateHUDScore();
  delay(24);
}




// -------------------------------------------------------------
// GAME 2: SPACE SHOOTER PRO (Sci-Fi Starfighter & Starfield)
// -------------------------------------------------------------
int spShipX, oldSpShipX;
int spBulX1, spBulX2, spBulY;
bool spBulAct = false;

int spAlienX, spAlienY, oldSpAlienX, oldSpAlienY;
int spAlienDirX = 2;
int spThrustAnim = 0;

// Starfield Background (5 Twinkling Stars)
int starX[5] = { 45, 110, 180, 240, 295 };
int starY[5] = { 40, 95, 150, 75, 185 };

void drawStarfighter(int x, int y, int thrustFrame, bool erase) {
  if (erase) {
    tft.fillRect(x - 4, y - 2, 28, 28, C_BG);
    return;
  }

  uint16_t cHull   = C_WHITE;
  uint16_t cWing   = C_CYAN;
  uint16_t cCockpit = C_YELLOW;
  uint16_t cEngine = (thrustFrame % 2 == 0) ? C_ORANGE : C_RED;

  // Aerodynamic Delta Wings
  tft.fillTriangle(x + 10, y, x - 2, y + 18, x + 22, y + 18, cWing);
  // Main Central Fuselage Body
  tft.fillRect(x + 8, y + 2, 4, 16, cHull);
  // Cockpit Glass
  tft.fillRect(x + 9, y + 5, 2, 5, cCockpit);
  // Wingtip Laser Pods
  tft.fillRect(x - 3, y + 10, 2, 8, C_RED);
  tft.fillRect(x + 21, y + 10, 2, 8, C_RED);
  // Plasma Engine Thruster Flame
  tft.fillTriangle(x + 8, y + 18, x + 12, y + 18, x + 10, y + 24, cEngine);
}

void drawAlienCruiser(int x, int y, bool erase) {
  if (erase) {
    tft.fillRect(x - 2, y - 2, 26, 18, C_BG);
    return;
  }

  // Alien Armor Hull
  tft.fillRect(x + 4, y + 2, 14, 8, C_MAGENTA);
  tft.fillRect(x, y + 4, 22, 5, C_RED);
  // Alien Robotic Core / Eye
  tft.fillRect(x + 9, y + 4, 4, 3, C_YELLOW);
  // Bottom Claws / Mandibles
  tft.fillRect(x + 2, y + 9, 3, 4, C_GREEN);
  tft.fillRect(x + 17, y + 9, 3, 4, C_GREEN);
  // Top Antennae
  tft.drawPixel(x + 4, y + 1, C_WHITE);
  tft.drawPixel(x + 17, y + 1, C_WHITE);
}

void initSpace() {
  drawGameHUD("SPACE INTERCEPTOR", C_MAGENTA);
  spShipX = 148; oldSpShipX = 148;
  spBulAct = false;
  spThrustAnim = 0;

  spAlienX = random(40, 260);
  spAlienY = HUD_H + 15;
  oldSpAlienX = spAlienX;
  oldSpAlienY = spAlienY;
  spAlienDirX = (random(0, 2) == 0) ? 3 : -3;

  // Draw Initial Cosmic Starfield
  for (int i = 0; i < 5; i++) {
    tft.drawPixel(starX[i], starY[i], C_WHITE);
  }

  drawStarfighter(spShipX, 202, spThrustAnim, false);
  drawAlienCruiser(spAlienX, spAlienY, false);
}

void loopSpace() {
  if (checkUniversalControls()) return;
  if (isPaused) return;

  spThrustAnim++;

  // --- 1. Cosmic Scrolling Starfield ---
  if (spThrustAnim % 2 == 0) {
    for (int i = 0; i < 5; i++) {
      tft.drawPixel(starX[i], starY[i], C_BG); // Erase star
      starY[i] += 2;
      if (starY[i] >= SCREEN_H - 4) {
        starY[i] = HUD_H + 4;
        starX[i] = random(15, 305);
      }
      tft.drawPixel(starX[i], starY[i], (i % 2 == 0) ? C_WHITE : C_CYAN);
    }
  }

  // --- 2. Spaceship Movement & Banking ---
  int nextShipX = spShipX;
  if (getJoyLeft())  nextShipX -= 6;
  if (getJoyRight()) nextShipX += 6;

  nextShipX = constrain(nextShipX, 15, 285);

  drawStarfighter(oldSpShipX, 202, spThrustAnim, true);
  oldSpShipX = nextShipX;
  spShipX = nextShipX;
  drawStarfighter(spShipX, 202, spThrustAnim, false);

  // --- 3. Fire Dual Laser Cannon [BTN A] ---
  if (isPressed(PIN_BTN_A) && !spBulAct) {
    spBulAct = true;
    spBulX1 = spShipX - 2;
    spBulX2 = spShipX + 22;
    spBulY = 196;
    sfxLaser();
  }

  // Dual Laser Trajectory
  if (spBulAct) {
    tft.drawFastVLine(spBulX1, spBulY, 7, C_BG);
    tft.drawFastVLine(spBulX2, spBulY, 7, C_BG);
    spBulY -= 12;

    if (spBulY <= HUD_H + 2) {
      spBulAct = false;
    } else {
      tft.drawFastVLine(spBulX1, spBulY, 7, C_CYAN);
      tft.drawFastVLine(spBulX2, spBulY, 7, C_CYAN);
    }
  }

  // --- 4. Alien Invader Movement & Strafe ---
  drawAlienCruiser(oldSpAlienX, oldSpAlienY, true);
  oldSpAlienX = spAlienX;
  oldSpAlienY = spAlienY;

  spAlienX += spAlienDirX;
  if (spAlienX < 20 || spAlienX > 280) {
    spAlienDirX = -spAlienDirX; // Bounce from screen edges
    spAlienY += 8;              // Dive down closer
  }

  spAlienY += 1;
  if (spAlienY > 210) {
    spAlienY = HUD_H + 10;
    spAlienX = random(30, 270);
  }

  drawAlienCruiser(spAlienX, spAlienY, false);

  // --- 5. Laser Hit Detection ---
  if (spBulAct) {
    bool hitLeftLaser  = (spBulX1 >= spAlienX && spBulX1 <= spAlienX + 22 && spBulY <= spAlienY + 12);
    bool hitRightLaser = (spBulX2 >= spAlienX && spBulX2 <= spAlienX + 22 && spBulY <= spAlienY + 12);

    if (hitLeftLaser || hitRightLaser) {
      drawAlienCruiser(spAlienX, spAlienY, true);
      tft.drawFastVLine(spBulX1, spBulY, 7, C_BG);
      tft.drawFastVLine(spBulX2, spBulY, 7, C_BG);
      spBulAct = false;

      // Respawn alien at top
      spAlienY = HUD_H + 10;
      spAlienX = random(30, 270);
      spAlienDirX = (random(0, 2) == 0) ? 3 : -3;

      score += 25;
      sfxExplosion();
    }
  }

  // --- 6. Ship Crash Detection ---
  if (abs(spShipX + 10 - (spAlienX + 11)) < 16 && abs(202 - spAlienY) < 14) {
    sfxExplosion();
    triggerGameOverScreen("STARFIGHTER DESTROYED");
    return;
  }

  // Keep HUD Boundary Clean
  tft.drawFastHLine(0, HUD_H - 1, SCREEN_W, C_MAGENTA);

  updateHUDScore();
  delay(24);
}


// -------------------------------------------------------------
// GAME 3: RETRO SNAKE PRO (Realistic Snake + Auto Changing BG)
// -------------------------------------------------------------
#define SNAKE_GRID_SIZE 10
#define SNAKE_MAX_LEN   60

struct SnakeSegment {
  int x;
  int y;
};

SnakeSegment snakeBody[SNAKE_MAX_LEN];
int snakeLen;
int snakeDirX, snakeDirY;
int nextSnakeDirX, nextSnakeDirY;
int snakeFoodX, snakeFoodY;
int tongueFlicker = 0;

// Dynamic Background Shift Engine
#define NUM_ARENA_THEMES 5
const uint16_t arenaColors[NUM_ARENA_THEMES] = {
  0x0000, // Classic Pure Black
  0x0010, // Midnight Deep Blue
  0x18C3, // Dark Forest Green
  0x2008, // Cyber Deep Purple
  0x3082  // Deep Wine Maroon
};
int currentThemeIdx = 0;
uint16_t currentSnakeBG = 0x0000;
unsigned long lastBGChangeMillis = 0;

void drawSnakeApple(int x, int y, bool erase) {
  uint16_t cBg = currentSnakeBG;
  if (erase) {
    tft.fillRect(x, y, SNAKE_GRID_SIZE, SNAKE_GRID_SIZE, cBg);
    return;
  }
  // Red Juicy Apple
  tft.fillRoundRect(x + 1, y + 2, 8, 7, 3, C_RED);
  // Apple Stem & Green Leaf
  tft.drawPixel(x + 5, y + 1, C_BROWN);
  tft.drawPixel(x + 6, y, C_GREEN);
}

void drawSnakeHead(int x, int y, int dx, int dy, int anim, bool erase) {
  uint16_t cBg = currentSnakeBG;
  if (erase) {
    tft.fillRect(x - 2, y - 2, SNAKE_GRID_SIZE + 4, SNAKE_GRID_SIZE + 4, cBg);
    return;
  }

  // Head Base
  tft.fillRoundRect(x, y, SNAKE_GRID_SIZE, SNAKE_GRID_SIZE, 3, C_GREEN);

  // Directional Eyes & Flickering Red Tongue
  if (dx == 1) { // Facing Right
    tft.drawPixel(x + 6, y + 2, C_WHITE); tft.drawPixel(x + 7, y + 2, C_BLACK);
    tft.drawPixel(x + 6, y + 7, C_WHITE); tft.drawPixel(x + 7, y + 7, C_BLACK);
    if (anim % 2 == 0) tft.drawFastHLine(x + 10, y + 4, 3, C_RED);
  } else if (dx == -1) { // Facing Left
    tft.drawPixel(x + 3, y + 2, C_WHITE); tft.drawPixel(x + 2, y + 2, C_BLACK);
    tft.drawPixel(x + 3, y + 7, C_WHITE); tft.drawPixel(x + 2, y + 7, C_BLACK);
    if (anim % 2 == 0) tft.drawFastHLine(x - 3, y + 4, 3, C_RED);
  } else if (dy == -1) { // Facing Up
    tft.drawPixel(x + 2, y + 3, C_WHITE); tft.drawPixel(x + 2, y + 2, C_BLACK);
    tft.drawPixel(x + 7, y + 3, C_WHITE); tft.drawPixel(x + 7, y + 2, C_BLACK);
    if (anim % 2 == 0) tft.drawFastVLine(x + 4, y - 3, 3, C_RED);
  } else { // Facing Down
    tft.drawPixel(x + 2, y + 6, C_WHITE); tft.drawPixel(x + 2, y + 7, C_BLACK);
    tft.drawPixel(x + 7, y + 6, C_WHITE); tft.drawPixel(x + 7, y + 7, C_BLACK);
    if (anim % 2 == 0) tft.drawFastVLine(x + 4, y + 10, 3, C_RED);
  }
}

void drawSnakeBodySegment(int x, int y, bool isEven, bool erase) {
  uint16_t cBg = currentSnakeBG;
  if (erase) {
    tft.fillRect(x, y, SNAKE_GRID_SIZE, SNAKE_GRID_SIZE, cBg);
    return;
  }
  uint16_t cBody = isEven ? C_DGREEN : C_GREEN;
  tft.fillRoundRect(x + 1, y + 1, SNAKE_GRID_SIZE - 2, SNAKE_GRID_SIZE - 2, 2, cBody);
  tft.drawPixel(x + 4, y + 4, C_PEACH); // Pattern scale detail
}

void spawnSnakeFood() {
  bool onSnake = true;
  while (onSnake) {
    onSnake = false;
    snakeFoodX = random(2, (SCREEN_W / SNAKE_GRID_SIZE) - 2) * SNAKE_GRID_SIZE;
    snakeFoodY = (random(HUD_H / SNAKE_GRID_SIZE + 2, (SCREEN_H / SNAKE_GRID_SIZE) - 2)) * SNAKE_GRID_SIZE;
    
    for (int i = 0; i < snakeLen; i++) {
      if (snakeBody[i].x == snakeFoodX && snakeBody[i].y == snakeFoodY) {
        onSnake = true;
        break;
      }
    }
  }
  drawSnakeApple(snakeFoodX, snakeFoodY, false);
}

void repaintSnakeField() {
  // Pure arena background refresh
  tft.fillRect(0, HUD_H, SCREEN_W, SCREEN_H - HUD_H, currentSnakeBG);

  // Outer Neon Boundary
  tft.drawRect(5, HUD_H + 4, SCREEN_W - 10, SCREEN_H - HUD_H - 8, C_GREEN);

  // Redraw Apple
  drawSnakeApple(snakeFoodX, snakeFoodY, false);

  // Redraw Snake
  for (int i = 1; i < snakeLen; i++) {
    drawSnakeBodySegment(snakeBody[i].x, snakeBody[i].y, (i % 2 == 0), false);
  }
  drawSnakeHead(snakeBody[0].x, snakeBody[0].y, snakeDirX, snakeDirY, tongueFlicker, false);
}

void initSnake() {
  drawGameHUD("RETRO SNAKE PRO", C_GREEN);
  snakeLen = 4;
  snakeDirX = 1; snakeDirY = 0;
  nextSnakeDirX = 1; nextSnakeDirY = 0;
  tongueFlicker = 0;

  currentThemeIdx = 0;
  currentSnakeBG = arenaColors[currentThemeIdx];
  lastBGChangeMillis = millis();

  // Start in center
  for (int i = 0; i < snakeLen; i++) {
    snakeBody[i].x = 120 - (i * SNAKE_GRID_SIZE);
    snakeBody[i].y = 120;
  }

  repaintSnakeField();
  spawnSnakeFood();
}

void loopSnake() {
  if (checkUniversalControls()) return;
  if (isPaused) return;

  tongueFlicker++;

  // --- 1. Dynamic Auto Background Shift (Har 6 Second Baad) ---
  if (millis() - lastBGChangeMillis > 6000) {
    lastBGChangeMillis = millis();
    currentThemeIdx = (currentThemeIdx + 1) % NUM_ARENA_THEMES;
    currentSnakeBG = arenaColors[currentThemeIdx];
    repaintSnakeField(); // Clean repaint with no leftover pixels
  }

  // --- 2. Joystick Controls (Opposite Turn Lock) ---
  if (getJoyLeft() && snakeDirX == 0)       { nextSnakeDirX = -1; nextSnakeDirY = 0; }
  else if (getJoyRight() && snakeDirX == 0) { nextSnakeDirX = 1;  nextSnakeDirY = 0; }
  else if (getJoyUp() && snakeDirY == 0)    { nextSnakeDirX = 0;  nextSnakeDirY = -1; }
  else if (getJoyDown() && snakeDirY == 0)  { nextSnakeDirX = 0;  nextSnakeDirY = 1; }

  snakeDirX = nextSnakeDirX;
  snakeDirY = nextSnakeDirY;

  // New Head Coordinates
  int newHeadX = snakeBody[0].x + (snakeDirX * SNAKE_GRID_SIZE);
  int newHeadY = snakeBody[0].y + (snakeDirY * SNAKE_GRID_SIZE);

  // --- 3. Wall Collision ---
  if (newHeadX < 10 || newHeadX > SCREEN_W - 20 || newHeadY < HUD_H + 10 || newHeadY > SCREEN_H - 20) {
    sfxExplosion();
    triggerGameOverScreen("SNAKE CRASHED IN WALL");
    return;
  }

  // --- 4. Self Body Collision ---
  for (int i = 1; i < snakeLen; i++) {
    if (newHeadX == snakeBody[i].x && newHeadY == snakeBody[i].y) {
      sfxExplosion();
      triggerGameOverScreen("SNAKE BIT ITSELF");
      return;
    }
  }

  // --- 5. Moving Snake & Tail Clear ---
  bool ateFood = (newHeadX == snakeFoodX && newHeadY == snakeFoodY);

  if (!ateFood) {
    // Tail erase cleanly matching current dynamic background
    drawSnakeBodySegment(snakeBody[snakeLen - 1].x, snakeBody[snakeLen - 1].y, false, true);
  } else {
    if (snakeLen < SNAKE_MAX_LEN - 1) snakeLen++;
    score += 20;
    sfxScoreDing();
    spawnSnakeFood();
  }

  // Convert previous head into body segment
  drawSnakeBodySegment(snakeBody[0].x, snakeBody[0].y, true, false);

  // Shift body array
  for (int i = snakeLen - 1; i > 0; i--) {
    snakeBody[i] = snakeBody[i - 1];
  }

  // Set new head position
  snakeBody[0].x = newHeadX;
  snakeBody[0].y = newHeadY;

  // Clear previous head tongue artifacts and draw new head
  drawSnakeHead(snakeBody[0].x, snakeBody[0].y, snakeDirX, snakeDirY, tongueFlicker, false);

  updateHUDScore();
  delay(110); // Optimal retro snake speed
}

// -------------------------------------------------------------
// GAME 4: BRICK BREAKER
// -------------------------------------------------------------
int padX, oldPadX;
float bX, bY, bVx, bVy;
bool bricks[4][8];

void initBrick() {
  drawGameHUD("BRICK BREAKER", C_ORANGE);
  padX = 130; oldPadX = 130;
  bX = 160; bY = 160; bVx = 3.2; bVy = -3.4;
  uint16_t bCols[] = { C_RED, C_ORANGE, C_YELLOW, C_GREEN };
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 8; c++) {
      bricks[r][c] = true;
      tft.fillRect(12 + c * 37, HUD_H + 10 + r * 14, 33, 10, bCols[r]);
    }
  }
}

void loopBrick() {
  if (checkUniversalControls()) return;
  if (isPaused) return;

  oldPadX = padX;
  if (getJoyLeft() && padX > 10) padX -= 6;
  if (getJoyRight() && padX < 250) padX += 6;

  if (oldPadX != padX) tft.fillRect(oldPadX, 215, 60, 8, C_BG);
  tft.fillRect(padX, 215, 60, 8, C_CYAN);

  tft.fillCircle((int)bX, (int)bY, 4, C_BG);
  bX += bVx; bY += bVy;

  if (bX <= 4 || bX >= 316) { bVx = -bVx; sfxBounce(); }
  if (bY <= HUD_H + 4) { bVy = -bVy; sfxBounce(); }

  if (bY >= 210 && bY <= 218 && bX >= padX && bX <= padX + 60) {
    bVy = -abs(bVy);
    sfxBounce();
  }

  int col = (bX - 12) / 37;
  int row = (bY - (HUD_H + 10)) / 14;
  if (row >= 0 && row < 4 && col >= 0 && col < 8 && bricks[row][col]) {
    bricks[row][col] = false;
    tft.fillRect(12 + col * 37, HUD_H + 10 + row * 14, 33, 10, C_BG);
    bVy = -bVy;
    score += 10;
    sfxScoreDing();
  }

  if (bY > SCREEN_H) {
    triggerGameOverScreen("BALL DROPPED");
    return;
  }

  tft.fillCircle((int)bX, (int)bY, 4, C_WHITE);
  updateHUDScore();
  delay(24);
}


// -------------------------------------------------------------
// GAME 5: MARIO RUNNER PRO (Clean NES - Fixed Redefinition)
// -------------------------------------------------------------
#define M_SKY        0x5D1F  // NES Sky Blue
#define M_GROUND_Y   196

float marioY, marioV;
bool marioIsGrounded;
int marioRunAnim = 0;

int goombaX, goombaOldX;
int pipeObsX, pipeObsOldX;
int mCloudX;

void drawNESMario(int x, int y, int runFrame, bool isJumping, bool erase) {
  if (erase) {
    tft.fillRect(x - 2, y - 2, 22, 28, M_SKY);
    return;
  }

  // Cap & Visor
  tft.fillRect(x + 4, y, 10, 4, C_RED);
  tft.fillRect(x + 2, y + 2, 14, 3, C_RED);

  // Face & Features
  tft.fillRect(x + 4, y + 5, 8, 5, C_PEACH);
  tft.fillRect(x + 10, y + 5, 2, 3, C_BLACK);
  tft.fillRect(x + 10, y + 8, 4, 2, C_BLACK);

  // Dress & Overalls
  tft.fillRect(x + 4, y + 10, 8, 6, C_RED);
  tft.fillRect(x + 2, y + 14, 12, 6, C_BLUE);
  tft.fillRect(x + 4, y + 15, 2, 2, C_YELLOW);
  tft.fillRect(x + 10, y + 15, 2, 2, C_YELLOW);

  // Shoes & Animation
  if (isJumping) {
    tft.fillRect(x + 2, y + 20, 5, 4, C_BROWN);
    tft.fillRect(x + 9, y + 22, 6, 4, C_BROWN);
  } else if (runFrame % 2 == 0) {
    tft.fillRect(x + 2, y + 20, 5, 5, C_BROWN);
    tft.fillRect(x + 10, y + 18, 5, 5, C_BROWN);
  } else {
    tft.fillRect(x + 2, y + 18, 5, 5, C_BROWN);
    tft.fillRect(x + 9, y + 20, 5, 5, C_BROWN);
  }
}

void drawNESGoomba(int x, int y, bool erase) {
  if (erase) {
    tft.fillRect(x - 2, y - 2, 18, 18, M_SKY);
    return;
  }
  tft.fillRoundRect(x + 1, y, 14, 10, 4, C_BROWN);
  tft.fillRect(x + 3, y + 7, 10, 5, C_PEACH);
  tft.fillRect(x + 4, y + 6, 2, 3, C_BLACK);
  tft.fillRect(x + 10, y + 6, 2, 3, C_BLACK);
  tft.fillRect(x, y + 12, 5, 3, C_BLACK);
  tft.fillRect(x + 11, y + 12, 5, 3, C_BLACK);
}

void drawWarpPipe(int x, bool erase) {
  if (erase) {
    tft.fillRect(x - 2, M_GROUND_Y - 24, 20, 24, M_SKY);
    return;
  }
  tft.fillRect(x, M_GROUND_Y - 22, 16, 6, C_GREEN);
  tft.drawRect(x, M_GROUND_Y - 22, 16, 6, C_DGREEN);
  tft.fillRect(x + 2, M_GROUND_Y - 16, 12, 16, C_GREEN);
  tft.drawRect(x + 2, M_GROUND_Y - 16, 12, 16, C_DGREEN);
}

void drawMarioCloud(int x, int y, bool erase) {
  uint16_t c = erase ? M_SKY : C_WHITE;
  tft.fillRoundRect(x, y, 26, 8, 3, c);
  tft.fillCircle(x + 8, y - 2, 5, c);
  tft.fillCircle(x + 16, y - 3, 6, c);
}

void initMario() {
  drawGameHUD("MARIO RUNNER NES", C_RED);
  marioY = M_GROUND_Y - 25;
  marioV = 0;
  marioIsGrounded = true;
  marioRunAnim = 0;

  goombaX = 310; goombaOldX = 310;
  pipeObsX = 480; pipeObsOldX = 480;
  mCloudX = 220;

  tft.fillRect(0, HUD_H, SCREEN_W, M_GROUND_Y - HUD_H, M_SKY);
  tft.fillRect(0, M_GROUND_Y, SCREEN_W, SCREEN_H - M_GROUND_Y, C_BROWN);
  tft.drawFastHLine(0, M_GROUND_Y, SCREEN_W, C_BLACK);
  tft.drawFastHLine(0, M_GROUND_Y + 1, SCREEN_W, C_ORANGE);

  drawMarioCloud(mCloudX, HUD_H + 20, false);
  drawNESMario(40, (int)marioY, marioRunAnim, false, false);
}

void loopMario() {
  if (checkUniversalControls()) return;
  if (isPaused) return;

  marioRunAnim++;

  if (marioRunAnim % 3 == 0) {
    drawMarioCloud(mCloudX, HUD_H + 20, true);
    mCloudX -= 1;
    if (mCloudX < -35) mCloudX = 320;
    drawMarioCloud(mCloudX, HUD_H + 20, false);
  }

  if ((isPressed(PIN_BTN_A) || getJoyUp()) && marioIsGrounded) {
    marioV = -9.2;
    marioIsGrounded = false;
    sfxJump();
  }

  drawNESMario(40, (int)marioY, marioRunAnim, !marioIsGrounded, true);

  marioV += 0.72;
  marioY += marioV;

  if (marioY >= M_GROUND_Y - 25) {
    marioY = M_GROUND_Y - 25;
    marioV = 0;
    marioIsGrounded = true;
  }

  drawNESMario(40, (int)marioY, marioRunAnim, !marioIsGrounded, false);

  drawNESGoomba(goombaOldX, M_GROUND_Y - 15, true);
  goombaOldX = goombaX;
  goombaX -= 5;
  if (goombaX < -22) {
    goombaX = 320 + random(20, 80);
    score += 10;
    sfxScoreDing();
  }
  drawNESGoomba(goombaX, M_GROUND_Y - 15, false);

  drawWarpPipe(pipeObsOldX, true);
  pipeObsOldX = pipeObsX;
  pipeObsX -= 5;
  if (pipeObsX < -25) {
    pipeObsX = 420 + random(40, 100);
    if (abs(pipeObsX - goombaX) < 60) pipeObsX += 70;
    score += 15;
  }
  drawWarpPipe(pipeObsX, false);

  if (goombaX >= 32 && goombaX <= 54) {
    if (marioY + 25 >= M_GROUND_Y - 13) {
      if (marioV > 0 && marioY + 25 <= M_GROUND_Y - 3) {
        drawNESGoomba(goombaX, M_GROUND_Y - 15, true);
        goombaX = 320 + random(40, 90);
        marioV = -5.0;
        score += 50;
        sfxScoreDing();
      } else {
        triggerGameOverScreen("GOOMBA GOT MARIO");
        return;
      }
    }
  }

  if (pipeObsX >= 32 && pipeObsX <= 54 && marioY + 25 >= M_GROUND_Y - 20) {
    triggerGameOverScreen("HIT WARP PIPE");
    return;
  }

  tft.drawFastHLine(0, M_GROUND_Y, SCREEN_W, C_BLACK);
  tft.drawFastHLine(0, M_GROUND_Y + 1, SCREEN_W, C_ORANGE);

  updateHUDScore();
  delay(24);
}


// -------------------------------------------------------------
// GAME 6: CHROME DINO PRO (Sky Theme + Clouds + Animated Bird)
// -------------------------------------------------------------
#define D_SKY         0x867D  // Retro Light Sky Blue
#define D_SAND        0xD571  // Desert Ground
#define D_FLOOR_Y     200

float dinoY, dinoV;
bool dinoIsGrounded;
int dinoLegAnim = 0;

// Obstacles: Cactus & Flying Pterodactyl Bird
int cactusX, cactusType;
int birdX, birdY, birdWingFrame;

// Background Floating Clouds
int cloud1X, cloud1Y;
int cloud2X, cloud2Y;

void drawDinoRex(int x, int y, int legFrame, bool erase) {
  uint16_t cBody = erase ? D_SKY : C_DGREY;
  uint16_t cEye  = erase ? D_SKY : C_WHITE;

  if (erase) {
    tft.fillRect(x - 2, y - 2, 26, 32, D_SKY);
    return;
  }

  // Head & Snout
  tft.fillRect(x + 10, y, 14, 10, cBody);
  tft.fillRect(x + 8, y + 2, 4, 8, cBody);
  tft.fillRect(x + 13, y + 2, 2, 2, cEye); // Eye
  tft.fillRect(x + 18, y + 7, 6, 2, D_SKY); // Mouth slit

  // Body & Little Arm
  tft.fillRect(x + 4, y + 9, 12, 12, cBody);
  tft.fillRect(x, y + 12, 5, 5, cBody);     // Tail curve
  tft.fillRect(x + 14, y + 12, 4, 2, cBody); // Tiny Arm

  // 2-Frame Animated Running Legs
  if (!dinoIsGrounded) {
    // Jump pose: Dono paanv piche
    tft.fillRect(x + 5, y + 21, 3, 7, cBody);
    tft.fillRect(x + 11, y + 21, 3, 5, cBody);
  } else if (legFrame % 2 == 0) {
    tft.fillRect(x + 5, y + 21, 3, 7, cBody);  // Left leg down
    tft.fillRect(x + 12, y + 21, 4, 4, cBody); // Right leg bent
  } else {
    tft.fillRect(x + 5, y + 21, 4, 4, cBody);  // Left leg bent
    tft.fillRect(x + 11, y + 21, 3, 7, cBody); // Right leg down
  }
}

void drawPterodactyl(int x, int y, int wingState, bool erase) {
  if (erase) {
    tft.fillRect(x - 2, y - 6, 26, 20, D_SKY);
    return;
  }
  uint16_t cBird = C_BLACK;

  // Bird Beak & Head
  tft.fillRect(x, y + 2, 8, 3, cBird);
  tft.fillRect(x + 8, y, 8, 5, cBird);

  // Animated Flapping Wings
  if (wingState % 2 == 0) {
    // Wings Up
    tft.fillTriangle(x + 6, y, x + 12, y - 6, x + 14, y, cBird);
  } else {
    // Wings Down
    tft.fillTriangle(x + 6, y + 4, x + 12, y + 10, x + 14, y + 4, cBird);
  }
  // Tail
  tft.fillRect(x + 16, y + 2, 4, 2, cBird);
}

void drawCactusCluster(int x, int type, bool erase) {
  uint16_t cCactus = erase ? D_SKY : C_DGREEN;
  uint16_t cDetail = erase ? D_SKY : C_GREEN;

  if (erase) {
    tft.fillRect(x - 4, D_FLOOR_Y - 28, 24, 28, D_SKY);
    return;
  }

  // Main Trunk
  tft.fillRect(x + 4, D_FLOOR_Y - 26, 6, 26, cCactus);
  tft.fillRect(x + 6, D_FLOOR_Y - 24, 2, 22, cDetail);

  // Left Arm
  tft.fillRect(x, D_FLOOR_Y - 20, 4, 4, cCactus);
  tft.fillRect(x, D_FLOOR_Y - 20, 2, 8, cCactus);

  // Right Arm
  tft.fillRect(x + 10, D_FLOOR_Y - 16, 4, 4, cCactus);
  tft.fillRect(x + 12, D_FLOOR_Y - 16, 2, 8, cCactus);

  // Second mini-cactus if double type
  if (type == 1) {
    tft.fillRect(x + 15, D_FLOOR_Y - 18, 4, 18, cCactus);
  }
}

void drawCloud(int x, int y, bool erase) {
  uint16_t c = erase ? D_SKY : C_WHITE;
  tft.fillRoundRect(x, y, 26, 8, 3, c);
  tft.fillCircle(x + 8, y - 2, 5, c);
  tft.fillCircle(x + 16, y - 3, 6, c);
}

void initDino() {
  drawGameHUD("CHROME DINO PRO", C_GREEN);
  dinoY = D_FLOOR_Y - 28;
  dinoV = 0;
  dinoIsGrounded = true;
  dinoLegAnim = 0;

  cactusX = 320;
  cactusType = 0;

  birdX = 490;
  birdY = D_FLOOR_Y - 45;
  birdWingFrame = 0;

  cloud1X = 120; cloud1Y = HUD_H + 20;
  cloud2X = 260; cloud2Y = HUD_H + 40;

  // Sky Background Fill
  tft.fillRect(0, HUD_H, SCREEN_W, D_FLOOR_Y - HUD_H, D_SKY);

  // Desert Ground Floor
  tft.fillRect(0, D_FLOOR_Y, SCREEN_W, SCREEN_H - D_FLOOR_Y, D_SAND);
  tft.drawFastHLine(0, D_FLOOR_Y, SCREEN_W, C_BROWN);

  // Initial Clouds & Dino
  drawCloud(cloud1X, cloud1Y, false);
  drawCloud(cloud2X, cloud2Y, false);
  drawDinoRex(35, (int)dinoY, dinoLegAnim, false);
}

void loopDino() {
  if (checkUniversalControls()) return;
  if (isPaused) return;

  dinoLegAnim++;

  // --- 1. Clouds Drift Animation ---
  if (dinoLegAnim % 2 == 0) {
    drawCloud(cloud1X, cloud1Y, true);
    cloud1X -= 1;
    if (cloud1X < -35) { cloud1X = 320; cloud1Y = random(HUD_H + 15, HUD_H + 45); }
    drawCloud(cloud1X, cloud1Y, false);

    drawCloud(cloud2X, cloud2Y, true);
    cloud2X -= 1;
    if (cloud2X < -35) { cloud2X = 320; cloud2Y = random(HUD_H + 25, HUD_H + 55); }
    drawCloud(cloud2X, cloud2Y, false);
  }

  // --- 2. Dino Jump Physics ---
  if ((isPressed(PIN_BTN_A) || getJoyUp()) && dinoIsGrounded) {
    dinoV = -9.6;
    dinoIsGrounded = false;
    sfxJump();
  }

  drawDinoRex(35, (int)dinoY, dinoLegAnim, true);

  dinoV += 0.72; // Gravity
  dinoY += dinoV;

  if (dinoY >= D_FLOOR_Y - 28) {
    dinoY = D_FLOOR_Y - 28;
    dinoV = 0;
    dinoIsGrounded = true;
  }

  drawDinoRex(35, (int)dinoY, dinoLegAnim, false);

  // --- 3. Cactus Movement ---
  drawCactusCluster(cactusX, cactusType, true);
  cactusX -= 6;
  if (cactusX < -25) {
    cactusX = 320 + random(20, 80);
    cactusType = random(0, 2);
    score += 10;
    sfxScoreDing();
  }
  drawCactusCluster(cactusX, cactusType, false);

  // --- 4. Flying Pterodactyl Bird ---
  drawPterodactyl(birdX, birdY, birdWingFrame, true);
  birdX -= 7;
  if (dinoLegAnim % 4 == 0) birdWingFrame++;

  if (birdX < -30) {
    birdX = 460 + random(60, 160);
    // Bird can fly at head height or mid height
    birdY = (random(0, 2) == 0) ? (D_FLOOR_Y - 45) : (D_FLOOR_Y - 22);
    score += 15;
  }
  drawPterodactyl(birdX, birdY, birdWingFrame, false);

  // --- 5. Collision Checks ---
  // Cactus Hitbox
  if (cactusX >= 25 && cactusX <= 48 && dinoY + 28 >= D_FLOOR_Y - 22) {
    sfxExplosion();
    triggerGameOverScreen("TRIPPED OVER CACTUS");
    return;
  }

  // Bird Hitbox
  if (birdX >= 22 && birdX <= 50 && abs((int)dinoY + 12 - (birdY + 4)) < 16) {
    sfxExplosion();
    triggerGameOverScreen("HIT BY PTERODACTYL");
    return;
  }

  // Ground line refresh
  tft.drawFastHLine(0, D_FLOOR_Y, SCREEN_W, C_BROWN);

  updateHUDScore();
  delay(24);
}

// -------------------------------------------------------------
// GAME 7: RETRO RACING (Smooth Highway Entry & Realistic Curbs)
// -------------------------------------------------------------
const int raceLanes[3] = { 85, 150, 215 };
int playerLane = 1, oldPlayerLane = 1;
unsigned long lastSteerMillis = 0;

int roadScrollY = 0;

struct TrafficVehicle {
  int lane;
  int y;
  int speed;
  uint16_t color;
};

TrafficVehicle traffic[2];

void drawRacerCar(int x, int y, uint16_t bodyColor, bool erase) {
  uint16_t cRoad = C_ROADGREY;
  uint16_t cBody = erase ? cRoad : bodyColor;
  uint16_t cTire = erase ? cRoad : C_BLACK;
  uint16_t cGlass = erase ? cRoad : C_CYAN;
  uint16_t cLight = erase ? cRoad : C_YELLOW;

  // Erase only on the road area below HUD
  if (erase) {
    int clearY = max((int)HUD_H, y - 2);
    int clearH = (y + 40) - clearY;
    if (clearH > 0 && clearY < SCREEN_H) {
      tft.fillRect(x - 2, clearY, 24, clearH, cRoad);
    }
    return;
  }

  // Draw 4 Wheels/Tires
  if (y + 4 >= HUD_H && y + 4 < SCREEN_H) {
    tft.fillRect(x - 1, max((int)HUD_H, y + 4), 3, min(8, (y + 12) - max((int)HUD_H, y + 4)), cTire);
    tft.fillRect(x + 18, max((int)HUD_H, y + 4), 3, min(8, (y + 12) - max((int)HUD_H, y + 4)), cTire);
  }
  if (y + 24 >= HUD_H && y + 24 < SCREEN_H) {
    tft.fillRect(x - 1, max((int)HUD_H, y + 24), 3, min(8, (y + 32) - max((int)HUD_H, y + 24)), cTire);
    tft.fillRect(x + 18, max((int)HUD_H, y + 24), 3, min(8, (y + 32) - max((int)HUD_H, y + 24)), cTire);
  }

  // Car Body Chassis (Top clipping protected)
  if (y + 34 >= HUD_H) {
    int bY = max((int)HUD_H, y + 2);
    int bH = (y + 34) - bY;
    if (bH > 0 && bY < SCREEN_H) tft.fillRoundRect(x + 2, bY, 16, bH, 3, cBody);
  }

  // Tinted Windshield Glass
  if (y + 10 >= HUD_H && y + 17 <= SCREEN_H) {
    tft.fillRect(x + 4, y + 10, 12, 7, cGlass);
  }
  // Rear Window
  if (y + 22 >= HUD_H && y + 26 <= SCREEN_H) {
    tft.fillRect(x + 5, y + 22, 10, 4, cGlass);
  }

  // Dual Headlights
  if (y + 2 >= HUD_H && y + 4 <= SCREEN_H) {
    tft.fillRect(x + 3, y + 2, 3, 2, cLight);
    tft.fillRect(x + 14, y + 2, 3, 2, cLight);
  }

  // Aerodynamic Rear Wing/Spoiler
  if (y + 32 >= HUD_H && y + 35 <= SCREEN_H) {
    tft.fillRect(x + 1, y + 32, 18, 3, cBody);
  }
}

void drawRoadMarkers() {
  // Red & White Rumble Curbs (Left & Right Borders)
  for (int y = HUD_H; y < SCREEN_H; y += 16) {
    int curY = (y + roadScrollY) % (SCREEN_H - HUD_H) + HUD_H;
    uint16_t curbColor = ((curY / 16) % 2 == 0) ? C_RED : C_WHITE;
    tft.fillRect(50, curY, 6, 8, curbColor);
    tft.fillRect(264, curY, 6, 8, curbColor);
  }

  // White Dashed Highway Dividers
  for (int y = HUD_H; y < SCREEN_H; y += 24) {
    int lineY = (y + roadScrollY) % (SCREEN_H - HUD_H) + HUD_H;
    tft.fillRect(124, lineY, 3, 12, C_WHITE);
    tft.fillRect(192, lineY, 3, 12, C_WHITE);
  }
}

void initRacing() {
  drawGameHUD("RETRO RACER PRO", C_YELLOW);
  playerLane = 1; oldPlayerLane = 1;
  roadScrollY = 0;
  lastSteerMillis = 0;

  // Road Surface & Green Sidewalks
  tft.fillRect(56, HUD_H, 208, SCREEN_H - HUD_H, C_ROADGREY);
  tft.fillRect(0, HUD_H, 50, SCREEN_H - HUD_H, C_DGREEN);
  tft.fillRect(270, HUD_H, 50, SCREEN_H - HUD_H, C_DGREEN);

  // Cars spawn off-screen above HUD
  traffic[0] = { 0, HUD_H - 42, 5, C_ORANGE };
  traffic[1] = { 2, HUD_H - 110, 4, C_MAGENTA };

  drawRoadMarkers();
  drawRacerCar(raceLanes[playerLane], 186, C_RED, false);
}

void loopRacing() {
  if (checkUniversalControls()) return;
  if (isPaused) return;

  // 1. High Speed Road Scrolling
  roadScrollY += 6;
  if (roadScrollY >= 48) roadScrollY = 0;

  // Clear previous dividers and redraw moving dashes
  for (int y = HUD_H; y < SCREEN_H; y += 24) {
    tft.fillRect(124, y, 3, 24, C_ROADGREY);
    tft.fillRect(192, y, 3, 24, C_ROADGREY);
  }
  drawRoadMarkers();

  // 2. Player Steering (Debounced 150ms)
  if (millis() - lastSteerMillis > 150) {
    int nextLane = playerLane;
    if (getJoyLeft() && playerLane > 0)       { nextLane--; lastSteerMillis = millis(); sfxBounce(); }
    else if (getJoyRight() && playerLane < 2) { nextLane++; lastSteerMillis = millis(); sfxBounce(); }

    if (nextLane != playerLane) {
      drawRacerCar(raceLanes[playerLane], 186, C_RED, true);
      playerLane = nextLane;
    }
  }

  // Draw Player Car
  drawRacerCar(raceLanes[playerLane], 186, C_RED, false);

  // 3. Traffic Cars Movement (Dheere-dheere top se enter hongi)
  for (int i = 0; i < 2; i++) {
    drawRacerCar(raceLanes[traffic[i].lane], traffic[i].y, traffic[i].color, true);

    traffic[i].y += traffic[i].speed;

    // Screen paar hone ke baad screen ke ekdam upar se enter karegi
    if (traffic[i].y > SCREEN_H) {
      drawRacerCar(raceLanes[traffic[i].lane], traffic[i].y, traffic[i].color, true);
      traffic[i].y = HUD_H - 42; 
      traffic[i].lane = random(0, 3);
      if (i == 1 && traffic[1].lane == traffic[0].lane) {
        traffic[1].lane = (traffic[0].lane + 1) % 3;
      }
      traffic[i].speed = random(4, 7);
      score += 15;
      sfxScoreDing();
    }

    drawRacerCar(raceLanes[traffic[i].lane], traffic[i].y, traffic[i].color, false);

    // Crash Detection
    if (traffic[i].lane == playerLane) {
      if (traffic[i].y + 34 >= 186 && traffic[i].y <= 218) {
        sfxExplosion();
        triggerGameOverScreen("HIGHWAY SPEED CRASH");
        return;
      }
    }
  }

  // Keep HUD header boundary sharp
  tft.drawFastHLine(0, HUD_H - 1, SCREEN_W, C_YELLOW);

  updateHUDScore();
  delay(25);
}


// -------------------------------------------------------------
// GAME 8: ADVANCED PONG VS AI (No Paddle Glitch + Spin Physics)
// -------------------------------------------------------------
int playerPaddleY, oldPlayerPaddleY;
int aiPaddleY, oldAiPaddleY;
float pongBallX, pongBallY, oldPongBallX, oldPongBallY;
float pongBallVx, pongBallVy;

#define PADDLE_H 44
#define PADDLE_W 6

void drawCenterNet() {
  for (int y = HUD_H + 6; y < SCREEN_H - 6; y += 14) {
    tft.drawFastVLine(160, y, 7, C_DGREY);
  }
}

void initPong() {
  drawGameHUD("PONG VS AI", C_WHITE);
  playerPaddleY = 100; oldPlayerPaddleY = 100;
  aiPaddleY = 100; oldAiPaddleY = 100;
  
  pongBallX = 160; pongBallY = 120;
  oldPongBallX = 160; oldPongBallY = 120;
  pongBallVx = 4.2; pongBallVy = 2.0;

  drawCenterNet();

  // Draw initial paddles
  tft.fillRect(15, playerPaddleY, PADDLE_W, PADDLE_H, C_GREEN);
  tft.fillRect(299, aiPaddleY, PADDLE_W, PADDLE_H, C_RED);
}

void loopPong() {
  if (checkUniversalControls()) return;
  if (isPaused) return;

  // --- 1. Player Joystick Input ---
  int nextPy = playerPaddleY;
  if (getJoyUp())   nextPy -= 6;
  if (getJoyDown()) nextPy += 6;

  nextPy = constrain(nextPy, HUD_H + 4, SCREEN_H - PADDLE_H - 4);

  if (nextPy != playerPaddleY) {
    tft.fillRect(15, playerPaddleY, PADDLE_W, PADDLE_H, C_BG);
    playerPaddleY = nextPy;
    tft.fillRect(15, playerPaddleY, PADDLE_W, PADDLE_H, C_GREEN);
  }

  // --- 2. Advanced AI Tracking ---
  int nextAiy = aiPaddleY;
  int aiCenter = aiPaddleY + (PADDLE_H / 2);

  // Ball agar AI ki taraf aa rahi ho tabhi react karega (Smooth tracking)
  if (pongBallVx > 0 && pongBallX > 110) {
    if (aiCenter < pongBallY - 4) nextAiy += 4;
    else if (aiCenter > pongBallY + 4) nextAiy -= 4;
  }

  nextAiy = constrain(nextAiy, HUD_H + 4, SCREEN_H - PADDLE_H - 4);

  if (nextAiy != aiPaddleY) {
    tft.fillRect(299, aiPaddleY, PADDLE_W, PADDLE_H, C_BG);
    aiPaddleY = nextAiy;
    tft.fillRect(299, aiPaddleY, PADDLE_W, PADDLE_H, C_RED);
  }

  // --- 3. Clean Ball Movement (No trail, no paddle erase) ---
  tft.fillCircle((int)oldPongBallX, (int)oldPongBallY, 4, C_BG);
  
  // Center net refresh agar ball uske upar se nikli ho
  if (abs(oldPongBallX - 160) < 8) {
    drawCenterNet();
  }

  pongBallX += pongBallVx;
  pongBallY += pongBallVy;

  // Top and Bottom Wall Bounce
  if (pongBallY <= HUD_H + 6) {
    pongBallY = HUD_H + 6;
    pongBallVy = -pongBallVy;
    sfxBounce();
  } else if (pongBallY >= SCREEN_H - 6) {
    pongBallY = SCREEN_H - 6;
    pongBallVy = -pongBallVy;
    sfxBounce();
  }

  // --- 4. Dynamic Paddle Collision with Spin Physics ---
  // Player Paddle Hit (Left)
  if (pongBallX - 4 <= 15 + PADDLE_W && pongBallX + 4 >= 15) {
    if (pongBallY >= playerPaddleY - 2 && pongBallY <= playerPaddleY + PADDLE_H + 2) {
      pongBallX = 15 + PADDLE_W + 5; // Glitch clipping prevent
      pongBallVx = abs(pongBallVx);  // Rebound right

      // Spin Angle: Paddle ke center se jitna door lagega, utna sharp angle banega
      float hitOffset = pongBallY - (playerPaddleY + (PADDLE_H / 2.0));
      pongBallVy = hitOffset * 0.16;

      score += 10;
      playSound(1200, 15);

      // Redraw paddle to heal any visual clipping immediately
      tft.fillRect(15, playerPaddleY, PADDLE_W, PADDLE_H, C_GREEN);
    }
  }

  // AI Paddle Hit (Right)
  if (pongBallX + 4 >= 299 && pongBallX - 4 <= 299 + PADDLE_W) {
    if (pongBallY >= aiPaddleY - 2 && pongBallY <= aiPaddleY + PADDLE_H + 2) {
      pongBallX = 299 - 5;
      pongBallVx = -abs(pongBallVx); // Rebound left

      float hitOffset = pongBallY - (aiPaddleY + (PADDLE_H / 2.0));
      pongBallVy = hitOffset * 0.16;

      sfxBounce();

      // Redraw paddle to heal any visual clipping immediately
      tft.fillRect(299, aiPaddleY, PADDLE_W, PADDLE_H, C_RED);
    }
  }

  // --- 5. Missed Ball Check (Out / Game Over) ---
  if (pongBallX < 6) {
    sfxExplosion();
    triggerGameOverScreen("AI WON THE MATCH");
    return;
  }
  if (pongBallX > 314) {
    score += 50;
    playSound(1800, 60);
    // Player scored goal - reset ball to center
    pongBallX = 160;
    pongBallY = 120;
    pongBallVx = -4.0;
    pongBallVy = random(-2, 3);
  }

  // Draw ball at new position
  tft.fillCircle((int)pongBallX, (int)pongBallY, 4, C_WHITE);
  oldPongBallX = pongBallX;
  oldPongBallY = pongBallY;

  updateHUDScore();
  delay(22);
}

// -------------------------------------------------------------
// GAME 9: TARGET SHOOT
// -------------------------------------------------------------
int cx = 160, cy = 120, ocx = 160, ocy = 120, tx = 40, ty = 70, tspd = 5;

void initShooter() {
  drawGameHUD("TARGET SHOOT", C_GREEN);
  cx = 160; cy = 120; tx = 40; ty = 70;
}

void loopShooter() {
  if (checkUniversalControls()) return;
  if (isPaused) return;

  tft.drawCircle(ocx, ocy, 8, C_BG);
  ocx = cx; ocy = cy;
  if (getJoyLeft() && cx > 15) cx -= 6;
  if (getJoyRight() && cx < 305) cx += 6;
  if (getJoyUp() && cy > HUD_H + 15) cy -= 6;
  if (getJoyDown() && cy < 225) cy += 6;

  tft.fillCircle(tx, ty, 14, C_BG);
  tx += tspd;
  if (tx < 30 || tx > 290) tspd = -tspd;
  tft.fillCircle(tx, ty, 14, C_RED);
  tft.fillCircle(tx, ty, 6, C_WHITE);

  if (isPressed(PIN_BTN_A)) {
    sfxLaser();
    if (abs(cx - tx) < 14 && abs(cy - ty) < 14) {
      score += 20; sfxExplosion();
      tft.fillCircle(tx, ty, 14, C_BG);
      tx = random(40, 280); ty = random(HUD_H + 20, 160);
    }
  }

  tft.drawCircle(cx, cy, 8, C_GREEN);
  updateHUDScore();
  delay(25);
}


// -------------------------------------------------------------
// GAME 10: ADVANCED RIVER RAID JET (No Stray Lines + Fuel Tanks)
// -------------------------------------------------------------
int jetX = 148, oldJetX = 148;
int jetShotX, jetShotY;
bool isJetShotActive = false;

// Targets: Enemy Chopper & Fuel Depot
int enemyChopperX, enemyChopperY, oldChopperX, oldChopperY;
int fuelDepotX, fuelDepotY, oldFuelX, oldFuelY;
bool fuelDepotActive = true;
int jetFlameAnim = 0;

void drawFighterJetSprite(int x, int y, bool erase) {
  uint16_t cDelta   = erase ? C_BG : C_LGREY;
  uint16_t cBody    = erase ? C_BG : C_WHITE;
  uint16_t cCanopy  = erase ? C_BG : C_CYAN;
  uint16_t cMissile = erase ? C_BG : C_RED;
  uint16_t cFlame   = erase ? C_BG : ((jetFlameAnim % 2 == 0) ? C_ORANGE : C_YELLOW);

  // Clear exact full bounding box (including flame & missiles)
  if (erase) {
    tft.fillRect(x - 3, y - 2, 30, 36, C_BG);
    return;
  }

  // Delta Wing Body
  tft.fillTriangle(x + 12, y, x, y + 24, x + 24, y + 24, cDelta);
  // Central Fuselage
  tft.fillRect(x + 10, y + 2, 4, 24, cBody);
  // Tinted Cockpit Glass
  tft.fillTriangle(x + 12, y + 4, x + 10, y + 11, x + 14, y + 11, cCanopy);
  // Dual Wingtip Missiles
  tft.fillRect(x - 1, y + 16, 2, 8, cMissile);
  tft.fillRect(x + 23, y + 16, 2, 8, cMissile);
  // Twin Afterburner Exhaust Flame (Flickering)
  tft.fillRect(x + 9, y + 26, 6, 5, cFlame);
}

void drawEnemyChopper(int x, int y, bool erase) {
  if (erase) {
    tft.fillRect(x - 3, y - 4, 30, 20, C_BG);
    return;
  }
  // Rotor Blade
  tft.drawFastHLine(x - 2, y, 28, C_WHITE);
  // Chopper Body
  tft.fillRoundRect(x + 4, y + 2, 16, 10, 3, C_RED);
  // Cockpit
  tft.fillRect(x + 14, y + 4, 4, 5, C_CYAN);
  // Tail Boom & Skid
  tft.drawFastHLine(x - 2, y + 5, 6, C_WHITE);
  tft.drawFastVLine(x - 2, y + 3, 5, C_YELLOW);
  tft.drawFastHLine(x + 4, y + 13, 16, C_LGREY);
}

void drawFuelTank(int x, int y, bool erase) {
  if (erase) {
    tft.fillRect(x - 1, y - 1, 24, 16, C_BG);
    return;
  }
  tft.fillRoundRect(x, y, 22, 14, 3, C_YELLOW);
  tft.drawRoundRect(x, y, 22, 14, 3, C_ORANGE);
  tft.setTextColor(C_BLACK);
  tft.setTextSize(1);
  tft.setCursor(x + 3, y + 3);
  tft.print("F");
}

void initRiver() {
  drawGameHUD("RIVER RAID JET", C_CYAN);
  jetX = 148; oldJetX = 148;
  isJetShotActive = false;
  jetFlameAnim = 0;

  enemyChopperX = random(75, 220);
  enemyChopperY = HUD_H + 15;
  oldChopperX = enemyChopperX;
  oldChopperY = enemyChopperY;

  fuelDepotX = random(75, 220);
  fuelDepotY = HUD_H + 80;
  oldFuelX = fuelDepotX;
  oldFuelY = fuelDepotY;
  fuelDepotActive = true;

  // Solid Left & Right Canyon Banks
  tft.fillRect(0, HUD_H, 60, SCREEN_H - HUD_H, C_DGREEN);
  tft.fillRect(260, HUD_H, 60, SCREEN_H - HUD_H, C_DGREEN);
  tft.drawFastVLine(60, HUD_H, SCREEN_H - HUD_H, C_GREEN);
  tft.drawFastVLine(260, HUD_H, SCREEN_H - HUD_H, C_GREEN);
}

void loopRiver() {
  if (checkUniversalControls()) return;
  if (isPaused) return;

  jetFlameAnim++;

  // Joystick Steering
  int nextJetX = jetX;
  if (getJoyLeft())  nextJetX -= 6;
  if (getJoyRight()) nextJetX += 6;

  // Canyon Boundary Limit (Bank crash prevention)
  nextJetX = constrain(nextJetX, 64, 232);

  // Redraw Jet Only on Movement/Flame Update
  drawFighterJetSprite(oldJetX, 194, true);
  oldJetX = nextJetX;
  jetX = nextJetX;
  drawFighterJetSprite(jetX, 194, false);

  // Shoot Laser Bullet [BTN A]
  if (isPressed(PIN_BTN_A) && !isJetShotActive) {
    isJetShotActive = true;
    jetShotX = jetX + 11;
    jetShotY = 190;
    sfxLaser();
  }

  // Laser Bullet Flight (No bottom line trace)
  if (isJetShotActive) {
    tft.fillRect(jetShotX - 1, jetShotY, 3, 9, C_BG); // Clean 3px wipe
    jetShotY -= 12;
    if (jetShotY <= HUD_H + 2) {
      isJetShotActive = false;
    } else {
      tft.fillRect(jetShotX - 1, jetShotY, 3, 9, C_YELLOW);
    }
  }

  // --- Move Enemy Combat Chopper ---
  drawEnemyChopper(oldChopperX, oldChopperY, true);
  oldChopperX = enemyChopperX;
  oldChopperY = enemyChopperY;

  enemyChopperY += 4;
  if (enemyChopperY > 230) {
    enemyChopperY = HUD_H + 10;
    enemyChopperX = random(75, 220);
  }
  drawEnemyChopper(enemyChopperX, enemyChopperY, false);

  // --- Move Fuel Depot Tank ---
  if (fuelDepotActive) {
    drawFuelTank(oldFuelX, oldFuelY, true);
    oldFuelX = fuelDepotX;
    oldFuelY = fuelDepotY;

    fuelDepotY += 3;
    if (fuelDepotY > 230) {
      fuelDepotY = HUD_H + 10;
      fuelDepotX = random(75, 220);
    }
    drawFuelTank(fuelDepotX, fuelDepotY, false);
  } else if (fuelDepotY > 230) {
    fuelDepotActive = true;
    fuelDepotY = HUD_H + 10;
    fuelDepotX = random(75, 220);
  }

  // --- Laser Hit Detection ---
  // Hit Chopper
  if (isJetShotActive && abs(jetShotX - (enemyChopperX + 12)) < 15 && abs(jetShotY - (enemyChopperY + 6)) < 12) {
    drawEnemyChopper(enemyChopperX, enemyChopperY, true);
    tft.fillRect(jetShotX - 1, jetShotY, 3, 9, C_BG);
    isJetShotActive = false;
    enemyChopperY = HUD_H + 10;
    enemyChopperX = random(75, 220);
    score += 30;
    sfxExplosion();
  }

  // Hit Fuel Depot (Destroy for points)
  if (isJetShotActive && fuelDepotActive && abs(jetShotX - (fuelDepotX + 11)) < 14 && abs(jetShotY - (fuelDepotY + 7)) < 10) {
    drawFuelTank(fuelDepotX, fuelDepotY, true);
    tft.fillRect(jetShotX - 1, jetShotY, 3, 9, C_BG);
    isJetShotActive = false;
    fuelDepotActive = false;
    score += 80;
    sfxScoreDing();
  }

  // --- Mid-Air Crashes ---
  // Crash into Chopper
  if (abs(jetX + 12 - (enemyChopperX + 12)) < 16 && abs(194 - enemyChopperY) < 16) {
    sfxExplosion();
    triggerGameOverScreen("MIDAIR JET CRASH");
    return;
  }

  // Crash into Fuel Tank
  if (fuelDepotActive && abs(jetX + 12 - (fuelDepotX + 11)) < 16 && abs(194 - fuelDepotY) < 14) {
    sfxExplosion();
    triggerGameOverScreen("FUEL TANK EXPLODED");
    return;
  }

  updateHUDScore();
  delay(26);
}
// -------------------------------------------------------------
// GAME 11: ADVANCED PAC-MAN ARCADE (4-Ghost AI + Win Condition)
// -------------------------------------------------------------
int pacX, pacY, oldPacX, oldPacY;
int pacDirX, pacDirY, pacHeading;
int pacAnim = 0;

struct PacGhost {
  int x, y, oldX, oldY;
  int dirX, dirY;
  uint16_t color;
  bool isEaten;
};

PacGhost ghosts[4];
int cherryX, cherryY;
bool cherryActive = false;

#define PELLET_COLS 10
#define PELLET_ROWS 7
bool pellets[PELLET_ROWS][PELLET_COLS];
int remainingPellets = 0;
bool powerActive = false;
unsigned long powerTimer = 0;

// Wall collision box check
bool isPacWall(int x, int y) {
  if (x < 22 || x > 284 || y < HUD_H + 12 || y > 214) return true;

  // Solid Inner Maze Blocks
  if (x + 13 > 45 && x < 90 && y + 13 > HUD_H + 25 && y < HUD_H + 55) return true;
  if (x + 13 > 230 && x < 275 && y + 13 > HUD_H + 25 && y < HUD_H + 55) return true;
  if (x + 13 > 115 && x < 205 && y + 13 > HUD_H + 25 && y < HUD_H + 45) return true;
  if (x + 13 > 45 && x < 90 && y + 13 > 150 && y < 180) return true;
  if (x + 13 > 230 && x < 275 && y + 13 > 150 && y < 180) return true;
  if (x + 13 > 115 && x < 205 && y + 13 > 140 && y < 155) return true;

  return false;
}

void drawGhostSprite(int x, int y, uint16_t c, bool isFrightened, bool erase) {
  if (erase) {
    tft.fillRect(x - 1, y - 1, 16, 17, C_BG);
    return;
  }
  uint16_t fillC = isFrightened ? C_BLUE : c;
  tft.fillRect(x + 2, y, 10, 12, fillC);
  tft.fillRect(x, y + 4, 14, 8, fillC);
  tft.fillRect(x, y + 12, 3, 3, fillC);
  tft.fillRect(x + 5, y + 12, 4, 3, fillC);
  tft.fillRect(x + 11, y + 12, 3, 3, fillC);

  if (!isFrightened) {
    tft.fillRect(x + 3, y + 3, 3, 4, C_WHITE);
    tft.fillRect(x + 8, y + 3, 3, 4, C_WHITE);
    tft.drawPixel(x + 4, y + 4, C_BLUE);
    tft.drawPixel(x + 9, y + 4, C_BLUE);
  } else {
    tft.fillRect(x + 4, y + 4, 2, 2, C_WHITE);
    tft.fillRect(x + 8, y + 4, 2, 2, C_WHITE);
  }
}

void drawPacmanSprite(int x, int y, int heading, int mouthOpen, bool erase) {
  if (erase) {
    tft.fillRect(x - 2, y - 2, 18, 18, C_BG);
    return;
  }
  tft.fillCircle(x + 7, y + 7, 7, C_YELLOW);
  if (mouthOpen % 2 == 1) {
    if (heading == 0)      tft.fillTriangle(x + 7, y + 7, x + 15, y + 2, x + 15, y + 12, C_BG); // Right
    else if (heading == 1) tft.fillTriangle(x + 7, y + 7, x - 1, y + 2, x - 1, y + 12, C_BG);  // Left
    else if (heading == 2) tft.fillTriangle(x + 7, y + 7, x + 2, y - 1, x + 12, y - 1, C_BG);  // Up
    else if (heading == 3) tft.fillTriangle(x + 7, y + 7, x + 2, y + 15, x + 12, y + 15, C_BG); // Down
  }
}

void drawCherry(int x, int y, bool erase) {
  uint16_t c = erase ? C_BG : C_RED;
  tft.fillCircle(x + 4, y + 7, 3, c);
  tft.fillCircle(x + 10, y + 7, 3, c);
  if (!erase) {
    tft.drawLine(x + 4, y + 4, x + 7, y, C_BROWN);
    tft.drawLine(x + 10, y + 4, x + 7, y, C_BROWN);
    tft.drawPixel(x + 8, y + 1, C_GREEN);
  } else {
    tft.fillRect(x, y, 16, 12, C_BG);
  }
}

void initPacmanPro() {
  drawGameHUD("PAC-MAN ARCADE", C_YELLOW);
  pacX = 153; pacY = 188; oldPacX = 153; oldPacY = 188;
  pacDirX = 0; pacDirY = 0; pacHeading = 0;
  pacAnim = 0;
  powerActive = false;
  remainingPellets = 0;

  // 4 Ghosts Spawned in open paths with starting directions
  ghosts[0] = { 26,  HUD_H + 14, 26,  HUD_H + 14, 2, 0, C_RED, false };    // Blinky
  ghosts[1] = { 278, HUD_H + 14, 278, HUD_H + 14, -2, 0, C_ORANGE, false }; // Clyde
  ghosts[2] = { 26,  208,        26,  208,        0, -2, C_CYAN, false };   // Inky
  ghosts[3] = { 278, 208,        278, 208,        0, -2, C_PINK, false };   // Pinky

  cherryX = 152; cherryY = 118; cherryActive = true;

  // Draw Neon Maze Borders
  tft.drawRoundRect(15, HUD_H + 6, 290, 198, 6, C_BLUE);
  tft.drawRoundRect(17, HUD_H + 8, 286, 194, 6, C_BLUE);

  tft.fillRoundRect(45, HUD_H + 25, 45, 30, 4, C_BLUE);
  tft.fillRoundRect(230, HUD_H + 25, 45, 30, 4, C_BLUE);
  tft.fillRoundRect(115, HUD_H + 25, 90, 20, 3, C_BLUE);

  tft.fillRoundRect(45, 150, 45, 30, 4, C_BLUE);
  tft.fillRoundRect(230, 150, 45, 30, 4, C_BLUE);
  tft.fillRoundRect(115, 140, 90, 15, 3, C_BLUE);

  // Distribute Pellets
  for (int r = 0; r < PELLET_ROWS; r++) {
    for (int c = 0; c < PELLET_COLS; c++) {
      int px = 30 + c * 28;
      int py = HUD_H + 15 + r * 28;

      if (isPacWall(px - 5, py - 5)) {
        pellets[r][c] = false;
        continue;
      }

      pellets[r][c] = true;
      remainingPellets++;

      // 4 Corners: Power Pellets
      if ((r == 0 && c == 0) || (r == 0 && c == PELLET_COLS - 1) ||
          (r == PELLET_ROWS - 1 && c == 0) || (r == PELLET_ROWS - 1 && c == PELLET_COLS - 1)) {
        tft.fillCircle(px, py, 3, C_WHITE);
      } else {
        tft.fillCircle(px, py, 1, C_PEACH);
      }
    }
  }

  drawCherry(cherryX, cherryY, false);
}

void loopPacmanPro() {
  if (checkUniversalControls()) return;
  if (isPaused) return;

  int nextDirX = pacDirX;
  int nextDirY = pacDirY;
  int nextHeading = pacHeading;

  // Joystick Input
  if (getJoyLeft())       { nextDirX = -4; nextDirY = 0;  nextHeading = 1; }
  else if (getJoyRight()) { nextDirX = 4;  nextDirY = 0;  nextHeading = 0; }
  else if (getJoyUp())    { nextDirX = 0;  nextDirY = -4; nextHeading = 2; }
  else if (getJoyDown())  { nextDirX = 0;  nextDirY = 4;  nextHeading = 3; }

  if (!isPacWall(pacX + nextDirX, pacY + nextDirY)) {
    pacDirX = nextDirX;
    pacDirY = nextDirY;
    pacHeading = nextHeading;
  }

  // Clear Pac-Man
  drawPacmanSprite(oldPacX, oldPacY, pacHeading, pacAnim, true);
  oldPacX = pacX; oldPacY = pacY;

  if (!isPacWall(pacX + pacDirX, pacY + pacDirY)) {
    pacX += pacDirX;
    pacY += pacDirY;
  }

  pacAnim++;
  drawPacmanSprite(pacX, pacY, pacHeading, pacAnim, false);

  if (pacAnim % 4 == 0 && (pacDirX != 0 || pacDirY != 0)) sfxWaka();

  // Power Energizer Timer Check
  if (powerActive && millis() - powerTimer > 6000) {
    powerActive = false;
  }

  // Eating Pellets
  for (int r = 0; r < PELLET_ROWS; r++) {
    for (int c = 0; c < PELLET_COLS; c++) {
      if (pellets[r][c]) {
        int px = 30 + c * 28;
        int py = HUD_H + 15 + r * 28;

        if (abs(pacX + 7 - px) < 8 && abs(pacY + 7 - py) < 8) {
          pellets[r][c] = false;
          remainingPellets--;
          tft.fillRect(px - 4, py - 4, 9, 9, C_BG);

          if ((r == 0 && c == 0) || (r == 0 && c == PELLET_COLS - 1) ||
              (r == PELLET_ROWS - 1 && c == 0) || (r == PELLET_ROWS - 1 && c == PELLET_COLS - 1)) {
            powerActive = true;
            powerTimer = millis();
            score += 50;
            playSound(1200, 30);
          } else {
            score += 10;
          }
        }
      }
    }
  }

  // WIN CONDITION: Saare dots khatam hone par level jeet
  if (remainingPellets <= 0) {
    playSound(1200, 60); delay(40);
    playSound(1500, 60); delay(40);
    playSound(1900, 100);
    score += 500;
    tft.fillRect(50, 90, 220, 55, C_GREEN);
    tft.drawRect(50, 90, 220, 55, C_WHITE);
    tft.setTextColor(C_BLACK);
    tft.setTextSize(2);
    tft.setCursor(95, 100);
    tft.print("YOU WIN!");
    tft.setTextSize(1);
    tft.setCursor(80, 125);
    tft.print("PAC-MAN CLEARED MAZE!");
    delay(2000);
    initPacmanPro();
    return;
  }

  // Active AI for all 4 Ghosts
  for (int i = 0; i < 4; i++) {
    drawGhostSprite(ghosts[i].oldX, ghosts[i].oldY, ghosts[i].color, powerActive, true);
    ghosts[i].oldX = ghosts[i].x;
    ghosts[i].oldY = ghosts[i].y;

    int gStep = powerActive ? 1 : 2;

    // Har ghost ka apna target logic
    int targetX = pacX;
    int targetY = pacY;

    if (powerActive) {
      // Darr kar Pac-man se door bhagna
      targetX = (pacX > 160) ? 25 : 280;
      targetY = (pacY > 120) ? HUD_H + 15 : 210;
    } else {
      if (i == 1) { targetX = pacX + (pacDirX * 8); targetY = pacY + (pacDirY * 8); } // Clyde: Ambush
      else if (i == 2) { targetX = pacX - (pacDirX * 6); targetY = pacY - (pacDirY * 6); } // Inky: Flank
      else if (i == 3 && random(0, 5) == 0) { targetX = random(30, 270); targetY = random(HUD_H + 15, 210); } // Pinky: Roam
    }

    // Best direction calculate karna (Never stuck in walls)
    int bestDx = ghosts[i].dirX;
    int bestDy = ghosts[i].dirY;
    long bestDist = 999999;

    int testDirs[4][2] = { {gStep, 0}, {-gStep, 0}, {0, gStep}, {0, -gStep} };

    for (int d = 0; d < 4; d++) {
      int nx = ghosts[i].x + testDirs[d][0];
      int ny = ghosts[i].y + testDirs[d][1];

      // Reverse direction ko discard karna
      if (testDirs[d][0] == -ghosts[i].dirX && testDirs[d][1] == -ghosts[i].dirY) continue;

      if (!isPacWall(nx, ny)) {
        long dist = (long)(nx - targetX) * (nx - targetX) + (long)(ny - targetY) * (ny - targetY);
        if (dist < bestDist) {
          bestDist = dist;
          bestDx = testDirs[d][0];
          bestDy = testDirs[d][1];
        }
      }
    }

    // Agar rasta band ho toh kisi bhi khule raste par mudna
    if (isPacWall(ghosts[i].x + bestDx, ghosts[i].y + bestDy)) {
      for (int d = 0; d < 4; d++) {
        if (!isPacWall(ghosts[i].x + testDirs[d][0], ghosts[i].y + testDirs[d][1])) {
          bestDx = testDirs[d][0];
          bestDy = testDirs[d][1];
          break;
        }
      }
    }

    ghosts[i].dirX = bestDx;
    ghosts[i].dirY = bestDy;
    ghosts[i].x += ghosts[i].dirX;
    ghosts[i].y += ghosts[i].dirY;

    drawGhostSprite(ghosts[i].x, ghosts[i].y, ghosts[i].color, powerActive, false);

    // Pac-Man collision
    if (abs(pacX - ghosts[i].x) < 10 && abs(pacY - ghosts[i].y) < 10) {
      if (powerActive) {
        // Ghost ko khana
        score += 200;
        playSound(1800, 50);
        drawGhostSprite(ghosts[i].x, ghosts[i].y, ghosts[i].color, powerActive, true);
        ghosts[i].x = 150;
        ghosts[i].y = HUD_H + 14;
      } else {
        triggerGameOverScreen("GHOST CAUGHT PAC-MAN");
        return;
      }
    }
  }

  // Fruit Cherry Pick
  if (cherryActive && abs(pacX - cherryX) < 12 && abs(pacY - cherryY) < 12) {
    cherryActive = false;
    drawCherry(cherryX, cherryY, true);
    score += 100;
    playSound(1800, 40);
  }

  // Maze wall repair
  tft.drawRoundRect(15, HUD_H + 6, 290, 198, 6, C_BLUE);
  tft.drawRoundRect(17, HUD_H + 8, 286, 194, 6, C_BLUE);

  updateHUDScore();
  delay(33);
}



// -------------------------------------------------------------
// GAME 12: ASTEROID FIELD (Header Bleed Fixed)
// -------------------------------------------------------------
int astShipX = 160, astShipY = 200;
int roidsX[4], roidsY[4], roidsSpd[4];
int roidBulX, roidBulY; bool roidBulAct = false;

void initAsteroids() {
  drawGameHUD("ASTEROID FIELD", C_CYAN);
  astShipX = 160; astShipY = 200; roidBulAct = false;
  for(int i = 0; i < 4; i++) {
    roidsX[i] = random(20, 290);
    roidsY[i] = random(HUD_H + 15, HUD_H + 80);
    roidsSpd[i] = random(2, 5);
  }
}

void loopAsteroids() {
  if (checkUniversalControls()) return;
  if (isPaused) return;

  tft.fillRect(astShipX - 10, astShipY - 10, 20, 20, C_BG);

  if (getJoyLeft() && astShipX > 20) astShipX -= 5;
  if (getJoyRight() && astShipX < 300) astShipX += 5;
  if (getJoyUp() && astShipY > HUD_H + 20) astShipY -= 5;
  if (getJoyDown() && astShipY < 225) astShipY += 5;

  tft.fillTriangle(astShipX, astShipY - 9, astShipX - 7, astShipY + 7, astShipX + 7, astShipY + 7, C_WHITE);

  if (isPressed(PIN_BTN_A) && !roidBulAct) {
    roidBulAct = true;
    roidBulX = astShipX;
    roidBulY = astShipY - 10;
    sfxLaser();
  }

  if (roidBulAct) {
    tft.drawFastVLine(roidBulX, roidBulY, 6, C_BG);
    roidBulY -= 9;
    if (roidBulY <= HUD_H) {
      roidBulAct = false;
    } else {
      tft.drawFastVLine(roidBulX, roidBulY, 6, C_YELLOW);
    }
  }

  for(int i = 0; i < 4; i++) {
    if (roidsY[i] - 9 >= HUD_H) {
      tft.fillCircle(roidsX[i], roidsY[i], 9, C_BG);
    }

    roidsY[i] += roidsSpd[i];

    if (roidsY[i] > SCREEN_H + 10) {
      roidsY[i] = HUD_H + 10;
      roidsX[i] = random(20, 290);
      roidsSpd[i] = random(2, 5);
    }

    if (roidsY[i] - 9 >= HUD_H) {
      tft.fillCircle(roidsX[i], roidsY[i], 9, C_BROWN);
      tft.drawCircle(roidsX[i], roidsY[i], 9, C_LGREY);
    }

    if (roidBulAct && abs(roidBulX - roidsX[i]) < 12 && abs(roidBulY - roidsY[i]) < 12) {
      if (roidsY[i] - 9 >= HUD_H) {
        tft.fillCircle(roidsX[i], roidsY[i], 9, C_BG);
      }
      tft.drawFastVLine(roidBulX, roidBulY, 6, C_BG);
      roidBulAct = false;
      roidsY[i] = HUD_H + 10;
      roidsX[i] = random(20, 290);
      score += 20;
      sfxExplosion();
    }

    if (abs(astShipX - roidsX[i]) < 12 && abs(astShipY - roidsY[i]) < 12) {
      sfxExplosion();
      triggerGameOverScreen("ASTEROID IMPACT");
      return;
    }
  }

  tft.drawFastHLine(0, HUD_H - 1, SCREEN_W, C_CYAN);
  updateHUDScore();
  delay(25);
}

// -------------------------------------------------------------
// NEW GAME 13: GALAGA WAVES
// -------------------------------------------------------------
int galaShipX = 150, oldGalaShipX = 150;
int galaAlienX[3], galaAlienY[3], galaAlienDir[3];
int galaLaserX, galaLaserY;
bool galaLaserAct = false;

void initGalaga() {
  drawGameHUD("GALAGA WAVES", C_MAGENTA);
  galaShipX = 150; oldGalaShipX = 150;
  galaLaserAct = false;
  for (int i = 0; i < 3; i++) {
    galaAlienX[i] = 40 + i * 90;
    galaAlienY[i] = HUD_H + 15 + i * 14;
    galaAlienDir[i] = (i % 2 == 0) ? 3 : -3;
  }
}

void loopGalaga() {
  if (checkUniversalControls()) return;
  if (isPaused) return;

  tft.fillRect(oldGalaShipX - 2, 205, 24, 20, C_BG);
  oldGalaShipX = galaShipX;

  if (getJoyLeft() && galaShipX > 15) galaShipX -= 6;
  if (getJoyRight() && galaShipX < 285) galaShipX += 6;

  tft.fillTriangle(galaShipX + 10, 205, galaShipX, 222, galaShipX + 20, 222, C_WHITE);
  tft.fillRect(galaShipX + 8, 210, 4, 12, C_RED);
  tft.drawFastHLine(galaShipX - 2, 220, 24, C_CYAN);

  if (isPressed(PIN_BTN_A) && !galaLaserAct) {
    galaLaserAct = true;
    galaLaserX = galaShipX + 9;
    galaLaserY = 200;
    sfxLaser();
  }

  if (galaLaserAct) {
    tft.drawFastVLine(galaLaserX, galaLaserY, 7, C_BG);
    galaLaserY -= 12;
    if (galaLaserY <= HUD_H) galaLaserAct = false;
    else tft.drawFastVLine(galaLaserX, galaLaserY, 7, C_YELLOW);
  }

  for (int i = 0; i < 3; i++) {
    tft.fillRect(galaAlienX[i], galaAlienY[i], 18, 14, C_BG);
    galaAlienX[i] += galaAlienDir[i];
    if (galaAlienX[i] < 20 || galaAlienX[i] > 280) {
      galaAlienDir[i] = -galaAlienDir[i];
      galaAlienY[i] += 4;
    }
    if (galaAlienY[i] > 195) {
      galaAlienY[i] = HUD_H + 15;
    }

    tft.fillRect(galaAlienX[i] + 4, galaAlienY[i], 10, 12, C_GREEN);
    tft.fillRect(galaAlienX[i], galaAlienY[i] + 3, 18, 5, C_YELLOW);
    tft.fillRect(galaAlienX[i] + 2, galaAlienY[i] + 11, 4, 3, C_RED);
    tft.fillRect(galaAlienX[i] + 12, galaAlienY[i] + 11, 4, 3, C_RED);

    if (galaLaserAct && galaLaserX >= galaAlienX[i] && galaLaserX <= galaAlienX[i] + 18 && galaLaserY <= galaAlienY[i] + 14) {
      tft.fillRect(galaAlienX[i], galaAlienY[i], 18, 14, C_BG);
      tft.drawFastVLine(galaLaserX, galaLaserY, 7, C_BG);
      galaLaserAct = false;
      galaAlienY[i] = HUD_H + 15;
      galaAlienX[i] = random(30, 270);
      score += 25;
      sfxExplosion();
    }

    if (abs(galaShipX + 10 - (galaAlienX[i] + 9)) < 14 && abs(212 - galaAlienY[i]) < 12) {
      sfxExplosion();
      triggerGameOverScreen("GALAGA SHIP DESTROYED");
      return;
    }
  }

  updateHUDScore();
  delay(24);
}

// -------------------------------------------------------------
// GAME 14: DAN THE MAN ARCADE (Permanent Wall & Instant Death Fix)
// -------------------------------------------------------------
#define DAN_SKY       0x5D3F
#define DAN_GRASS     0x2404
#define DAN_DIRT      0x7A43
#define DAN_FLOOR_Y   198
#define DAN_MTN_COL   0x39E7

// Hero (Dan) State
int danX = 40, danY = DAN_FLOOR_Y;
int oldDanX = 40, oldDanY = DAN_FLOOR_Y;
float danVy = 0;
bool danGrounded = true;
bool danCrouch = false;
int danFacing = 1;
bool danPunching = false;
int danPunchTimer = 0;

// Hero Projectile
int danBulletX = -10, danBulletY = -10;
int danBulletDir = 1;
bool danBulletActive = false;

// Enemy Soldier
int enemyX = 270, enemyY = DAN_FLOOR_Y;
int oldEnemyX = 270, oldEnemyY = DAN_FLOOR_Y;
int enemyDir = -1;
bool enemyAlive = true;
int enemyHealth = 3;
int enemyShootTimer = 0;
int respawnCounter = 0;

// Enemy Projectile (Shuriken)
int enBulletX = -10, enBulletY = -10;
int enBulletDir = -1;
bool enBulletActive = false;

// Floating Brick Platform
int platX = 115, platY = 150, platW = 90, platH = 10;

// Ambient Birds & Clouds
int danCloud1X = 130, danCloud2X = 270;
int danBirdX = 210, danBirdY = HUD_H + 16;
int danBirdWing = 0;
int danTick = 0;

bool isPointInsideMountain(int px, int py) {
  if (py < 70 || py > 142) return false;
  
  if (px >= 15 && px <= 135) {
    int mtnTopY = (px <= 75) ? (142 - ((px - 15) * 67) / 60) : (75 + ((px - 75) * 67) / 60);
    if (py >= mtnTopY) return true;
  }

  if (px >= 185 && px <= 315) {
    int mtnTopY = (px <= 250) ? (142 - ((px - 185) * 72) / 65) : (70 + ((px - 250) * 72) / 65);
    if (py >= mtnTopY) return true;
  }

  return false;
}

// Platform Protection Check
bool isPointInsidePlatform(int px, int py) {
  return (px >= platX && px < platX + platW && py >= platY && py < platY + platH);
}

// Smart Area Restore (Preserves Mountain, Platform & Sky)
void restoreSceneryRect(int rx, int ry, int rw, int rh) {
  int xEnd = min(SCREEN_W, rx + rw);
  int yEnd = min(DAN_FLOOR_Y, ry + rh);
  int xStart = max(0, rx);
  int yStart = max(HUD_H, ry);

  for (int x = xStart; x < xEnd; x++) {
    for (int y = yStart; y < yEnd; y++) {
      uint16_t col;
      if (isPointInsidePlatform(x, y)) {
        col = (y == platY) ? C_GREEN : C_BROWN; // Wall brick protect
      } else if (isPointInsideMountain(x, y)) {
        col = DAN_MTN_COL;                      // Mountain protect
      } else {
        col = DAN_SKY;                          // Sky
      }
      tft.drawPixel(x, y, col);
    }
  }
}

void drawDanCloud(int x, int y, bool erase) {
  uint16_t c = erase ? DAN_SKY : C_WHITE;
  tft.fillRoundRect(x, y + 3, 26, 8, 3, c);
  tft.fillRoundRect(x + 5, y, 16, 8, 3, c);
}

void drawDanBird(int x, int y, int wingState, bool erase) {
  if (erase) {
    tft.fillRect(x - 2, y - 4, 14, 10, DAN_SKY);
    return;
  }
  if (wingState % 2 == 0) {
    tft.drawLine(x, y, x + 4, y + 2, C_BLACK);
    tft.drawLine(x + 4, y + 2, x + 8, y, C_BLACK);
  } else {
    tft.drawLine(x, y + 2, x + 4, y, C_BLACK);
    tft.drawLine(x + 4, y, x + 8, y + 2, C_BLACK);
  }
}

void drawDanSprite(int x, int y, int facing, bool punch, bool crouch, bool erase) {
  if (erase) {
    restoreSceneryRect(x - 8, y - 25, 30, 27);
    return;
  }

  int offsetY = crouch ? 6 : 0;

  // Hair & Headband
  tft.fillRect(x + 2, y - 24 + offsetY, 8, 5, C_BROWN);
  tft.fillRect(x, y - 19 + offsetY, 12, 3, C_YELLOW);
  tft.fillRoundRect(x + 1, y - 16 + offsetY, 10, 6, 1, C_PEACH);
  tft.drawPixel(facing == 1 ? x + 8 : x + 2, y - 14 + offsetY, C_BLACK);

  // Karate Suit
  tft.fillRect(x + 1, y - 10 + offsetY, 10, crouch ? 4 : 6, C_WHITE);
  tft.fillRect(x + 2, y - 4 + offsetY, 8, 2, C_GREEN);

  // Legs
  if (!crouch) {
    tft.fillRect(x + 1, y - 2, 4, 4, C_DGREY);
    tft.fillRect(x + 7, y - 2, 4, 4, C_DGREY);
  } else {
    tft.fillRect(x, y - 1, 12, 3, C_DGREY);
  }

  // Punch Attack Arm
  if (punch && !crouch) {
    int px = (facing == 1) ? x + 10 : x - 6;
    tft.fillRect(px, y - 10, 6, 4, C_PEACH);
    tft.drawFastHLine(px, y - 8, 6, C_WHITE);
  } else if (!crouch) {
    tft.fillRect(facing == 1 ? x + 8 : x + 1, y - 9, 3, 4, C_WHITE);
  }
}

void drawEnemyNinja(int x, int y, int facing, bool erase) {
  if (erase) {
    restoreSceneryRect(x - 6, y - 25, 24, 28);
    return;
  }

  tft.fillRoundRect(x, y - 20, 12, 8, 2, C_RED);
  tft.drawFastHLine(facing == 1 ? x + 5 : x + 1, y - 16, 5, C_BLACK);
  tft.fillRect(x + 1, y - 12, 10, 7, C_RED);
  tft.fillRect(x + 2, y - 5, 8, 2, C_BLACK);
  tft.fillRect(x + 1, y - 3, 3, 4, C_BLACK);
  tft.fillRect(x + 7, y - 3, 3, 4, C_BLACK);
}

void renderCleanDanArena() {
  tft.fillRect(0, HUD_H, SCREEN_W, SCREEN_H - HUD_H, DAN_SKY);

  // Distant Mountains
  tft.fillTriangle(15, 142, 75, 75, 135, 142, DAN_MTN_COL);
  tft.fillTriangle(185, 142, 250, 70, 315, 142, DAN_MTN_COL);

  // Floating Platform (Wall)
  tft.fillRoundRect(platX, platY, platW, platH, 3, C_BROWN);
  tft.drawFastHLine(platX, platY, platW, C_GREEN);

  // Floor Ground
  tft.fillRect(0, DAN_FLOOR_Y + 2, SCREEN_W, 6, DAN_GRASS);
  tft.fillRect(0, DAN_FLOOR_Y + 8, SCREEN_W, SCREEN_H - (DAN_FLOOR_Y + 8), DAN_DIRT);
  tft.drawFastHLine(0, DAN_FLOOR_Y + 2, SCREEN_W, C_GREEN);

  drawDanCloud(danCloud1X, HUD_H + 10, false);
  drawDanCloud(danCloud2X, HUD_H + 22, false);
  drawDanBird(danBirdX, danBirdY, danBirdWing, false);
}

void initDan() {
  drawGameHUD("DAN THE MAN", C_YELLOW);
  danX = 40; danY = DAN_FLOOR_Y;
  oldDanX = 40; oldDanY = DAN_FLOOR_Y;
  danVy = 0;
  danGrounded = true;
  danCrouch = false;
  danFacing = 1;
  danPunching = false;
  danPunchTimer = 0;

  danBulletActive = false;
  enBulletActive = false;
  enemyShootTimer = 0;

  enemyX = 270; enemyY = DAN_FLOOR_Y;
  oldEnemyX = 270; oldEnemyY = DAN_FLOOR_Y;
  enemyDir = -1;
  enemyAlive = true;
  enemyHealth = 3;
  respawnCounter = 0;

  danCloud1X = 130;
  danCloud2X = 270;
  danBirdX = 210;
  danBirdY = HUD_H + 16;
  danTick = 0;

  renderCleanDanArena();
  drawDanSprite(danX, danY, danFacing, danPunching, danCrouch, false);
  drawEnemyNinja(enemyX, enemyY, enemyDir, false);
}

void loopDan() {
  if (checkUniversalControls()) return;
  if (isPaused) return;

  danTick++;

  // --- 1. Ambient Sky Animation ---
  if (danTick % 3 == 0) {
    drawDanBird(danBirdX, danBirdY, danBirdWing, true);
    danBirdX -= 2;
    if (danBirdX < -15) {
      danBirdX = 320;
      danBirdY = random(HUD_H + 8, HUD_H + 34);
    }
    if (danTick % 6 == 0) danBirdWing++;
    drawDanBird(danBirdX, danBirdY, danBirdWing, false);

    tft.fillRect(danCloud1X + 26, HUD_H + 10, 3, 14, DAN_SKY);
    danCloud1X -= 1;
    if (danCloud1X < -30) {
      tft.fillRect(0, HUD_H + 10, 32, 14, DAN_SKY);
      danCloud1X = 320;
    }
    drawDanCloud(danCloud1X, HUD_H + 10, false);

    tft.fillRect(danCloud2X + 26, HUD_H + 22, 3, 14, DAN_SKY);
    danCloud2X -= 1;
    if (danCloud2X < -30) {
      tft.fillRect(0, HUD_H + 22, 32, 14, DAN_SKY);
      danCloud2X = 320;
    }
    drawDanCloud(danCloud2X, HUD_H + 22, false);
  }

  // --- 2. Crouch Control ---
  bool wasCrouching = danCrouch;
  if (getJoyDown() && danGrounded) {
    danCrouch = true;
  } else {
    danCrouch = false;
  }

  // --- 3. Walking & Jump Controls ---
  int nextDanX = danX;
  if (!danCrouch) {
    if (getJoyLeft()) {
      nextDanX -= 6;
      danFacing = -1;
    } else if (getJoyRight()) {
      nextDanX += 6;
      danFacing = 1;
    }
  }
  nextDanX = constrain(nextDanX, 10, SCREEN_W - 20);

  // Jump [Joy Up]
  if (getJoyUp() && danGrounded && !danCrouch) {
    danVy = -7.2;
    danGrounded = false;
    sfxJump();
  }

  // Punch Attack [BTN A]
  if (isPressed(PIN_BTN_A) && !danPunching && !danCrouch) {
    danPunching = true;
    danPunchTimer = 4;
    playSound(1100, 10);

    if (!danBulletActive) {
      danBulletActive = true;
      danBulletDir = danFacing;
      danBulletX = (danFacing == 1) ? danX + 16 : danX - 8;
      danBulletY = danY - 10;
    }
  }

  if (danPunchTimer > 0) {
    danPunchTimer--;
    if (danPunchTimer == 0) danPunching = false;
  }

  // --- 4. Platform Drop & Gravity Physics ---
  bool onPlatformX = (nextDanX + 6 >= platX && nextDanX <= platX + platW);

  if (danGrounded && danY == platY && !onPlatformX) {
    danGrounded = false;
  }

  if (!danGrounded) {
    danVy += 0.55;
    danY += danVy;

    if (danVy >= 0 && danY >= platY && oldDanY <= platY + 6 && onPlatformX) {
      danY = platY;
      danVy = 0;
      danGrounded = true;
    } else if (danY >= DAN_FLOOR_Y) {
      danY = DAN_FLOOR_Y;
      danVy = 0;
      danGrounded = true;
    }
  }

  // Redraw Dan Sprite cleanly
  if (nextDanX != danX || danY != oldDanY || danPunching || danCrouch != wasCrouching) {
    drawDanSprite(oldDanX, oldDanY, danFacing, false, wasCrouching, true);
    danX = nextDanX;
    drawDanSprite(danX, danY, danFacing, danPunching, danCrouch, false);
    oldDanX = danX;
    oldDanY = danY;
  }

  // Platform Edge Guard
  tft.drawFastHLine(platX, platY, platW, C_GREEN);

  // --- 5. Dan Bullet Projectile ---
  if (danBulletActive) {
    restoreSceneryRect(danBulletX, danBulletY, 8, 3);
    danBulletX += (danBulletDir * 12);

    if (danBulletX < 5 || danBulletX > SCREEN_W - 5) {
      danBulletActive = false;
    } else {
      tft.fillRect(danBulletX, danBulletY, 8, 3, C_YELLOW);
    }
  }

  // --- 6. Enemy Ninja AI & Instant Erasure ---
  if (enemyAlive) {
    drawEnemyNinja(oldEnemyX, oldEnemyY, enemyDir, true);

    if (enemyX > danX + 40) {
      enemyDir = -1;
      enemyX -= 3;
    } else if (enemyX < danX - 40) {
      enemyDir = 1;
      enemyX += 3;
    }

    oldEnemyX = enemyX;
    oldEnemyY = enemyY;
    drawEnemyNinja(enemyX, enemyY, enemyDir, false);

    // Enemy Shoot Logic
    enemyShootTimer++;
    if (enemyShootTimer == 22) {
      tft.setCursor(enemyX + 2, enemyY - 28);
      tft.setTextColor(C_YELLOW);
      tft.setTextSize(1);
      tft.print("!");
    }

    if (enemyShootTimer > 28 && !enBulletActive) {
      restoreSceneryRect(enemyX + 2, enemyY - 28, 8, 8);
      enemyShootTimer = 0;
      enBulletActive = true;
      enBulletDir = (danX < enemyX) ? -1 : 1;
      enBulletX = (enBulletDir == -1) ? enemyX - 6 : enemyX + 14;
      enBulletY = enemyY - 11;
      playSound(800, 10);
    }

    // Damage on Enemy
    bool hitByBullet = (danBulletActive && abs(danBulletX - enemyX) < 16 && abs(danBulletY - (enemyY - 10)) < 12);
    bool hitByFist   = (danPunching && abs(danX - enemyX) < 22 && abs(danY - enemyY) < 12);

    if (hitByBullet || hitByFist) {
      if (hitByBullet) {
        restoreSceneryRect(danBulletX, danBulletY, 8, 3);
        danBulletActive = false;
      }
      enemyHealth--;
      playSound(1400, 15);
      enemyX += (danFacing * 16);
      enemyX = constrain(enemyX, 20, SCREEN_W - 20);

      // Instant Kill: Turant erase aur frame flush
      if (enemyHealth <= 0) {
        drawEnemyNinja(enemyX, enemyY, enemyDir, true);
        drawEnemyNinja(oldEnemyX, oldEnemyY, enemyDir, true);
        restoreSceneryRect(enemyX - 8, enemyY - 26, 28, 30);
        restoreSceneryRect(oldEnemyX - 8, oldEnemyY - 26, 28, 30);
        enemyAlive = false;
        respawnCounter = 0;
        score += 75;
        sfxScoreDing();
      }
    }

    // Direct Collision Damage
    if (abs(danX - enemyX) < 12 && abs(danY - enemyY) < 12 && !danPunching) {
      sfxExplosion();
      triggerGameOverScreen("SQUASHED BY NINJA");
      return;
    }
  } else {
    // Respawn incoming enemy
    respawnCounter++;
    if (respawnCounter > 30) {
      enemyAlive = true;
      enemyHealth = 3;
      enemyX = (danX < 160) ? 290 : 30;
      oldEnemyX = enemyX;
      oldEnemyY = enemyY;
      enemyDir = (enemyX > danX) ? -1 : 1;
      drawEnemyNinja(enemyX, enemyY, enemyDir, false);
      respawnCounter = 0;
    }
  }

  // --- 7. Enemy Bullet Travel & Parry ---
  if (enBulletActive) {
    restoreSceneryRect(enBulletX, enBulletY, 5, 5);
    enBulletX += (enBulletDir * 9);

    if (enBulletX < 5 || enBulletX > SCREEN_W - 5) {
      enBulletActive = false;
    } else {
      tft.fillRect(enBulletX, enBulletY, 5, 5, C_RED);
      tft.drawPixel(enBulletX + 2, enBulletY + 2, C_WHITE);

      // Parry Clash
      if (danBulletActive && abs(danBulletX - enBulletX) < 12 && abs(danBulletY - enBulletY) < 8) {
        restoreSceneryRect(danBulletX, danBulletY, 8, 3);
        restoreSceneryRect(enBulletX, enBulletY, 5, 5);
        danBulletActive = false;
        enBulletActive = false;
        sfxBounce();
      }

      int playerHitH = danCrouch ? 6 : 14;
      if (abs(enBulletX - danX) < 10 && abs(enBulletY - (danY - 10)) < playerHitH) {
        restoreSceneryRect(enBulletX, enBulletY, 5, 5);
        enBulletActive = false;
        sfxExplosion();
        triggerGameOverScreen("HIT BY ENEMY FIRE");
        return;
      }
    }
  }

  updateHUDScore();
  delay(10);
}

// System Aliases
void initFrogger() { initDan(); }
void loopFrogger() { loopDan(); }
void initMountainClimb() { initDan(); }
void loopMountainClimb() { loopDan(); }




// -------------------------------------------------------------
// GAME 15: COMBAT TANK BATTLE (Solid Bricks + Auto Respawn 5s)
// -------------------------------------------------------------
int tankX, tankY, oldTankX, oldTankY;
int tankDir; // 0: Up, 1: Right, 2: Down, 3: Left
int eTankX, eTankY, oldETankX, oldETankY, eTankDir;

// Player Bullet
int tShellX, tShellY, tShellDx, tShellDy;
bool tShellActive = false;

// Enemy Bullet
int eShellX, eShellY, eShellDx, eShellDy;
bool eShellActive = false;
unsigned long lastEnemyShotTime = 0;

// Destructible & Auto-Regenerating Brick Walls
#define NUM_TANK_WALLS 6
struct TankBrickWall {
  int x, y, w, h;
  bool active;
  unsigned long breakTime; // Break hone ka time note karne ke liye
};
TankBrickWall tankWalls[NUM_TANK_WALLS];

// Tank & Wall Collision Check
bool checkTankObstacle(int tx, int ty) {
  // Screen Boundaries
  if (tx < 10 || tx > 294 || ty < HUD_H + 8 || ty > 214) return true;

  // Active Brick Walls Collision
  for (int i = 0; i < NUM_TANK_WALLS; i++) {
    if (tankWalls[i].active) {
      if (tx + 18 > tankWalls[i].x && tx < tankWalls[i].x + tankWalls[i].w &&
          ty + 18 > tankWalls[i].y && ty < tankWalls[i].y + tankWalls[i].h) {
        return true;
      }
    }
  }
  return false;
}

void drawTankVehicle(int x, int y, int dir, uint16_t bodyColor, bool erase) {
  uint16_t cBody = erase ? C_BG : bodyColor;
  uint16_t cTread = erase ? C_BG : C_DGREY;

  tft.fillRect(x - 2, y - 2, 22, 22, C_BG);

  if (erase) return;

  if (dir == 0 || dir == 2) { // Vertical Treads
    tft.fillRect(x, y, 4, 18, cTread);
    tft.fillRect(x + 14, y, 4, 18, cTread);
    tft.fillRect(x + 4, y + 3, 10, 12, cBody);
    if (dir == 0) tft.drawFastVLine(x + 9, y - 4, 8, cBody); // Barrel Up
    else tft.drawFastVLine(x + 9, y + 14, 8, cBody);          // Barrel Down
  } else { // Horizontal Treads
    tft.fillRect(x, y, 18, 4, cTread);
    tft.fillRect(x, y + 14, 18, 4, cTread);
    tft.fillRect(x + 3, y + 4, 12, 10, cBody);
    if (dir == 3) tft.drawFastHLine(x - 4, y + 9, 8, cBody); // Barrel Left
    else tft.drawFastHLine(x + 14, y + 9, 8, cBody);          // Barrel Right
  }
}

void drawSingleBrick(int idx) {
  if (tankWalls[idx].active) {
    tft.fillRect(tankWalls[idx].x, tankWalls[idx].y, tankWalls[idx].w, tankWalls[idx].h, C_BROWN);
    tft.drawRect(tankWalls[idx].x, tankWalls[idx].y, tankWalls[idx].w, tankWalls[idx].h, C_ORANGE);
  } else {
    tft.fillRect(tankWalls[idx].x - 1, tankWalls[idx].y - 1, tankWalls[idx].w + 2, tankWalls[idx].h + 2, C_BG);
  }
}

void initTank() {
  drawGameHUD("COMBAT TANK 90", C_YELLOW);
  tankX = 150; tankY = 194; oldTankX = 150; oldTankY = 194; tankDir = 0;
  eTankX = 150; eTankY = HUD_H + 12; oldETankX = 150; oldETankY = HUD_H + 12; eTankDir = 2;
  
  tShellActive = false;
  eShellActive = false;
  lastEnemyShotTime = millis();

  // 6 Brick Bunkers Setup
  tankWalls[0] = { 45,  HUD_H + 35, 35, 25, true, 0 };
  tankWalls[1] = { 240, HUD_H + 35, 35, 25, true, 0 };
  tankWalls[2] = { 120, HUD_H + 50, 80, 16, true, 0 };
  tankWalls[3] = { 45,  140,        35, 25, true, 0 };
  tankWalls[4] = { 240, 140,        35, 25, true, 0 };
  tankWalls[5] = { 120, 135,        80, 16, true, 0 };

  for (int i = 0; i < NUM_TANK_WALLS; i++) {
    drawSingleBrick(i);
  }

  drawTankVehicle(tankX, tankY, tankDir, C_GREEN, false);
  drawTankVehicle(eTankX, eTankY, eTankDir, C_RED, false);
}

void loopTank() {
  if (checkUniversalControls()) return;
  if (isPaused) return;

  // --- Auto Wall Re-generation Engine (5 Seconds Timer) ---
  for (int i = 0; i < NUM_TANK_WALLS; i++) {
    if (!tankWalls[i].active && (millis() - tankWalls[i].breakTime > 5000)) {
      // Check karein ki wall generate hone ki jagah koi tank toh nahi khada
      bool playerInside = (tankX + 18 > tankWalls[i].x && tankX < tankWalls[i].x + tankWalls[i].w &&
                           tankY + 18 > tankWalls[i].y && tankY < tankWalls[i].y + tankWalls[i].h);
      bool enemyInside  = (eTankX + 18 > tankWalls[i].x && eTankX < tankWalls[i].x + tankWalls[i].w &&
                           eTankY + 18 > tankWalls[i].y && eTankY < tankWalls[i].y + tankWalls[i].h);

      if (!playerInside && !enemyInside) {
        tankWalls[i].active = true;
        drawSingleBrick(i);
        playSound(1500, 20); // Wall rebuild tone
      }
    }
  }

  int nextTx = tankX;
  int nextTy = tankY;
  int nextDir = tankDir;

  // Joystick Controls
  if (getJoyLeft())       { nextTx -= 4; nextDir = 3; }
  else if (getJoyRight()) { nextTx += 4; nextDir = 1; }
  else if (getJoyUp())    { nextTy -= 4; nextDir = 0; }
  else if (getJoyDown())  { nextTy += 4; nextDir = 2; }

  // Solid Collision Check
  if (!checkTankObstacle(nextTx, nextTy)) {
    if (nextTx != tankX || nextTy != tankY || nextDir != tankDir) {
      drawTankVehicle(tankX, tankY, tankDir, C_BG, true);
      tankX = nextTx;
      tankY = nextTy;
      tankDir = nextDir;
      drawTankVehicle(tankX, tankY, tankDir, C_GREEN, false);
    }
  } else if (nextDir != tankDir) {
    drawTankVehicle(tankX, tankY, tankDir, C_BG, true);
    tankDir = nextDir;
    drawTankVehicle(tankX, tankY, tankDir, C_GREEN, false);
  }

  // Player Fire Shell [BTN A]
  if (isPressed(PIN_BTN_A) && !tShellActive) {
    tShellActive = true;
    sfxLaser();
    tShellX = tankX + 9;
    tShellY = tankY + 9;
    if (tankDir == 0)      { tShellDx = 0;  tShellDy = -8; }
    else if (tankDir == 1) { tShellDx = 8;  tShellDy = 0;  }
    else if (tankDir == 2) { tShellDx = 0;  tShellDy = 8;  }
    else if (tankDir == 3) { tShellDx = -8; tShellDy = 0;  }
  }

  // Player Bullet Physics & Destruction
  if (tShellActive) {
    tft.fillRect(tShellX - 1, tShellY - 1, 3, 3, C_BG);
    tShellX += tShellDx;
    tShellY += tShellDy;

    if (tShellX < 8 || tShellX > 312 || tShellY < HUD_H + 4 || tShellY > 234) {
      tShellActive = false;
    } else {
      // Brick hit check
      for (int i = 0; i < NUM_TANK_WALLS; i++) {
        if (tankWalls[i].active) {
          if (tShellX >= tankWalls[i].x && tShellX <= tankWalls[i].x + tankWalls[i].w &&
              tShellY >= tankWalls[i].y && tShellY <= tankWalls[i].y + tankWalls[i].h) {
            tankWalls[i].active = false;
            tankWalls[i].breakTime = millis(); // 5 second ka countdown shuru
            drawSingleBrick(i);
            tShellActive = false;
            sfxExplosion();
            break;
          }
        }
      }
      if (tShellActive) tft.fillRect(tShellX - 1, tShellY - 1, 3, 3, C_YELLOW);
    }
  }

  // Enemy Tank AI
  int eNextX = eTankX;
  int eNextY = eTankY;
  
  if (random(0, 14) == 0) {
    if (random(0, 2) == 0) {
      if (abs(tankX - eTankX) > abs(tankY - eTankY)) {
        eTankDir = (tankX > eTankX) ? 1 : 3;
      } else {
        eTankDir = (tankY > eTankY) ? 2 : 0;
      }
    } else {
      eTankDir = random(0, 4);
    }
  }

  int eStep = 2;
  if (eTankDir == 0) eNextY -= eStep;
  else if (eTankDir == 1) eNextX += eStep;
  else if (eTankDir == 2) eNextY += eStep;
  else if (eTankDir == 3) eNextX -= eStep;

  if (!checkTankObstacle(eNextX, eNextY)) {
    drawTankVehicle(eTankX, eTankY, eTankDir, C_BG, true);
    eTankX = eNextX;
    eTankY = eNextY;
    drawTankVehicle(eTankX, eTankY, eTankDir, C_RED, false);
  } else {
    eTankDir = random(0, 4);
  }

  // Enemy Firing AI
  if (!eShellActive && (millis() - lastEnemyShotTime > 1400)) {
    eShellActive = true;
    lastEnemyShotTime = millis();
    playSound(800, 15);
    eShellX = eTankX + 9;
    eShellY = eTankY + 9;
    if (eTankDir == 0)      { eShellDx = 0;  eShellDy = -6; }
    else if (eTankDir == 1) { eShellDx = 6;  eShellDy = 0;  }
    else if (eTankDir == 2) { eShellDx = 0;  eShellDy = 6;  }
    else if (eTankDir == 3) { eShellDx = -6; eShellDy = 0;  }
  }

  // Enemy Bullet Physics
  if (eShellActive) {
    tft.fillRect(eShellX - 1, eShellY - 1, 3, 3, C_BG);
    eShellX += eShellDx;
    eShellY += eShellDy;

    if (eShellX < 8 || eShellX > 312 || eShellY < HUD_H + 4 || eShellY > 234) {
      eShellActive = false;
    } else {
      for (int i = 0; i < NUM_TANK_WALLS; i++) {
        if (tankWalls[i].active) {
          if (eShellX >= tankWalls[i].x && eShellX <= tankWalls[i].x + tankWalls[i].w &&
              eShellY >= tankWalls[i].y && eShellY <= tankWalls[i].y + tankWalls[i].h) {
            tankWalls[i].active = false;
            tankWalls[i].breakTime = millis(); // 5 second countdown shuru
            drawSingleBrick(i);
            eShellActive = false;
            sfxExplosion();
            break;
          }
        }
      }

      // Check if Enemy hit Player Tank
      if (eShellActive && abs(eShellX - (tankX + 9)) < 11 && abs(eShellY - (tankY + 9)) < 11) {
        sfxExplosion();
        triggerGameOverScreen("TANK DESTROYED BY ENEMY");
        return;
      }

      if (eShellActive) tft.fillRect(eShellX - 1, eShellY - 1, 3, 3, C_RED);
    }
  }

  // Player hits Enemy Tank
  if (tShellActive && abs(tShellX - (eTankX + 9)) < 12 && abs(tShellY - (eTankY + 9)) < 12) {
    tft.fillRect(tShellX - 1, tShellY - 1, 3, 3, C_BG);
    drawTankVehicle(eTankX, eTankY, eTankDir, C_BG, true);
    tShellActive = false;
    score += 100;
    sfxExplosion();
    eTankX = random(30, 260);
    eTankY = HUD_H + 12;
    eTankDir = 2;
  }

  // Direct Body Crash
  if (abs(tankX - eTankX) < 14 && abs(tankY - eTankY) < 14) {
    sfxExplosion();
    triggerGameOverScreen("HEAD-ON TANK CRASH");
    return;
  }

  updateHUDScore();
  delay(26);
}


// -------------------------------------------------------------
// ARDUINO SETUP & MASTER LOOP
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
  tft.invertDisplay(false);

  drawSplashScreen();
}

void loop() {
  switch (currentState) {
    case SPLASH:       handleSplash();    break;
    case MENU:         handleMenu();      break;
    case G_FLAPPY:     loopFlappy();      break;
    case G_SPACE:      loopSpace();       break;
    case G_SNAKE:      loopSnake();       break;
    case G_BRICK:      loopBrick();       break;
    case G_MARIO:      loopMario();       break;
    case G_DINO:       loopDino();        break;
    case G_RACING:     loopRacing();      break;
    case G_PONG:       loopPong();        break;
    case G_SHOOTER:    loopShooter();     break;
    case G_RIVER:      loopRiver();       break;
    case G_PACMAN:     loopPacmanPro();   break;
    case G_ASTEROIDS:  loopAsteroids();   break;
    case G_GALAGA:     loopGalaga();      break;
    case G_COPTER:     loopFrogger();     break;
    case G_SUBMARINE:  loopTank();        break;
}}
