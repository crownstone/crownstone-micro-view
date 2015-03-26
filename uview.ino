#include <ctype.h>
#include <MicroView.h>

#define INT_READSTRING_MAX_LEN 10
#define INT_ARRAY_MAX_LEN 200

// Min and max of drawn temperature value
#define TEMPERATURE_MIN  0
#define TEMPERATURE_MAX  100

// Min and max of rssi drawn value: -99 so you only need 2 digits :)
#define RSSI_MIN         -99
#define RSSI_MAX         -40

#define TEMPERATURE     49
#define RSSI            50
#define CURRVENT_CURVE  51

#define PARSED_SUCCESS        0
#define PARSING_NOT_DONE      1
#define PARSED_INVALID_INPUT  2

#define READ_ARRAY_MODE_START 0
#define READ_ARRAY_MODE_SPACE 1
#define READ_ARRAY_MODE_INT   2

#define PAGE_TEMP_AND_RSSI  0
#define PAGE_CURRENT_CURVE  1
#define NUM_PAGES           2

#define CLOCK_FREQUENCY 32768 

const int buttonPin = 2;
int _buttonState = 0;

// Micro view:
// X axis goes from 0-63 (left to right)
// Y axis goes from 0-47 (up to down)

// args: x, y, minVal, maxVal
// x,y pos of topleft of widget
// WIDGETSTYLE0 horizontal, text 
// WIDGETSTYLE1 horizontal, text 
// WIDGETSTYLE2 vertical, (about ?x32) text below slider
// WIDGETSTYLE3 vertical, (?x43) text top right next to slider
// add WIDGETNOVALUE to disable text
MicroViewSlider* _slider = NULL;

// args: x, y, minVal, maxVal
// x,y pos of center of gauge
// WIDGETSTYLE0 small (about 32x32) gauge, text below
// WIDGETSTYLE1 large (about 48x48) gauge, text inside
// add WIDGETNOVALUE to disable text
MicroViewGauge* _gauge = NULL;

int _pageNum = 0;

int _readByte = 0;
int _readMode = 0;

String _readStringInt = "";
int _readArrayMode = READ_ARRAY_MODE_START;

int _currentCurve[INT_ARRAY_MAX_LEN];
int _currentCurveLenght = 0;

int _temperature;
int _rssi;


void setup() {
	pinMode(buttonPin, INPUT);
	Serial.begin(38400);
	uView.begin();
	showPage(PAGE_TEMP_AND_RSSI);
}


void showPage(int num) {
	uView.clear(PAGE);
//	uView.clear(ALL);
	
	delete _gauge;
	_gauge = NULL;
	delete _slider;
	_slider = NULL;
	
	Serial.print("Show page ");
	Serial.println(num);
	
	switch (num) {
		case PAGE_TEMP_AND_RSSI:
			// 16 for half the size of the gauge, 1 pixel space in between text and gauge
			_gauge = new MicroViewGauge(16, 16+uView.getFontHeight()+1, RSSI_MIN, RSSI_MAX, WIDGETSTYLE0);
			uView.setCursor(_gauge->getX()-16, 0);
			uView.print("RSSI");
		//	uView.drawChar(1, 1, 'T');
			
			// 16 for half the size of the gauge, 8 pixels space between gauge and slider
			// 4 pixels space in between text and slider
			_slider = new MicroViewSlider(_gauge->getX()+16+8, uView.getFontHeight()+4, TEMPERATURE_MIN, TEMPERATURE_MAX, WIDGETSTYLE2);
			uView.setCursor(_slider->getX(), 0);
			uView.print("Temp");
			
//			_gauge->reDraw();
//			_slider->reDraw();
			
			updateTemperature(_temperature);
			updateRssi(_rssi);
			
			uView.display();
			break;
		
		case PAGE_CURRENT_CURVE:
			if (!updateCurrentCurve(_currentCurve, _currentCurveLenght)) {
				uView.setCursor(10, 24-uView.getFontHeight());
				uView.println("Current");
				uView.setCursor(10, 24);
				uView.println(" curve");
				uView.display();
			}
			break;
	}
}


void loop() {

	//for (int i=-20; i<=20; ++i) {
		//_slider.setValue(i);
		//_gauge.setValue(i);
		//uView.display();
		//delay(100);
	//}
	
	while (Serial.available() > 0) {
		_readByte = Serial.read();
//		Serial.println("");
//		Serial.print("read: ");
//		Serial.println(_readByte);
		
//		Serial.print("mode=");
//		Serial.println(_readMode);
		if (_readMode) {
			char readChar = (char)_readByte;
			switch (_readMode) {
				case TEMPERATURE:
				{
					int res = parseToInt(readChar, _temperature);
					if (res == PARSED_SUCCESS) {
						updateTemperature(_temperature);
					}
					if (res != PARSING_NOT_DONE) {
						// Go back to mode selection
						_readMode = 0;
					}
					break;
				}
				case RSSI:
				{
					int res = parseToInt(readChar, _rssi);
					if (res == PARSED_SUCCESS) {
						updateRssi(_rssi);
					}
					if (res != PARSING_NOT_DONE) {
						// Go back to mode selection
						_readMode = 0;
					}
					break;
				}
				case CURRVENT_CURVE:
				{
					int res = parseToIntArray(readChar, _currentCurve, _currentCurveLenght);
					if (res == PARSED_SUCCESS) {
						updateCurrentCurve(_currentCurve, _currentCurveLenght);
					}
					if (res != PARSING_NOT_DONE) {
						// Go back to mode selection
						_readMode = 0;
					}
					break;
				}
				default:
					_readMode = 0;
					break;
			}
		}
		else {
			switch (_readByte) {
				case TEMPERATURE:
					_readMode = TEMPERATURE;
					break;
				case RSSI:
					_readMode = RSSI;
					break;
				case CURRVENT_CURVE:
					_readMode = CURRVENT_CURVE;
					break;
				default:
					break;
			}
			Serial.print("new mode=");
			Serial.println(_readMode);
		}
	}
	
	int state = digitalRead(buttonPin);
	if (state == HIGH && _buttonState == LOW) {
		_pageNum = (_pageNum+1) % NUM_PAGES;
		showPage(_pageNum);
		//uView.setCursor(35, 40);
		//uView.print(_pageNum);
		//uView.display();
	}
	if (state != _buttonState) {
		_buttonState = state;
	}
	//uView.setCursor(35, 40);
	//uView.print(_pageNum);
	//uView.display();
}

// Parses chars to read an integer
// Returns:
// PARSED_SUCCESS         Done parsing, value is set
// PARSING_NOT_DONE       Not yet done parsing, keep feeding more characters
// PARSED_INVALID_INPUT   Done parsing, but input was invalid, value is unchanged
int parseToInt(char c, int& val) {
	
	// Only accept digits
	if (isdigit(c)) {
		_readStringInt += c;
	}
	// But also accept minus as first char, and ignore leading white space
	else if (_readStringInt.length() == 0) {
		if (c == '-') {
			_readStringInt += c;
		}
		else if (isspace(c)) {
		}
		else {
			return PARSED_INVALID_INPUT;
		}
	}
	// If there is a white space after valid chars, or if we reached the max size, start calculating the value
	else if ((_readStringInt.length() && isspace(c)) || _readStringInt.length() >= INT_READSTRING_MAX_LEN) {
		// The .toInt function doesn't deal with negative numbers according to doc
		bool negative=false;
		if (_readStringInt.charAt(0) == '-') {
			negative=true;
//			_readStringInt.remove(0);
			_readStringInt = _readStringInt.substring(1);
		}
		val = _readStringInt.toInt();
		if (negative) {
			val = -val;
		}
		_readStringInt = "";
		return PARSED_SUCCESS;
	}
	// Otherwise, it's invalid input, return
	else {
		_readStringInt = "";
		return PARSED_INVALID_INPUT;
	}
	return PARSING_NOT_DONE;
}

// Parses chars to read an integer, will change the array during parsing
// Returns:
// PARSED_SUCCESS         Done parsing, value is set
// PARSING_NOT_DONE       Not yet done parsing, keep feeding more characters
// PARSED_INVALID_INPUT   Done parsing, but input was invalid
int parseToIntArray(char c, int val[], int& length) {
//	Serial.print("array mode=");
//	Serial.println(_readArrayMode);
	switch (_readArrayMode) {
		case READ_ARRAY_MODE_START:
//			Serial.println("array mode start");
			length = 0;
			if (c == '[') {
				_readArrayMode = READ_ARRAY_MODE_SPACE;
				break;
			}
			if (!isspace(c)) {
				_readArrayMode = READ_ARRAY_MODE_START;
				return PARSED_INVALID_INPUT;
			}
		case READ_ARRAY_MODE_SPACE:
//			Serial.println("array mode space");
			if (isspace(c)) {
				break;
			}
			if (c == ']') {
				_readArrayMode = READ_ARRAY_MODE_START;
				return PARSED_SUCCESS;
			}
			_readArrayMode = READ_ARRAY_MODE_INT;
		case READ_ARRAY_MODE_INT:
		{
//			Serial.println("array mode int");
			if (length >= INT_ARRAY_MAX_LEN) {
				_readArrayMode = READ_ARRAY_MODE_START;
				return PARSED_INVALID_INPUT;
			}
			int res = parseToInt(c, val[length]);
			if (res == PARSED_INVALID_INPUT) {
				_readArrayMode = READ_ARRAY_MODE_START;
				return PARSED_INVALID_INPUT;
			}
			if (res == PARSED_SUCCESS) {
//				Serial.print("int=");
//				Serial.println(val[length]);
				++length;
				_readArrayMode = READ_ARRAY_MODE_SPACE;
			}
		}
	}
	return PARSING_NOT_DONE;
}


void updateTemperature(int temp) {
	Serial.print("temp: ");
	Serial.print(temp);
	Serial.print("\r\n");
	if (temp < TEMPERATURE_MIN)
		temp = TEMPERATURE_MIN;
	if (temp > TEMPERATURE_MAX)
		temp = TEMPERATURE_MAX;
	if (_slider != NULL) {
		_slider->setValue(temp);
		uView.display();
	}
}

void updateRssi(int rssi) {
	Serial.print("rssi: ");
	Serial.print(rssi);
	Serial.print("\r\n");
	if (rssi < RSSI_MIN)
		rssi = RSSI_MIN;
	if (rssi > RSSI_MAX)
		rssi = RSSI_MAX;
	if (_gauge != NULL) {
		_gauge->setValue(rssi);
		uView.display();
	}
}

bool updateCurrentCurve(int curve[], int length) {
	if (_pageNum != PAGE_CURRENT_CURVE) {
		return true;
	}
	
	// There should be at least 2 samples to display and they should be an even number
	if (length < 4 || (length%2)) {
		return false;
	}
	uView.clear(PAGE);
	
	// curve will be in the form: time0 val0 time1 val1 ...
	int maxVal=0;
	int minVal=1024;
	Serial.print("curve:");
	for (int i=0; i<length; i+=2) {
		Serial.print(" ");
		Serial.print(curve[i]);
		Serial.print(" ");
		Serial.print(curve[i+1]);
		if (curve[i+1] > maxVal) {
			maxVal = curve[i+1];
		}
		if (curve[i+1] < minVal) {
			minVal = curve[i+1];
		}
	}
	Serial.println("\r\n");
	
	unsigned long tStart = curve[0];
	unsigned long tEnd = curve[length-2];
	double tScale = 63.0 / (tEnd - tStart);
	double vScale = 47.0 / (maxVal-minVal);
	
	Serial.println("plot:");
	unsigned long lastX = tScale*(curve[0] - tStart);
	int lastY = 47.0-vScale*(curve[1] - minVal);
	
	Serial.print("[");
	Serial.print(lastX);
	Serial.print(", ");
	Serial.print(lastY);
	Serial.print("] ");
	
	for (int i=2; i<length; i+=2) {
		unsigned long sampleX = tScale*(curve[i] - tStart);
		int sampleY = 47.0-vScale*(curve[i+1] - minVal);
		uView.line(lastX, lastY, sampleX, sampleY, WHITE, NORM);
		lastX = sampleX;
		lastY = sampleY;
		
		Serial.print("[");
		Serial.print(sampleX);
		Serial.print(", ");
		Serial.print(sampleY);
		Serial.print("] ");
	}
	Serial.println("");
	
	// Print the axis scale
	uView.setCursor(0, 0);
	uView.print(maxVal);
	uView.setCursor(0, 41);
	uView.print(minVal);
	//uView.setCursor(63-3*uView.getFontWidth(), 41);
	//uView.print(tEnd-tStart);
	uView.setCursor(32, 41);
//	uView.print((int)((tEnd-tStart)*1000.0/CLOCK_FREQUENCY));
	uView.print((tEnd-tStart)*1000.0/CLOCK_FREQUENCY, 1);
	uView.print("ms");
	
	Serial.print("uView.getFontHeight()=");
	Serial.println(uView.getFontHeight());
	
	uView.display();
	return true;
}
