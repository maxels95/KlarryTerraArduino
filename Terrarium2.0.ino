
#include <RTClib.h>
#include <Adafruit_NeoPixel.h>
#include <NewPing.h>
#include <DHT.h>
#include <DHT_U.h>
#include <TimeLib.h>
#include <Wire.h>
#include <LiquidCrystal.h>



//--------------------------------------------------------------------------
// PIN defenitions.
//--------------------------------------------------------------------------

#define DHTPIN_Warm  2
#define DHTPIN_Cold  3
#define DHTTYPE DHT11
#define Level_PIN_TRIG 5
#define Level_PIN_ECHO 4
#define LCD_PIN_RS  11
#define LCD_PIN_E 10
#define LCD_PIN_DB4 6
#define LCD_PIN_DB5 7
#define LCD_PIN_DB6 8
#define LCD_PIN_DB7 9
#define LED 12
#define NUMPIXELS 8
#define ResetButton 13

// Relays
#define HeatPanel A0
#define Rain A1
#define Light A2
#define Fan A3

//--------------------------------------------------------------------------
// Globala värden och defenitioner.
//--------------------------------------------------------------------------

// Array för relän
int RelayModule[] = { HeatPanel, Rain, Light, Fan };


// DHT sensorer
DHT dht_Warm(DHTPIN_Warm, DHTTYPE);
DHT dht_Cold(DHTPIN_Cold, DHTTYPE);

// Radar
NewPing Level(Level_PIN_TRIG, Level_PIN_ECHO);

// LCD
LiquidCrystal lcd(LCD_PIN_RS, LCD_PIN_E, LCD_PIN_DB4, LCD_PIN_DB5, LCD_PIN_DB6, LCD_PIN_DB7);

// RTC
 RTC_DS3231 rtc;
 byte hours;
 byte minutes;
 byte seconds;

// LED
Adafruit_NeoPixel pixels(NUMPIXELS, LED, NEO_GRB + NEO_KHZ800);

//--------------------------------------------------------------------------
// Variabler för vilkor
//--------------------------------------------------------------------------

// Temperatur och fuktighet gräns.
int maxHum = 80;
int minHum = 60;
bool heatSwitch = false; // ------------------------- bool för att skapa hysteres för värmeelement.
//int maxTemp1 = 26;
//int maxTemp2 = 35;
double distance;
double temp1;
double hum1;
double temp2;
double hum2;
double humidity;

// Timertider för regn och bylysning.
const int onLjusTimme = 7;
const int onLjusMin = 0;
const int offLjusTimme = 20;
const int offLjusMin = 0;

const int waterChangeHour = 19;
const int waterChangeMinute = 20;

const int regnMinut = 5;
const int onRegnTimme1 = 9;
const int onRegnTimme2 = 11;
const int onRegnTimme3 = 13;
const int onRegnTimme4 = 15;
const int onRegnTimme5 = 19;

// Array för regntimmar
int RainHours[] = {onRegnTimme1, onRegnTimme2, onRegnTimme3, onRegnTimme4, onRegnTimme5,};

// bool för att återställa vattenlarm och visa error
bool WaterChange = false;
bool WaterLevelError = false;
bool TemperatureError1 = false;
bool TemperatureError2 = false;
bool HumidityError1 = false;
bool HumidityError2 = false;
bool TimeError = false;
bool RelayHeat = false;
bool RelayHumidity = false;
bool RelayFan = false;

// timers
unsigned long FanBlockTimer;
unsigned long HeatCheck;
unsigned long RainCheck;
unsigned long FanCheck;

// temperatur/fukt för reläkontroller
double lastTemperature;
double lastHumidityRising;
double lastHumiditySinking;

//--------------------------------------------------------------------------
// Setup
//--------------------------------------------------------------------------

void setup() {
  
   // Starta komponenter
  Serial.begin(9600);
  Wire.begin();
  rtc.begin();
  dht_Warm.begin();
  dht_Cold.begin();
  pixels.begin();
  lcd.begin(16, 2);
  lcd.cursor();

   // Sätter läge på relän
  digitalWrite(HeatPanel, LOW);
  digitalWrite(Rain, LOW);
  digitalWrite(Light, LOW);
  digitalWrite(Fan, LOW);
  
  // Sätter relän som output 
  pinMode(HeatPanel, OUTPUT);
  pinMode(Rain, OUTPUT);
  pinMode(Light, OUTPUT);
  pinMode(Fan, OUTPUT);
  
  // Radar pins
  pinMode(Level_PIN_TRIG, OUTPUT);
  pinMode(Level_PIN_ECHO, INPUT);

  // sätter tiden.
  rtc.adjust(DateTime(2021, 12, 17, 16, 54, 0));

}

//--------------------------------------------------------------------------------------------------------------------------------
// Loop
//--------------------------------------------------------------------------------------------------------------------------------

void loop() {

  if(digitalRead(ResetButton) == 1)
  {
    Serial.println("---------------------------->> Button is pressed!");
    ResetAlarms();
  }

  LoopMethod();
  UpdateLCD(lcd);
}

//--------------------------------------------------------------------------
// Methods
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------------------------
// Water deaph meassure method
//--------------------------------------------------------------------------------------------------------------------------------

double GetWaterdeaph(){

  Serial.println("Messure water deapht");
  
  digitalWrite(Level_PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(Level_PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(Level_PIN_TRIG, LOW);
  
 const unsigned long duration = pulseIn(Level_PIN_ECHO, HIGH);
 distance = duration/29/2;

 Serial.print("Waterdeapth cm: ");
 Serial.println(distance);

 if(duration == 0 || distance > 20){
  WaterLevelError = true;
  LedChange();
  return distance;
 }
 
 delay(100);
 WaterLevelError = false;
 LedChange();
 return distance;
}

//--------------------------------------------------------------------------------------------------------------------------------
// Temperature meassure method
//--------------------------------------------------------------------------------------------------------------------------------

double GetTemperatureWarm(DHT dht){

  Serial.println("Messure High temp.");
  double temp = dht.readTemperature();
  
  if(isnan(temp)){
      Serial.println("High temp reading bad.");
      TemperatureError1 = true; 
      LedChange(); 
      return temp;
    }
  Serial.print("High temp: ");
  Serial.println(temp);
  
  delay(100);
  TemperatureError1 = false;
  LedChange();
  return temp;
}

double GetTemperatureCold(DHT dht){

  Serial.println("Messure low temp.");
  double temp = dht.readTemperature();
  
  if(isnan(temp)){
      Serial.println("Low temp reading bad.");
      TemperatureError2 = true;    
      LedChange(); 
      return temp;
  }

  Serial.print("Low temp: ");
  Serial.println(temp);

  delay(100);
  TemperatureError2 = false;
  LedChange();
  return temp;
}

//--------------------------------------------------------------------------------------------------------------------------------
// Humidity meassure method
//--------------------------------------------------------------------------------------------------------------------------------

double GetHumidity(DHT dht1, DHT dht2){

  double hum1 = dht1.readHumidity();
  String humMessage1 = "------------------------------->> höger fukt = ";
  humMessage1.concat(hum1);
  Serial.println(humMessage1);
  
  double hum2 = dht2.readHumidity();
  String humMessage2 = "------------------------------->> vänster fukt = ";
  humMessage2.concat(hum2);
  Serial.println(humMessage2);

  double humidity = (hum1 + hum2) / 2;

  if(isnan(hum1) && isnan(hum2)){
    HumidityError1 = true;
    HumidityError2 = true;
    LedChange();
    return humidity;
  }
  if(isnan(hum1)){
    HumidityError1 = true;
    humidity = hum2;
    LedChange();
    return humidity;
  }
  if(isnan(hum2)){
    HumidityError2 = true;
    humidity = hum1;
    LedChange();
    return humidity;
  }

  Serial.print("Humidity: ");
  Serial.println(humidity);
  
  delay(100);
  HumidityError1 = false;
  HumidityError2 = false;
  LedChange();
  return humidity;
}

//--------------------------------------------------------------------------------------------------------------------------------
// LCD update method
//--------------------------------------------------------------------------------------------------------------------------------

void UpdateLCD(LiquidCrystal lcd){

  Serial.println("Updating LCD");
  lcd.clear();
  Serial.println("LCD clear");
  lcd.setCursor(0, 0);

  // Temp 1
  if(TemperatureError1){
    lcd.print("ERROR");
  }
  else{
    lcd.print(temp1, 1); lcd.print("C");
  }

  // Temp 2
  if(TemperatureError2){
    lcd.setCursor(11, 0);
    lcd.print("ERROR");
  }
   else{
    lcd.setCursor(11, 0);
    lcd.print(temp2, 1); lcd.print("C");
  }

  // Fukt
  lcd.setCursor(0, 1);

  if(HumidityError1 || HumidityError2 || RelayHumidity){
    
   if(HumidityError1 && HumidityError2){
     
     lcd.print("ERROR");
    }
   else if(HumidityError1){
     lcd.print(humidity, 1); lcd.print("-E1");
   }
   else if(HumidityError2){
     lcd.print(humidity, 1); lcd.print("-E2");
   }
   else{
     lcd.print("RELAY");
   }
  }
  else{
      lcd.print(humidity);
  }

  // Tid
  if(TimeError){
    lcd.setCursor(11, 1);
    lcd.print("ERROR");
  }
  else{
    lcd.setCursor(11, 1);
    lcd.print(rtc.now().hour()); lcd.print(":"); lcd.print(rtc.now().minute());
  }
}

//--------------------------------------------------------------------------------------------------------------------------------
// LED colour changing method
//--------------------------------------------------------------------------------------------------------------------------------

  void LedChange()
  {
    Serial.println("Entering LedChange method.");

    if (rtc.now().hour() >= 8 && rtc.now().hour() < 22)
    {
         if(WaterChange == false && WaterLevelError == false
          && TemperatureError1 == false && TemperatureError2 == false
          && HumidityError1 == false && HumidityError2 == false 
          && TimeError == false && RelayHumidity == false)
         {
             for(int i = 0; i < NUMPIXELS; i++)
             {   
               pixels.setPixelColor(i, pixels.Color(127, 127, 127));
               pixels.show(); 
             }
         }
         else
         {
            for(int i = 0; i < NUMPIXELS; i++)
            {
              pixels.setPixelColor(i, pixels.Color(127, 0, 0));
              pixels.show(); 
            }
         }
    }

    else
    {
        pixels.clear();
        pixels.show(); 
    }
  }

//--------------------------------------------------------------------------------------------------------------------------------
// Reset alarms method
//--------------------------------------------------------------------------------------------------------------------------------

  void ResetAlarms(){

    Serial.println("Reset Alarms pressed.");
    
    WaterChange = false;
    TemperatureError1 = false;
    TemperatureError2 = false;
    HumidityError1 = false;
    HumidityError2 = false;
    RelayHumidity = false;
    RelayHumidity = false;

    if(distance <= 20){
      WaterLevelError = false;
    }

    LedChange();
  }

//--------------------------------------------------------------------------------------------------------------------------------
// Daylight time check method
//--------------------------------------------------------------------------------------------------------------------------------

  void DayLight()
    {
      Serial.println("Entering Daylight method.");
      
      if (rtc.now().hour() >= 7 && rtc.now().hour() < 20)
       {
        Serial.println("Day");
         digitalWrite(Light, HIGH);
       }
       
      else
       {
        Serial.println("Night");
         digitalWrite(Light, LOW);
       }
    }

//--------------------------------------------------------------------------------------------------------------------------------
// Rain method, increase humidity
//--------------------------------------------------------------------------------------------------------------------------------

 void RainTimer(int Rainhours[], double humidity, double waterDeaph)
  {
    Serial.println("Rain method started.");
    bool matchHour = false;
    bool matchMinute = false;

    Serial.println("Checking hours");
    for(int i = 0; i < 5; i++){
      if(rtc.now().hour() == Rainhours[i]){
        matchHour = true;
      }
    }

    Serial.println("Checking minutes");
    if(rtc.now().minute() == regnMinut || rtc.now().minute() <= (regnMinut + 3)){
      matchMinute = true;
    }

    Serial.println("should it rain? ");
    if (matchHour == true && matchMinute == true && humidity < minHum && waterDeaph <= 20)
     {
       Serial.println("Raining");
       digitalWrite(Rain, HIGH);
       // Regna i 1 minuter.
       delay(60000);
       digitalWrite(Rain, LOW);

       // starta räknare för fläkt.
       FanBlockTimer = 0;
       FanBlockTimer = millis();

       // starta räknare för reläkontroll.
       RainCheck = 0;
       RainCheck = millis();
       lastHumidityRising = humidity;
       RelayHumidity = false;
     }  
     else
     {
        if(RainCheck > 3600000 && lastHumidityRising <= humidity)
        {
          RainCheck = 0;
          RelayHumidity = true;
          LedChange();
        }
        digitalWrite(Rain, LOW);
     }

     delay(3000);
  }
  
  // ----------------------------------------------------------------------------------------------
  // HeatPanel method, Heat control
  // ----------------------------------------------------------------------------------------------
  
  void HeatOnOff(double tempWarm, double tempCold){

      Serial.println("HeatOnOff started.");
  
      if(rtc.now().hour() >= 7 && rtc.now().hour() < 20) // --------- kolla om det är dag och hålla varmare temp.
      {
             if(TemperatureError1)
             {
                if(tempCold >= 26)
                {
                    heatSwitch = true;
                }
                else if(tempCold <= 24)
                {
                    heatSwitch = false;
                }
             }

             else
             {
                if(tempWarm >= 30)
                {
                    heatSwitch = true;
                }
                else if(tempWarm <= 27)
                {
                    heatSwitch = false;
                }
             }

             if(heatSwitch)
             {
                digitalWrite(HeatPanel, LOW);
             }
             else
             {
                digitalWrite(HeatPanel, HIGH);
             }
       }

      else // ---------------------------- om det inte är dag håll svalare temp.
      {
           if(TemperatureError1)
             {
                if(tempCold >= 22)
                {
                    heatSwitch = true;
                }
                else if(tempCold <= 21)
                {
                    heatSwitch = false;
                }
             }

             else
             {
                if(tempWarm >= 25)
                {
                    heatSwitch = true;
                }
                else if(tempWarm <= 23)
                {
                    heatSwitch = false;
                }
             }

             if(heatSwitch)
             {
                digitalWrite(HeatPanel, LOW);
             }
             else
             {
                digitalWrite(HeatPanel, HIGH);
             }   
         }
  }

//--------------------------------------------------------------------------------------------------------------------------------
// Fan method for decreasing humidity
//--------------------------------------------------------------------------------------------------------------------------------

  void FanOnOff(double humidity, double temperatureLow, double temperatureHigh){

    Serial.println("FanOnOff started.");
    if(HumidityError1 || TemperatureError2 || TemperatureError2){
      digitalWrite(Fan, LOW);
    }

          if(FanBlockTimer < 1800000){
            digitalWrite(Fan, LOW);
          }
          
          else if(humidity > maxHum || temperatureLow > 25 || temperatureHigh > 35){
            digitalWrite(Fan, HIGH);
            FanBlockTimer = 0;
          }
    
    else{
      digitalWrite(Fan, LOW);
    }
  }

//--------------------------------------------------------------------------------------------------------------------------------
// WaterChange reminder method
//--------------------------------------------------------------------------------------------------------------------------------
  bool CheckWaterChangeTimer()
  {
      if(rtc.now().hour() == waterChangeHour && rtc.now().minute() == waterChangeMinute)
      {
          WaterChange = true;
      }
      
      return WaterChange;
  }

//--------------------------------------------------------------------------------------------------------------------------------
// LoopMethod
//--------------------------------------------------------------------------------------------------------------------------------

  void LoopMethod(){
    
    Serial.println("LoopMethod started");

    temp1 = GetTemperatureWarm(dht_Warm);

    temp2 = GetTemperatureCold(dht_Cold);
     
    delay(5000);

    humidity = GetHumidity(dht_Cold, dht_Warm);

    distance = GetWaterdeaph();

    RainTimer(RainHours, humidity, distance);
  
    HeatOnOff(temp1, temp2);

    FanOnOff(humidity, temp2, temp1);

    DayLight();
    
    CheckWaterChangeTimer();

    Serial.println("LoopMethod done.");
  }
 
