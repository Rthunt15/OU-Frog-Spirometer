/*                                                                                      
 __         _                               _                 _____            _    _                         _  _   
/ _\ _ __  (_) _ __  ___   _ __ ___    ___ | |_  ___  _ __   /__   \ ___  ___ | |_ (_) _ __    __ _   /\   /\| || |  
\ \ | '_ \ | || '__|/ _ \ | '_ ` _ \  / _ \| __|/ _ \| '__|    / /\// _ \/ __|| __|| || '_ \  / _` |  \ \ / /| || |_ 
_\ \| |_) || || |  | (_) || | | | | ||  __/| |_|  __/| |      / /  |  __/\__ \| |_ | || | | || (_| |   \ V / |__   _|
\__/| .__/ |_||_|   \___/ |_| |_| |_| \___| \__|\___||_|      \/    \___||___/ \__||_||_| |_| \__, |    \_/     |_|  
    |_|                                                                                       |___/                  
*/

/*________________________________________________                                
   __   _  _                         _            
  / /  (_)| |__   _ __   __ _  _ __ (_)  ___  ___ 
 / /   | || '_ \ | '__| / _` || '__|| | / _ \/ __|
/ /___ | || |_) || |   | (_| || |   | ||  __/\__ \
\____/ |_||_.__/ |_|    \__,_||_|   |_| \___||___/                                                                                   
*/                                   

//SD card library (included in Arduino IDE)
//SD card library requires digital pins 10, 11, 12, 13
#include <SD.h>

//I2C communications library (included in Arduino IDE)
#include <Wire.h>

//Real-time clock library (uses Wire.h, from https://github.com/adafruit/RTClib)
//Set RTC clock via https://learn.adafruit.com/adafruit-pcf8523-real-time-clock/rtc-with-circuitpython?view=all#usage)
#include <RTClib.h>

// Software serial library for communicating with the LCD
#include <SoftwareSerial.h>

// Define the pin for software serial TX
const int txPin = 8;

// Set up a new serial connection for the LCD (only TX)
SoftwareSerial lcdSerial(-1, txPin); // RX pin set to -1 since it's not used

// Empty data logger file
File logFile;

// Real Time Clock object
RTC_Millis rtc;

// Minimum time for applied voltage to flow sensor before readings are valid (30ms)
const int warmupFlowTime = 30;

// Minimum time for applied voltage to O2 sensor before readings are valid (20min = 1200000ms = 300000ms * 4 breaks)
const long warmupO2Time = 300000;

// Interval between readings (ms)
const int readingInterval = 0;

// Empty variable for analog readings
int flowAnalog;
int o2Analog;

// Empty variables for time (milliseconds)
unsigned long startTime = millis(); // Logger start time since power on
unsigned long readingTime; // Time of each reading
unsigned long endTime; // Logger stop time since power on

// Debouncing of button variables
int buttonVal = 0; // Store current button state
int buttonOldVal = 0; // Store previous button state
int buttonState = 0; // 0 = 'dormant state', 1 = 'data logging state'

// Previous values for display comparison
float prevFlowRate = -1;
float prevO2Percent = -1;

// Button input
int startSwitchPin = 5;

// Flow sensor voltage supply
int flowSupplyPin = 4;

// O2 sensor voltage supply
int o2SupplyPin = 3;

// LEDs
int indicatorLedPin = 8;
int powerLedPin = 6;

// SD detector pin
int SdPresencePin = 10;

void setup(){
    // Start serial communication through at 115200 baud (bits/s)
    Serial.begin(115200);
    
    // Initialize software serial for LCD at 9600 baud
    lcdSerial.begin(9600);

    // Set led pins to output and off
    pinMode(indicatorLedPin, OUTPUT);
    digitalWrite(indicatorLedPin, LOW);
    pinMode(powerLedPin, OUTPUT);
    digitalWrite(powerLedPin, LOW);
    
    // Set button pin to input
    pinMode(startSwitchPin, INPUT);
    
    // Set sensor pins to input
    pinMode(A2, INPUT);
    pinMode(A3, INPUT);
    
    // Set sensor voltage supply pins to output
    pinMode(flowSupplyPin, OUTPUT);
    pinMode(o2SupplyPin, OUTPUT);
    
    // Initialize RTC
    rtc.begin(DateTime(F(__DATE__), F(__TIME__)));
    
    // Initialize SD card
    Serial.println("Initializing SD card...");
    pinMode(SdPresencePin, OUTPUT);
    
    // SD card detection
    if (!SD.begin(SdPresencePin)){
        Serial.println("No SD Card.");
        delay(100);
    } else {
        Serial.println("SD Card Ready.");
    }
    
    // Wait before starting main loop
    delay(150);
    
    Serial.println("Waiting for input...");
}

void newFile(){
    // Create new data file
    char fileName[] = "logger00.csv";
    
    // Loop to create multiple new files
    for (uint8_t i = 0; i < 100; i++){
        fileName[6] = i/10 + '0';
        fileName[7] = i%10 + '0';
        
        // Open new file only if it doesn't exist
        if (!SD.exists(fileName)){
            logFile = SD.open(fileName, FILE_WRITE);
            break;
        }
    }
    // Check for logger file
    if (!logFile){
        Serial.println("Could not create file");
    } else if (logFile){
        Serial.print("Logging to: ");
        Serial.println(fileName);
        
        // Write new header if file exists
        logFile.println("Date,ReadTime,Millis,FlowAnalog,FlowVoltage,FlowRate,O2Analog,O2Voltage,O2Percent");
    }
}

void loop(){
    // Read and store button input
    buttonVal = digitalRead(startSwitchPin);
    
    // Check for a transition in button
    if ((buttonVal == HIGH) && (buttonOldVal == LOW)){
        // Change state
        buttonState = 1 - buttonState;
        
        // Create and open new data file
        newFile();
        
        // Enter 'data logger state'
        Serial.println("Logging initiated");
        
        // Wait for minimum warmup time to equilibrate the o2 sensor
        digitalWrite(o2SupplyPin, HIGH);
        digitalWrite(flowSupplyPin, HIGH);
        delay(warmupFlowTime);
        
        // Start Time (rtc reading)
        DateTime now = rtc.now();
        
        // Date
        logFile.print(now.month(), DEC);
        logFile.print('/');
        logFile.print(now.day(), DEC);
        logFile.print('/');
        logFile.print(now.year(), DEC);
        logFile.print(',');
        // Time
        logFile.print(now.hour(), DEC);
        logFile.print(':');
        logFile.print(now.minute(), DEC);
        logFile.print(':');
        logFile.print(now.second(), DEC);
        logFile.print(',');
        // Empty readings
        logFile.println(" , , , , , , ");
        
        delay(100);
        
    }
    if ((buttonVal == LOW) && (buttonOldVal == HIGH)){
        // Change state
        buttonState = 1 - buttonState;
        
        // End time (rtc reading)
        DateTime now = rtc.now();
        
        // Date
        logFile.print(now.month(), DEC);
        logFile.print('/');
        logFile.print(now.day(), DEC);
        logFile.print('/');
        logFile.print(now.year(), DEC);
        logFile.print(',');
        // Time
        logFile.print(now.hour(), DEC);
        logFile.print(':');
        logFile.print(now.minute(), DEC);
        logFile.print(':');
        logFile.print(now.second(), DEC);
        logFile.print(',');
        // Empty readings
        logFile.println(" , , , , , , ");
        
        // Close data file.
        logFile.close();
    }
    
    // Button value is old, store it for comparison
    buttonOldVal = buttonVal;
    
    if (buttonState == 1){
        // Indicator (green) LED solid during data collection
        digitalWrite(indicatorLedPin, HIGH);
        
        // Read voltage from sensors
        flowAnalog = analogRead(A3);
        o2Analog = analogRead(A2);
        readingTime = millis();
        
        Serial.print("Reading Time (ms)");
        Serial.print(readingTime);
        Serial.print("\t");
        
        Serial.print("Flow Analog Read: ");
        Serial.print(flowAnalog);
        Serial.print("\t");
        
        // Calculate flow voltage from reading [analog * sensor reference voltage]/1023
        float flowVolt = flowAnalog * 5.0;
        flowVolt /= 1023.0;
        
        Serial.print("Flow Voltage: ");
        Serial.print(flowVolt);
        Serial.print("\t");
        
        // Calculate air flow rate from voltage in mL/minute for MEMS Flow Sensor D6F-P0001A1 (Bufo breathalyzer 2.0 - 01.31.2024)
        float flowRate = 50 * flowVolt - 25;
        
        Serial.print("Flow Rate: ");
        Serial.print(flowRate);
        Serial.print("\t");
        
        Serial.print("O2 Analog Read: ");
        Serial.print(o2Analog);
        Serial.print("\t");
        
        // Calculate o2 voltage from reading [analog * sensor reference voltage]/1023
        float o2Volt = o2Analog * 5.0;
        o2Volt /= 1023.0;
        
        Serial.print("O2 Voltage: ");
        Serial.print(o2Volt);
        Serial.print("\t");
        
        // Calculate o2 percent from voltage for Alphasense oxygen sensor (Bufo breathalyzer 2.0 - 01.31.2024)
        float o2Percent = ((o2Volt * 0.21)/2.0) * 100;
        
        Serial.print("O2 Percent: ");
        Serial.println(o2Percent);
        
        // Print and log date
        DateTime now = rtc.now();
        
        // Date
        logFile.print(now.month(), DEC);
        logFile.print('/');
        logFile.print(now.day(), DEC);
        logFile.print('/');
        logFile.print(now.year(), DEC);
        logFile.print(',');
        // Time
        logFile.print(now.hour(), DEC);
        logFile.print(':');
        logFile.print(now.minute(), DEC);
        logFile.print(':');
        logFile.print(now.second(), DEC);
        logFile.print(',');
        
        // Reading time in milliseconds
        logFile.print(readingTime);
        logFile.print(",");
        
        // Flow analog reading
        logFile.print(flowAnalog);
        logFile.print(",");
        
        // Flow voltage
        logFile.print(flowVolt);
        logFile.print(",");
        
        // Flow rate
        logFile.print(flowRate);
        logFile.print(",");
        
        // O2 analog reading
        logFile.print(o2Analog);
        logFile.print(",");
        
        // O2 voltage
        logFile.print(o2Volt);
        logFile.print(",");
        
        // O2 percentage
        logFile.println(o2Percent);
        
        // Update LCD only if values have changed
        if (flowRate != prevFlowRate || o2Percent != prevO2Percent) {
            updateLCD(flowRate, o2Percent);

            // Update previous values
            prevFlowRate = flowRate;
            prevO2Percent = o2Percent;
        }
        
        // Delay between readings
        delay(readingInterval);
        
    } else if (buttonState == 0){
        // Indicator (green) LED blink during dormant state
        digitalWrite(indicatorLedPin, LOW);
    }
}

// Function to update the LCD display
void updateLCD(float flowRate, float o2Percent) {
    lcdSerial.write(0xFE);  // Send command character
    lcdSerial.write(0x01);  // Clear screen command

    lcdSerial.print("Flow Rate: ");
    lcdSerial.print(flowRate);

    lcdSerial.write(0xFE);  // Send command character
    lcdSerial.write(0xC0);  // Move to the second line

    lcdSerial.print("O2 %: ");
    lcdSerial.print(o2Percent);
}
