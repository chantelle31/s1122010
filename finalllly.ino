#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DFRobotDFPlayerMini.h> // 使用 DFRobot 官方程式庫

// 初始化 LCD 與語音模組
LiquidCrystal_I2C lcd(0x27, 16, 2);
DFRobotDFPlayerMini myDFPlayer; // 宣告 DFPlayer 物件

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

void setup() {
  // 設定傳給 TouchDesigner 的序列埠速率
  Serial.begin(115200);
  
  // 初始化與模組通訊的硬體序列埠 (Arduino R4 專用 Serial1)
  Serial1.begin(9600); 
  
  lcd.init();
  lcd.backlight();
  
  for (int i = 0; i < 10; i++) {
    pinMode(buttonPins[i], INPUT);
    lastButtonStates[i] = digitalRead(buttonPins[i]); 
  }
  
  // 🎯 初始化磁簧開關腳位
  pinMode(reedPin, INPUT);
  lastReedState = digitalRead(reedPin);
  
  showWelcomeScreen();
  
  // 初始化晶片連線
  myDFPlayer.begin(Serial1);
  
  // 設定音量 (0~30)。接 0.5W 小喇叭給 15 左右最安全、不破音
  delay(300);
  myDFPlayer.volume(15); 
}

void loop() {
  unsigned long currentTime = millis();

  // ──────────────────────────────────────────
  // 🎯 部分一：偵測「磁簧開關掛斷」（話筒放回、磁鐵靠近）
  // ──────────────────────────────────────────
  int currentReedState = digitalRead(reedPin);
  
  if (currentReedState != lastReedState) {
    if (currentTime - lastReedTriggerTime > DEBOUNCE_DELAY) {
      
      // 当 currentReedState == HIGH，代表磁鐵靠近（COM 與 NO 導通），也就是話筒放回電話底座了
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
                
                // 每按一鍵，立刻指定播放對應的 0001.mp3 ~ 0010.mp3
                myDFPlayer.playLargeFolder(1, i + 1); 
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
  
  // 當按滿 10 碼打通時，自動播放 mp3 資料夾裡的 0011.mp3 通話背景音樂
  myDFPlayer.playLargeFolder(1, 11); 
}

void handleHangUp() {
  inputString = "";
  
  // 掛斷時立刻讓模組停止播放
  myDFPlayer.stop(); 
  
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