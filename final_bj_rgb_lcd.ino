#include <LiquidCrystal.h>
#include <IRremote.hpp>

// ===== LCD setup =====
LiquidCrystal lcd(9, 8, 4, 5, 6, 7);

// ===== IR setup =====
const int IR_RECEIVE_PIN = 11;

// Remote button codes
unsigned long HIT_BUTTON    = 0xF30CFF00;  // Power
unsigned long STAND_BUTTON  = 0xE718FF00;  // Func/Stop
unsigned long EXIT_BUTTON   = 0xA15EFF00;  // Fast Forward
unsigned long MINUS_BUTTON  = 0xF807FF00;  // Minus
unsigned long PLUS_BUTTON   = 0xEA15FF00;  // Plus

// ===== RGB LED pins =====
const int redPin   = 2;
const int greenPin = 3;
const int bluePin  = 10;

// ===== Game variables =====
int playerTotal = 0;
int dealerTotal = 0;
bool gameOver = false;

// ===== Stats =====
int playerWins = 0;
int dealerWins = 0;
int ties = 0;
int resultValue = 0;      // 1=win, 0=tie, -1=loss
long cumulativeScore = 0; // Serial Plotter

// ===== Betting System =====
int bet = 50;
int wallet = 1000;
int betStep = 10;
int maxBet = 500;

// ===== Setup =====
void setup() {
  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("BLACKJACK GAME!");
  lcd.setCursor(0, 1);
  lcd.print("Use Remote to Play");

  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);

  randomSeed(analogRead(A0));
  Serial.begin(9600);
  Serial.println("CumulativeScore");

  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
  Serial.println("IR Receiver Ready!");

  delay(2000);
  showBetScreen();
}

// ===== Loop =====
void loop() {
  // Idle breathing effect when waiting
  if (!IrReceiver.decode()) {
    if (gameOver) idleBreathing();
    return;
  }

  unsigned long value = IrReceiver.decodedIRData.decodedRawData;
  Serial.print("Received: ");
  Serial.println(value, HEX);

  if (!gameOver) {
    if (value == HIT_BUTTON) hit();
    else if (value == STAND_BUTTON) stand();
    else if (value == PLUS_BUTTON) increaseBet();
    else if (value == MINUS_BUTTON) decreaseBet();
  }

  if (value == EXIT_BUTTON) {
    resetGame();
  }

  IrReceiver.resume();
}

// ===== Game Logic =====
void hit() {
  int card = random(2, 12);
  playerTotal += card;
  updateLCD();

  if (playerTotal > 21) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Player: " + String(playerTotal));
    lcd.setCursor(0, 1);
    lcd.print("BUST! Dealer Wins!");
    wallet -= bet;
    dealerWins++;
    resultValue = -1;
    fadeToColor(255, 0, 0); // RED
    endRound();
  }
}

void stand() {
  while (dealerTotal < 17) {
    dealerTotal += random(2, 12);
    updateLCD();
    delay(400);
  }

  if (dealerTotal > 21) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Dealer: " + String(dealerTotal));
    lcd.setCursor(0,1);
    lcd.print("Dealer Busts!");
    wallet += bet;
    playerWins++;
    resultValue = 1;
    fadeToColor(0,255,0); // GREEN
  } else if (dealerTotal > playerTotal) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Dealer: " + String(dealerTotal));
    lcd.setCursor(0,1);
    lcd.print("Dealer Wins!");
    wallet -= bet;
    dealerWins++;
    resultValue = -1;
    fadeToColor(255,0,0); // RED
  } else if (dealerTotal < playerTotal) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Player: " + String(playerTotal));
    lcd.setCursor(0,1);
    lcd.print("Player Wins!");
    wallet += bet;
    playerWins++;
    resultValue = 1;
    fadeToColor(0,255,0); // GREEN
  } else {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("TIE Game!");
    ties++;
    resultValue = 0;
    fadeToColor(0,0,255); // BLUE
  }

  endRound();
}

// ===== Betting System =====
void increaseBet() {
  bet += betStep;
  if (bet > maxBet) bet = maxBet;
  showBetScreen();
}

void decreaseBet() {
  bet -= betStep;
  if (bet < 10) bet = 10;
  showBetScreen();
}

void showBetScreen() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Bet: " + String(bet));
  lcd.setCursor(0,1);
  lcd.print("Wallet: " + String(wallet));
}

// ===== End Round =====
void endRound() {
  gameOver = true;
  cumulativeScore += resultValue;

  for (int i = 0; i < 5; i++) Serial.println(cumulativeScore);

  Serial.print("Player Wins: "); Serial.print(playerWins);
  Serial.print(" | Dealer Wins: "); Serial.print(dealerWins);
  Serial.print(" | Ties: "); Serial.print(ties);
  Serial.print(" | Wallet: "); Serial.println(wallet);

  delay(1500);
  showBetScreen();
}

// ===== LCD Update =====
void updateLCD() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Dealer - " + String(dealerTotal));
  lcd.setCursor(0,1);
  lcd.print("Player - " + String(playerTotal));
}

// ===== Reset Game =====
void resetGame() {
  playerTotal = random(2,12);
  dealerTotal = random(2,12);
  gameOver = false;
  setRGB(0,0,0);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Dealer - " + String(dealerTotal));
  lcd.setCursor(0,1);
  lcd.print("Player - " + String(playerTotal));
}

// ===== RGB Helpers =====
void setRGB(int r, int g, int b) {
  analogWrite(redPin, r);
  analogWrite(greenPin, g);
  analogWrite(bluePin, b);
}

// Smooth fade to color
void fadeToColor(int r, int g, int b) {
  for (int i = 0; i <= 255; i += 5) {
    analogWrite(redPin, map(i, 0, 255, 0, r));
    analogWrite(greenPin, map(i, 0, 255, 0, g));
    analogWrite(bluePin, map(i, 0, 255, 0, b));
    delay(5);
  }
}

// Soft breathing effect while waiting
void idleBreathing() {
  static int brightness = 0;
  static int fadeAmount = 5;

  brightness += fadeAmount;
  if (brightness <= 0 || brightness >= 255) fadeAmount = -fadeAmount;

  analogWrite(redPin, brightness / 6);
  analogWrite(greenPin, brightness / 6);
  analogWrite(bluePin, brightness / 3);
  delay(15);
}
