#include <Wire.h>
#include <Filters.h>

////////////////////////// data ////////////////////////////

char buffer[10];

uint8_t v_i = 0;
double v_sum = 0;
double v_avg = 0;
const uint8_t v_avg_amount = 30;
double voltage_buffer[v_avg_amount];
char voltage_str[10];
char current_str[10];

#define ACS_Pin A1                        //Sensor data pin on A0 analog input

float ACS_Value;                              //Here we keep the raw data valuess
float testFrequency = 50;                    // test signal frequency (Hz)
float windowLength = 40.0/testFrequency;     // how long to average the signal, for statistist



double intercept = -0.038; // to be adjusted based on calibration testing
double slope = 0.0115; // to be adjusted based on calibration testing
                      //Please check the ACS712 Tutorial video by SurtrTech to see how to get them because it depends on your sensor, or look below


double Amps_TRMS; // estimated actual current in amps

unsigned long printPeriod = 10; 
unsigned long previousMillis = 0;


float voltage_fc_1 = 1;
float voltage_fc_2 = 0.1;
float voltage_fc_3 = 0.1;

float current_fc_1 = 0.1;
float current_fc_2 = 0.1;

/////////////////////////////////////////////////////

RunningStatistics inputStats;                 // create statistics to look at the raw test signal

FilterTwoPole voltage_2pole_filter1;
FilterTwoPole voltage_2pole_filter2;
FilterTwoPole voltage_2pole_filter3;

FilterTwoPole current_2pole_filter1;
FilterTwoPole current_2pole_filter2;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(ACS_Pin,INPUT);
  inputStats.setWindowSecs( windowLength );
  Wire.begin(0x08);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);
  voltage_2pole_filter1.setAsFilter(LOWPASS_BUTTERWORTH, voltage_fc_1);
  voltage_2pole_filter2.setAsFilter(LOWPASS_BUTTERWORTH, voltage_fc_2);
  voltage_2pole_filter3.setAsFilter(LOWPASS_BUTTERWORTH, voltage_fc_3);

  current_2pole_filter1.setAsFilter(LOWPASS_BUTTERWORTH, current_fc_1);
  current_2pole_filter2.setAsFilter(LOWPASS_BUTTERWORTH, current_fc_2);

}


void loop() {
  
  uint16_t value = analogRead(A0);
  
  voltage_2pole_filter1.input(value);
  voltage_2pole_filter2.input(voltage_2pole_filter1.output());
  voltage_2pole_filter3.input(voltage_2pole_filter2.output());
  v_avg = ceil(voltage_2pole_filter3.output()) * (10.0/1024.0) * 88 * 1.056 * 0.95;
  Serial.print("Voltage Average: ");
  Serial.print(String(v_avg, 3));

  ACS_Value = analogRead(ACS_Pin);  // read the analog in value:
  inputStats.input(ACS_Value);
  if((unsigned long)(millis() - previousMillis) >= printPeriod) { //every second we do the calculation
    previousMillis = millis();   // update time
    
    Amps_TRMS = (intercept + slope * inputStats.sigma()) * 1000.0;
    current_2pole_filter1.input(Amps_TRMS);
    current_2pole_filter2.input(current_2pole_filter1.output());
    if (Amps_TRMS < 0) {
      Amps_TRMS = 0;
      Serial.print( "\t MilliAmps: " ); 
      Serial.println( Amps_TRMS );
    } else {
      Serial.print( "\t MilliAmps: " ); 
      Serial.println( current_2pole_filter1.output() , 3);
    }
    
  }
}

void receiveEvent(int howmany) {
  memset(buffer, 0, sizeof(char) * 10);
  uint8_t i = 0;
  while (Wire.available() > 0) {
    buffer[i++] = Wire.read();      /* receive byte as a character */
  }
  buffer[i] = 0;
//  Serial.println(buffer);
}

void requestEvent(int howmany) {
  if ( strstr(buffer, "voltage") != NULL ) {
    dtostrf(v_avg, 5, 4, voltage_str);
    Wire.write(voltage_str);
  } else if ( strstr(buffer, "current") != NULL) {
    if (Amps_TRMS < 0) {
      Amps_TRMS = 0;
      dtostrf(Amps_TRMS, 5, 2, current_str);
    } else {
      dtostrf(current_2pole_filter1.output(), 5, 2, current_str);
    }
    dtostrf(current_2pole_filter1.output(), 5, 2, current_str);
    Wire.write(current_str);
  }
}

