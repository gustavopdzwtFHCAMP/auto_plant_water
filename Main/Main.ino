//-----------------------------------------------------------------------------------------------------
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
DHT dht_array[] = {dht1, dht2};
int dht_array_size;
//++++++++++++++++++++++++++++++++++++
//Defines soil moisture sensor pins
#define SM1_PIN 35
#define SM2_PIN 34
int sm_array[] = {SM1_PIN, SM2_PIN};
int sm_array_size;
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
String waterStatus = "";
//++++++++++++++++++++++++++++++++++++
#define PUMP_PIN 15
//++++++++++++++++++++++++++++++++++++
//-----------------------------------------------------------------------------------------------------
//++++++++++++++++++++++++++++++++++++
void IRAM_ATTR Action_BTN1(){
  if((millis() - btn1_last_debounce_time) > btn1_debounce_delay){
    if(btn1_pressed == false){
      if(digitalRead(BTN1_PIN) == HIGH){
        btn1_pressed = true;
        Serial_NewLine();
        Serial.print("Button 1 pressed");
        Change_Alarm(1);
        Change_Pump(1);
      }
    }
    else if(btn1_pressed == true){
      if(digitalRead(BTN1_PIN) == LOW){
        btn1_pressed = false;
        Serial_NewLine();
        Serial.print("Button 1 released");
        Change_Alarm(0);
        Change_Pump(0);
      }
    }
    btn1_last_debounce_time = millis();
  }
}
//++++++++++++++++++++++++++++++++++++
void IRAM_ATTR Action_BTN2(){
  if((millis() - btn2_last_debounce_time) > btn2_debounce_delay){
    if(digitalRead(BTN2_PIN) == HIGH){
      Serial_NewLine();
      //Serial.print("\n");
      Serial.print("Button 2 pressed");
      Change_Mode();
    }
    btn2_last_debounce_time = millis();
  }
}
//++++++++++++++++++++++++++++++++++++
void IRAM_ATTR Action_BTN3(){
  if((millis() - btn3_last_debounce_time) > btn3_debounce_delay){
    if(digitalRead(BTN3_PIN) == HIGH){
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

  Serial_NewLine();
  Init_DHT();
  Serial_NewLine();
  Init_SM();
  
  Init_BTN();
  Init_ALM();
  Init_WL();
  Init_PUMP();

  Init_LCD();

  //Sets the sensor timer accordingly so the sensors measure right away with system start
  sensor_last_time = -(sensor_delay + 1);
}
//-----------------------------------------------------------------------------------------------------
void loop() {
  //Only needed when trying to find the address of the lcd display
  //Find_LCD_ADR();

  //Reads and displays sensor data
  if((millis() - sensor_last_time) > sensor_delay){
    Serial_NewLine();
    Read_DHT();
    Serial_NewLine();
    Read_SM();
    Serial_NewLine();
    Read_WL();

    //Resets timer
    sensor_last_time = millis();
  }

  //Refreshes the displayed mode
  if((millis() - mode_last_time) > mode_delay){
    Print_Mode();
    
    //Resets timer
    mode_last_time = millis();
  }
}
//-----------------------------------------------------------------------------------------------------
void Serial_NewLine(){
  Serial.print("\n");
  Serial.print("+---------------------------+");
  Serial.print("\n");
}
//-----------------------------------------------------------------------------------------------------
void Init_DHT(){
  //Checks how many dht sensors are in the dht array
  Serial.print("Amount of dht sensors: ");
  dht_array_size = sizeof(dht_array)/sizeof(DHT);
  Serial.print(dht_array_size);

  //Initializes all dht sensors in the dht array
  for(int i = 0; i < dht_array_size; i++)
  {
    //pinMode(dht_array[i], INPUT);
    dht_array[i].begin();
    Serial.print("\n");
    Serial.print("DHT sensor ");
    Serial.print(i + 1);
    Serial.print(" initialized successfully!");
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

    if(!isnan(h)){
      total_h += h;
    }
    if(!isnan(t)){
      total_t += t;
    }

    if(isnan(h) || isnan(t)){
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
  float average_h = total_h/adjusted_dht_array_size;
  float average_t = total_t/adjusted_dht_array_size;
  //average_h = 100;
  //average_t = 100;

  Write_LCD(0, 0, "H%");
  Write_LCD(2, 0, "   ");
  Write_LCD(2, 0, String(average_h, 0));

  Write_LCD(0, 1, "T");
  //Write_LCD(2, 1, "C");
  Write_LCD(1, 1, String(char(223)));
  Write_LCD(2, 1, "   ");
  Write_LCD(2, 1, String(average_t, 0));

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
  Serial.print("Amount of soil moisture sensors: ");
  sm_array_size = sizeof(sm_array)/sizeof(int);
  Serial.print(sm_array_size);

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
    
    if(!isnan(sm)){
      total_sm += sm;
    }
    else{
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
  float average_sm = total_sm/adjusted_sm_array_size;
  //average_sm = 100;

  Write_LCD(6, 0, "S%");
  Write_LCD(8, 0, "   ");
  Write_LCD(8, 0, String(average_sm, 0));

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
  
  //Since 1555 is the lowest value possible when the sensor is in water,
  //meaning maximum moisture,
  //all values beyond 1555 do not have to be considered and can be clamped to 1555
  int clamp_value = 1555;

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
void Change_Mode(){
  Serial_NewLine();
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
void Print_Mode(){
  int x = 14;
  int y = 1;

  switch(current_mode){
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
void Init_ALM(){
  pinMode(ALM_PIN, OUTPUT);
}
//-----------------------------------------------------------------------------------------------------
void Change_Alarm(bool set){
  digitalWrite(ALM_PIN, set); //turn buzzer on
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
    waterStatus = ">75";
  }
  else if(upper_water_level == !LOW && lower_water_level == !HIGH){
    //waterStatus = "~50";
    waterStatus = String(char(126)) + "50";
  }
  else if(upper_water_level == !LOW && lower_water_level == !LOW){
    waterStatus = "<25";
  }
  else
  {
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
void Init_PUMP(){
  pinMode(PUMP_PIN, OUTPUT);
  Change_Pump(0);
}
//-----------------------------------------------------------------------------------------------------
void Change_Pump(bool set){
  digitalWrite(PUMP_PIN, !set);
  Serial_NewLine();
  if(set == true){
    Serial.print("Pump started!");
  }
  else{
    Serial.print("Pump stopped!");
  }
}
//-----------------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------------------------------