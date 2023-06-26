//-----------------------------------------------------------------------------------------------------
//++++++++++++++++++++++++++++++++++++
#include <DHT.h>
#define DHTTYPE DHT11
//Defines dht sensor pins
#define DHT1_PIN 26
//#define DHT2_PIN 25
//Declares dht sensors
//DHT dht(DHTPIN, DHTTYPE);
DHT dht1(DHT1_PIN, DHT11);
//DHT dht2(DHT2_PIN, DHT11);
DHT dht_array[] = {dht1};//, dht2};
//This array saves the separate values of each sensor with [sensor][type of data]
//so if you want to get the first sensors temparature value, you would call it like so:
//dht_array_values[0][1]
//for the second array, 0 will return humidity and 1 will return temperature
float dht_array_values[1][1];
int dht_array_size;
float average_h;
float average_t;
//++++++++++++++++++++++++++++++++++++
#include <math.h>
//Defines soil moisture sensor pins
#define SM1_PIN 35
//#define SM2_PIN 34
int sm_array[] = {SM1_PIN};//, SM2_PIN};
//This array saves the separate values of each sensor
float sm_array_values[1];
int sm_array_size;
float average_sm;
//++++++++++++++++++++++++++++++++++++
//The interval in which the sensors should be read and displayed
unsigned long sensor_delay = 5000;
unsigned long sensor_last_time = 0;
//++++++++++++++++++++++++++++++++++++
//Alarm
#define ALM_PIN 33
bool alarm_mute = true;
bool alarm_t = 0;
bool alarm_w = 0;
bool alarm_s = 0;
unsigned long alarm_delay = 500;
unsigned long alarm_last_time = 0;
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
enum Mode { outdoor, indoor, manual };
Mode current_mode = manual;
//The interval in which the mode display should be refreshed
unsigned long mode_delay = 0;//500;
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
String water_status = "";
//++++++++++++++++++++++++++++++++++++
#define PUMP_PIN 4
//pins tested: 0, 1, 2, 3, 4, 5, 6-11, 12, 13, 14, 15
//these are all output only pins, meaning they should be best suited
unsigned long max_pump_time = 500;
unsigned long pump_last_time = 0;
bool pump_checked = false;
//++++++++++++++++++++++++++++++++++++
unsigned long weather_forecast_delay = 1000 * 60 * 60 * 4;
unsigned long weather_forecast_last_time = 0;
unsigned long watering_delay = 1000 * 10;//60 * 60 * 2;
unsigned long watering_last_time = 0;
//++++++++++++++++++++++++++++++++++++
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "time.h"           //internet time
#include "elapsedMillis.h"  // repeat a task at regular intervals by checking if enough time has elapsed since the task
//make sure your WiFi is 2.4 GHz
const char* wifi_ssid = "";  // Enter SSID here 
const char* wifi_password = "";    //Enter Password here
const String location = "Vienna";            //Enter your City Name from api.weathermap.org
const String api_key = "a1c734b2aaa54624a1b93405230206"; // API key for weather data
//++++++++++++++++++++++++++++++++++++
//enter your GMT Time offset
#define GMT_OFFSET +2                   // vienna GMT: +02:00  //CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00
#define NPT_TIME_SERVER "pool.ntp.org"  //synchronized clocks
const long gmt_offset_sec = 3600 * GMT_OFFSET;
const int daylight_offset_sec = 3600 * 0;
byte hour, minute, second;
//++++++++++++++++++++++++++++++++++++
//ooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo
// time for weather data
struct tm timeinfo;
elapsedSeconds current_time;
elapsedSeconds get_time_data;
elapsedSeconds get_weather_data;
elapsedSeconds wait_period;
elapsedSeconds pump_delay;
//++++++++++++++++++++++++++++++++++++
// Weather forecast data
String json_data;
DynamicJsonDocument doc(24000);
DeserializationError error;
//++++++++++++++++++++++++++++++++++++
float rain_probability = 0;  // is the highest of 6hr chance of rain
bool daily_will_it_rain;  // will it rain today?
int watering_duration = 0;
float threshold = 60.0;  // SM change  threshold
float discrepancy_threshold = 15.00;  //sm diff value
float* sm_values;
int pump_on_count = 0;
int max_pump_on_count = 4;
//ooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo
//++++++++++++++++++++++++++++++++++++
//-----------------------------------------------------------------------------------------------------
//++++++++++++++++++++++++++++++++++++
//Adding more functions to the interrupts may result in a guru meditation error,
//as an ISR utilizes the processor fully
//This means that the processor wants to access certain ressources that are not free yet
//So after adding a function to the ISR, immediately test out your changes on the microcontroller
//to see if anything breaks
void IRAM_ATTR Action_BTN1(){
  if((millis() - btn1_last_debounce_time) > btn1_debounce_delay){
    if(btn1_pressed == false){
      if(digitalRead(BTN1_PIN) == HIGH){
        btn1_pressed = true;
        Serial_New_Line();
        Serial.print("Button 1 pressed");
        if(current_mode == manual){
          Change_PUMP(1);
          //pump_last_time = millis();
        }
      }
    }
    else if(btn1_pressed == true){
      if(digitalRead(BTN1_PIN) == LOW){
        btn1_pressed = false;
        Serial_New_Line();
        Serial.print("Button 1 released");
        if(current_mode == manual){
          Change_PUMP(0);
        }
        //Read_WL();
      }
    }
    btn1_last_debounce_time = millis();
  }
}
//++++++++++++++++++++++++++++++++++++
void IRAM_ATTR Action_BTN2(){
  if((millis() - btn2_last_debounce_time) > btn2_debounce_delay){
    if(digitalRead(BTN2_PIN) == HIGH){
      Serial_New_Line();
      //Serial.print("\n");
      Serial.print("Button 2 pressed");
      Change_MODE();
      Change_PUMP(0);
    }
    btn2_last_debounce_time = millis();
  }
}
//++++++++++++++++++++++++++++++++++++
void IRAM_ATTR Action_BTN3(){
  if((millis() - btn3_last_debounce_time) > btn3_debounce_delay){
    if(digitalRead(BTN3_PIN) == HIGH){
      Serial_New_Line();
      //Serial.print("\n");
      Serial.print("Button 3 pressed");
      //Change_ALM(0);
      alarm_mute = !alarm_mute;
      Serial_New_Line();
      if(alarm_mute == true){
        Serial.print("Changed alarm to: OFF");
      }
      else{
        Serial.print("Changed alarm to: ON");
      }
    }
    btn3_last_debounce_time = millis();
  }
}
//++++++++++++++++++++++++++++++++++++
//-----------------------------------------------------------------------------------------------------
void setup() {
  //Mostly needed when trying to find the address of the lcd display
  //Wire.begin();
  //Sets baud rate
  Serial.begin(115200);
  Init_LCD();
  /*
  Init_WIFI();
  //++++++++++++++++++++++++++++++++++++
  if(Check_WIFI() == true){
    // config internet time
    configTime(gmt_offset_sec, daylight_offset_sec, NPT_TIME_SERVER);
    // now get the time
    Get_TIME();
    // optional to print time to serial monitor
    Print_TIME();
    Get_Day_Forecast(rain_probability, daily_will_it_rain);
  }
  get_time_data = 0;
  get_weather_data = 0;
  wait_period = 0;
  */
  //++++++++++++++++++++++++++++++++++++
  //On setup, the pump gets set to LOW and initializes the DHT sensors, as they can get disrupted during pump usage
  Init_PUMP();
  //Init_DHT();
  Init_SM();
  Init_ALM();
  Init_WL();
  Init_PUMP();
  Init_BTN();
  //++++++++++++++++++++++++++++++++++++
  //Sets the sensor timer accordingly so the sensors measure right away with system start
  sensor_last_time = -(sensor_delay + 1);
  //mode_last_time = -(mode_delay + 1);
  alarm_last_time = -(alarm_delay + 1);
}
//-----------------------------------------------------------------------------------------------------
void loop() {
  if(digitalRead(PUMP_PIN) == LOW){
    pump_checked = false;
    //++++++++++++++++++++++++++++++++++++
    //Only needed when trying to find the address of the lcd display
    //Find_LCD_ADR();
    //++++++++++++++++++++++++++++++++++++
    /*
    // update the time every day, once we have internet time we'll use millis() to keep it updated
    if (GetTimeData > 86400) {  // 86400 reset the time every day
      GetTimeData = 0;
      GetInternetTime();
      //  pumpOnCount = 0; // Reset the pump activation count
    }
    */
    //++++++++++++++++++++++++++++++++++++
    //Reads and displays sensor data
    if((millis() - sensor_last_time) > sensor_delay){
      Serial_New_Line();
      Read_DHT();
      Serial.print("\n");
      Read_SM();
      Serial.print("\n");
      Read_WL();
      Serial.print("\n");
      Print_MODE();
      Serial.print("\n");
      //Monitor_ALM();
      Print_ALM();

      //Resets timer
      sensor_last_time = millis();
    }
    //++++++++++++++++++++++++++++++++++++
    if(current_mode == outdoor){
      if((millis() - weather_forecast_last_time) > weather_forecast_delay){
        if(Check_WIFI() == false){
          Init_WIFI();
        }
        if(Check_WIFI() == true){
          Get_Day_Forecast(rain_probability, daily_will_it_rain);
          //Resets timer
          weather_forecast_last_time = millis();
        }
        else{
          weather_forecast_last_time = millis() - weather_forecast_delay / 2;
        }
      }
    }
    //++++++++++++++++++++++++++++++++++++
    if((millis() - watering_last_time) > watering_delay){
      Serial_New_Line();
      Serial.print("Handling watering...");
      Handle_Watering();

      //Resets timer
      watering_last_time = millis();
    }
    //++++++++++++++++++++++++++++++++++++
    /*
    //Refreshes the displayed mode
    if((millis() - mode_last_time) > mode_delay){
      Print_MODE();
      
      //Resets timer
      mode_last_time = millis();
    }
    */
    //++++++++++++++++++++++++++++++++++++
    //Monitors alarm
    if((millis() - alarm_last_time) > alarm_delay){
      Monitor_ALM();
      
      //Resets timer
      alarm_last_time = millis();
    }
    //++++++++++++++++++++++++++++++++++++
  }
  else{
    //Sets the start time of when the pump was activated
    if(pump_checked == false)
    {
      pump_last_time = millis();
      pump_checked = true;
    }
    //Limits the pump so it can only stay on for a certain period of time continuously
    if((millis() - pump_last_time) > max_pump_time){
      Change_PUMP(0);
    }
  }
}
//-----------------------------------------------------------------------------------------------------
void Serial_New_Line(){
  Serial.print("\n");
  Serial.print("+---------------------------+");
  Serial.print("\n");
}
//-----------------------------------------------------------------------------------------------------
void Init_DHT(){
  //Checks how many dht sensors are in the dht array
  //Serial.print("Amount of dht sensors: ");
  dht_array_size = sizeof(dht_array)/sizeof(DHT);
  //Serial.print(dht_array_size);

  //Initializes all dht sensors in the dht array
  for(int i = 0; i < dht_array_size; i++)
  {
    //pinMode(dht_array[i], INPUT);
    dht_array[i].begin();
    /*
    Serial.print("\n");
    Serial.print("DHT sensor ");
    Serial.print(i + 1);
    Serial.print(" initialized successfully!");
    */  
  }
}
//-----------------------------------------------------------------------------------------------------
void Read_DHT(){
  //Stores the total sum of read sensor data (of one cycle)
  float total_h = 0;
  float total_t = 0;

  int adjusted_dht_array_size = dht_array_size;

  //Reads values from the dht sensors
  for(int i = 0; i < dht_array_size; i++)
  {
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
    if(!isnan(h) && !isnan(t)){
      total_h += h;
      total_t += t;
    }
    else{
      adjusted_dht_array_size--;
    }

    if(adjusted_dht_array_size > 1){
      Serial.print(i + 1);
      Serial.print(", ");
      Serial.print("Humidity: ");
      Serial.print(h);
      Serial.print(" %");
      Serial.print(", ");
      Serial.print("Temperature: ");
      Serial.print(t);
      Serial.print(" °C ||| ");
      //Serial.print("\n");
    }
  }
  
  //Calculates average values of the read sensor data (of one cycle)
  average_h = total_h/adjusted_dht_array_size;
  average_t = total_t/adjusted_dht_array_size;
  
  Write_LCD(0, 0, "H%");
  Write_LCD(2, 0, "   ");
  if(!isnan(average_h)){
    Write_LCD(2, 0, String(average_h, 0));
  }
  else{
    Write_LCD(2, 0, "ERR");
  }

  Write_LCD(0, 1, "T");
  //Write_LCD(2, 1, "C");
  Write_LCD(1, 1, String(char(223)));
  Write_LCD(2, 1, "   ");
  if(!isnan(average_t)){
    Write_LCD(2, 1, String(average_t, 0));
  }
  else{
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
void Init_SM(){
  //Checks how many soil moisture sensors are in the sm array
  //Serial.print("Amount of soil moisture sensors: ");
  sm_array_size = sizeof(sm_array)/sizeof(int);
  //Serial.print(sm_array_size);

  for(int i = 0; i < sm_array_size; i++)
  {
    pinMode(sm_array[i], INPUT);
  }
}
//-----------------------------------------------------------------------------------------------------
void Read_SM(){
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
  for(int i = 0; i < sm_array_size; i++)
  {
    float sm = Adjust_SM(analogRead(sm_array[i]));
    sm_array_values[i] = sm;
    /*
    Serial.print(sm_array_values[i]);
    Serial.print("\n");
    */
    if(!isnan(sm)){
      total_sm += sm;
    }
    else{
      adjusted_sm_array_size--;
    }
    
    if(adjusted_sm_array_size > 1){
      Serial.print(i + 1);
      Serial.print(", ");
      Serial.print("Soil moisture: ");
      Serial.print(sm);
      Serial.print(" % ||| ");
      //Serial.print("\n");
    }
  }

  //Calculates average values of the read sensor data (of one cycle)
  average_sm = total_sm/adjusted_sm_array_size;

  Write_LCD(6, 0, "S%");
  Write_LCD(8, 0, "   ");
  if(!isnan(average_sm)){
    Write_LCD(8, 0, String(average_sm, 0));
  }
  else{
    Write_LCD(8, 0, "ERR");
  }

  Serial.print("A, ");
  Serial.print("Soil moisture: ");
  Serial.print(average_sm);
  Serial.print(" %");
}
//-----------------------------------------------------------------------------------------------------
float Adjust_SM(float data){
  
  //When completely dry, the sensor returns 4095
  data = abs(data - 4095);

  //After using the absolute value, following values mean the following:
  //0 - completely dry
  //1555 - lowest possible value when 
  //1947 (~1950) - max value

  //If the datas value is still higher than 1950 even after adjustment, it means that the sensor is disconnected
  //If the sensor is clearly disconnected, but still returns something else than an error,
  //lower the threshold closer to 1850
  if(data > 1950){
    data = nan("");
    return data;
  }
  
  //Since 1555 is the lowest value possible when the sensor is in water,
  //meaning maximum moisture,
  //all values beyond 1555 do not have to be considered and can be clamped to 1555
  int clamp_value = 1950;//1555;

  if(data > clamp_value){
    data -= (data - clamp_value);
  }
  
  data = (data/clamp_value)*100;

  return data;
  
  //return ( 100 - ( ( data / 4095 ) * 100 ) );
}
//-----------------------------------------------------------------------------------------------------
void Init_BTN(){
  attachInterrupt(BTN1_PIN, Action_BTN1, CHANGE);
  attachInterrupt(BTN2_PIN, Action_BTN2, RISING);
  attachInterrupt(BTN3_PIN, Action_BTN3, RISING);
}
//-----------------------------------------------------------------------------------------------------
void Find_LCD_ADR(){
  byte error, address;
  int nDevices;
  Serial.println("Scanning...");
  nDevices = 0;
  for(address = 1; address < 127; address++ ) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address<16) {
        Serial.print("0");
      }
      Serial.println(address,HEX);
      nDevices++;
    }
    else if (error==4) {
      Serial.print("Unknow error at address 0x");
      if (address<16) {
        Serial.print("0");
      }
      Serial.println(address,HEX);
    }    
  }
  if (nDevices == 0) {
    Serial.println("No I2C devices found\n");
  }
  else {
    Serial.println("done\n");
  }
}
//-----------------------------------------------------------------------------------------------------
void Init_LCD(){
  //Initializes LCD
  lcd.init();
  //Turns on LCD backlight                      
  lcd.backlight();
}
//-----------------------------------------------------------------------------------------------------
void Write_LCD(int x, int y, String message){
  //LCD.clear();
  //Set cursor to x column, y row
  lcd.setCursor(x, y);
  //Print message on display
  lcd.print(message);
}
//-----------------------------------------------------------------------------------------------------
void Change_MODE(){
  Serial_New_Line();
  //Serial.print("\n");
  switch(current_mode){
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
void Print_MODE(){
  Write_LCD(12, 1, "M:");

  int x = 14;
  int y = 1;

  Serial.print("Current mode: ");

  switch(current_mode){
    case outdoor:
      Write_LCD(x, y, "OD");
      Serial.print("Outdoor");
    break;
    case indoor:
      Write_LCD(x, y, "ID");
      Serial.print("Indoor");
    break;
    case manual:
      Write_LCD(x, y, "MN");
      Serial.print("Manual");
    break;
  }

}
//-----------------------------------------------------------------------------------------------------
void Init_ALM(){
  pinMode(ALM_PIN, OUTPUT);
  //Print_ALM();
}
//-----------------------------------------------------------------------------------------------------
void Change_ALM(bool set){
  digitalWrite(ALM_PIN, set); //turn buzzer on
}
//-----------------------------------------------------------------------------------------------------
void Monitor_ALM(){
  if(water_status == "<25"){
    alarm_w = 1;
  }
  else{
    alarm_w = 0;
  }

  if(average_sm > 90 || average_sm < 30){
    alarm_s = 1;
  }
  else{
    alarm_s = 0;
  }

  bool alarm_t_check = false;
  for(int i = 0; i < dht_array_size; i++)
  {
    if(isnan(dht_array_values[i][0]) || isnan(dht_array_values[i][1]))
    {
      alarm_t_check = true;
    }
  }
  for(int i = 0; i < sm_array_size; i++)
  {
    if(isnan(sm_array_values[i]))
    {
      alarm_t_check = true;
    }
  }
  if(water_status == "ERR"){
    alarm_t_check = true;
  }
  if(alarm_t_check == true){
    alarm_t = 1;
  }
  else{
    alarm_t = 0;
  }

  if((alarm_t == 0 && alarm_w == 0 && alarm_s == 0) || alarm_mute == true){
    Change_ALM(0);
  }
  else if(alarm_mute == false){
    Change_ALM(1);
  }

  //Print_ALM();
}
//-----------------------------------------------------------------------------------------------------
void Print_ALM(){
  int x = 12;
  if(alarm_mute == false){Write_LCD(x, 0, "A"); Serial.print("A");}
  else{Write_LCD(x, 0, "a"); Serial.print("a");}
  x++;
  if(alarm_t == true){Write_LCD(x, 0, "T"); Serial.print("T");}
  else{Write_LCD(x, 0, "t"); Serial.print("t");}
  x++;
  if(alarm_w == true){Write_LCD(x, 0, "W"); Serial.print("W");}
  else{Write_LCD(x, 0, "w"); Serial.print("w");}
  x++;
  if(alarm_s == true){Write_LCD(x, 0, "S"); Serial.print("S");}
  else{Write_LCD(x, 0, "s"); Serial.print("s");}
}
//-----------------------------------------------------------------------------------------------------
void Init_WL(){
  pinMode(UPPER_WL_PIN, INPUT_PULLUP);
  pinMode(LOWER_WL_PIN, INPUT_PULLUP);
}
//-----------------------------------------------------------------------------------------------------
void Read_WL(){
  int upper_water_level = digitalRead(UPPER_WL_PIN);
  int lower_water_level = digitalRead(LOWER_WL_PIN);

  if(upper_water_level == !HIGH && lower_water_level == !HIGH){
    water_status = ">75";
  }
  else if(upper_water_level == !LOW && lower_water_level == !HIGH){
    //water_status = "~50";
    water_status = String(char(126)) + "50";
  }
  else if(upper_water_level == !LOW && lower_water_level == !LOW){
    water_status = "<25";
  }
  else{
    water_status = "ERR";
  }

  Write_LCD(6, 1, "L%");
  Write_LCD(8, 1, "   ");
  Write_LCD(8, 1, water_status);

  Serial.print("Water level: ");
  Serial.print(water_status);
  Serial.print(" %");
}
//-----------------------------------------------------------------------------------------------------
void Init_PUMP(){
  pinMode(PUMP_PIN, OUTPUT);
  Change_PUMP(0);
}
//-----------------------------------------------------------------------------------------------------
void Change_PUMP(bool set){
  digitalWrite(PUMP_PIN, set);
  //Serial_New_Line();
  if(set == true){
    //Serial.print("Pump started!");
  }
  else{
    //Serial.print("Pump stopped!");
    Init_DHT();
    delay(500);
  }
}
//-----------------------------------------------------------------------------------------------------
void Init_WIFI(){
  WiFi.mode(WIFI_STA);
  // Connect to Wi-Fi
  WiFi.begin(wifi_ssid, wifi_password);
  Serial_New_Line();
  Serial.print("Connecting to WiFi...");
  Write_LCD(0, 0, "Connecting WiFi!");

  float start_time = millis();
  int timeout = 20000; //equals 20 seconds

  while(1){
    if(WiFi.status() == WL_CONNECTED){
      Serial_New_Line();
      Serial.print("WiFi connection was established successfully!");
      Write_LCD(0, 0, "                ");
      Write_LCD(0, 0, "WiFi connected!");
      break;
    }
    if(millis() - start_time > timeout){
      Serial_New_Line();
      Serial.print("Connection timeout!");
      Serial.print("\n");
      Serial.print("WiFi connection was not established!");
      Write_LCD(0, 0, "                ");
      Write_LCD(0, 0, "WiFi failed!");
      break;
    }
  }

  delay(2500);
  Write_LCD(0, 0, "                ");
}
//-----------------------------------------------------------------------------------------------------
bool Check_WIFI(){
  Serial_New_Line();
  if(WiFi.status() == WL_CONNECTED){
    Serial.print("Connected to WiFi");
    return true;
  }
  else{
    Serial.print("Not connected to WiFi");
    return false;
  }
}

//ooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo
///*

//-----------------------------------------------------------------------------------------------------
//Function to handle/manage  watering based on the selected model
void Handle_Watering() {
  Serial_New_Line();
  /*
  bool below_threshold = Check_Sensor_Values(sm_values, sm_array_size);
  
  switch (current_mode) {
    case outdoor:
      if (below_threshold) {
        int watering_duration = Decide_Outdoor_Watering_Duration(sm_values, sm_array_size);
        Handle_Watering_Process(watering_duration);
      } else {
        Serial.println("Sufficient soil moisture in outdoor mode");
        //delay(3600000);  // Delay for remaining time of 1 hour (1 hr - 1 min)
      }
      break;

    case indoor:
      if (below_threshold) {
        int watering_duration = Decide_Indoor_Watering_Duration(sm_values, sm_array_size);
        Handle_Watering_Process(watering_duration);
      } else {
        Serial.println("Sufficient soil moisture in indoor mode");
        //delay(3600000);  // Delay for remaining time of 1 hour (1 hr - 1 min)
      }
      break;

    case manual:
      //  manual control.
      // Manual mode: Control irrigation based on button state
      // No need to control pump here, as it is handled by the interrupt
      Serial.println("Manual mode");
      //delay(1000);

      break;
    default:
      // Handle unsupported  model
      Serial.println("Invalid mode");
      //delay(1000);
      return;
  }
  */
}
//-----------------------------------------------------------------------------------------------------
bool Check_Sensor_Values(float* sm_values, int sm_array_size) {
  bool below_threshold = true;
  float max_sensor_value = sm_values[0];
  float min_sensor_value = sm_values[0];

  // Check if all sensor values are below the threshold
  for (int i = 0; i < sm_array_size; i++) {
    if (sm_values[i] >= threshold) {
      below_threshold = false;
      break;
    }

    // Update max and min sensor values
    if (sm_values[i] > max_sensor_value) {
      max_sensor_value = sm_values[i];
    }
    if (sm_values[i] < min_sensor_value) {
      min_sensor_value = sm_values[i];
    }
  }

  
  // Debug print statements
  //Serial.print("Max sensor value: ");
  //Serial.println(maxSensorValue);
  //Serial.print("Min sensor value: ");
  //Serial.println(minSensorValue);
  

  // Check if the discrepancy between sensors is greater than the discrepancy threshold
  if (max_sensor_value - min_sensor_value > discrepancy_threshold) {
    below_threshold = false;
    // Display sensor values and ask for sensor check
    for (int i = 0; i < sm_array_size; i++) {
      Serial.print("SM Sensor ");
      Serial.print(i + 1);
      Serial.print(" value: ");
      Serial.println(sm_values[i]);
    }
    Serial.println("Please check the sensors.");
  }

  return below_threshold;
}
//-----------------------------------------------------------------------------------------------------
// Decide watering duration based on sensor data
int Decide_Indoor_Watering_Duration(float* sm_values, int sm_array_size) {
  int watering_duration = 0;  // Default duration is 0 (no watering needed)

  bool below_threshold = Check_Sensor_Values(sm_values, sm_array_size);

  if (below_threshold) {
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
//-----------------------------------------------------------------------------------------------------
//Decide watering duration based on sensor data and weather forecast
int Decide_Outdoor_Watering_Duration(float* sm_values, int sm_array_size) {
  int watering_duration = 0;  // Default duration is 0 (no watering needed)

  bool below_threshold = Check_Sensor_Values(sm_values, sm_array_size);


  if (below_threshold) {
    if (sm_values[0] < 10) {
      watering_duration = 10000;  // Water for 10 seconds
    } else if (daily_will_it_rain && rain_probability < 50) {
      watering_duration = Decide_Indoor_Watering_Duration(sm_values, sm_array_size) / 2;
    } else if (!daily_will_it_rain) {
      watering_duration = Decide_Indoor_Watering_Duration(sm_values, sm_array_size);
    } else {
      // Wait for 3 hours before checking again
      watering_duration = -1;
      delay(300000);
    }
  }

  return watering_duration;
}
//-----------------------------------------------------------------------------------------------------
void Handle_Watering_Process(int watering_duration) {
  if (watering_duration > 0) {
    Serial.print("Watering ");
    Serial.print(current_mode);
    Serial.print(" mode for: ");
    Serial.print(watering_duration);
    float adjusted_watering_duration = watering_duration/max_pump_time;
    for(int i = 0; i < watering_duration; i+=adjusted_watering_duration){
      Change_PUMP(1);
      delay(adjusted_watering_duration);
      Change_PUMP(0);
      delay(500);
    }
    delay(watering_duration);
  } else {
    Serial.print("Sufficient soil moisture");
    //delay(60000);  // 1 minute delay
  }
}
//-----------------------------------------------------------------------------------------------------

//*/
//ooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo

void Get_TIME() {
  getLocalTime(&timeinfo);
  // here's where we convert internet time to an unsigned long, then use the ellapsed counter to keep it up to date
  // if the ESP restarts, we'll start with the the internet time
  // note CurrentTime is managed by the ellapsed second counter
  current_time = (timeinfo.tm_hour * 3600) + (timeinfo.tm_min * 60) + timeinfo.tm_sec;
  hour = timeinfo.tm_hour;
  minute = timeinfo.tm_min;
  second = timeinfo.tm_sec;
}
//-----------------------------------------------------------------------------------------------------
void Print_TIME() {
  Serial_New_Line();
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.print("Failed to obtain time");
  }

  // print the day of the week, date, month, and year:
  char buf[80];
  strftime(buf, sizeof(buf), "Day: %A, %B %d %Y", &timeinfo);
  Serial.print(buf);
  Serial.print("\n");

  // print the time:
  strftime(buf, sizeof(buf), "Time: %H:%M:%S", &timeinfo);
  Serial.print(buf);
}
//-----------------------------------------------------------------------------------------------------
void Get_Day_Forecast(float& rain_probability, bool& daily_will_it_rain) {
  Serial_New_Line();
  
  HTTPClient http;
  //?
  http.setTimeout(10000);

  float humidity = 0;
  float temperature = 0;
  rain_probability = 0;
  daily_will_it_rain = false;

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Weather forecast started");
    //Serial.println("WL_CONNECTED");

    // Construct the API URL
    String url = "https://api.weatherapi.com/v1/forecast.json?key=" + api_key + "&q=" + location + "&days=1";

    http.begin(url.c_str());
    int http_response_code = http.GET();

    Serial.print("\n");
    Serial.print("URL: ");
    Serial.print(url);
    Serial.print("\n");
    Serial.print("HTTP Response code: ");
    Serial.print(http_response_code);
    //Serial.print("\n");
    
    if (http_response_code > 200) {
      Serial.print("\n");
      Serial.print("Service unavailable!");
      http.end();
      return;
    }
    else if (http_response_code > 0) {
      json_data = http.getString();
      //Serial.print(json_data);
      //Serial.print("\n");
      error = deserializeJson(doc, json_data);

      /*
      Serial.print(F("deserializeJson() return code: "));
      Serial.print("\n");
      Serial.print(error.c_str());
      */

      rain_probability = Extract_Rain_Probability(doc);
      temperature = doc["current"]["temp_c"];
      humidity = doc["current"]["humidity"];
      daily_will_it_rain = Get_Daily_Rain(doc);
      http.end();
    }
  } 
  else {
    Serial.print("\n");
    Serial.print("Not connected to WiFi, thus the weather forecast could not be accessed!");
  }

  Serial_New_Line();
  Serial.print("Weather forecast data: ");
  Serial.print("\n");
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print(" °C");
  Serial.print("\n");
  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.print(" %");
  Serial.print("\n");
  String rainStatus = daily_will_it_rain ? "yes" : "no";
  Serial.print("Will it rain today?: ");
  Serial.print(rainStatus);
  if(rainStatus == "yes"){
    Serial.print("\n");
    Serial.print("Rain probability: ");
    Serial.print(rain_probability);
    Serial.print(" %");
  }
}
//-----------------------------------------------------------------------------------------------------
float Extract_Rain_Probability(JsonDocument& doc) {
  // Extract the rain probability from the forecast for the next 6 hrs
  JsonArray hour_data = doc["forecast"]["forecastday"][0]["hour"].as<JsonArray>();
  float rainHighestProbability = 0;

  int currentHour = hour;

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
bool Get_Daily_Rain(JsonDocument& doc) {
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