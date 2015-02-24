/* 
Purpose:
  Provide standard digital clock
    receive user input to set clock
    RGB-backlight control
      upgrade: replace contrast-adjust potentiometer w/ a digital potentiometer. Uses SPI. 128 or 256 bit resolution.
    upgrade to periodic GPS time-sync
  Provide alarm clock
    receive user input to set alarm(s)
    upgrade to set day-of-the-week alarms
    use the alarms in the RTC if possible, because it has a good battery backup (very low current draw).
    create custom bitmap character for showing that the alarm is set. The traditional "bell" icon would be perfect
  Operate the integrated lamp, via a power mosfet via PWM output.
    integrate the lamp with the wakeup alarm
    upgrade to switching regulator.
    upgrade to sleep function
Components
  Arduino Mega 2560 for development platform
    migrate to smaller (cheaper) unit later
  16x2 tri-color LCD from Adafruit
    using LiquidCrystal.h
  ChronoDot 2.1 precision RTC
    based on DS3231 chip
  operator interface (pushbuttons)
    To set the time, adjust time zone, daylight savings, whatnot
    And set alarms, adjust lamp brightness
    TBD: how many buttons are needed? they come in a 10-pack
  
  day 1 should equal sunday
  
Borrowed from ChronoDot example code, using RTClib as guidance.
*/

#include <Wire.h>;
#include <LiquidCrystal.h>;

// initialize the LCD library with the correct PWM pin numbers
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);

// establish our RTC variables here so they're universally accessible
int seconds;
int minutes;
int hours;
int weekday;
int day;
int month;
int year;
int tempWhole; // temperature in degrees C. Whole numbers.
int tempPart; // fractions of a degree C

//LED backlighting for LCD uses shared anode, needs a 0V sink. 255 = off, 0 = on full.
int red = 200; //is pin 6
int green = 205; //is pin 5
int blue = 210; //is pin 4
int lamp = 0; //pin 2. drives a mosfet for dimming the lamp.

// Utility stuff
boolean refresh = true; //used to call an extra clock-screen refresh
boolean alarmSet = true; //Is there an alarm set?
int menuSelect = 0; //keep track of which menu we're on

//define the pin numbers for our interface buttons
const int menuButton = 22;
const int incButton = 24;
const int decButton = 26;

void setup() {
  Wire.begin(); //this is the 2-wire interface protocol.
  Serial.begin(9600);
  lcd.begin(16, 2);   // set up the LCD's number of columns and rows
  
  pinMode(menuButton, INPUT);
  pinMode(incButton, INPUT);
  pinMode(decButton, INPUT);
  
// One-time setup of clock; only needs to happen if clock is reset
/*
  Wire.beginTransmission(0x68); // I2C address of DS3231
  Wire.write(0x0E); // select Control register
  Wire.write(0b00000000); // write register bitmap: 1Hz square wave, no alarms, oscillator will run on battery, square wave output will not run on battery
  Wire.endTransmission();
*/
/*Set the status register / aux controls
  Wire.beginTransmission(0x68); // I2C address of DS3231
  Wire.write(0x0F); // select Status register
  Wire.write(0b00000000); // write register bitmap: reset Oscillator Stop Flag, disable 32khz output, clear alarms
  Wire.endTransmission();
*/
/*Set the year, day, etc:
  Wire.beginTransmission(0x68); // 0x68 is DS3231 (chronodot) device address
  Wire.write(0x00); // start at defined register ('seconds' shown)
  // First byte: set seconds to "0", minutes to "23". Second byte: set hours to "22", 24-hour clock mode; set day to "2"
  Wire.write(0b00000000); // transmit the data. add more write bytes to write to multiple sequential registers at once
  Wire.write(0b00100011);
  Wire.write(0b00100010);
  Wire.write(0b00000010);
  Wire.endTransmission();
  */
}


void loop() {
  if (menuSelect > 0){
    menu(); //enter the menu
  }
  else {
    getRTC();
    if(seconds == 0 || refresh == true) { // the screen only needs to be updated once per minute, or when we ask, and not when we're in the menu
      getTemp();
      digitalclock();
      refresh == false;
    }
  }
  //set the screen color
  analogWrite(6, red);
  analogWrite(5, green);
  analogWrite(4, blue);
delay(100);
}


void getRTC(){ // query the time from the RTC, translate it to useful values
  Wire.beginTransmission(0x68); // 0x68 is DS3231 (chronodot) device address
  Wire.write(0x00); // start at register 0
  Wire.endTransmission();
  Wire.requestFrom(0x68, 7); // request seven registers (seconds, minutes, hours, day of week, day of month, month, year)
 
  while(Wire.available()) //receive the data, place each sequential byte into a variable
  { 
    seconds = Wire.read();
    minutes = Wire.read();
    hours = Wire.read(); 
    weekday = Wire.read();
    day = Wire.read();
    month = Wire.read();
    year = Wire.read();
   }
   // digits 1-10 are relayed as nibbles, two nibbles per byte. This makes sense for relaying digits to a traditional digital clock display, but not for actually understanding the numbers. So, we reinterpret the separate digits as one decimal integer:
    seconds = (((seconds & 0b01110000)>>4)*10 + (seconds & 0b00001111)); // convert BCD to decimal
    minutes = (((minutes & 0b01110000)>>4)*10 + (minutes & 0b00001111)); // convert BCD to decimal
    hours = (((hours & 0b00110000)>>4)*10 + (hours & 0b00001111)); // convert BCD to decimal (24 hour mode)
    day = (((day & 0b00110000)>>4)*10 + (day & 0b00001111)); // convert BCD to decimal
    year = (2000 + ((month & 0b10000000)>>7)*100 + ((year & 0b11110000)>>4)*10 + (year & 0b00001111)); // Year "00" is 2000. There is leapyear compensation through 2100. century is indicated by the Month's MSB when 'year' rolls past 99
    month = (((month & 0b00010000)>>4)*10 + (month & 0b00001111)); // convert BCD to decimal
}


void getTemp(){ //query the RTC for its internal temperature data  
  Wire.beginTransmission(0x68); // 0x68 is DS3231 (chronodot) device address
  Wire.write(0x11); // start at register 11
  Wire.endTransmission();
  Wire.requestFrom(0x68, 2); //ask for two bytes; temperature is stored in 10 bits

  while(Wire.available()) //receive the data, place each sequential byte into a variable
  { 
    tempWhole = Wire.read(); // first bit is the sign
    tempPart = Wire.read(); // this is the decimal portion, a two-bit fractional (so, quarters of a degree) starting at the MSB.
  }
    tempPart = (((tempPart & 0b10000000)>>6) * 5 / 2); //store decimal as an integer. truncate to half-degrees by ignoring the "one" bit.
}


void menu(){ // Where we change settings. This is the most ambitious section.
  lcd.clear();
  lcd.blink();
  switch (menuSelect){
   case 1: //adjust display brightness
   
   break;
   case 2:
   
   break;
   default:
      lcd.print("error: no menu"); 
  }
}
  
  
void digitalclock(){  // digital clock display of the time, date, and such. this is the "home screen"
  // set the cursor to column 0, line 1
  // (note: line 1 is the second row, since counting begins with 0):
  lcd.clear();
  if (hours < 10) lcd.print(" ");
  lcd.print(hours); 
  lcd.print(":");
  if (minutes < 10) lcd.print("0");
  lcd.print(minutes);
  
  lcd.setCursor(7,0);
  if (alarmSet == true) lcd.print("SET");
 
  lcd.setCursor(11,0); //print Temperature at the end of the first line
    lcd.print(tempWhole);
    lcd.print(".");
    lcd.print(tempPart);
    lcd.print("C");
    
  lcd.setCursor(0,1);  //new line
  switch (weekday) { //translate numeric day to text day
    case 1:
      lcd.print("Sun");
      break;
    case 2:
      lcd.print("Mon");
      break;
    case 3:
      lcd.print("Tues");
      break;
    case 4:
      lcd.print("Wednes");
      break;
    case 5:
      lcd.print("Thurs");
      break;
    case 6:
      lcd.print("Fri");
      break;
    case 7:
      lcd.print("Satur");
      break;
    default:
      lcd.print("err");
  }
  lcd.print(" ");
  if (day < 10) lcd.print("0");
  lcd.print(day);
  lcd.print(" ");
  switch (month) { //translate numeric month to text month
    case 1:
      lcd.print("Jan");
      break;
    case 2:
      lcd.print("Feb");
      break;
    case 3:
      lcd.print("Mar");
      break;
    case 4:
      lcd.print("Apr");
      break;
    case 5:
      lcd.print("May");
      break;
    case 6:
      lcd.print("Jun");
      break;
    case 7:
      lcd.print("Jul");
      break;
    case 8:
      lcd.print("Aug");
      break;
    case 9:
      lcd.print("Sep");
      break;
    case 10:
      lcd.print("Oct");
      break;
    case 11:
      lcd.print("Nov");
      break;
    case 12:
      lcd.print("Dec");
      break;
    default:
      lcd.print("err");
  }
  lcd.print(" ");
  lcd.print(year);
}
