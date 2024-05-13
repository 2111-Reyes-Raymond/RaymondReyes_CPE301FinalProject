//Raymond Reyes
//CPE 301.1001
//May 10, 2024
//Final Project

#include <LiquidCrystal.h>
#include <Stepper.h>
#include <RTClib.h>
#include <DHT.h>

#define WATER_THRESHOLD 400
#define TEMP_THRESHOLD 10

//defining states for state machine
enum State{
  IDLE,
  DISABLED,
  RUNNING,
  ERROR
};

State currentState = DISABLED;
State prevState;

//Definitions
#define DHT_PIN 7
#define DHT_TYPE DHT11
#define STEPPER_PIN1 8
#define STEPPER_PIN2 9
#define STEPPER_PIN3 10
#define STEPPER_PIN4 11
#define FANMOTOR 0x10
#define RDA 0x80
#define TBE 0x20 
#define PCI 0x0C 

#define WATER_LEVEL 0

//Defining buttons and LEDs
#define RESET 0x04
#define STEPPER 0x08
#define CONTROL 0x02

#define GREEN_LED 0x80
#define YELLOW_LED 0X20
#define RED_LED 0x08
#define BLUE_LED 0x02

//UART Pointers
volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0  = (unsigned int *) 0x00C4;
volatile unsigned char *myUDR0   = (unsigned char *)0x00C6;

//ADC Pointers
volatile unsigned char* my_ADMUX = (unsigned char*) 0x7C;
volatile unsigned char* my_ADCSRB = (unsigned char*) 0x7B;
volatile unsigned char* my_ADCSRA = (unsigned char*) 0x7A;
volatile unsigned int* my_ADC_DATA = (unsigned int*) 0x78;

//GPIO Pointers
volatile unsigned char *portB     = 0x25;
volatile unsigned char *portDDRB  = 0x24;
volatile unsigned char *pinB      = 0x23;

volatile unsigned char *portC     = 0x28;
volatile unsigned char *portDDRC  = 0x27;
volatile unsigned char *pinC      = 0x26;

//interrupts
volatile unsigned char *myPCICR   = 0x68;
volatile unsigned char *myPCIFR   = 0x3B;
volatile unsigned char *myPCMSK0  = 0x6B;

#define BUTTON 0

RTC_DS3231 rtc;
DHT dht(DHT_PIN, DHT_TYPE);
const int RS = 11, EN = 12, D4 = 3, D5 = 4, D6 = 5, D7 = 6;
const int stepsPerRevolution = 2038;
Stepper myStepper(stepsPerRevolution, 22, 24, 26, 28);
char cooler_state[4][16] = {"DISABLED", "IDLE", "ERROR", "RUNNING"};
unsigned char state_led[4] = {YELLOW_LED, GREEN_LED, RED_LED, BLUE_LED};


LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);

float temperature = 0;
float humidity = 0;
int checkLED = -1;

bool fan = false;
bool tempHumidityState = false;
bool waterState = false;
bool stepperState = false;

void setup() {
  lcd.begin(16, 2);
  dht.begin();
  rtc.begin();
  rtc.adjust(DateTime(F(__DATE__),F(__TIME__)));
  io_init();
  adc_init();
  U0init(9600);

  myStepper.setSpeed(10);
}

void loop() {
  // put your main code here, to run repeatedly:
  DateTime now = rtc.now();

  if(tempHumidityState){
    readTempHum();
  }

  change_led(currentState);
  void updateState();

  if(*pinB & CONTROL){
    U0putchar('V');
    U0putchar('E');
    U0putchar('N');
    U0putchar('T');
    U0putchar(' ');
    U0putchar('U');
    U0putchar('P');
    U0putchar('D');
    U0putchar('A');
    U0putchar('T');
    U0putchar('E');
    U0putchar('D');
  }

  if(prevState != currentState){
    int month = now.month();
    int day = now.day();
    int year = now.year();
    int sec = now.second();
    int min = now.minute();
    int hour = now.hour();
    
    //Values for Date and Time
    int firstMonth = month % 10;
    int secondMonth = month / 10 % 10;
    int firstDay = day % 10;
    int secondDay = day / 10 % 10;
    int firstYear = year % 10;
    int secondYear = year / 10 % 10;
    int firstSec = sec % 10;
    int secondSec = sec / 10 % 10;
    int firstMin = min % 10;
    int secondMin = min / 10 % 10;
    int firstHour = hour % 10;
    int secondHour = hour / 10 % 10;
    char time_date[10] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};

    U0putchar(time_date[secondMonth]);
    U0putchar(time_date[firstMonth]);
    U0putchar(':');
    U0putchar(time_date[secondDay]);
    U0putchar(time_date[firstDay]);
    U0putchar(':');
    U0putchar(time_date[secondYear]);
    U0putchar(time_date[firstYear]);

    U0putchar(' ');

    U0putchar(time_date[secondHour]);
    U0putchar(time_date[firstHour]);
    U0putchar(':');
    U0putchar(time_date[secondMin]);
    U0putchar(time_date[firstMin]);
    U0putchar(':');
    U0putchar(time_date[secondSec]);
    U0putchar(time_date[firstSec]);

    U0putchar('\n');

    if(currentState == IDLE || currentState == RUNNING){
      char *buf;

      temperature = dht.readTemperature(true);
      humidity    = dht.readHumidity();

      snprintf(buf, 16, "Temperature:%d Humidity:%d", temperature, humidity);
    }
  }

  prevState = currentState;

}

void readTempHum(){
  temperature = dht.readTemperature(true);
  humidity    = dht.readHumidity();

}

void change_led(State states){
  *portC = state_led[states];
}

void updateState(){
  switch(currentState){
    case DISABLED:
      *portB &= ~FANMOTOR;
      break;
    case IDLE:
      *portB &= ~FANMOTOR;
      lcd.setCursor(0,0);
      lcd.print(temperature);
      lcd.print(humidity);
      if(dht.readTemperature(true) >= TEMP_THRESHOLD){
        currentState = RUNNING;
      }
      if(WATER_LEVEL < WATER_THRESHOLD){
        lcd.print("Low water level!");
        currentState = ERROR;
      }
      break;
    case ERROR:
      *portB &= ~FANMOTOR;
      lcd.setCursor(0, 0);
      lcd.print("Error");
      break;
    case RUNNING:
      lcd.setCursor(0, 0);
      lcd.print(temperature);
      lcd.print(humidity);
      *portB |= FANMOTOR;
      if(dht.readTemperature(true) < TEMP_THRESHOLD){
        currentState = IDLE;
        *portC &= ~BLUE_LED;
      }
      if(WATER_LEVEL < WATER_THRESHOLD){
        lcd.print("Low water level!");
        currentState = ERROR;
        *portC &= ~BLUE_LED;
      }
      break;
  }
}


//ISR Function
ISR(PCINT0_vect){
  if(*pinB & RESET){
    if(currentState == ERROR){
      currentState = IDLE;
    }
  }
  else if(*pinB & STEPPER){
    if(currentState == IDLE || currentState == RUNNING || currentState || ERROR){
      currentState = DISABLED;
    }
    else if(currentState == DISABLED){
      currentState = IDLE;
    }
  }
}

//GPIO Function
void io_init(){
  *portDDRC |= GREEN_LED;
  *portDDRC |= YELLOW_LED;
  *portDDRC |= RED_LED;
  *portDDRC |= BLUE_LED;
  *portDDRB |= FANMOTOR;
  *portB    |= RESET;
  *portDDRB |= ~(RESET);

  *portB    |= STEPPER;
  *portDDRB |= ~(STEPPER);

  *portB    |= CONTROL;
  *portDDRB |= ~(CONTROL);

  *myPCICR  |= 0x01;
  *myPCMSK0 |= PCI;
}

//ADC Functions
void adc_init()
{
  // setup the A register
  *my_ADCSRA |= 0b10000000; // set bit   7 to 1 to enable the ADC
  *my_ADCSRA &= 0b11011111; // clear bit 6 to 0 to disable the ADC trigger mode
  *my_ADCSRA &= 0b11110111; // clear bit 5 to 0 to disable the ADC interrupt
  *my_ADCSRA &= 0b11111000; // clear bit 0-2 to 0 to set prescaler selection to slow reading
  // setup the B register
  *my_ADCSRB &= 0b11110111; // clear bit 3 to 0 to reset the channel and gain bits
  *my_ADCSRB &= 0b11111000; // clear bit 2-0 to 0 to set free running mode
  // setup the MUX Register
  *my_ADMUX  &= 0b01111111; // clear bit 7 to 0 for AVCC analog reference
  *my_ADMUX  |= 0b01000000; // set bit   6 to 1 for AVCC analog reference
  *my_ADMUX  &= 0b11011111; // clear bit 5 to 0 for right adjust result
  *my_ADMUX  &= 0b11100000; // clear bit 4-0 to 0 to reset the channel and gain bits
}
unsigned int adc_read(unsigned char adc_channel_num)
{
  // clear the channel selection bits (MUX 4:0)
  *my_ADMUX  &= 0b11100000;
  // clear the channel selection bits (MUX 5)
  *my_ADCSRB &= 0b11110111;
  // set the channel number
  if(adc_channel_num > 7)
  {
    // set the channel selection bits, but remove the most significant bit (bit 3)
    adc_channel_num -= 8;
    // set MUX bit 5
    *my_ADCSRB |= 0b00001000;
  }
  // set the channel selection bits
  *my_ADMUX  += adc_channel_num;
  // set bit 6 of ADCSRA to 1 to start a conversion
  *my_ADCSRA |= 0x40;
  // wait for the conversion to complete
  while((*my_ADCSRA & 0x40) != 0);
  // return the result in the ADC data register
  return *my_ADC_DATA;
}

//UART Functions
void U0init(unsigned long U0baud)
{
 unsigned long FCPU = 16000000;
 unsigned int tbaud;
 tbaud = (FCPU / 16 / U0baud - 1);
 *myUCSR0A = 0x20;
 *myUCSR0B = 0x18;
 *myUCSR0C = 0x06;
 *myUBRR0  = tbaud;
}

unsigned char U0kbhit()
{
  return((*myUCSR0A) & RDA);
}

unsigned char U0getchar()
{
  return (*myUDR0);
}

void U0putchar(unsigned char U0pdata)
{
  while(!(TBE & *myUCSR0A));
  *myUDR0 = U0pdata;
}
