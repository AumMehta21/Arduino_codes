/* Final Password Manager for Uno R3
   - Navigation: 2=UP, 8=DOWN, OK=Select
   - Accounts: Gmail, Microsoft, LinkedIn, Twitter (X), Reddit
   - No "See My Password"
   - Icons: 8x8 PROGMEM pixel icons
   - Auto word-wrap for long label text (no overlap / no off-screen)
   - EEPROM initialize once: A=1111, B=2222, C=5090
*/

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <IRremote.h>
#include <EEPROM.h>
#include <avr/pgmspace.h>

// ---------- Hardware pins ----------
#define TFT_DC 9
#define TFT_CS 10
#define TFT_RST 8
#define IR_PIN 6

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

// ---------- IR codes (from your remote) ----------
#define IR_0 0xE916FF00
#define IR_1 0xF30CFF00
#define IR_2 0xE718FF00
#define IR_3 0xA15EFF00
#define IR_4 0xF708FF00
#define IR_5 0xE31CFF00
#define IR_6 0xA55AFF00
#define IR_7 0xBD42FF00
#define IR_8 0xAD52FF00
#define IR_9 0xB54AFF00
#define IR_DEL 0xF807FF00
#define IR_OK  0xEA15FF00

// ---------- Labels & users ----------
const char* labels[] = {"Gmail", "Microsoft", "Linked in", "Twitter (X)", "Reddit"};
const uint8_t NUM_LABELS = sizeof(labels) / sizeof(labels[0]); // 5

const char* userNames[] = {"User A", "User B", "User C"};
const uint8_t MAX_USERS = 3;

// ---------- UI enums & globals ----------
enum Screen {
  ACTION_SELECT,
  USER_SELECT,
  MASTER_ENTER,
  NEWUSER_SELECT,
  NEWUSER_CREATE,
  ACCOUNT_SELECT,
  PASSWORD_ENTRY,
  CHANGE_SELECT,
  CHANGE_ENTER
};

Screen screen = ACTION_SELECT;

int userSelectedIdx = -1;     // chosen user for login/change
int selectedLabelIndex = 0;   // 0..NUM_LABELS-1
String entryBuffer = "";
const int MAX_LEN = 20;

// DEL long-press tracking
unsigned long delFirstMillis = 0;
bool delActive = false;
const unsigned long LONG_PRESS_MS = 900;

// Display constants
#define VIEW_W 240
#define VIEW_H 240
const int PASS_BOX_X = 20;
const int PASS_BOX_Y = 70;
const int PASS_BOX_W = 200;
const int PASS_BOX_H = 40;

// Colors
const uint16_t BG = ILI9341_BLACK;
const uint16_t BORDER = ILI9341_WHITE;
const uint16_t HEAD = ILI9341_GREEN;
const uint16_t TXT = ILI9341_CYAN;
const uint16_t SUB = ILI9341_YELLOW;
const uint16_t ACCENT = ILI9341_BLUE;
const uint16_t ALERT = ILI9341_RED;

// EEPROM layout
const uint8_t MAX_PW_LEN = 20;
const uint16_t USER_BLOCK = (uint16_t)MAX_PW_LEN;

// One-time init flag
#define INIT_FLAG_ADDR 200
#define INIT_FLAG_VALUE 123

// ---------- ICON DATA (8x8 monochrome) in PROGMEM ----------
const uint8_t icon_gmail[8] PROGMEM = {
  0b00111100,0b01000010,0b10011001,0b10100101,0b10000001,0b01000010,0b00111100,0b00000000
};
const uint8_t icon_microsoft[8] PROGMEM = {
  0b11000011,0b11000011,0b00000000,0b00100100,0b00100100,0b00000000,0b11011011,0b11011011
};
const uint8_t icon_linkedin[8] PROGMEM = {
  0b10000001,0b10000001,0b10111101,0b10100101,0b10100101,0b10100101,0b10000001,0b00000000
};
const uint8_t icon_twitter[8] PROGMEM = {
  0b00000000,0b00111000,0b01111100,0b11011110,0b00111100,0b00011000,0b00110000,0b00000000
};
const uint8_t icon_reddit[8] PROGMEM = {
  0b00111100,0b01000010,0b10100101,0b10000001,0b10100101,0b01000010,0b00111100,0b00000000
};

const uint8_t* const icons[] PROGMEM = {
  icon_gmail, icon_microsoft, icon_linkedin, icon_twitter, icon_reddit
};

// ---------- Forward declarations ----------
void drawHeader(const char* title);
void drawActionSelect();
void drawUserSelect();
void drawNewUserSelect();
void drawMasterPrompt(const char* title, const char* maskLabel);
void drawAccountSelect();
void drawPasswordEntryForLabel(const char* label);
void drawChangeSelect();
void showTempMessage(const char* msg, uint16_t color=ACCENT, uint16_t ms=600);
void drawIcon8x8(int x, int y, const uint8_t* iconPtr, uint8_t scale=2);
void wrapAndPrint(int x, int y, const char* text, uint8_t textSize, int maxWidth);

// ---------- EEPROM helper functions ----------
String readUserMaster(uint8_t userIdx) {
  uint16_t addr = (uint16_t)userIdx * USER_BLOCK;
  String s = "";
  for (uint8_t i = 0; i < MAX_PW_LEN; ++i) {
    char c = (char)EEPROM.read(addr + i);
    if (c == 0) break;
    s += c;
  }
  return s;
}
void writeUserMaster(uint8_t userIdx, const String &s) {
  uint16_t addr = (uint16_t)userIdx * USER_BLOCK;
  uint8_t i = 0;
  for (; i < MAX_PW_LEN - 1 && i < s.length(); ++i) {
    EEPROM.write(addr + i, s[i]);
  }
  EEPROM.write(addr + i, 0);
  for (uint8_t j = i + 1; j < MAX_PW_LEN; ++j) EEPROM.write(addr + j, 0);
}
void clearUserMaster(uint8_t userIdx) {
  uint16_t addr = (uint16_t)userIdx * USER_BLOCK;
  for (uint8_t i = 0; i < MAX_PW_LEN; ++i) EEPROM.write(addr + i, 0);
}

// ---------- Utility ----------
bool userSlotAvailable(uint8_t idx) {
  if (idx >= MAX_USERS) return false;
  return (readUserMaster(idx).length() == 0);
}

void initializeUserPasswordsOnce() {
  uint8_t flag = EEPROM.read(INIT_FLAG_ADDR);
  if (flag == INIT_FLAG_VALUE) return;
  writeUserMaster(0, "1111");
  writeUserMaster(1, "2222");
  writeUserMaster(2, "5090");
  EEPROM.write(INIT_FLAG_ADDR, INIT_FLAG_VALUE);
}

// ---------- Drawing helpers ----------
void drawHeader(const char* title) {
  tft.fillRect(0, 0, VIEW_W, VIEW_H, BG);
  tft.drawRect(0, 0, VIEW_W, VIEW_H, BORDER);
  tft.setTextColor(HEAD);
  tft.setTextSize(3);
  tft.setCursor(10, 8);
  tft.print(title);
}

void drawIcon8x8(int x, int y, const uint8_t* iconPtr, uint8_t scale) {
  for (uint8_t r = 0; r < 8; ++r) {
    uint8_t row = pgm_read_byte_near(iconPtr + r);
    for (uint8_t c = 0; c < 8; ++c) {
      bool bit = row & (1 << (7 - c));
      if (bit) {
        int px = x + c * scale;
        int py = y + r * scale;
        tft.fillRect(px, py, scale, scale, TXT);
      }
    }
  }
}

// Simple word-wrap printer: splits on spaces and builds lines that fit maxWidth
void wrapAndPrint(int x, int y, const char* text, uint8_t textSize, int maxWidth) {
  // approximate char width: 6 * textSize
  int charW = 6 * textSize;
  String word = "";
  String line = "";
  int curX = x;
  int curY = y;

  for (size_t i = 0; ; ++i) {
    char c = text[i];
    bool end = (c == '\0');
    if (!end && c != ' ') {
      word += c;
    } else {
      // word boundary or end
      int lineLen = line.length();
      int wordLen = word.length();
      int newLen = lineLen == 0 ? wordLen : (lineLen + 1 + wordLen);
      int pixelLen = newLen * charW;
      if (pixelLen > maxWidth) {
        // print current line and wrap
        if (line.length() > 0) {
          tft.setTextSize(textSize);
          tft.setTextColor(TXT);
          tft.setCursor(curX, curY);
          tft.print(line);
          curY += 8 * textSize + 2; // next line (8 px font height * textSize + spacing)
          line = word; // start new line with word
        } else {
          // single word longer than line: break the word itself
          // print maximum chars that fit, then continue
          int charsFit = maxWidth / charW;
          int pos = 0;
          while (pos < wordLen) {
            String part = word.substring(pos, pos + charsFit);
            tft.setTextSize(textSize);
            tft.setTextColor(TXT);
            tft.setCursor(curX, curY);
            tft.print(part);
            pos += charsFit;
            curY += 8 * textSize + 2;
          }
          line = "";
        }
      } else {
        // append to line
        if (lineLen == 0) line = word;
        else line += " " + word;
      }
      word = "";
      if (end) break;
    }
  }
  // print remainder line
  if (line.length() > 0) {
    tft.setTextSize(textSize);
    tft.setTextColor(TXT);
    tft.setCursor(x, y + 0); // note: previous curY might have advanced; compute actual vertical position
    // We need to compute final Y: recompute by printing properly. Simpler: track curY as above.
    // To fix, we'll re-run a small routine: print nothing here; instead use the curY used earlier.
    // But we tracked curY in loop — so print at latest curY.
    // So adjust: find last used curY: it's stored in local curY variable.
    tft.setCursor(x, curY);
    tft.print(line);
  }
}

// Draw action menu
void drawActionSelect() {
  drawHeader("Select Action");
  tft.setTextSize(2); tft.setTextColor(TXT);
  tft.setCursor(20, 60); tft.print("1. Login");
  tft.setCursor(20, 100); tft.print("2. Add User");
  tft.setCursor(20, 140); tft.print("3. Change Password");
  tft.setTextSize(1); tft.setTextColor(SUB);
  tft.setCursor(10, VIEW_H - 18);
  tft.print("Remote: num / - del / + OK");
}

void drawUserSelect() {
  drawHeader("Select User");
  tft.setTextSize(2);
  for (uint8_t i = 0; i < MAX_USERS; ++i) {
    String label = String(i + 1) + ". " + String(userNames[i]);
    String master = readUserMaster(i);
    tft.setTextColor(master.length() == 0 ? SUB : TXT);
    tft.setCursor(20, 60 + i * 36);
    tft.print(master.length() == 0 ? label + " (unused)" : label);
  }
  tft.setTextSize(1); tft.setTextColor(SUB);
  tft.setCursor(10, VIEW_H - 18);
  tft.print("Choose user to login");
}

void drawNewUserSelect() {
  drawHeader("Choose Username");
  tft.setTextSize(2);
  for (uint8_t i = 0; i < MAX_USERS; ++i) {
    String label = String(i + 1) + ". " + String(userNames[i]);
    bool free = userSlotAvailable(i);
    tft.setTextColor(free ? TXT : SUB);
    tft.setCursor(20, 60 + i * 36);
    tft.print(free ? label : label + " (used)");
  }
  tft.setTextSize(1); tft.setTextColor(SUB);
  tft.setCursor(10, VIEW_H - 18);
  tft.print("Pick an unused username");
}

void drawMasterPrompt(const char* title, const char* maskLabel) {
  drawHeader(title);
  tft.drawRect(PASS_BOX_X, PASS_BOX_Y, PASS_BOX_W, PASS_BOX_H, BORDER);
  if (entryBuffer.length() == 0) {
    tft.setTextSize(2); tft.setTextColor(SUB);
    tft.setCursor(PASS_BOX_X + 10, PASS_BOX_Y + 10);
    tft.print("(empty)");
  } else {
    uint8_t len = entryBuffer.length();
    uint8_t textSize = 3;
    int charW = 6 * textSize;
    int textW = len * charW;
    int startX = PASS_BOX_X + (PASS_BOX_W - textW) / 2;
    int startY = PASS_BOX_Y + 6;
    tft.setTextSize(textSize);
    tft.setTextColor(TXT);
    for (uint8_t i = 0; i < len; ++i) {
      tft.setCursor(startX + i * charW, startY);
      tft.print("*");
    }
  }
  tft.setTextSize(2); tft.setTextColor(SUB);
  tft.setCursor(20, 140); tft.print(maskLabel);
  tft.setCursor(20, 165); tft.print("- = Del (hold: back)");
  tft.setCursor(20, 190); tft.print("+ = OK");
}

void drawAccountSelect() {
  drawHeader("Select Account");
  tft.setTextSize(2);
  const uint8_t iconScale = 2; // icon drawn at 16x16
  int maxTextX = VIEW_W - 20; // allow 20 px right margin
  int textStartX = 16 + 8 * iconScale + 10;
  int maxWidth = VIEW_W - textStartX - 10; // width available for label

  for (uint8_t i = 0; i < NUM_LABELS; ++i) {
    int y = 60 + i * 36;
    const uint8_t* iconPtr = (const uint8_t*)pgm_read_word(&(icons[i]));
    drawIcon8x8(16, y, iconPtr, iconScale);

    // draw wrapped label starting at textStartX, y
    tft.setTextSize(2);
    tft.setTextColor(i == selectedLabelIndex ? ACCENT : TXT);
    // Draw item number prefix manually
    String prefix = String(i + 1) + ". ";
    tft.setTextSize(2);
    tft.setTextColor(i == selectedLabelIndex ? ACCENT : TXT);
    tft.setCursor(textStartX, y + 2);
    tft.print(prefix); // print prefix first

    // compute x after prefix:
    int prefixChars = prefix.length();
    int charW = 6 * 2;
    int afterPrefixX = textStartX + prefixChars * charW;

    // word-wrap the label text into remaining width
    wrapAndPrint(afterPrefixX, y + 2, labels[i], 2, VIEW_W - afterPrefixX - 10);
  }
  // footer hint
  tft.setTextSize(1); tft.setTextColor(SUB);
  tft.setCursor(10, VIEW_H - 18);
  tft.print("Use 2=UP 8=DOWN  + = OK  - = DEL");
}

void drawPasswordEntryForLabel(const char* label) {
  drawHeader("PASS ENTRY");
  tft.setTextSize(2); tft.setTextColor(SUB);
  tft.setCursor(20, 40); tft.print("Account:");
  tft.setTextColor(TXT); tft.setCursor(110, 40); tft.print(label);
  tft.drawRect(PASS_BOX_X, PASS_BOX_Y, PASS_BOX_W, PASS_BOX_H, BORDER);
  if (entryBuffer.length() == 0) {
    tft.setTextSize(2); tft.setTextColor(SUB);
    tft.setCursor(PASS_BOX_X + 10, PASS_BOX_Y + 10); tft.print("(empty)");
  } else {
    uint8_t len = entryBuffer.length();
    uint8_t ts = 3;
    int cw = 6 * ts;
    int tw = len * cw;
    int sx = PASS_BOX_X + (PASS_BOX_W - tw) / 2;
    int sy = PASS_BOX_Y + 6;
    tft.setTextSize(ts); tft.setTextColor(TXT);
    for (uint8_t i = 0; i < len; ++i) {
      tft.setCursor(sx + i * cw, sy); tft.print("*");
    }
  }
  tft.setTextSize(2); tft.setTextColor(SUB);
  tft.setCursor(20, 140); tft.print("Use 0-9 to enter");
  tft.setCursor(20, 165); tft.print("- = Del (hold: back)");
  tft.setCursor(20, 190); tft.print("+ = OK (send)");
}

void drawChangeSelect() {
  drawHeader("Change Password");
  tft.setTextSize(2);
  for (uint8_t i = 0; i < MAX_USERS; ++i) {
    String label = String(i + 1) + ". " + String(userNames[i]);
    String master = readUserMaster(i);
    tft.setTextColor(master.length() == 0 ? SUB : TXT);
    tft.setCursor(20, 60 + i * 36);
    tft.print(master.length() == 0 ? label + " (unused)" : label);
  }
  tft.setTextSize(1); tft.setTextColor(SUB);
  tft.setCursor(10, VIEW_H - 18);
  tft.print("Pick user to change (no login required)");
}

void showTempMessage(const char* msg, uint16_t color, uint16_t ms) {
  tft.fillRect(20, VIEW_H - 60, VIEW_W - 40, 46, BG);
  tft.setTextSize(2); tft.setTextColor(color);
  tft.setCursor(26, VIEW_H - 52); tft.print(msg);
  delay(ms);
  // redraw current screen
  switch (screen) {
    case ACTION_SELECT: drawActionSelect(); break;
    case USER_SELECT: drawUserSelect(); break;
    case MASTER_ENTER: drawMasterPrompt("Enter Master", "Digits 0-9"); break;
    case NEWUSER_SELECT: drawNewUserSelect(); break;
    case NEWUSER_CREATE: drawMasterPrompt("Create Master", "Digits 0-9"); break;
    case ACCOUNT_SELECT: drawAccountSelect(); break;
    case PASSWORD_ENTRY: drawPasswordEntryForLabel(labels[selectedLabelIndex]); break;
    case CHANGE_SELECT: drawChangeSelect(); break;
    case CHANGE_ENTER: drawMasterPrompt("New PIN", "Digits 0-9"); break;
    default: drawActionSelect(); break;
  }
}

// ---------- Actions & input ----------
void submitAccountPassword() {
  Serial.print("LBL:"); Serial.println(labels[selectedLabelIndex]);
  Serial.print("PW:"); Serial.println(entryBuffer);
  showTempMessage("SENT!", ACCENT, 500);
  entryBuffer = "";
  screen = ACCOUNT_SELECT;
  drawAccountSelect();
}

void handleDigit(char d) {
  if (entryBuffer.length() < (MAX_LEN - 1)) {
    entryBuffer += d;
    if (screen == MASTER_ENTER) drawMasterPrompt("Enter Master", "Digits 0-9");
    else if (screen == NEWUSER_CREATE) drawMasterPrompt("Create Master", "Digits 0-9");
    else if (screen == PASSWORD_ENTRY) drawPasswordEntryForLabel(labels[selectedLabelIndex]);
    else if (screen == CHANGE_ENTER) drawMasterPrompt("New PIN", "Digits 0-9");
  }
}

void handleDeleteShortOrBack() {
  if (entryBuffer.length() > 0) {
    entryBuffer.remove(entryBuffer.length() - 1);
    if (screen == MASTER_ENTER) drawMasterPrompt("Enter Master", "Digits 0-9");
    else if (screen == NEWUSER_CREATE) drawMasterPrompt("Create Master", "Digits 0-9");
    else if (screen == PASSWORD_ENTRY) drawPasswordEntryForLabel(labels[selectedLabelIndex]);
    else if (screen == CHANGE_ENTER) drawMasterPrompt("New PIN", "Digits 0-9");
  } else {
    // if empty, back out to appropriate screen
    if (screen == MASTER_ENTER || screen == NEWUSER_CREATE) {
      screen = USER_SELECT; drawUserSelect();
    } else if (screen == PASSWORD_ENTRY) {
      screen = ACCOUNT_SELECT; drawAccountSelect();
    } else if (screen == CHANGE_ENTER) {
      screen = CHANGE_SELECT; drawChangeSelect();
    }
  }
}

// navigation helpers (wrap-around)
void accountMoveUp() {
  if (selectedLabelIndex <= 0) selectedLabelIndex = NUM_LABELS - 1;
  else selectedLabelIndex--;
  drawAccountSelect();
}
void accountMoveDown() {
  if (selectedLabelIndex >= (int)NUM_LABELS - 1) selectedLabelIndex = 0;
  else selectedLabelIndex++;
  drawAccountSelect();
}

// ---------- IR handling ----------
void handleIRCode(uint32_t code) {
  // numeric mapping + navigation mapping:
  if (code == IR_0) handleDigit('0');
  else if (code == IR_1) handleDigit('1');
  else if (code == IR_2) { // UP OR digit 2
    if (screen == ACCOUNT_SELECT) accountMoveUp();
    else handleDigit('2');
  }
  else if (code == IR_3) handleDigit('3');
  else if (code == IR_4) handleDigit('4'); // left is no longer navigation
  else if (code == IR_5) handleDigit('5');
  else if (code == IR_6) handleDigit('6'); // right removed
  else if (code == IR_7) handleDigit('7');
  else if (code == IR_8) { // DOWN OR digit 8
    if (screen == ACCOUNT_SELECT) accountMoveDown();
    else handleDigit('8');
  }
  else if (code == IR_9) handleDigit('9');

  // DEL (short/long)
  if (code == IR_DEL) {
    unsigned long now = millis();
    if (!delActive) {
      delActive = true;
      delFirstMillis = now;
      handleDeleteShortOrBack();
    } else {
      if (now - delFirstMillis > LONG_PRESS_MS) {
        // long press -> back to higher menu
        if (screen == USER_SELECT || screen == NEWUSER_SELECT || screen == CHANGE_SELECT) {
          screen = ACTION_SELECT; drawActionSelect();
        } else if (screen == MASTER_ENTER || screen == NEWUSER_CREATE) {
          screen = USER_SELECT; drawUserSelect();
        } else if (screen == ACCOUNT_SELECT) {
          screen = ACTION_SELECT; drawActionSelect();
        } else if (screen == PASSWORD_ENTRY) {
          screen = ACCOUNT_SELECT; drawAccountSelect();
        }
        delActive = false;
      }
    }
  }

  // OK handling
  if (code == IR_OK) {
    if (screen == MASTER_ENTER) {
      if (userSelectedIdx < 0 || userSelectedIdx >= MAX_USERS) {
        showTempMessage("Select user first", ALERT, 700);
        screen = USER_SELECT; drawUserSelect(); entryBuffer = ""; return;
      }
      String stored = readUserMaster((uint8_t)userSelectedIdx);
      if (stored.length() == 0) { showTempMessage("No user", ALERT, 700); entryBuffer=""; screen=USER_SELECT; drawUserSelect(); return; }
      if (entryBuffer.equals(stored)) {
        // <<< ADDED: send user id for DB tracking >>>
        Serial.print("USR:");
        Serial.println(userSelectedIdx + 1);
        // <<< end addition >>>

        entryBuffer = "";
        screen = ACCOUNT_SELECT;
        selectedLabelIndex = 0;
        drawAccountSelect();
        showTempMessage("Login OK", ACCENT, 400);
      } else {
        entryBuffer = "";
        drawMasterPrompt("Enter Master", "Digits 0-9");
        showTempMessage("Wrong PIN", ALERT, 500);
      }
    }
    else if (screen == NEWUSER_CREATE) {
      if (userSelectedIdx < 0 || userSelectedIdx >= MAX_USERS) { showTempMessage("Select slot first", ALERT, 700); screen=NEWUSER_SELECT; drawNewUserSelect(); entryBuffer=""; return; }
      if (!userSlotAvailable((uint8_t)userSelectedIdx)) { showTempMessage("Slot used", ALERT, 700); screen=NEWUSER_SELECT; drawNewUserSelect(); entryBuffer=""; return; }
      if (entryBuffer.length() == 0) { showTempMessage("Empty not allowed", ALERT, 700); drawMasterPrompt("Create Master", "Digits 0-9"); return; }
      writeUserMaster((uint8_t)userSelectedIdx, entryBuffer);
      entryBuffer=""; showTempMessage("User created", ACCENT, 700);
      screen=ACTION_SELECT; drawActionSelect();
    }
    else if (screen == ACCOUNT_SELECT) {
      // OK opens selected account entry
      screen = PASSWORD_ENTRY;
      entryBuffer = "";
      drawPasswordEntryForLabel(labels[selectedLabelIndex]);
    }
    else if (screen == PASSWORD_ENTRY) {
      if (entryBuffer.length() > 0) {
        submitAccountPassword();
      } else {
        showTempMessage("Empty", ALERT, 400);
      }
    }
    else if (screen == CHANGE_ENTER) {
      if (userSelectedIdx < 0 || userSelectedIdx >= MAX_USERS) { showTempMessage("Select user first", ALERT,700); screen=CHANGE_SELECT; drawChangeSelect(); entryBuffer=""; return;}
      if (entryBuffer.length() == 0) { showTempMessage("Empty not allowed", ALERT,700); drawMasterPrompt("New PIN","Digits 0-9"); return;}
      writeUserMaster((uint8_t)userSelectedIdx, entryBuffer);
      entryBuffer=""; showTempMessage("Changed", ACCENT, 700);
      screen = ACTION_SELECT; drawActionSelect();
    }
  } // end OK handling

  // Context-specific numeric shortcuts & navigation
  if (screen == ACTION_SELECT) {
    if (code == IR_1) { screen = USER_SELECT; drawUserSelect(); userSelectedIdx=-1; entryBuffer=""; return; }
    else if (code == IR_2) { screen = NEWUSER_SELECT; drawNewUserSelect(); userSelectedIdx=-1; entryBuffer=""; return; }
    else if (code == IR_3) { screen = CHANGE_SELECT; drawChangeSelect(); userSelectedIdx=-1; entryBuffer=""; return; }
  }
  else if (screen == USER_SELECT) {
    if (code == IR_1 || code == IR_2 || code == IR_3) {
      uint8_t idx = (code == IR_1) ? 0 : (code == IR_2 ? 1 : 2);
      String master = readUserMaster(idx);
      if (master.length() == 0) { showTempMessage("User unused", ALERT, 700); drawUserSelect(); }
      else { userSelectedIdx = idx; entryBuffer = ""; screen = MASTER_ENTER; drawMasterPrompt("Enter Master", "Digits 0-9"); }
    }
  }
  else if (screen == NEWUSER_SELECT) {
    if (code == IR_1 || code==IR_2 || code==IR_3) {
      uint8_t idx = (code == IR_1) ? 0 : (code == IR_2 ? 1 : 2);
      if (!userSlotAvailable(idx)) { showTempMessage("Slot used", ALERT, 700); drawNewUserSelect(); }
      else { userSelectedIdx = idx; entryBuffer=""; screen = NEWUSER_CREATE; drawMasterPrompt("Create Master", "Digits 0-9"); }
    }
  }
  else if (screen == CHANGE_SELECT) {
    if (code == IR_1 || code==IR_2 || code==IR_3) {
      uint8_t idx = (code == IR_1) ? 0 : (code == IR_2 ? 1 : 2);
      if (readUserMaster(idx).length() == 0) { showTempMessage("User unused", ALERT, 700); drawChangeSelect(); }
      else { userSelectedIdx = idx; entryBuffer=""; screen = CHANGE_ENTER; drawMasterPrompt("New PIN", "Digits 0-9"); }
    }
  }

} // end handleIRCode

// ---------- Setup & Loop ----------
void setup() {
  Serial.begin(115200);
  pinMode(7, OUTPUT);
  digitalWrite(7, HIGH);

  tft.begin();
  tft.setRotation(1);

  IrReceiver.begin(IR_PIN);

  initializeUserPasswordsOnce();

  screen = ACTION_SELECT;
  drawActionSelect();
}

void loop() {
  if (delActive && (millis() - delFirstMillis > 1200)) delActive = false;

  if (IrReceiver.decode()) {
    uint32_t code = IrReceiver.decodedIRData.decodedRawData;
    handleIRCode(code);
    IrReceiver.resume();
  }
}
