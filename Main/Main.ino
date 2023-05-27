//-----------------------------------------------------------------------------------------------------
//#include "DHT.h"
#include <DHT.h>
#define DHTTYPE DHT11

//Defines dht sensor pins
#define DHT1_PIN 14
#define DHT2_PIN 27
#define DHT3_PIN 26
//Declares dht sensors
//DHT dht(DHTPIN, DHTTYPE);
DHT dht1(DHT1_PIN, DHT11);
DHT dht2(DHT2_PIN, DHT11);
DHT dht3(DHT3_PIN, DHT11);
DHT dht_array[] = {dht1, dht2, dht3};
int dht_array_size;

//Defines soil moisture sensor pins
#define SM1_PIN 25
#define SM2_PIN 33
#define SM3_PIN 32
int sm_array[] = {SM1_PIN};//, SM2_PIN, SM3_PIN};
int sm_array_size;

//Defines button pins
#define BTN1_PIN 35
#define BTN2_PIN 34
bool btn1_pressed = false;
//-----------------------------------------------------------------------------------------------------
void setup() {
  //Sets baud rate
  Serial.begin(115200);
  
  Serial_NewLine();
  Init_DHT();
  
  Serial_NewLine();
  Init_SM();

  pinMode(BTN1_PIN, INPUT);
}
//-----------------------------------------------------------------------------------------------------
void loop() {
  
  Serial_NewLine();
  Read_DHT();
  
  Serial_NewLine();
  Read_SM();

  /*
  int btn1_value = digitalRead(BTN1_PIN);
  
  if(btn1_value == HIGH)
  {
    if(btn1_pressed == false){
      btn1_pressed = true;
      Serial.println("Button pressed");
    }
  }
  else if(btn1_value == LOW)
  {  
    if(btn1_pressed == true){
      btn1_pressed = false;
      Serial.println("Button released");
    }
  }
  //This delay removes the problem of bouncing, which is when a button registers input multiple times
  //with only one press as it bounces on the contact of the breadboard
  delay(50);
  */

  delay(5000);
}
//-----------------------------------------------------------------------------------------------------
void Serial_NewLine(){
  Serial.print("+---------------------------+");
  Serial.print("\n");
}
//-----------------------------------------------------------------------------------------------------
void Init_DHT(){
  //Checks how many dht sensors are in the dht array
  Serial.print("Amount of DHT sensors: ");
  dht_array_size = sizeof(dht_array)/sizeof(DHT);
  Serial.print(dht_array_size);
  Serial.print("\n");

  //Initializes all dht sensors in the dht array
  for(int i = 0; i < dht_array_size; i++)
  {
    dht_array[i].begin();
    Serial.print("DHT sensor ");
    Serial.print(i + 1);
    Serial.print(" initialized successfully!");
    Serial.print("\n");
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

  Serial.print("A, ");
  Serial.print("Humidity: ");
  Serial.print(average_h);
  Serial.print(" %");
  Serial.print(", ");
  Serial.print("Temperature: ");
  Serial.print(average_t);
  Serial.print(" °C");
  Serial.print("\n");
}
//-----------------------------------------------------------------------------------------------------
void Init_SM(){
  //Checks how many soil moisture sensors are in the sm array
  Serial.print("Amount of Soil moisture sensors: ");
  sm_array_size = sizeof(sm_array)/sizeof(int);
  Serial.print(sm_array_size);
  Serial.print("\n");
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
    total_sm += sm;
    
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

  /*
  if(isnan(average_sm)){
    average_sm = 0;
  }
  */

  Serial.print("A, ");
  Serial.print("Soil moisture: ");
  Serial.print(average_sm);
  Serial.print(" %");
  Serial.print("\n");
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
}
//-----------------------------------------------------------------------------------------------------