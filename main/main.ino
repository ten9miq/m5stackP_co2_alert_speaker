#include <M5StickCPlus.h>
#include <MHZ19.h>

// Co2センサー
#define RX_PIN 33			// Rx pin which the MHZ19 Tx pin is attached to
#define TX_PIN 32			// Tx pin which the MHZ19 Rx pin is attached to
#define BAUDRATE 9600		// Device to MH-Z19 Serial baudrate (should not be changed)
MHZ19 myMHZ19;				// Constructor for library
HardwareSerial mySerial(1); // (ESP32 Example) create device to MH-Z19 serial

#define BRIGHTNESS 9

bool lcdOn = true;
bool ledOn = false;
bool ledValue = false;
unsigned long nextLedOperationTime = 0;

unsigned long getDataTimer = 0;
unsigned long getViewDataTimer = 0;

int history[240] = {};
int historyPos = 0;

void setup()
{
	M5.begin();
	// dacWrite(25, 0); // Speaker OFF
	M5.Axp.ScreenBreath(BRIGHTNESS);
	pinMode(M5_LED, OUTPUT);
	M5.Lcd.setRotation(1);
	Serial.begin(9600); // Device to serial monitor feedback

	// CO2センサー初期化
	mySerial.begin(BAUDRATE, SERIAL_8N1, RX_PIN, TX_PIN); // (ESP32 Example) device to MH-Z19 serial start
	myMHZ19.begin(mySerial);							  // *Serial(Stream) refence must be passed to library begin().
	myMHZ19.autoCalibration(true);						  // 電源投入後24時間ごとに「ゼロキャリブレーション」（その時点の二酸化炭素濃度を「大気=400ppm」と判断すること

	render();
}

void loop()
{
	auto now = millis();

	// Aボタン: モードを切り替える
	M5.update();
	M5.Beep.update(); // tone関数で鳴らした音が指定時間経過していたら止める
	if (M5.BtnA.wasPressed())
	{
	}

	// Aボタン長押し: ゼロキャリブレーション
	if (M5.BtnA.pressedFor(2000))
	{
		myMHZ19.calibrateZero();
		M5.Beep.tone(1000, 100);
	}

	// Bボタン: 画面表示を ON/OFF する
	if (M5.BtnB.wasPressed())
	{
		lcdOn = !lcdOn;

		if (lcdOn)
		{
			render();
			M5.Axp.ScreenBreath(BRIGHTNESS);
		}
		else
		{
			M5.Axp.ScreenBreath(0);
		}
	}

	if ((ledOn || !ledValue) && nextLedOperationTime <= now)
	{
		ledValue = !ledValue;
		digitalWrite(M5_LED, ledValue);
		M5.Beep.tone(4000, 1000);

		// Lチカ
		if (ledValue)
		{
			nextLedOperationTime = now + 15000;
		}
		else
		{
			nextLedOperationTime = now + 800;
		}
	}

	if (now - getViewDataTimer >= 5000)
	{
		int CO2 = myMHZ19.getCO2();						   // Request CO2 (as ppm)
		int8_t temp = myMHZ19.getTemperature(false, true); // Request Temperature (as Celsius)
		Serial.print("CO2 (ppm): ");
		Serial.println(CO2);
		Serial.print(", Temperature (C): ");
		Serial.println(temp);
		render();
		getViewDataTimer = now;
	}

	if (now - getDataTimer >= 30000)
	{
		/* note: getCO2() default is command "CO2 Unlimited". This returns the correct CO2 reading even
		if below background CO2 levels or above range (useful to validate sensor). You can use the
		usual documented command with getCO2(false) */
		int CO2 = myMHZ19.getCO2(); // Request CO2 (as ppm)
		ledOn = CO2 >= 2000;

		// 測定結果の表示
		historyPos = (historyPos + 1) % (sizeof(history) / sizeof(int));
		// Serial.println(sizeof(history) / sizeof(int));
		history[historyPos] = CO2;

		getDataTimer = now;
	}
}

void render()
{
	if (!lcdOn)
	{
		return;
	}

	// Clear
	int height = M5.Lcd.height();
	int width = M5.Lcd.width();
	M5.Lcd.fillRect(0, 0, width, height, BLACK);

	int len = sizeof(history) / sizeof(int);
	for (int i = 0; i < len; i++)
	{
		auto value = max(0, history[(historyPos + 1 + i) % len] - 400);
		auto y = min(height, (int)(value * (height / 1200.0)));
		auto color = min(255, (int)(value * (255 / 1200.0)));
		M5.Lcd.drawLine(i, height - y, i, height, M5.Lcd.color565(255, 255 - color, 255 - color));
	}
	M5.Lcd.setTextSize(2);
	M5.Lcd.setTextColor(TFT_DARKGREY);
	M5.Lcd.drawString("CO2 : " + (String)history[historyPos] + " ppm", 10, height - 30, 2);
}