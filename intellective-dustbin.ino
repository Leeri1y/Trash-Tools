_#include <Servo.h>          // 包含舵机库
#include <SoftSerialSTM32.h> // 包含软串口库
#include <U8g2lib.h>
#include <Wire.h>

	// 音符频率定义
#define do  261  // do = 1
#define re  294  // re = 2
#define mi  329  // mi = 3
#define fa  349  // fa = 4
#define sol  392  // sol = 5
#define la  440  // la = 6
#define si  493  // si = 7
#define dol  523  // dol = 1

// 小星星的旋律
int melody[] = {
  do, do, sol, sol, la, la, sol,
  fa, fa, mi, mi, re, re, do,
  sol, sol, fa, fa, mi, mi, re,
  sol, sol, fa, fa, mi, mi, re,
  do, do, sol, sol, la, la, sol,
  fa, fa, mi, mi, re, re, do,
  do
};

// 每个音符的时长
int noteDurations[] = {
  1000, 550, 550, 550, 550, 550, 550,
  1000, 550, 550, 550, 550, 550, 550,
  1000, 550, 550, 550, 550, 550, 550,
  1000, 550, 550, 550, 550, 550, 550,
  1000, 550, 550, 550, 550, 550, 550,
  1000, 550, 550, 550, 550, 550, 550,
  550
};

int servoAngles[8][4] = {
  {35, 10, 50, 0},  // re
  {50, 70, 25, 55},  // mi
  {70, 25, 40, 35},  // fa
  {20, 30, 55, 0},  // sol
  {45, 60, 30, 20},  // la
  {60, 35, 70, 45},  // si
  {40, 55, 80, 50}   // dol
};

	// 检测距离
	const  int distance = 4070;   
	
	// 检测和延时间隔
	const unsigned long checkInterval = 200;           // 设置检测间隔为200毫秒
	const unsigned long lidDelayTime = 5000;           // 盖子延时6.5秒
	unsigned long lastCheckTime = 0;

	// 音符和温湿度更新的时间变量
	unsigned long lastNoteTime = 0;
	int currentNote = 0;

	// 舵机和传感器引脚数组
	const int servoPins[] = {0, 1, 2, 3};
	const int analogPins[] = {A0, A1, A2, A3};

	// 新增红外传感器引脚
	const int proximityPin1 = 4;

	const int LED[] = {9,12,13,14};

	// 软串口配置
	SoftSerialSTM32 mySerial(10, 11);

	// 舵机状态和延时
	Servo myServos[4];                           // 舵机对象数组
	bool lidStates[4] = {false, false, false, false};  // 每个舵机的开闭状态
	bool closingDelayStarted[4] = {false, false, false, false}; // 标记延迟关闭是否已开始
  bool personDetectedLastTime[4] = {false, false, false, false}; // 用于存储每个垃圾桶的上次检测状态
	unsigned long lidCloseStartTimes[4] = {0, 0, 0, 0};      // 记录延迟关闭的开始时间

	char a = 0; // 语音模块传来的字符
  bool shouldLoopPlayMelody  = false; // 添加一个标志，用于控制循环是否继续


	// 初始化 U8g2 显示对象，基于 SSD1306 的 128x64 I2C OLED
	U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

	void setup() {
		
		u8g2.begin();

		Serial.begin(9600);
		mySerial.begin(9600);

		// 初始化引脚
		pinMode(proximityPin1, INPUT);

		for (int i = 0; i < 4; i++) {
			pinMode(LED[i], OUTPUT);
      digitalWrite(LED[i],HIGH);
		}

		displayFace(true);

		// 初始化舵机和传感器引脚
		for (int i = 0; i < 4; i++) {
			myServos[i].attach(servoPins[i]);
			myServos[i].write(0);       // 初始角度0度（关闭状态）
		}
		for (int i = 0; i < 4; i++) {
			pinMode(analogPins[i], INPUT);
		}

		Serial.flush();
		mySerial.flush();
	}

	void loop() {
	    checkProximitySensor();   // 检测是否有人靠近垃圾桶
	    checkVoiceCommands();     // 检查语音模块指令
		  checkBinSensor();

       //为了实现计数10000次后跳出while循环，可以添加一个计数器变量。以下是修改后的代码：

      int playCount = 0;
      while(shouldLoopPlayMelody) {
        playMelody();
        playCount++;
        if (playCount >= 500) {
            break;
        }
      //playMelody();
      }
	}


        // 检测红外传感器是否检测到人靠近垃圾桶
   void checkProximitySensor() {
      if (millis() - lastCheckTime >= checkInterval) {
        lastCheckTime = millis();

        for (int i = 0; i < 4; i++) {
        int analogValue = analogRead(analogPins[i]);
         Serial.print("模拟值 A");
        Serial.print(i);
        Serial.print(": ");
        Serial.println(analogValue);

        bool personDetected = (analogValue < distance);

        // 如果上次未检测到人且这次检测到人，则切换盖子状态
        if (personDetected &&!personDetectedLastTime[i]) {
            toggleLid(myServos[i], i);
            lidCloseStartTimes[i] = millis(); // 记录打开时间，用于自动关闭判断
        }

        // 如果盖子是打开的并且距离上次打开已经过了自动关闭延迟时间且当前无人靠近，则关闭盖子
            if (lidStates[i] && millis() - lidCloseStartTimes[i] >= lidDelayTime &&!personDetected) {
                closeLid(myServos[i], i);
            }

            // 更新上次检测状态
            personDetectedLastTime[i] = personDetected;
        }
      }
   }

void toggleLid(Servo &myServo, int index) {
  if (lidStates[index]) {
    closeLid(myServo, index);
  } else {
      openLid(myServo, index);
  }
 }

	// 检查语音模块指令
void checkVoiceCommands() {
    if (mySerial.available()) {
        a = mySerial.read(); // 读取语音模块的数据
        Serial.println(a);

        switch (a) {
            case '0':
                // 添加功能
                break;
            case '1':
                openLid(myServos[0], 0);
                break;
            case '2':
                openLid(myServos[2], 2);
                break;
            case '3':
                openLid(myServos[3], 3);
                break;
            case '4':
                openLid(myServos[1], 1);
                break;

            case '5':
                shouldLoopPlayMelody = true;
                break;

            case '6':
                for(int i=0 ; i < 4;i++)
                {
                  closeLid(myServos[i],i);
                }
                break;

            case '7':
                shouldLoopPlayMelody = false; // 停止播放旋律
                for (int i = 0; i < 4; i++) {
                    myServos[i].write(0); // 将舵机角度设置为初始值
                }
                break;

            default:
                Serial.println("未定义的指令");
                break;
            
        }
      mySerial.flush(); 
    }
}
	// 打开垃圾桶盖子
	void openLid(Servo &myServo, int index) {
		if(index == 0) {
			myServo.write(90);
		}else {
			myServo.write(90);       // 打开盖子到90度
		}
      digitalWrite(LED[index],LOW);
		 lidStates[index] = true; // 标记盖子为打开状态
		 Serial.print("Bin");
		 Serial.print(index);
		 Serial.println(" 检测到靠近的物体，已打开盖子！");
     lidCloseStartTimes[index] = millis(); // 在打开盖子时重置计时器

	}

	// 关闭垃圾桶盖子
	void closeLid(Servo &myServo, int index) {
		 myServo.write(0);                // 关闭盖子
     digitalWrite(LED[index],HIGH);

		 lidStates[index] = false;        // 标记盖子为关闭状态
		 closingDelayStarted[index] = false; // 重置延时关闭标志

		 Serial.print("Bin");
		 Serial.print(index);
		 Serial.println(" 无物体靠近，已关闭盖子！");
	}

// 播放旋律并调节舵机
void playMelody() {
    unsigned long currentMillis = millis();
    static bool ledState = false;

    if (currentMillis - lastNoteTime >= noteDurations[currentNote]) {

        for (int i = 0; i < 4; i++) {
            myServos[i].write(servoAngles[currentNote % 8][i]);
        }

        for (int i = 0; i < 4; i++) {
            digitalWrite(LED[i], ledState);
        }
        ledState =!ledState;

        lastNoteTime = currentMillis;
        currentNote++;

        if (currentNote >= sizeof(melody) / sizeof(melody[0])) {
            currentNote = 0;
            for (int j = 0; j < 4; j++) {
                myServos[j].write(0);
            }
        }
    }
}

void drawChineseCharacter(int x, int y, const uint8_t* charData) {
    for (int i = 0; i < 16; i++) {
        uint8_t line = charData[i * 2];
        uint8_t line2 = charData[i * 2 + 1];
        for (int j = 0; j < 8; j++) {
            if (line & (0x80 >> j)) u8g2.drawPixel(x + j, y + i);
            if (line2 & (0x80 >> j)) u8g2.drawPixel(x + j + 8, y + i);
        }
    }
}

	// 绘制更大的笑脸
	void drawLargerSmileyFace(int x, int y) {
	  u8g2.drawCircle(x, y, 28);      // 脸部圆圈，半径为28
	  u8g2.drawDisc(x - 12, y - 10, 4); // 左眼
	  u8g2.drawDisc(x + 12, y - 10, 4); // 右眼
	  
	  // 使用更宽的 drawLine 绘制笑脸的嘴巴
	  u8g2.drawLine(x - 14, y + 10, x - 7, y + 15);
	  u8g2.drawLine(x - 7, y + 15, x + 7, y + 15);
	  u8g2.drawLine(x + 7, y + 15, x + 14, y + 10);
	}

	// 绘制更大的哭脸
	void drawLargerSadFace(int x, int y) {
	  u8g2.drawCircle(x, y, 28);      // 脸部圆圈，半径为28
	  u8g2.drawDisc(x - 12, y - 10, 4); // 左眼
	  u8g2.drawDisc(x + 12, y - 10, 4); // 右眼

	  // 使用更宽的 drawLine 绘制哭脸的嘴巴（向下的弧形）
	  u8g2.drawLine(x - 14, y + 18, x - 7, y + 13); // 左侧线条，朝下
	  u8g2.drawLine(x - 7, y + 13, x + 7, y + 13);  // 中间线条
	  u8g2.drawLine(x + 7, y + 13, x + 14, y + 18); // 右侧线条，朝下
	}

	// 显示表情，根据参数选择笑脸或哭脸
	void displayFace(bool isSmiley) {
	  u8g2.clearBuffer(); // 清除缓冲区
	  if (isSmiley) {
		drawLargerSmileyFace(64, 32); // 居中显示更大的笑脸
	  } else {
		drawLargerSadFace(64, 32); // 居中显示更大的哭脸
	  }
	  u8g2.sendBuffer(); // 将缓冲区内容发送到屏幕
	}

  // 检测到垃圾桶溢出时，OLED显示哭脸
  void checkBinSensor() {
	  int proximityDetected = digitalRead(proximityPin1);
	  Serial.print("数字值 P");
	  Serial.print(": ");
	  Serial.println(proximityDetected);
	  if(proximityDetected) {
		  displayFace(true);
	  }else {
		  displayFace(false);
      }
	}
  
