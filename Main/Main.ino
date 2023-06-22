//-----------------------------------------------------------------------------------------------------
//++++++++++++++++++++++++++++++++++++
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "time.h"           //internet time
#include "elapsedMillis.h"  // repeat a task at regular intervals by checking if enough time has elapsed since the task

//++++++++++++++++++++++++++++++++++++
#include <DHT.h>
#define DHTTYPE DHT11
//Defines dht sensor pins
#define DHT1_PIN 26
#define DHT2_PIN 25
//Declares dht sensors
//DHT dht(DHTPIN, DHTTYPE);
DHT dht1(DHT1_PIN, DHT11);
DHT dht2(DHT2_PIN, DHT11);
DHT dht_array[] = { dht1, dht2 };
//This array saves the separate values of each sensor with [sensor][type of data]
//so if you want to get the first sensors temparature value, you would call it like so:
//dht_array_values[0][1]
//for the second array, 0 will return humidity and 1 will return temperature
float dht_array_values[2][2];
int dht_array_size;
float average_h = 0;
float average_t = 0;
//++++++++++++++++++++++++++++++++++++
#include <math.h>
//Defines soil moisture sensor pins
#define SM1_PIN 35
#define SM2_PIN 34
int sm_array[] = { SM1_PIN, SM2_PIN };
//This array saves the separate values of each sensor
float sm_array_values[2] = { 0 };
int sm_array_size;
float average_sm = 0;

float threshold = 35.0;               // SM change  threshold
float discrepancy_threshold = 10.00;  //sm diff value

enum SMState {
  belowThreshold,
  aboveThreshold,
  Invalid  // high discrepancy
};

//++++++++++++++++++++++++++++++++++++
//make sure your WiFi is 2.4 GHz
const char* WIFI_ssid = "";  // Enter SSID here
const char* WIFI_password = "";    //Enter Password here
const String location = "Vienna";            //Enter your City Name from api.weathermap.org

const String apiKey = "a1c734b2aaa54624a1b93405230206";  // API key for weather data
//++++++++++++++++++++++++++++++++++++
//time
// enter your GMT Time offset
#define GMT_OFFSET +2                   // vienna GMT: +02:00  //CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00
#define NPT_TIME_SERVER "pool.ntp.org"  //synchronized clocks
const long gmtOffset_sec = 3600 * GMT_OFFSET;
const int daylightOffset_sec = 3600 * 0;
byte Hour, Minute, Second;

//++++++++++++++++++++++++++++++++++++
// time for weather data
struct tm timeinfo;
elapsedSeconds CurrentTime;
elapsedSeconds GetTimeData;
elapsedSeconds GetWeatherData;
elapsedSeconds WaitPeriod;
//elapsedSeconds PumpDelay;

//++++++++++++++++++++++++++++++++++++
// Weather forecast data
String JSONData;
DynamicJsonDocument doc(24000);
DeserializationError error;
//++++++++++++++++++++++++++++++++++++
float rainProbability = 0;  // is the highest of 6hr chance of rain
bool dailyWillItRain;       // will it rain today?
float api_humidity = 0;
float api_temperature = 0;
int watering_duration = 0;

//++++++++++++++++++++++++++++++++++++
//The interval in which the sensors should be read and displayed
unsigned long sensor_delay = 10000;
unsigned long sensor_last_time = 0;
//++++++++++++++++++++++++++++++++++++
//Alarm
#define ALM_PIN 33
bool alarm_mute = true;
bool alarm_t = 0;
bool alarm_w = 0;
bool alarm_s = 0;
//++++++++++++++++++++++++++++++++++++
//LCD I2C
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
//Sets the LCD number of columns and rows
int lcdColumns = 16;
int lcdRows = 2;
//Sets LCD address, number of columns and rows
LiquidCrystal_I2C lcd(0x3F, lcdColumns, lcdRows);
//++++++++++++++++++++++++++++++++++++
//Mode variables
enum Mode { outdoor,
            indoor,
            manual };
Mode current_mode = outdoor;
//The interval in which the mode display should be refreshed
unsigned long mode_delay = 0;  //500;
unsigned long mode_last_time = 0;
//++++++++++++++++++++++++++++++++++++
//Defines button pins
#define BTN1_PIN 5
#define BTN2_PIN 14
#define BTN3_PIN 27
#include <OneButton.h>

// Create a button object with pin (5,14 and 27) and active high logic
OneButton btn1(BTN1_PIN, false);
OneButton btn2(BTN2_PIN, false);
OneButton btn3(BTN3_PIN, false);
//For the buttons it does not make sense to create an array, as each button has to
//have a separate isr assigned, as isr's dont take parameters
//Debounce times
//unsigned long btn1_debounce_delay = 50;
//unsigned long btn2_debounce_delay = 250;
//unsigned long btn3_debounce_delay = 250;
//For every button added, the following variables and functions (isr) have to be declared (dependent on type of interrupt):

//bool btn1_pressed = false;
//unsigned long btn1_last_debounce_time = 0;  //The last time the output pin was toggled
//unsigned long btn2_last_debounce_time = 0;  //The last time the output pin was toggled
//unsigned long btn3_last_debounce_time = 0;  //The last time the output pin was toggled
//++++++++++++++++++++++++++++++++++++
//Defines water level pin
#define UPPER_WL_PIN 18
#define LOWER_WL_PIN 19
String waterStatus = "";
//++++++++++++++++++++++++++++++++++++
#define PUMP_PIN 15
//++++++++++++++++++++++++++++++++++++
//-----------------------------------------------------------------------------------------------------
//++++++++++++++++++++++++++++++++++++
//Adding more functions to the interrupts may result in a guru meditation error,
//as an ISR utilizes the processor fully
//This means that the processor wants to access certain ressources that are not free yet
//So after adding a function to the ISR, immediately test out your changes on the microcontroller
//to see if anything breaks
/*void IRAM_ATTR Action_BTN1() {
  if ((millis() - btn1_last_debounce_time) > btn1_debounce_delay) {
    if (current_mode == manual) {
      if (btn1_pressed == false) {
        if (digitalRead(BTN1_PIN) == HIGH) {
          btn1_pressed = true;
          Serial_NewLine();
          Serial.print("Button 1 pressed");
          // if (current_mode == manual) {  // Check if the current mode is manual
            if (waterStatus != "<25") {
            Serial.println("Water level is low. Cannot start watering.");
            return;
          }
          Change_PUMP(1);
          //  }
        }
      } else if (btn1_pressed == true) {
        if (digitalRead(BTN1_PIN) == LOW) {
          btn1_pressed = false;
          Serial_NewLine();
          Serial.print("Button 1 released");
          //  if (current_mode == manual) {
          Change_PUMP(0);
        }
      }
    }
    btn1_last_debounce_time = millis();
  }
}
*/
// Callback function for button press event for manual watering
void onPress_Action_BTN1() {
  Serial_NewLine();
  Serial.print("Button 1 pressed");
  if (current_mode == manual) {  // Check if the current mode is manual
    if (waterStatus != "<25") {  // Check if water level is low
      Serial.println("\n Water level is low. Cannot start watering.");
      return;
    }
    Change_PUMP(1);  // Turn on pump
  }
  else {
    Serial.println("\n Invalid mode, current mode is not manual.");
  }
}

// Callback function for button release eventfor manual watering
void onRelease_Action_BTN1() {
  Serial_NewLine();
  Serial.print("Button 1 released");
  if (current_mode == manual) {  // Check if the current mode is manual
    Change_PUMP(0);              // Turn off pump
  }
}

//++++++++++++++++++++++++++++++++++++

/*void IRAM_ATTR Action_BTN2() {
  if ((millis() - btn2_last_debounce_time) > btn2_debounce_delay) {
    if (digitalRead(BTN2_PIN) == HIGH) {
      Serial_NewLine();
      //Serial.print("\n");
      Serial.print("Button 2 pressed");
      Change_MODE();
    }
    btn2_last_debounce_time = millis();
  }
}
*/
//++++++++++++++++++++++++++++++++++++
/*void IRAM_ATTR Action_BTN3() {
  if ((millis() - btn3_last_debounce_time) > btn3_debounce_delay) {
    if (digitalRead(BTN3_PIN) == HIGH) {
      Serial_NewLine();
      //Serial.print("\n");
      Serial.print("Button 3 pressed");
      //Change_ALM(0);
      alarm_mute = !alarm_mute;
    }
    btn3_last_debounce_time = millis();
  }
}
*/
 // Callback function for button alarm stop
void Action_BTN3() {
  Serial_NewLine();
  Serial.print("Button 3 pressed");
  // Toggle the alarm mute flag
  alarm_mute = !alarm_mute;
}
//+++++++++++++++++++++++++++++++++++++
//-----------------------------------------------------------------------------------------------------
void setup() {
  //Mostly needed when trying to find the address of the lcd display
  //Wire.begin();

  //Sets baud rate
  Serial.begin(115200);

  // Initialize the button with debounce time in ms
  //sensitivity of the button on press
  btn1.setDebounceTicks(100);
  btn2.setDebounceTicks(50);
  btn3.setDebounceTicks(50);

  // Attach callback functions for button press and release events
  btn1.attachLongPressStart(onPress_Action_BTN1);
  btn1.attachLongPressStop(onRelease_Action_BTN1);
  // Attach a callback function for the button press event
  btn2.attachClick(Change_MODE);
  btn3.attachClick(Action_BTN3);

  Init_LCD();

  Serial_NewLine();
  Init_DHT();
  Serial_NewLine();
  Init_SM();


  Init_WL();
  Init_PUMP();

  //Init_BTN();

  //Sets the sensor timer accordingly so the sensors measure right away with system start
  sensor_last_time = -(sensor_delay + 1);

  WiFi.mode(WIFI_STA);
  // Connect to Wi-Fi
  WiFi.begin(WIFI_ssid, WIFI_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial_NewLine();
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  // config internet time
  configTime(gmtOffset_sec, daylightOffset_sec, NPT_TIME_SERVER);

  // now get the time
  GetInternetTime();

  // optional to print time to serial monitor
  printLocalTime();

  GetDayForcast(rainProbability, dailyWillItRain);

  GetTimeData = 0;
  GetWeatherData = 0;
  WaitPeriod = 0;

  current_mode = outdoor;

  // Check the mode and trigger watering if necessary
  if (current_mode == indoor || current_mode == outdoor) {
    handleWatering();
  }
}
//-----------------------------------------------------------------------------------------------------
void loop() {
  //Only needed when trying to find the address of the lcd display
  //Find_LCD_ADR();

  // Update the button state and trigger the callbacks
  btn1.tick();
  btn2.tick();
  btn3.tick();

  // update the time every day, once we have internet time we'll use millis() to keep it updated
  if (GetTimeData > 86400) {  // 86400 reset the time every day
    GetTimeData = 0;
    GetInternetTime();
    //  pumpOnCount = 0; // Reset the pump activation count
  }

  //Reads and displays sensor data
  if ((millis() - sensor_last_time) > sensor_delay) {
    Serial_NewLine();
    printLocalTime();
    Serial_NewLine();
    Read_DHT(average_h, average_t);
    Serial_NewLine();
    Read_SM(sm_array_size, average_sm);
    Serial_NewLine();
    Read_WL();
    Serial_NewLine();
    Read_Weather();

    //Resets timer
    sensor_last_time = millis();
  }

  //Refreshes the displayed mode
  if ((millis() - mode_last_time) > mode_delay) {
    Print_MODE();

    //Resets timer
    mode_last_time = millis();
  }

  // update the weather forecase every hour
  if (GetWeatherData > 3600 * 1) {  // 3600 check every 1 hour
    GetDayForcast(rainProbability, dailyWillItRain);
    GetWeatherData = 0;
  }

  //Check if 1 minutes have passed
  if (WaitPeriod > 60 * 1) {
    handleWatering();
    WaitPeriod = 0;
  }

  Monitor_ALM();
}
//-----------------------------------------------------------------------------------------------------
void Serial_NewLine() {
  Serial.print("\n");
  Serial.print("+---------------------------+");
  Serial.print("\n");
}
//-----------------------------------------------------------------------------------------------------
void Init_DHT() {
  //Checks how many dht sensors are in the dht array
  Serial.print("Amount of dht sensors: ");
  dht_array_size = sizeof(dht_array) / sizeof(DHT);
  Serial.print(dht_array_size);

  //Initializes all dht sensors in the dht array
  for (int i = 0; i < dht_array_size; i++) {
    //pinMode(dht_array[i], INPUT);
    dht_array[i].begin();
    Serial.print("\n");
    Serial.print("DHT sensor ");
    Serial.print(i + 1);
    Serial.print(" initialized successfully!");
  }
}
//-----------------------------------------------------------------------------------------------------
void Read_DHT(float& average_h, float& average_t) {
  //Stores the total sum of read sensor data (of one cycle)
  float total_h = 0;
  float total_t = 0;

  int adjusted_dht_array_size = dht_array_size;

  //Reads values from the dht sensors
  for (int i = 0; i < dht_array_size; i++) {
    float h = dht_array[i].readHumidity();
    float t = dht_array[i].readTemperature();
    dht_array_values[i][0] = h;
    dht_array_values[i][1] = t;
    /*
    Serial.print(dht_array_values[i][0]);
    Serial.print("\n");
    Serial.print(dht_array_values[i][1]);
    Serial.print("\n");
    */
    if (!isnan(h) && !isnan(t)) {
      total_h += h;
      total_t += t;
    } else {
      adjusted_dht_array_size--;
    }

    Serial.print(i + 1);
    Serial.print(", ");
    Serial.print("Humidity: ");
    Serial.print(h);
    Serial.print(" %");
    Serial.print(", ");
    Serial.print("Temperature: ");
    Serial.print(t);
    Serial.print(" °C");
    Serial.print("\n");
  }

  //Calculates average values of the read sensor data (of one cycle)
  average_h = total_h / adjusted_dht_array_size;
  average_t = total_t / adjusted_dht_array_size;

  Write_LCD(0, 0, "H%");
  Write_LCD(2, 0, "   ");
  if (!isnan(average_h)) {
    Write_LCD(2, 0, String(average_h, 0));
  } else {
    Write_LCD(2, 0, "ERR");
  }

  Write_LCD(0, 1, "T");
  //Write_LCD(2, 1, "C");
  Write_LCD(1, 1, String(char(223)));
  Write_LCD(2, 1, "   ");
  if (!isnan(average_t)) {
    Write_LCD(2, 1, String(average_t, 0));
  } else {
    Write_LCD(2, 1, "ERR");
  }

  Serial.print("A, ");
  Serial.print("Humidity: ");
  Serial.print(average_h);
  Serial.print(" %");
  Serial.print(", ");
  Serial.print("Temperature: ");
  Serial.print(average_t);
  Serial.print(" °C");
}
//-----------------------------------------------------------------------------------------------------
void Init_SM() {
  //Checks how many soil moisture sensors are in the sm array
  Serial.print("Amount of soil moisture sensors: ");
  sm_array_size = sizeof(sm_array) / sizeof(int);
  Serial.print(sm_array_size);

  for (int i = 0; i < sm_array_size; i++) {
    pinMode(sm_array[i], INPUT);
  }
}
//-----------------------------------------------------------------------------------------------------
void Read_SM(int sm_array_size, float& average_sm) {
  //Stores the total sum of read sensor data (of one cycle)
  float total_sm = 0;

  //When at least one but max two (or amount of sensors - 1) of the soil moisture sensors return a zero as value
  //we do not want to take their values into consideration for the average soil moisture value
  //This is because if some of the sensors malfunction, unplug, ... we do not want them to spoil our measurements
  //and if at least one of the sensors does not return a zero as value while the others do, we can be assured
  //that one of the sensors works and is positioned in soil
  //If all of them are zero, meaning all are out of soil (or not functioning, ...), the result will be zero so or so
  int adjusted_sm_array_size = sm_array_size;

  //Reads values from the soil moisture sensors
  for (int i = 0; i < sm_array_size; i++) {
    float sm = Adjust_SM(analogRead(sm_array[i]));
    sm_array_values[i] = sm;
    /*
    Serial.print(sm_array_values[i]);
    Serial.print("\n");
    */
    if (!isnan(sm)) {
      total_sm += sm;
    } else {
      adjusted_sm_array_size--;
    }

    Serial.print(i + 1);
    Serial.print(", ");
    Serial.print("Soil moisture: ");
    Serial.print(sm);
    Serial.print(" %");
    Serial.print("\n");
  }

  //Calculates average values of the read sensor data (of one cycle)
  average_sm = total_sm / adjusted_sm_array_size;

  Write_LCD(6, 0, "S%");
  Write_LCD(8, 0, "   ");
  if (!isnan(average_sm)) {
    Write_LCD(8, 0, String(average_sm, 0));
  } else {
    Write_LCD(8, 0, "ERR");
  }

  Serial.print("A, ");
  Serial.print("Soil moisture: ");
  Serial.print(average_sm);
  Serial.print(" %");
}
//-----------------------------------------------------------------------------------------------------
float Adjust_SM(float data) {

  //When completely dry, the sensor returns 4095
  data = abs(data - 4095);

  //After using the absolute value, following values mean the following:
  //0 - completely dry
  //1555 - lowest possible value when
  //1947 (~1950) - max value

  //If the datas value is still higher than 1950 even after adjustment, it means that the sensor is disconnected
  //If the sensor is clearly disconnected, but still returns something else than an error,
  //lower the threshold closer to 1850
  if (data > 1950) {
    data = nan("");
    return data;
  }

  //Since 1555 is the lowest value possible when the sensor is in water,
  //meaning maximum moisture,
  //all values beyond 1555 do not have to be considered and can be clamped to 1555
  int clamp_value = 1950;  //1555;

  if (data > clamp_value) {
    data -= (data - clamp_value);
  }

  data = (data / clamp_value) * 100;

  return data;

  //return ( 100 - ( ( data / 4095 ) * 100 ) );
}
//-----------------------------------------------------------------------------------------------------

/*void Init_BTN() {
   attachInterrupt(BTN1_PIN, Action_BTN1, CHANGE);
  attachInterrupt(BTN2_PIN, Action_BTN2, RISING);
  attachInterrupt(BTN3_PIN, Action_BTN3, RISING);
}*/
//-----------------------------------------------------------------------------------------------------
void Find_LCD_ADR() {
  byte error, address;
  int nDevices;
  Serial.println("Scanning...");
  nDevices = 0;
  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.println(address, HEX);
      nDevices++;
    } else if (error == 4) {
      Serial.print("Unknow error at address 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0) {
    Serial.println("No I2C devices found\n");
  } else {
    Serial.println("done\n");
  }
}
//-----------------------------------------------------------------------------------------------------
void Init_LCD() {
  //Initializes LCD
  lcd.init();
  //Turns on LCD backlight
  lcd.backlight();
}
//-----------------------------------------------------------------------------------------------------
void Write_LCD(int x, int y, String message) {
  //LCD.clear();
  //Set cursor to x column, y row
  lcd.setCursor(x, y);
  //Print message on display
  lcd.print(message);
}
//-----------------------------------------------------------------------------------------------------
// Callback function for the button press event to change mode
void Change_MODE() {
  Serial_NewLine();
  switch (current_mode) {
    case outdoor:
      current_mode = indoor;
      Serial.print("Switched mode to indoor");
      break;
    case indoor:
      current_mode = manual;
      Serial.print("Switched mode to manual");
      break;
    case manual:
      current_mode = outdoor;
      Serial.print("Switched mode to outdoor");
      break;
  }
}
//-----------------------------------------------------------------------------------------------------
void Print_MODE() {
  Write_LCD(12, 1, "M:");

  int x = 14;
  int y = 1;

  switch (current_mode) {
    case outdoor:
      Write_LCD(x, y, "OD");
      break;
    case indoor:
      Write_LCD(x, y, "ID");
      break;
    case manual:
      Write_LCD(x, y, "MN");
      break;
  }
}
//-----------------------------------------------------------------------------------------------------
void Init_ALM() {
  pinMode(ALM_PIN, OUTPUT);
  Print_ALM();
}
//-----------------------------------------------------------------------------------------------------
void Change_ALM(bool set) {
  digitalWrite(ALM_PIN, set);  //turn buzzer on
}
//-----------------------------------------------------------------------------------------------------
void Monitor_ALM() {
  if (waterStatus == "<25") {
    alarm_w = 1;
  } else {
    alarm_w = 0;
  }

  if (average_sm > 90 || average_sm < 30) {
    alarm_s = 1;
  } else {
    alarm_s = 0;
  }

  //++++++++++++++++++++++++++++++++++++
  bool alarm_t_check = false;

  for (int i = 0; i < dht_array_size; i++) {
    if (isnan(dht_array_values[i][0]) || isnan(dht_array_values[i][1])) {
      alarm_t_check = true;
    }
  }

  for (int i = 0; i < sm_array_size; i++) {
    if (isnan(sm_array_values[i])) {
      alarm_t_check = true;
    }
  }

  if (waterStatus == "ERR") {
    alarm_t_check = true;
  }

  if (alarm_t_check == true) {
    alarm_t = 1;
  } else {
    alarm_t = 0;
  }
  //++++++++++++++++++++++++++++++++++++

  if ((alarm_t == 0 && alarm_w == 0 && alarm_s == 0) || alarm_mute == true) {
    Change_ALM(0);
  } else if (alarm_mute == false) {
    Change_ALM(1);
  }

  Print_ALM();
}
//-----------------------------------------------------------------------------------------------------
void Print_ALM() {
  int x = 12;
  if (alarm_mute == false) {
    Write_LCD(x, 0, "A");
  } else {
    Write_LCD(x, 0, "a");
  }
  x++;
  if (alarm_t == true) {
    Write_LCD(x, 0, "T");
  } else {
    Write_LCD(x, 0, "t");
  }
  x++;
  if (alarm_w == true) {
    Write_LCD(x, 0, "W");
  } else {
    Write_LCD(x, 0, "w");
  }
  x++;
  if (alarm_s == true) {
    Write_LCD(x, 0, "S");
  } else {
    Write_LCD(x, 0, "s");
  }
}
//-----------------------------------------------------------------------------------------------------
void Init_WL() {
  pinMode(UPPER_WL_PIN, INPUT_PULLUP);
  pinMode(LOWER_WL_PIN, INPUT_PULLUP);
}
//-----------------------------------------------------------------------------------------------------
void Read_WL() {
  int upper_water_level = digitalRead(UPPER_WL_PIN);
  int lower_water_level = digitalRead(LOWER_WL_PIN);

  if (upper_water_level == !HIGH && lower_water_level == !HIGH) {
    waterStatus = ">75";
  } else if (upper_water_level == !LOW && lower_water_level == !HIGH) {
    //waterStatus = "~50";
    waterStatus = String(char(126)) + "50";
  } else if (upper_water_level == !LOW && lower_water_level == !LOW) {
    waterStatus = "<25";
  } else {
    waterStatus = "ERR";
  }

  Write_LCD(6, 1, "L%");
  Write_LCD(8, 1, "   ");
  Write_LCD(8, 1, waterStatus);

  Serial.print("Water level: ");
  Serial.print(waterStatus);
  Serial.print(" %");
}
//-----------------------------------------------------------------------------------------------------
void Init_PUMP() {
  pinMode(PUMP_PIN, OUTPUT);
  Change_PUMP(0);
}
//-----------------------------------------------------------------------------------------------------
void Change_PUMP(bool set) {

  digitalWrite(PUMP_PIN, set);
  Serial_NewLine();
  if (set) {
    Serial.print("Pump started!");
  } else {
    Serial.print("Pump stopped!");
  }
}
//-----------------------------------------------------------------------------------------------------

// To check if the sensor value ar below or above the threshold
SMState checkSensorValues(float sm_array_values[], int sm_array_size) {
  Serial_NewLine();
  SMState state = belowThreshold;
  // bool isDiscrepancyTooHigh = false;

  // Check if both sensor values are above the threshold
  if (sm_array_values[0] > threshold && sm_array_values[1] > threshold) {
    state = aboveThreshold;
  }
  // Check if both sensor values are below the threshold
  else if (sm_array_values[0] < threshold && sm_array_values[1] < threshold) {
    state = belowThreshold;
  }
  // One sensor value is above the threshold and the other is below
  else {
    // Check for discrepancies between sensor values
    float discrepancy_sm = abs(sm_array_values[0] - sm_array_values[1]);

    if (discrepancy_sm >= discrepancy_threshold) {
      state = Invalid;
      //The discrepancy value
      Serial.print("Discrepancy to high, value: ");
      Serial.println(discrepancy_sm);
      // Set alarm discrepancy_threshold high to true

    } else {
      // Check if sensor average is below threshold
      if (average_sm < threshold) {
        state = belowThreshold;
      } else {
        state = aboveThreshold;
      }
    }
  }

  return state;
}

//-----------------------------------------------------------------------------------------------------

// A helper function that prints an error message
void handleError() {
  Serial.println("Error: Discrepancy too high");
  return;
}
//-----------------------------------------------------------------------------------------------------

// Decide watering duration based on sensor data
int decide_indoor_watering_duration(float* sm_array_values, int sm_array_size) {
  Serial_NewLine();
  int watering_duration = 0;  // Default duration is 0 (no watering needed)

  SMState sensorState = checkSensorValues(sm_array_values, sm_array_size);

    if (sensorState == Invalid) {  // Check for error value
      handleError();
      return -1;
    }

  if (sensorState == belowThreshold) {
    if (average_h < 60 && average_t > 24 && average_t < 30) {
      watering_duration = 8;  // 8 seconds
    } else if (average_h < 60 && average_t > 30) {
      watering_duration = 10;  // 10 seconds
    } else {
      watering_duration = 5;  // 5 seconds
    }
  }

  return watering_duration;
}

//--------------------------------------------------------------------------------------------------
// Decide watering duration based on sensor data and weather forecast
int decide_outdoor_watering_duration(float* sm_array_values, int sm_array_size) {
  Serial_NewLine();
  int watering_duration = 0;  // Default duration is 0 (no watering needed)

  SMState sensorState = checkSensorValues(sm_array_values, sm_array_size);


    if (sensorState == Invalid) {  // Check for error value
      handleError();
      return -1;
    }

  if (sensorState == belowThreshold) {
    if (average_sm < 15) {
      watering_duration = 10;  // Water for 10 seconds
    } else if (dailyWillItRain && rainProbability < 50) {
      watering_duration = decide_indoor_watering_duration(sm_array_values, sm_array_size) / 2;
    } else if (!dailyWillItRain) {
      watering_duration = decide_indoor_watering_duration(sm_array_values, sm_array_size);
    } else {
      watering_duration = -1;
    }
  }

  return watering_duration;
}

//-----------------------------------------------------------------------------------------------------
// Function to handle/manage  watering based on the selected model
void handleWatering() {
  Serial_NewLine();
  SMState sensorState = checkSensorValues(sm_array_values, sm_array_size);

  switch (current_mode) {
    case outdoor:
      if (sensorState == Invalid) {  // Check for error value
        handleError();
      } else if (sensorState == belowThreshold) {
        int watering_duration = decide_outdoor_watering_duration(sm_array_values, sm_array_size);
        handleWateringProcess(watering_duration, current_mode);
      } else {
        Serial.println("Sufficient soil moisture in outdoor mode");
      }
      break;

    case indoor:
      if (sensorState == Invalid) {  // Check for error value
        handleError();
      } else if (sensorState == belowThreshold) {
        int watering_duration = decide_indoor_watering_duration(sm_array_values, sm_array_size);
        handleWateringProcess(watering_duration, current_mode);
      } else {
        Serial.println("Sufficient soil moisture in indoor mode");
      }
      break;

    default:
      // Handle unsupported mode
      Serial.println("Manual mode");
      return;
  }
}

//-----------------------------------------------------------------------------------------------------
void handleWateringProcess(int watering_duration, Mode current_mode) {
  Serial_NewLine();
  unsigned long startTime = millis();                            // Get the start time
  unsigned long endTime = startTime + watering_duration * 1000;  // Calculate the end time


  //Helps prevent damage to the pump when water level is low.
  // Check water level is low if waterStatus is "<25" exit the function

  if (waterStatus != "<25") {
    Serial.println("Water level is low. Cannot start watering.");
    return;
  }

  Change_PUMP(1);
  Serial.print("\n Watering ");

  if (current_mode == indoor) {
    Serial.print("indoor");
  } else if (current_mode == outdoor) {
    Serial.print("outdoor");
  } else {
    Serial.print("Manual");
  }

  Serial.print(" for: ");
  Serial.print(watering_duration);
  Serial.println(" seconds");

  while (millis() < endTime) {
    // Continue watering until the specified duration is reached
    // Additional code for controlling water pump goes here
  }

  Change_PUMP(0);
  Serial.println("Watering completed");
}

//-----------------------------------------------------------------------------------------------------
void GetInternetTime() {
  getLocalTime(&timeinfo);
  // here's where we convert internet time to an unsigned long, then use the ellapsed counter to keep it up to date
  // if the ESP restarts, we'll start with the the internet time
  // note CurrentTime is managed by the ellapsed second counter
  CurrentTime = (timeinfo.tm_hour * 3600) + (timeinfo.tm_min * 60) + timeinfo.tm_sec;
  Hour = timeinfo.tm_hour;
  Minute = timeinfo.tm_min;
  Second = timeinfo.tm_sec;
}

//-----------------------------------------------------------------------------------------------------
void printLocalTime() {
  Serial_NewLine();
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
  }

  // print the day of the week, date, month, and year:
  char buf[80];
  strftime(buf, sizeof(buf), "Day: %A, %B %d %Y", &timeinfo);
  Serial.println(buf);

  // print the time:
  strftime(buf, sizeof(buf), "Time: %H:%M:%S", &timeinfo);
  Serial.println(buf);
}



//-----------------------------------------------------------------------------------------------------

void Read_Weather() {
  Serial.println();
  Serial.println("+---------------------------+");
  Serial.println("Weather data");
  Serial.print("Chance of rain: ");
  Serial.print(rainProbability);
  Serial.println("%");
  Serial.print("Will it rain today: ");
  Serial.println(dailyWillItRain ? "Yes" : "No");
  Serial.print("API_Humidity: ");
  Serial.print(api_humidity);
  Serial.println("%");
  Serial.print("API_Temperature: ");
  Serial.print(api_temperature);
  Serial.println(" °C");
}
//-----------------------------------------------------------------------------------------------------
void GetDayForcast(float& rainProbability, bool& dailyWillItRain) {
  Serial_NewLine();
  HTTPClient http;
  http.setTimeout(6000);

  api_humidity = 0;
  api_temperature = 0;
  rainProbability = 0;
  dailyWillItRain = false;

  Serial.println("Weather Forecast");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WL_CONNECTED");

    // Construct the API URL
    String url = "https://api.weatherapi.com/v1/forecast.json?key=" + apiKey + "&q=" + location + "&days=1";

    http.begin(url.c_str());
    int httpResponseCode = http.GET();

    Serial.print("HTTP to get day data: ");
    Serial.println(url);

    if (httpResponseCode > 200) {
      Serial.println("Service unavailable.");
      http.end();
      return;
    }

    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      JSONData = http.getString();
      Serial.println(JSONData);
      error = deserializeJson(doc, JSONData);
      Serial.print(F("deserializeJson() return code: "));
      Serial.println(error.c_str());

      rainProbability = extractRainProbability(doc);
      dailyWillItRain = getDailyRain(doc);
      api_temperature = doc["current"]["temp_c"];
      api_humidity = doc["current"]["humidity"];

      http.end();
    }
  } else {
    Serial.println("no wifi connection");
  }
  Read_Weather();
}


//-----------------------------------------------------------------------------------------------------
float extractRainProbability(JsonDocument& doc) {
  // Extract the rain probability from the forecast for the next 6 hrs
  JsonArray hour_data = doc["forecast"]["forecastday"][0]["hour"].as<JsonArray>();
  float rainHighestProbability = 0;

  int currentHour = Hour;

  //
  for (int i = currentHour; i < currentHour + 6; i++) {
    const JsonVariant& hour = hour_data[i];

    if (hour && hour.containsKey("chance_of_rain")) {
      float probability = hour["chance_of_rain"];
      if (probability > rainHighestProbability) {
        rainHighestProbability = probability;
      }
    }
  }

  return rainHighestProbability;
}

//-----------------------------------------------------------------------------------------------------
bool getDailyRain(JsonDocument& doc) {
  // // Check if it will rain today
  JsonObject day_data = doc["forecast"]["forecastday"][0].as<JsonObject>();
  if (day_data.containsKey("day")) {
    JsonObject day = day_data["day"].as<JsonObject>();
    if (day.containsKey("daily_will_it_rain")) {
      return day["daily_will_it_rain"] == 1;
    }
  }
  return false;
}


//-----------------------------------------------------------------------------------------------------


//-----------------------------------------------------------------------------------------------------