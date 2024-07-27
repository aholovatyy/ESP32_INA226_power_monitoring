/***************************************************************************
* Test task for power monitoring 
* based on the ESP32 microcontroller and INA226 current shunt and power monitor sensor with an I2C interface
* For reading shunt voltage, bus voltage, current, and bus power from the INA226 sensor, the INA226_WE library is used
* This sketch shows how to use the INA219 sensor. 
*  
* Further information about the INA226_WE library can be found on:
* https://wolles-elektronikkiste.de/en/ina226-current-and-power-sensor (English)
* https://wolles-elektronikkiste.de/ina226 (German)
* 
***************************************************************************/
#include <Wire.h>
#include <LiquidCrystal.h>
#include <INA226_WE.h>
#include <EEPROM.h>

// LCD pins
/*#define RS 19
#define EN 23
#define D4 18
#define D5 17
#define D6 16
#define D7 15*/

#define RS 9
#define EN 8
#define D4 7
#define D5 6
#define D6 5
#define D7 4

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);

// I2C address of the INA226 sensor
#define I2C_ADDRESS 0x40 // SCL 21, SDA 20

/* There are several ways to create your INA226 object:
 * INA226_WE ina226 = INA226_WE(); -> uses I2C Address = 0x40 / Wire
 * INA226_WE ina226 = INA226_WE(I2C_ADDRESS);   
 * INA226_WE ina226 = INA226_WE(&Wire); -> uses I2C_ADDRESS = 0x40, pass any Wire Object
 * INA226_WE ina226 = INA226_WE(&Wire, I2C_ADDRESS); 
 */
INA226_WE ina226 = INA226_WE(I2C_ADDRESS);

const int deltat = 1000; // sampling period in milliseconds, in our case 1 s
unsigned long cmilli, pmilli;
const float deltath = deltat / 3.6e6; // sampling period in hours (deltat in [ms]) 0.28 hours
float power_consumption_J = 0.0, power_consumption_Wh = 0.0;
float previous_power_W = 0.0;
byte power_consumption_flag = 0;

const byte power_consumption_flag_address = 0;
const int power_consumption_Wh_address = sizeof(power_consumption_flag_address);

void setup() {
  Serial.begin(9600);
  Wire.begin();
  // default parameters are set - for change check the other examples
  ina226.init();
  
  /* Set Number of measurements for shunt and bus voltage which shall be averaged
  * Mode *     * Number of samples *
  AVERAGE_1            1 (default)
  AVERAGE_4            4
  AVERAGE_16          16
  AVERAGE_64          64
  AVERAGE_128        128
  AVERAGE_256        256
  AVERAGE_512        512
  AVERAGE_1024      1024
  */
  //ina226.setAverage(AVERAGE_16); // choose mode and uncomment for change of default

  /* Set conversion time in microseconds
     One set of shunt and bus voltage conversion will take: 
     number of samples to be averaged x conversion time x 2
     
     * Mode *         * conversion time *
     CONV_TIME_140          140 µs
     CONV_TIME_204          204 µs
     CONV_TIME_332          332 µs
     CONV_TIME_588          588 µs
     CONV_TIME_1100         1.1 ms (default)
     CONV_TIME_2116       2.116 ms
     CONV_TIME_4156       4.156 ms
     CONV_TIME_8244       8.244 ms  
  */
  //ina226.setConversionTime(CONV_TIME_1100); //choose conversion time and uncomment for change of default
  
  /* Set measure mode
  POWER_DOWN - INA226 switched off
  TRIGGERED  - measurement on demand
  CONTINUOUS  - continuous measurements (default)
  */
  //ina226.setMeasureMode(CONTINUOUS); // choose mode and uncomment for change of default
  
  /* Set Resistor and Current Range
     resistor is 5.0 mOhm
     current range is up to 10.0 A
     default was 100 mOhm and about 1.3 A
  */
  ina226.setResistorRange(0.005,10.0); // choose resistor 5 mOhm and gain range up to 10 A
  
  /* If the current values delivered by the INA226 differ by a constant factor
     from values obtained with calibrated equipment you can define a correction factor.
     Correction factor = current delivered from calibrated equipment / current delivered by INA226
  */
  // ina226.setCorrectionFactor(0.95);
  
  ina226.waitUntilConversionCompleted(); //if you comment this line the first data might be zero


  Serial.println("ESP32 and INA226 Current Sensor Example Sketch");
  //Serial.println("Continuous Sampling starts");
  Serial.println();
  /*
   The ESP32 does not have an EEPROM as such. However, the developers of the ESP32 Core for Arduino included an EEPROM library that emulates the behavior, making it easy to use.
   With the ESP32 and the EEPROM library, you can use up to 512 bytes in flash memory. This means you have 512 different addresses, and you can store a value between 0 and 255 in each address position.
   Unlike RAM, the data we store with the EEPROM library is not lost when the power is cut off. This feature makes it an ideal choice for retaining settings, adjustments, and other important data in embedded devices such as the ESP32.
  */
  // initialize the EEPROM with the appropriate size
  EEPROM.begin(512);
  //EEPROM.write(address_deltat, deltat);
  //EEPROM.commit();

  if (EEPROM.read(power_consumption_flag_address))
  {
    power_consumption_Wh = EEPROM.read(power_consumption_Wh_address); // Reads the total consumed power in Wh stored at the given FLASH memory address
  }
  // set up the number of LCD columns and rows:
  lcd.begin(16, 2);
  // Print messages to the LCD
  lcd.setCursor(1, 0);
  lcd.print(F("Power Tracking"));
  lcd.setCursor(2, 1);
  lcd.print(F("ESP32 Device"));
  delay(1000);
  lcd.clear();
}

void loop() {
  /*for(int i=0; i<5; i++){
    continuousSampling();
    delay(3000);
  }
  
  Serial.println("Power down for 10s");
  ina226.powerDown();
  for(int i=0; i<10; i++){
    Serial.print(".");
    delay(1000);
  }
  
  Serial.println("Power up!");
  Serial.println("");
  ina226.powerUp();*/

  cmilli = millis();
  if (cmilli - pmilli > deltat) { // measure every second, sampling interval 1 second
    pmilli = cmilli;
    continuousSampling();
  }
}

void continuousSampling(){
  float shuntVoltage_mV = 0.0;
  float loadVoltage_V = 0.0;
  float busVoltage_V = 0.0;
  float current_mA = 0.0;
  float power_mW = 0.0; 
  float power_W = 0.0;
 
  ina226.readAndClearFlags();
  shuntVoltage_mV = ina226.getShuntVoltage_mV();
  busVoltage_V = ina226.getBusVoltage_V();
  current_mA = ina226.getCurrent_mA();
  power_mW = ina226.getBusPower();
  power_W = power_mW / 1000; 
  loadVoltage_V  = busVoltage_V + (shuntVoltage_mV/1000);

  power_consumption_J = power_consumption_J + (power_W + previous_power_W)/2.0*deltat*0.001; // integer for trapezoids
  power_consumption_Wh = power_consumption_J / 3.6e6;
  // 
  //power_consumption_Wh = power_consumption_Wh + (power_W + previous_power_W)/2.0*deltath; // integer for trapezoids
  
  previous_power_W = power_W; // then updates the power
  //deltath = deltat / 3.6e6; // sampling period in hours (deltat in [ms])
  
  Serial.print(F("Shunt Voltage [mV]: ")); Serial.println(shuntVoltage_mV);
  Serial.print(F("Bus Voltage [V]: ")); Serial.println(busVoltage_V);
  Serial.print(F("Load Voltage [V]: ")); Serial.println(loadVoltage_V);
  Serial.print(F("Current[mA]: ")); Serial.println(current_mA);
  Serial.print(F("Bus Power [mW]: ")); Serial.println(power_mW);
  Serial.print(F("Energy consumption [J]: ")); Serial.println(power_consumption_J);
  Serial.print(F("Power consumption [W*h]: ")); Serial.println(power_consumption_Wh);

  lcd.setCursor(0,0);
  lcd.print(shuntVoltage_mV, 0);
  lcd.print("mV ");
  lcd.print((int)busVoltage_V);
  lcd.print("V ");
  lcd.setCursor(0,1);
  lcd.print((int)loadVoltage_V);
  lcd.print("V ");
  delay(2000);
  lcd.clear();

  lcd.setCursor(0,0);
  lcd.print((int)current_mA);
  lcd.print("mA ");
  lcd.print((int)power_W);
  lcd.print("W ");
  lcd.setCursor(0,1);
  lcd.print((int)power_consumption_J); // energy consumption Ws = J
  lcd.print("J ");
  lcd.print((int)power_consumption_Wh);
  lcd.print("Wh ");
  delay(2000);

  if (!EEPROM.read(power_consumption_flag_address)) // if power_consumption_flag is equal to 0
  {
    EEPROM.write(power_consumption_flag_address, 1); // Stores the power_consumption_flag value '1' at the given address
  }
  EEPROM.write(power_consumption_Wh_address, power_consumption_Wh); // Stores the power_consumption_Wh value at the given address  
  EEPROM.commit(); // Saves the last value of the power consumption in Watt-hours to the EEPROM - ESP32 FLASH memory
  
  if(!ina226.overflow){
    Serial.println("Values OK - no overflow");
  }
  else{
    Serial.println("Overflow! Choose higher current range");
  }
  Serial.println();
}
