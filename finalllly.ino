#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// 初始化 LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- 按鈕腳位與數字對應 ---
const int buttonPins[10] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
const char numChars[10] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0'};

// 防爆衝雙重鎖變數
int lastButtonStates[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
unsigned long lastTriggerTimes[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const unsigned long DEBOUNCE_DELAY = 400; 

// 🎯 磁簧開關（話筒掛斷偵測）在 D12 
// 接法：COM 接 5V，NO 接 D12 且過 10K 電阻接地(GND)
const int reedPin = 12; 
int lastReedState = -1;
unsigned long lastReedTriggerTime = 0;

String inputString = "";
const int maxDigits = 10; 
bool isCalling = false;   

// 🎯 HW-311 語音模組專用：免程式庫直接發送播放指令
void playHW311(int trackNumber) {
  byte playCmd[8] = {0xAA, 0x07, 0x02, 0x00, 0x00, 0x00, 0x00, 0xB3};
  
  playCmd[3] = (trackNumber >> 8) & 0xFF; // 曲目高位元
  playCmd[4] = trackNumber & 0xFF;        // 曲目低位元
  
  // 計算校驗碼 (Checksum)
  int sum = 0;
  for(int i = 0; i < 6; i++) {
    sum += playCmd[i];
  }
  playCmd[6] = sum & 0xFF;
  
  Serial1.write(playCmd, 7); // 透過 D1 (TX) 傳送 7 個位元組給 HW-311
}

// 🎯 HW-311 語音模組專用：免程式庫直接發送停止指令
void stopHW311() {
  byte stopCmd[4] = {0xAA, 0x04, 0x00, 0xAE};
  Serial1.write(stopCmd, 4);
}

// 🎯 HW-311 語音模組專用：免程式庫設定音量 (0~30)
void setVolumeHW311(int vol) {
  byte volCmd[5] = {0xAA, 0x13, 0x01, 0x00, 0xBE};
  volCmd[3] = vol & 0xFF;
  
  int sum = 0;
  for(int i = 0; i < 4; i++) {
    sum += volCmd[i];
  }
  volCmd[4] = sum & 0xFF;
  Serial1.write(volCmd, 5);
}

void setup() {
  // 設定傳給 TouchDesigner 的序列埠速率
  Serial.begin(115200);
  
  // 初始化與 HW-311 通訊的硬體序列埠 (Arduino R4 專用 Serial1)
  Serial1.begin(9600); 
  
  lcd.init();
  lcd.backlight();
  
  // 🎯 重大修改：啟動內建上拉電阻，萬用板按鈕免焊任何電阻！
  for (int i = 0; i < 10; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP); // 🌟 改為 INPUT_PULLUP
    lastButtonStates[i] = digitalRead(buttonPins[i]); 
  }
  
  // 🎯 初始化磁簧開關腳位
  pinMode(reedPin, INPUT);
  lastReedState = digitalRead(reedPin);
  
  showWelcomeScreen();
  
  // 🎯 設定 HW-311 音量為 15，既大聲又保護 0.5W 小喇叭不破音
  delay(500);
  setVolumeHW311(15); 
}

void loop() {
  unsigned long currentTime = millis();

  // ──────────────────────────────────────────
  // 🎯 部分一：偵測「磁簧開關掛斷」（話筒放回、磁鐵靠近）
  // ──────────────────────────────────────────
  int currentReedState = digitalRead(reedPin);
  
  if (currentReedState != lastReedState) {
    if (currentTime - lastReedTriggerTime > DEBOUNCE_DELAY) {
      
      // 當 currentReedState == HIGH，代表磁鐵靠近（COM 與 NO 導通），也就是話筒放回電話底座了
      if (currentReedState == HIGH) { 
        // 只要目前有輸入號碼，或者正在通話中，放回話筒就執行掛斷
        if (inputString.length() > 0 || isCalling) {
          
          // 🎯 只在放回話筒的這一瞬間，把累積的字數傳給 TouchDesigner
          Serial.println(inputString.length()); 
          
          handleHangUp();
        }
      }
      lastReedTriggerTime = currentTime;
    }
    lastReedState = currentReedState;
  }

  // ──────────────────────────────────────────
  // 部分二：偵測「撥號按鈕 1~10」
  // ──────────────────────────────────────────
  if (!isCalling) {
    // 🎯 只有當話筒被拿起來（磁鐵離開，D12 變成 LOW）時，才允許玩家撥號
    if (currentReedState == LOW) {
      for (int i = 0; i < 10; i++) {
        int currentState = digitalRead(buttonPins[i]);
        
        if (currentState != lastButtonStates[i]) {
          if (currentTime - lastTriggerTimes[i] > DEBOUNCE_DELAY) {
            
            if (currentState == LOW) { 
              
              if (inputString.length() < maxDigits) {
                inputString += numChars[i];
                updateDisplay();
                
                // 🎯 HW-311 播音：每按一鍵，立刻播放 0001.mp3 ~ 0010.mp3
                playHW311(i + 1); 
              }
              
              if (inputString.length() == maxDigits) {
                handleDialSuccess();
              }
              
              lastTriggerTimes[i] = currentTime;
            }
          }
          lastButtonStates[i] = currentState;
        }
      }
    }
  }
}

void showWelcomeScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  Inner Echo  ");
  lcd.setCursor(0, 1);
  lcd.print("Ready to Dial...");
  
  isCalling = false;
}

void updateDisplay() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Dialing...");
  lcd.setCursor(0, 1);
  lcd.print(inputString); 
}

void handleDialSuccess() {
  isCalling = true; 
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Calling...");
  lcd.setCursor(0, 1);
  lcd.print(inputString);
  
  // 🎯 HW-311 播音：當按滿 10 碼打通時，自動播放 0011.mp3 通話背景音樂
  playHW311(11); 
}

void handleHangUp() {
  inputString = "";
  
  // 🎯 HW-311 動作：掛斷時立刻讓模組停止播放
  stopHW311(); 
  
  // 強迫 LCD 重新初始化，洗掉所有亂碼與移位
  lcd.init(); 
  lcd.backlight();
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Call Finished");
  lcd.setCursor(0, 1);
  lcd.print("Hanging up...");
  
  delay(1500); 
  showWelcomeScreen(); 
}