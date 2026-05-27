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
  setVolumeHW311(8); 
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
  setVolumeHW311(8); 
  
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
        // 🌟 把不論按了多少次的總按鍵數（包含第11、12次...）一次傳給 TD
        Serial.println(buttonPressCount);
        
        // 🌟 輸出狀態 0 給 TD
        Serial.println("0"); 
        
        handleHangUp();
      } 
      else if (currentReedState == HIGH) {
        // 🟢【狀態：1】代表話筒被拿起來
        Serial.println("1"); 
      }
    }
  }

  // ──────────────────────────────────────────
  // 🎹 部分二：偵測「撥號按鈕」（無限續按、保留聲音、螢幕鎖定10碼）
  // ──────────────────────────────────────────
  // 🌟 修改核心：拿掉 !isCalling 限制，只要話筒拿起來 (D12 為 HIGH) 就能無限一直按
  if (currentReedState == HIGH) {
    for (int i = 0; i < 10; i++) {
      int currentState = digitalRead(buttonPins[i]);
      
      if (currentState != lastButtonStates[i]) {
        if (currentTime - lastTriggerTimes[i] > DEBOUNCE_DELAY) {
          lastTriggerTimes[i] = currentTime;
          lastButtonStates[i] = currentState;
          
          if (currentState == LOW) { 
            
            // 🌟 核心修改 1：只要按鈕被按下，計數器就無條件永遠往上加（按到第 100 下也可以）
            buttonPressCount++; 
            
            // 🌟 核心修改 2：只有在字數小於 10 碼時才去更新螢幕字串
            if (inputString.length() < maxDigits) {
              inputString += numChars[i];
              updateDisplay(); // 更新 LCD 顯示目前累積的字
            }
            // 💡 如果已經滿 10 碼了（按到第 11 下開始），上面這個 if 就不會進去，所以螢幕畫面會完全凍結不動！
            
            // 🌟 核心修改 3：不論按第幾下、螢幕有沒有動，對應的聲音依然每一次都要發射出來！
            playHW311(i + 1); 
            
          }
        }
      }
    }
  }
}