/**
 * 電池残量表示
 */
#include <M5StickCPlus.h>

/**
 * Batteryクラス
 *
 * 電池残量を表示する。
 */
class Battery {
   public:
	Battery();
	void setPosAndSize(int posX, int posY, int showSizeNum);
	void setDeleteBgColor(uint32_t color);
	void setTextColor(uint32_t color);
	void showBattery();
	void deleteBattery();
	void batteryUpdate(int percent);
	int calcBatteryPercent();

   private:
	const float MAX_BATTERY_VOLTAGE = 4.2f;
	const float MIN_BATTERY_VOLTAGE = 3.0f;
	const int8_t BITS_PER_PIXEL = 1;
	const int TRANS_PARENTS = 0;
	const int MAX_SHOW_SIZE = 7;
	const int MIN_SHOW_SIZE = 1;

	TFT_eSprite _sprite = TFT_eSprite(&M5.Lcd);
	bool _showFlg;
	int _x;
	int _y;
	int _showSize;
	int _width;
	int _height;
	int _top_width;
	uint32_t _bg_color;
	uint32_t _line_color;
	uint32_t _text_color;

	void drawBatteryLines();
	uint32_t getBatteryColor();
	void showBatteryPercent(int i_percent);
	bool isLowBattery();
	bool isUsingBattery();
};

/**
 * コンストラクタ
 */
Battery::Battery() {
	// 初期化
	_showFlg = true;
	_x = 0;
	_y = 0;
	_showSize = 1;
	_width = 28 * _showSize;
	_height = 10 * _showSize;
	_top_width = 2 * _showSize;
	_bg_color = TFT_BLACK;
	_line_color = TFT_WHITE;
	_text_color = TFT_WHITE;
}

/**
 * 表示位置(x, y)と表示サイズを設定する関数
 */
void Battery::setPosAndSize(int posX, int posY, int showSizeNum) {
	_x = posX;
	_y = posY;

	if (MAX_SHOW_SIZE < showSizeNum) {
		_showSize = MAX_SHOW_SIZE;
	} else if (showSizeNum < MIN_SHOW_SIZE) {
		_showSize = MIN_SHOW_SIZE;
	} else {
		_showSize = showSizeNum;
	}

	_width = 28 * _showSize;
	_height = 10 * _showSize;
	_top_width = 2 * _showSize;
}

/**
 * 削除時の背景色を設定する関数
 */
void Battery::setDeleteBgColor(uint32_t color) { _bg_color = color; }

/**
 * 電池図形と文字の色を設定する関数
 */
void Battery::setTextColor(uint32_t color) {
	_line_color = color;
	_text_color = color;
}

/**
 * 電池残量を表示する関数
 */
void Battery::showBattery() {
	_showFlg = true;
	drawBatteryLines();
	_sprite.setColorDepth(16);
	_sprite.createSprite(_width - 1, _height - 1);
}

/**
 * 電池残量を非表示(塗りつぶし)する関数
 */
void Battery::deleteBattery() {
	// スプライト全体を_bg_colorで塗りつぶしてメモリ開放
	_sprite.deleteSprite();
	_sprite.createSprite(_width + _top_width, _height + 1);
	_sprite.fillSprite(_bg_color);
	_sprite.pushSprite(_x, _y);
	_sprite.deleteSprite();
	_showFlg = false;
}

/**
 * バッテリー残量の表示を更新する関数
 */
void Battery::batteryUpdate(int percent = -1) {
	if (!_showFlg) {
		return;
	}

	// 電池図形内部背景塗りつぶし
	_sprite.fillRect(0, 0, _width - 1, _height - 1, TFT_BLACK);

	// 実際にバッテリーの値を計算するか判定
	int i_percent = percent;
	if (i_percent == -1) {
		i_percent = calcBatteryPercent();
	}

	// バッテリー残量の割合を計算して背景色塗りつぶし
	int b_width = int((_width - 2) * (i_percent / 100.0f));
	_sprite.fillRect(0, 0, b_width + 1, _height - 1, getBatteryColor());

	showBatteryPercent(i_percent);		 // バッテリー数値を表示
	_sprite.pushSprite(_x + 1, _y + 1);	 // ディスプレイに表示
}

/**
 * 電池の図形を作成する関数
 */
void Battery::drawBatteryLines() {
	// 透明部分も作れるスプライト範囲を作成
	_sprite.setColorDepth(BITS_PER_PIXEL);
	_sprite.createSprite(_width + _top_width, _height + 1);
	_sprite.fillSprite(TRANS_PARENTS);

	// 電池図形を作成
	_sprite.fillRect(0, 0, _width + 1, _height + 1, _line_color);
	_sprite.fillRect(1, 1, _width - 1, _height - 1, TRANS_PARENTS);
	_sprite.fillRect(_width + 1, _top_width, _top_width, _height - (_top_width * 2) + 1, _line_color);

	_sprite.setBitmapColor(_line_color, TRANS_PARENTS);	 // 色を設定
	_sprite.pushSprite(_x, _y, TRANS_PARENTS);			 // 表示
	_sprite.deleteSprite();								 // メモリ開放
}

/**
 * バッテリー残量%を計算する関数(戻り値は0～100)
 */
int Battery::calcBatteryPercent() {
	float _vbat = M5.Axp.GetBatVoltage();
	float percent = (_vbat - MIN_BATTERY_VOLTAGE) / (MAX_BATTERY_VOLTAGE - MIN_BATTERY_VOLTAGE);
	return roundf(percent * 100.0f);
}

/**
 * バッテリーの状態に応じて色を取得する関数
 */
uint32_t Battery::getBatteryColor() {
	// バッテリー稼働中は緑、電圧が低い時は赤色、充電中は青色
	uint32_t color = TFT_DARKGREEN;
	if (isLowBattery()) {
		color = TFT_RED;
	} else if (!isUsingBattery()) {
		color = TFT_BLUE;
	}
	return color;
}

/**
 * バッテリー残量を表示する関数
 */
void Battery::showBatteryPercent(int i_percent) {
	// バッテリー数値を表示
	_sprite.setCursor(0 + _showSize, 0 + _showSize);
	_sprite.setTextFont(1);
	_sprite.setTextColor(_text_color);
	_sprite.setTextSize(_showSize);
	_sprite.print(i_percent);
	_sprite.print("%");
}

/**
 * 低電圧状態かを判定する関数
 */
bool Battery::isLowBattery() {
	// 低電圧状態(3.4V以下)だと1それ以外は0
	uint8_t _low_bat = M5.Axp.GetWarningLeve();
	if (_low_bat == 0) {
		return false;
	} else {
		return true;
	}
}

/**
 * バッテリー稼働か充電中かを判定する関数
 */
bool Battery::isUsingBattery() {
	// プラスが充電、マイナスがバッテリー稼働
	float _ibat = M5.Axp.GetBatCurrent();
	if (_ibat < 0.0f) {
		return true;
	} else {
		return false;
	}
}