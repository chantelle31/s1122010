#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// 初始化 LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- 🎹 按鈕腳位與數字對應 ---
const int buttonPins[10] = {2, 3, 4, 5, 6, 7, 8, 9, A0, A1};
const char numChars[10] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0'};

// 防爆衝變數（維持 500ms，強力壓制杜邦線接觸不良的跳字雜訊）
int lastButtonStates[10] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH};
unsigned long lastTriggerTimes[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const unsigned long DEBOUNCE_DELAY = 500; 

// --- 🎯 磁簧開關（話筒掛斷偵測）在 Pin 12 ---
const int reedPin = 12; 
int lastReedState = HIGH;
unsigned long lastReedTriggerTime = 0;
const unsigned long REED_DEBOUNCE_DELAY = 400; 

String inputString = "";
const int maxDigits = 10; // 螢幕最多只顯示 10 碼
bool isCalling = false;   

// 用來計算觀眾這一次總共「按了幾次按鍵」的計數器（按到第 11、12 下也會繼續往上加）
int buttonPressCount = 0;

// 🎯 HW-311 語音模組：指定播放指令
void playHW311(int trackNumber) {
  byte playCmd[8] = {0x7E, 0xFF, 0x06, 0x03, 0x00, 0x00, 0x00, 0xEF};
  playCmd[5] = (byte)((trackNumber >> 8) & 0xFF); 
  playCmd[6] = (byte)(trackNumber & 0xFF);        
  Serial1.write(playCmd, 8); 
}

// 🎯 HW-311 語音模組：停止指令
void stopHW311() {
  byte stopCmd[8] = {0x7E, 0xFF, 0x06, 0x16, 0x00, 0x00, 0x00, 0xEF}; 
  Serial1.write(stopCmd, 8);
}

// 🎯 HW-311 語音模組：設定音量 (0~30)
void setVolumeHW311(int vol) {
  byte volCmd[8] = {0x7E, 0xFF, 0x06, 0x06, 0x00, 0x00, (byte)vol, 0xEF}; 
  Serial1.write(volCmd, 8);
}

// 🧼 顯示歡迎畫面
void showWelcomeScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  Inner Echo  ");
  lcd.setCursor(0, 1);
  lcd.print("Ready to Dial...");
  isCalling = false;
}

// 📱 更新螢幕輸入畫面
void updateDisplay() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Dialing...");
  lcd.setCursor(0, 1);
  lcd.print(inputString); 
}

// 🧼 全系統重置
void resetWholeSystem() {
  inputString = "";   
  buttonPressCount = 0; 
  isCalling = false;  
  
  for (int i = 0; i < 10; i++) {
    lastButtonStates[i] = digitalRead(buttonPins[i]);
  }
  lastReedState = digitalRead(reedPin);
  
  stopHW311();
  setVolumeHW311(4); // 🌟 順利改回原本的音量 4
  delay(100);
  showWelcomeScreen();
}

// ❌ 掛斷重置處理
void handleHangUp() {
  stopHW311(); // 🌟 話筒一掛斷，音樂在這一瞬間立刻掐斷！
  
  lcd.init(); 
  lcd.backlight();
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Call Finished");
  lcd.setCursor(0, 1);
  lcd.print("Hanging up...");
  
  delay(1500); 
  resetWholeSystem(); 
}

void setup() {
  // 給 TouchDesigner 讀取的 Serial Port 速率
  Serial.begin(115200);
  
  // 與 HW-311 通訊的序列埠
  Serial1.begin(9600); 
  
  lcd.init();
  lcd.backlight();
  
  for (int i = 0; i < 10; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP); 
  }
  pinMode(reedPin, INPUT_PULLUP);
  
  delay(500);
  setVolumeHW311(4); // 🌟 順利改回原本的音量 4
  
  resetWholeSystem();
}

void loop() {
  unsigned long currentTime = millis();
  int currentReedState = digitalRead(reedPin);
  
  // ──────────────────────────────────────────
  // 🎯 部分一：偵測「磁簧開關」（只有掛斷才會結束通話）
  // ──────────────────────────────────────────
  if (currentReedState != lastReedState) {
    if (currentTime - lastReedTriggerTime > REED_DEBOUNCE_DELAY) {
      lastReedTriggerTime = currentTime;
      lastReedState = currentReedState;
      
      if (currentReedState == LOW) { 
        // 🛑【狀態：0】代表話筒放回底座（掛斷）
        Serial.println(buttonPressCount);
        
        // 🌟 輸出狀態 0 給 TD (掛斷代表狀態結束，丟 1)
        Serial.println("1"); 
        
        handleHangUp();
      } 
      else if (currentReedState == HIGH) {
        // 🟢【狀態：1】代表話筒被拿起來 (連續丟兩個 0 給 TD)
        Serial.println("0"); 
        Serial.println("0"); 
      }
    }
  }

  // ──────────────────────────────────────────
  // 🎹 部分二：偵測「撥號按鈕」（無線續按、保留聲音、螢幕鎖定10碼）
  // ──────────────────────────────────────────
  if (currentReedState == HIGH) {
    for (int i = 0; i < 10; i++) {
      int currentState = digitalRead(buttonPins[i]);
      
      if (currentState != lastButtonStates[i]) {
        if (currentTime - lastTriggerTimes[i] > DEBOUNCE_DELAY) {
          lastTriggerTimes[i] = currentTime;
          lastButtonStates[i] = currentState;
          
          if (currentState == LOW) { 
            
            buttonPressCount++; 
            
            if (inputString.length() < maxDigits) {
              inputString += numChars[i];
              updateDisplay(); 
            }
            
            // 🌟 每次播放前再次確保音量在原本的 4
            setVolumeHW311(30);
            delay(10);
            playHW311(i + 1); 
            
          }
        }
      }
    }
  }
}