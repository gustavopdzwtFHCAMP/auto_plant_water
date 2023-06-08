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
int dht_array_size;
//++++++++++++++++++++++++++++++++++++
//Defines soil moisture sensor pins
#define SM1_PIN 35
#define SM2_PIN 34
int sm_array[] = { 
  SM1_PIN,
 SM2_PIN };
int sm_array_size;
//++++++++++++++++++++++++++++++++++++
//make sure your WiFi is 2.4 GHz
const char* WIFI_ssid = "Enter SSID here";  // Enter SSID here 
const char* WIFI_password = "Enter SSID Password here";    //Enter Password here
const String location = "Vienna";            //Enter your City Name from api.weathermap.org

const String apiKey = "a1c734b2aaa54624a1b93405230206"; // API key for weather data
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
elapsedSeconds PumpDelay;

//++++++++++++++++++++++++++++++++++++
// Weather forecast data
String JSONData;
DynamicJsonDocument doc(24000);
DeserializationError error;
//++++++++++++++++++++++++++++++++++++
float rainProbability = 0;  // is the highest of 6hr chance of rain
bool dailyWillItRain;       // will it rain today?
int watering_duration = 0;

float average_h = 0;
float average_t = 0;
int waterStatus1 = 0;

float threshold = 60.0;               // SM change  threshold
float discrepancy_threshold = 15.00;  //sm diff value
float* sm_values;

int pumpOnCount = 0;
int maxPumpOnCount = 4;
//++++++++++++++++++++++++++++++++++++
//The interval in which the sensors should be read and displayed
unsigned long sensor_delay = 5000;
unsigned long sensor_last_time = 0;
//++++++++++++++++++++++++++++++++++++
//Alarm
#define ALM_PIN 33
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
Mode current_mode =indoor;
//The interval in which the mode display should be refreshed
unsigned long mode_delay = 500;
unsigned long mode_last_time = 0;
//++++++++++++++++++++++++++++++++++++
//Defines button pins
#define BTN1_PIN 5
#define BTN2_PIN 14
#define BTN3_PIN 27
//For the buttons it does not make sense to create an array, as each button has to
//have a separate isr assigned, as isr's dont take parameters
//Debounce times
unsigned long btn1_debounce_delay = 5;
unsigned long btn2_debounce_delay = 250;
unsigned long btn3_debounce_delay = 250;
//For every button added, the following variables and functions (isr) have to be declared (dependent on type of interrupt):
bool btn1_pressed = false;
unsigned long btn1_last_debounce_time = 0;  //The last time the output pin was toggled
unsigned long btn2_last_debounce_time = 0;  //The last time the output pin was toggled
unsigned long btn3_last_debounce_time = 0;  //The last time the output pin was toggled
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
void IRAM_ATTR Action_BTN1() {
  if ((millis() - btn1_last_debounce_time) > btn1_debounce_delay) {
    if (current_mode == manual) {  // Check if the current mode is manual
      if (btn1_pressed == false) {
        if (digitalRead(BTN1_PIN) == HIGH) {
          btn1_pressed = true;
          Serial_NewLine();
          Serial.print("Button 1 pressed");
          Change_Alarm(1);
          Change_Pump(1);
          //Start_Pump();
        }
      } else if (btn1_pressed == true) {
        if (digitalRead(BTN1_PIN) == LOW) {
          btn1_pressed = false;
          Serial_NewLine();
          Serial.print("Button 1 released");
          Change_Alarm(0);
          Change_Pump(0);
         // Start_Pump() ;
        }
      }
      btn1_last_debounce_time = millis();
    }
  }
}
//++++++++++++++++++++++++++++++++++++
void IRAM_ATTR Action_BTN2() {
  if ((millis() - btn2_last_debounce_time) > btn2_debounce_delay) {
    if (digitalRead(BTN2_PIN) == HIGH) {
      Serial_NewLine();
      //Serial.print("\n");
      Serial.print("Button 2 pressed");
      Change_Mode();
    }
    btn2_last_debounce_time = millis();
  }
}
//++++++++++++++++++++++++++++++++++++
void IRAM_ATTR Action_BTN3() {
  if ((millis() - btn3_last_debounce_time) > btn3_debounce_delay) {
    if (digitalRead(BTN3_PIN) == HIGH) {
      Serial_NewLine();
      //Serial.print("\n");
      Serial.print("Button 3 pressed");
    }
    btn3_last_debounce_time = millis();
  }
}
//++++++++++++++++++++++++++++++++++++
//-----------------------------------------------------------------------------------------------------
void setup() {
  //Only needed when trying to find the address of the lcd display
  //Wire.begin();
  //Sets baud rate
  Serial.begin(115200);
  pinMode(UPPER_WL_PIN, INPUT_PULLUP);
  pinMode(LOWER_WL_PIN, INPUT_PULLUP);
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, HIGH);  // set default pump pin to be off

  Serial_NewLine();
  Init_DHT();
  Serial_NewLine();
  Init_SM();

  Init_BTN();
  Init_LCD();
  Init_ALM();
  Init_WL();
  Init_PUMP();

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

  current_mode = indoor;
}
//-----------------------------------------------------------------------------------------------------
void loop() {
  //Only needed when trying to find the address of the lcd display
  //Find_LCD_ADR();

  // update the time every day, once we have internet time we'll use millis() to keep it updated
  if (GetTimeData > 86400) {  // 86400 reset the time every day
    GetTimeData = 0;
    GetInternetTime();
    //  pumpOnCount = 0; // Reset the pump activation count
  }
  //Reads and displays sensor data
  if ((millis() - sensor_last_time) > sensor_delay) {
    Serial_NewLine();
    Read_DHT(average_h, average_t);
    Serial_NewLine();
    Read_SM(sm_array_size);
    Serial_NewLine();
    Read_WL();
    Serial_NewLine();
    readWaterLevel();

    //Resets timer
    sensor_last_time = millis();
  }
  // Check water level and perform watering if applicable
  if (waterStatus1 != 0) {
    handleWatering();
  } else {
    // Display error message
    Serial.println("Error: Water level is Low, watering process not possible");
    delay(60000);
  }

  // update the weather forecase every hour
  if (GetWeatherData > 3600 * 3) {  // 3600 check every 3 hour
    GetDayForcast(rainProbability, dailyWillItRain);
    GetWeatherData = 0;
  }

  // Check if 3 hours have passed
  if (WaitPeriod > 3600 * 3) {
    WaitPeriod = 0;
    // Start checking soil moisture and other data here
    decide_outdoor_watering_duration(sm_values, sm_array_size);
  }
  //Refreshes the displayed mode
  if ((millis() - mode_last_time) > mode_delay) {
    Print_Mode();

    //Resets timer
    mode_last_time = millis();
  }
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
// using pointer so return type is not necesary
void Read_DHT(float& average_h, float& average_t) {
  //Stores the total sum of read sensor data (of one cycle)
  float total_h = 0;
  float total_t = 0;

  int adjusted_dht_array_size = dht_array_size;

  //Reads values from the dht sensors
  for (int i = 0; i < dht_array_size; i++) {
    float h = dht_array[i].readHumidity();
    float t = dht_array[i].readTemperature();

    if (!isnan(h)) {
      total_h += h;
    }
    if (!isnan(t)) {
      total_t += t;
    }

    if (isnan(h) || isnan(t)) {
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
  //average_h = 100;
  //average_t = 100;


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
  sm_values = new float[sm_array_size];  // actual sm value

  for (int i = 0; i < sm_array_size; i++) {
    pinMode(sm_array[i], INPUT);
  }
}
//-----------------------------------------------------------------------------------------------------
void Read_SM(int sm_array_size) {
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
    sm_values[i] = sm;

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
  float average_sm = total_sm / adjusted_sm_array_size;
  //average_sm = 100;

  Serial.print("A, ");
  Serial.print("Soil moisture: ");
  Serial.print(average_sm);
  Serial.print(" %");
}
//-----------------------------------------------------------------------------------------------------
float Adjust_SM(float data) {
  /*
  //When completely dry, the sensor returns 4095
  data = abs(data - 4095);

  //After using the absolute value, following values mean the following:
  //0 - completely dry
  //1555 - lowest possible value when 
  //1947 (~1950) - max value
  
  //Since 1555 is the lowest value possible when the sensor is in water,
  //meaning maximum moisture,
  //all values beyond 1555 do not have to be considered and can be clamped to 1555
  int clamp_value = 1555;

  if(data > clamp_value){
    data -= (data - clamp_value);
  }
  
  data = (data/clamp_value)*100;
  return data;
  */

  return (100 - ((data / 4095) * 100));
}
//-----------------------------------------------------------------------------------------------------
void Init_BTN() {
  attachInterrupt(BTN1_PIN, Action_BTN1, CHANGE);
  attachInterrupt(BTN2_PIN, Action_BTN2, RISING);
  attachInterrupt(BTN3_PIN, Action_BTN3, RISING);
}
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
void Change_Mode() {
  Serial_NewLine();
  //Serial.print("\n");
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
void Print_Mode() {
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
}
//-----------------------------------------------------------------------------------------------------
void Change_Alarm(bool set) {
  digitalWrite(ALM_PIN, set);  //turn buzzer on
}
//-----------------------------------------------------------------------------------------------------
void Init_WL() {
  pinMode(UPPER_WL_PIN, INPUT_PULLUP);
  pinMode(LOWER_WL_PIN, INPUT_PULLUP);
}

//-----------------------------------------------------------------------------------------------------
void readWaterLevel() {
  int highWater = digitalRead(UPPER_WL_PIN);
  int lowWater = digitalRead(LOWER_WL_PIN);

  Serial.print("Water Level: ");

  if (highWater == LOW) {  // If water level is high
    Serial.println("High");
    waterStatus1 = 2;
  } else if (lowWater == LOW || lowWater == HIGH) {  // If water level is low or in between
    if (lowWater == LOW) {
      Serial.println("Sufficient");
      waterStatus1 = 1;  // Water level is sufficient
    } else {
      Serial.println("Low");
      waterStatus1 = 0;  // Water level is low
    }
  }
}

//-----------------------------------------------------------------------------------------------------
void Read_WL() {
  int upper_water_level = digitalRead(UPPER_WL_PIN);
  int lower_water_level = digitalRead(LOWER_WL_PIN);

  if (upper_water_level == HIGH && lower_water_level == HIGH) {
    waterStatus = ">75";
  } else if (upper_water_level == LOW && lower_water_level == HIGH) {
    waterStatus = "~50";
  } else if (upper_water_level == LOW && lower_water_level == LOW) {
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
}
//-----------------------------------------------------------------------------------------------------
void Change_Pump(bool set) {
  digitalWrite(PUMP_PIN, set);
  Serial_NewLine();
  if (set == true) {
    Serial.print("Pump started!");
  } else {
    Serial.print("Pump stopped!");
  }
}
//-----------------------------------------------------------------------------------------------------
void Start_Pump() {
  digitalWrite(PUMP_PIN, LOW);  // is high check 
  Serial_NewLine();
  Serial.println("Pump started.");
}

//-----------------------------------------------------------------------------------------------------
void Stop_Pump() {
  digitalWrite(PUMP_PIN, HIGH);  //is low
  Serial_NewLine();
  Serial.println("Pump stopped.");
}

//--------------------------------------------------------------------------------------------------
// Decide watering duration based on sensor data
int decide_indoor_watering_duration(float* sm_values, int sm_array_size) {
  int watering_duration = 0;  // Default duration is 0 (no watering needed)

  bool belowThreshold = checkSensorValues(sm_values, sm_array_size);

  if (belowThreshold) {
    if (average_h < 60 && average_t > 24 && average_t < 30) {
      watering_duration = 8000;  // 8 seconds
    } else if (average_h < 60 && average_t > 30) {
      watering_duration = 10000;  // 10 seconds
    } else {
      watering_duration = 5000;  // 5 seconds
    }
  }

  return watering_duration;
}

//--------------------------------------------------------------------------------------------------
// Decide watering duration based on sensor data and weather forecast
int decide_outdoor_watering_duration(float* sm_values, int sm_array_size) {
  int watering_duration = 0;  // Default duration is 0 (no watering needed)

  bool belowThreshold = checkSensorValues(sm_values, sm_array_size);


  if (belowThreshold) {
    if (sm_values[0] < 10) {
      watering_duration = 10000;  // Water for 10 seconds
    } else if (dailyWillItRain && rainProbability < 50) {
      watering_duration = decide_indoor_watering_duration(sm_values, sm_array_size) / 2;
    } else if (!dailyWillItRain) {
      watering_duration = decide_indoor_watering_duration(sm_values, sm_array_size);
    } else {
      // Wait for 3 hours before checking again
      watering_duration = -1;
      delay(300000);
    }
  }

  return watering_duration;
}
// Function to handle/manage  watering based on the selected model
void handleWatering() {
  Serial_NewLine();
  bool belowThreshold = checkSensorValues(sm_values, sm_array_size);
  switch (current_mode) {
    case outdoor:
      if (belowThreshold) {
        int watering_duration = decide_outdoor_watering_duration(sm_values, sm_array_size);
        handleWateringProcess(watering_duration);
      } else {
        Serial.println("Sufficient soil moisture in outdoor mode");
        delay(3600000);  // Delay for remaining time of 1 hour (1 hr - 1 min)
      }
      break;

    case indoor:
      if (belowThreshold) {
        int watering_duration = decide_indoor_watering_duration(sm_values, sm_array_size);
        handleWateringProcess(watering_duration);
      } else {
        Serial.println("Sufficient soil moisture in indoor mode");
        delay(3600000);  // Delay for remaining time of 1 hour (1 hr - 1 min)
      }
      break;

    case manual:
      //  manual control.
      // Manual mode: Control irrigation based on button state
      // No need to control pump here, as it is handled by the interrupt
      Serial.println("Manual model");
      delay(1000);

      break;
    default:
      // Handle unsupported  model
      Serial.println("Invalid model");
      delay(1000);
      return;
  }
}
//-----------------------------------------------------------------------------------------------------
void handleWateringProcess(int watering_duration) {
  if (watering_duration > 0) {
    Start_Pump();
    Serial.print("Watering ");
    Serial.println(current_mode);
    Serial.print(" mode for: ");
    Serial.println(watering_duration);
    delay(watering_duration);
    Stop_Pump();
    delay(5000);
  } else {
    Serial.println("Sufficient soil moisture");
    delay(10000);  // 1 minute delay
  }
}

//-----------------------------------------------------------------------------------------------------
bool checkSensorValues(float* sm_values, int sm_array_size) {
  bool belowThreshold = true;
  float maxSensorValue = sm_values[0];
  float minSensorValue = sm_values[0];

  // Check if all sensor values are below the threshold
  for (int i = 0; i < sm_array_size; i++) {
    if (sm_values[i] >= threshold) {
      belowThreshold = false;
      break;
    }

    // Update max and min sensor values
    if (sm_values[i] > maxSensorValue) {
      maxSensorValue = sm_values[i];
    }
    if (sm_values[i] < minSensorValue) {
      minSensorValue = sm_values[i];
    }
  }

/*
  // Debug print statements
  Serial.print("Max sensor value: ");
  Serial.println(maxSensorValue);
  Serial.print("Min sensor value: ");
  Serial.println(minSensorValue);
*/


  // Check if the discrepancy between sensors is greater than the discrepancy threshold
  if (maxSensorValue - minSensorValue > discrepancy_threshold) {
    belowThreshold = false;
    // Display sensor values and ask for sensor check
    for (int i = 0; i < sm_array_size; i++) {
      Serial.print("SM Sensor ");
      Serial.print(i + 1);
      Serial.print(" value: ");
      Serial.println(sm_values[i]);
    }
    Serial.println("Please check the sensors.");
  }

  return belowThreshold;
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
void GetDayForcast(float& rainProbability, bool& dailyWillItRain) {
  Serial_NewLine();
  HTTPClient http;
  http.setTimeout(6000);

  float humidity = 0;
  float temperature = 0;
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
      temperature = doc["current"]["temp_c"];
      humidity = doc["current"]["humidity"];
      dailyWillItRain = getDailyRain(doc);
      http.end();
    }
  } else {
    Serial.println("no wifi connection");
  }
  Serial_NewLine();
  Serial.println("Weather Forecast Data");
  Serial.print("Temp: ");
  Serial.print(temperature);
  Serial.println(" °C");
  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");
  String rainStatus = dailyWillItRain ? "yes" : "no";
  Serial.print("Raining Today ?: ");
  Serial.println(rainStatus);
  Serial.print("Rain probability: ");
  Serial.print(rainProbability);
  Serial.println(" %");
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
