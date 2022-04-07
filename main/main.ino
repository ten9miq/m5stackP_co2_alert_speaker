#include <M5StickCPlus.h>
#include <MHZ19_uart.h>
MHZ19_uart mhz19;

// Co2センサー
#define RX_PIN 36	   // Rx pin which the MHZ19 Tx pin is attached to
#define TX_PIN 0	   // Tx pin which the MHZ19 Rx pin is attached to
#define BAUDRATE 9600  // Device to MH-Z19 Serial baudrate (should not be changed)

#define BRIGHTNESS 8

bool lcdOn = true;
bool ledOn = false;
bool ledValue = false;
unsigned long nextLedOperationTime = 0;
unsigned long get_graph_timer = 0;
unsigned long getViewDataTimer = 0;

int history[240] = {};
int historyPos = 0;

float co2_tone_on_limit = 2000.0;  // トーンを鳴らすco2の閾値
unsigned long renderTime = 2000;   // 描画感覚のミリ秒

// TFT_eSPI liblary
TFT_eSprite Lcd_buff = TFT_eSprite(&M5.Lcd);  // LCDのスプライト表示ライブラリ

#include "Battery.h"
Battery battery = Battery();

// LCD Timer
const uint8_t timer_mm = 10;  // タイマーの分数
const uint8_t timer_ss = 0;	  // タイマーの秒数
unsigned long get_silent_timer = 0;
const unsigned long silent_timer_limit = 600000;  // タイマーの分と秒の合算ミリ秒数(10分)
uint8_t mm = 0, ss = 0;
boolean is_measuring = true;
byte xsecs = 0, omm = 99, oss = 99;

void setup() {
	M5.begin(true, true, true);	 // LCDEnable, PowerEnable, SerialEnable(115200)
	// dacWrite(25, 0); // Speaker OFF
	M5.Axp.ScreenBreath(BRIGHTNESS);
	pinMode(M5_LED, OUTPUT);
	M5.Lcd.setRotation(1);
	Serial.begin(BAUDRATE);	 // Device to serial monitor feedback

	// TFT_eSPI setup
	Lcd_buff.createSprite(m5.Lcd.width(), m5.Lcd.height());
	Lcd_buff.fillRect(0, 0, m5.Lcd.width(), m5.Lcd.height(), TFT_BLACK);
	Lcd_buff.pushSprite(0, 0);

	// CO2センサー初期化
	mhz19.begin(RX_PIN, TX_PIN);
	mhz19.setAutoCalibration(false);

	render();
}

void loop() {
	auto now = millis();

	// Aボタン: モードを切り替える
	M5.update();
	M5.Beep.update();  // tone関数で鳴らした音が指定時間経過していたら止める
	// Aボタン: サイレントモード
	if (M5.BtnA.wasReleased()) {
		silent_timer_reset();
	}

	// Aボタン長押し: ゼロキャリブレーション
	if (M5.BtnA.pressedFor(3000)) {
		mhz19.calibrateZero();
		M5.Beep.tone(1000, 100);
	}

	// Bボタン: 画面表示を ON/OFF する
	if (M5.BtnB.wasReleased()) {
		lcdOn = !lcdOn;

		if (lcdOn) {
			render();
			M5.Axp.ScreenBreath(BRIGHTNESS);
		} else {
			M5.Axp.ScreenBreath(0);
		}
	}

	if ((ledOn || !ledValue) && nextLedOperationTime <= now) {
		ledValue = !ledValue;
		digitalWrite(M5_LED, ledValue);
		// サイレント時間であれば音を鳴らさない
		if (get_silent_timer <= now) {
			M5.Beep.tone(4000, 1000);
		}

		// Lチカ
		if (ledValue) {
			nextLedOperationTime = now + 15000;
		} else {
			nextLedOperationTime = now + 800;
		}
	}

	if (now - getViewDataTimer >= renderTime) {
		render();
		getViewDataTimer = now;
	}

	if (now - get_graph_timer >= 180000)  // 3分ごとにグラフへ追加
	{
		/* note: getCO2() default is command "CO2 Unlimited". This returns the correct CO2 reading even
		if below background CO2 levels or above range (useful to validate sensor). You can use the
		usual documented command with getCO2(false) */
		int CO2 = mhz19.getCO2PPM();
		// 測定結果の記録
		historyPos = (historyPos + 1) % (sizeof(history) / sizeof(int));
		history[historyPos] = CO2;

		get_graph_timer = now;
	}
}

void render() {
	// Clear
	Lcd_buff.fillSprite(TFT_BLACK);	 // 画面を黒塗りでリセット

	int CO2 = mhz19.getCO2PPM();
	ledOn = CO2 >= co2_tone_on_limit;  // LEDをON

	if (!lcdOn)	 // 描画OFFであれば処理を抜ける
	{
		return;
	}

	int height = m5.Lcd.height();
	int width = m5.Lcd.width();
	int len = sizeof(history) / sizeof(int);
	for (int i = 0; i < len; i++) {
		auto value = max(0, history[(historyPos + 1 + i) % len] - 400);
		auto y = min(height, (int)(value * (height / co2_tone_on_limit)));
		auto color = min(255, (int)(value * (255 / co2_tone_on_limit)));
		Lcd_buff.drawLine(i, height - y, i, height, Lcd_buff.color565(255, 255 - color, 255 - color));
	}
	int temp = mhz19.getTemperature();
	Serial.println("CO2 (ppm): " + (String)CO2 + ", Temperature (C): " + (String)temp);
	co2_text_render("CO2 : " + (String)CO2 + " ppm");
	tone_timer_view();
	battery_render();
	Lcd_buff.pushSprite(0, 0);	// LCDに描画
}

void co2_text_render(const String &string) {
	Lcd_buff.setTextSize(2);
	Lcd_buff.setTextColor(TFT_BLACK);
	Lcd_buff.drawString(string, 11, 11, 2);
	Lcd_buff.drawString(string, 9, 9, 2);

	Lcd_buff.setTextColor(TFT_LIGHTGREY);
	Lcd_buff.drawString(string, 10, 10, 2);
}

void battery_render() {
	Lcd_buff.setTextSize(1);
	Lcd_buff.setCursor(m5.Lcd.width() - 33, 7);
	int battery_percent = battery.calcBatteryPercent();
	if (battery_percent > 80) {
		Lcd_buff.setTextColor(TFT_GREEN);
	} else if (battery_percent > 30) {
		Lcd_buff.setTextColor(TFT_YELLOW);
	} else {
		Lcd_buff.setTextColor(TFT_RED);
	}
	Lcd_buff.printf("%3d %%", battery_percent);
}

void silent_timer_reset() {
	get_silent_timer = millis() + silent_timer_limit;  // 10分サイレント
	mm = timer_mm;
	ss = timer_ss;
}

void tone_timer_view() {
	if (get_silent_timer >= millis()) {
		if (ss == 0 && mm == 0) {
			return;
		} else {
			ss = ss - renderTime / 1000;
		}
		if (ss >= 60) {			   // Check for roll-over
			ss = 59 + (ss - 255);  // Reset seconds to zero
			mm = mm - 1;		   // Advance minute
			if (mm == 255) {	   // Check for roll-over
				mm = 0;
			}
		}
	} else {
		return;
	}
	// Draw digital time
	int xpos = 60;
	int ypos = 60;	// Top left corner ot clock text, about half way down
	int ysecs = ypos;
	Lcd_buff.setTextColor(TFT_GREEN);
	// Redraw hours and minutes time every minute
	omm = mm;
	if (mm < 10) xpos += Lcd_buff.drawChar('0', xpos, ypos, 4);
	xpos += Lcd_buff.drawNumber(mm, xpos, ypos, 4);
	xsecs = xpos;
	// Redraw seconds time every second
	oss = ss;
	xpos = xsecs;
	if (ss % 2) {											  // Flash the colons on/off
		Lcd_buff.setTextColor(0x39C4);						  // Set colour to grey to dim colon
		xpos += Lcd_buff.drawChar(':', xsecs, ysecs - 8, 4);  // Seconds colon
		Lcd_buff.setTextColor(TFT_GREEN);
	} else {
		xpos += Lcd_buff.drawChar(':', xsecs, ysecs - 8, 4);  // Seconds colon
	}
	// Draw seconds
	if (ss < 10) xpos += Lcd_buff.drawChar('0', xpos, ysecs, 4);  // Add leading zero
	Lcd_buff.drawNumber(ss, xpos, ysecs, 4);					  // Draw seconds
}