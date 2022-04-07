// Ed Katynski, Mar. 10 2020
// BATTERY MANAGEMENT SYSTEM (SYSTEM)
//
// TO DO:
// - Configure fault report delay to be equal regardless of number of reports - sample at 0.25Hz with correction?
// - Configure digital outputs for driving relay-control FETs
// - Set up to change between charge/drive mode
// - Set up solar charge mode to periodically detach solar module to measure cell values
// - Set up communication via serial(?) w/ Jetson

int ledPin = 13;      //indicator light
int sampleCount = 10; //number of samples taken for moving average filter - not super helpful variable because Arduino doesn't like dynamic arrays
int val[7][10];       //set sample array width to sampleCount - easier than setting up a dynamically sized array
int measured_val[7];  //averaged sample values - corresponds to connector pin voltage measurements
int cell_val[7];      //derived individual cell voltages
int faults[4];        //fault condition boolean array
float v_ratio[] = {1.276, 3.015, 3.948, 5.124, 6.493, 7.653, 9.053}; //proportion of ADC value to battery pin voltage, solar module voltage

void sample()
{
  for(int i = 0; i < 7; i++){
    int sum = 0;
    for(int j = sampleCount; j > 1; j--){
      val[i][j-1] = val[i][j-2]; //advance all sample values one further into the array, discarding the oldest one
      sum += val[i][j-1];  //add sample to sum
    }
    val[i][0] = 3.3*(analogRead(i)/1.023); //take new voltage sample at a0 - 3.3V is max value, return value in mV, 10 bit ADC value
    sum += val[i][0];  //add newest sample to sum
    measured_val[i] = int(v_ratio[i] * float(sum/10)); //take the average
  }
  cell_val[0] = measured_val[0];
  for(int i = 1; i < 6; i++) cell_val[i] = (measured_val[i] - measured_val[i-1]); //calculate cell values
  cell_val[6] = measured_val[6];
}

void report()
{
  Serial.print("\n\n");

  //ADC voltages, for proportion
  Serial.print("ADC VOLTAGE: \t[");
  for(int i = 0; i < 5; i++){
  Serial.print((int)((float)measured_val[i]/v_ratio[i]));
    Serial.print("mV | ");
  }
  Serial.print((int)((float)measured_val[5]/v_ratio[5]));
  Serial.println("mV]");

  //Display measurement voltages
  Serial.print("MEASUREMENTS: \t[");
  for(int i = 0; i < 5; i++){
    Serial.print(measured_val[i]);
    Serial.print("mV | ");
  }
  Serial.print(measured_val[5]);
  Serial.println("mV]");

  //Display battery cell voltages
  Serial.print("BATTERY CELLS: \t[");
  for(int i = 0; i < 5; i++){
    Serial.print(cell_val[i]);
    Serial.print("mV | ");
  }
  Serial.print(cell_val[5]);
  Serial.println("mV]");

  //Display solar module boost converter output voltage
  Serial.print("SOLAR MODULE: \t[");
  Serial.print(cell_val[6]);
  Serial.println("mv]");

  //Display Faults
  Serial.print("FAULTS: \t[");
  if(faults[0] == 1) Serial.print("OVERVOLTAGE");
  if((faults[0] == 1) && ((faults[1] == 1) || (faults[2] == 1) || (faults[3] == 1))) Serial.print(" | ");
  if(faults[1] == 1) Serial.print("UNDERVOLTAGE");
  if((faults[1] == 1) && ((faults[2] == 1) || (faults[3] == 1))) Serial.print(" | ");
  if(faults[2] == 1) Serial.print("IMBALANCE");
  if((faults[2] == 1) && (faults[3] == 1)) Serial.print(" | ");
  if(faults[3] == 1) Serial.print("SOLAR MODULE FAILURE");
  if((faults[0] == 0) && (faults[1] == 0) && (faults[2] == 0) && (faults [3] == 0)) Serial.print("NONE");
  Serial.print("]");
}

void faultCheck()
{
  int faultFlag = 0;
  int checker[] = {0,0,0,0};  //internal fault checker
  int v_max = 4100;           //max cell voltage allowed is 4100mV
  int v_min = 3100;           //min cell voltage allowed is 3100mV
  int v_diff = 500;           //max cell imbalance allowed is 500mV
  int v_high = cell_val[0];   //measured maximum
  int v_low = cell_val[0];    //measured minimum
  int v_delta = 0;            //measured max difference

  //determine highest, lowest cell voltages and delta between them
  for(int i = 1; i < 6; i++) //cell_val[0] is your initial high and low, which is why the loop starts at i = 1
  {
    if (cell_val[i] > v_max) v_high = cell_val[i];
    if (cell_val[i] < v_min) v_low = cell_val[i];
    if (abs(v_high - v_low) > v_delta) v_delta = abs(v_high - v_low);
  }

  //overvoltage condition detection
  if(v_high > v_max){
    faultFlag = 1;
    checker[0] = 1;
    faults[0] = 1;
    faultBlink(1);
  }

  //undervoltage condition detection
  if(v_low < v_min){
    faultFlag = 1;
    checker[1] = 1;
    faults[1] = 1;
    faultBlink(2);
  }

  //cell imbalance condition detection
  if(v_delta > v_diff){
    faultFlag = 1;
    checker[2] = 1;
    faults[2] = 1;
    faultBlink(3);
  }

  //solar module output bound violation
  if((cell_val[6] < 2200) || (cell_val[6] > 2500)){
    faultFlag = 1;
    checker[3] = 1;
    faults[3] = 1;
    faultBlink(4);
  }

  //"everything's OK alarm" - for consistent sample rate
  if(faultFlag == 0) faultBlink(0);

  //reset fault conditions as they deactivate
  for(int i = 0; i < 4; i++){
    if(checker[i] == 0) faults[i] = 0;
  }
}

void faultBlink(int condition)
{
  for(int i = 0; i < condition; i++){
       digitalWrite(ledPin, HIGH);
       delay(125);
       digitalWrite(ledPin, LOW);
       delay(125);
    }
  delay((4-condition)*250);
}

void setup()
{
  //zero everything out at startup
  for(int i = 0; i < 7; i++){
    for(int j = 0; j < sampleCount; j++){
    val[i][j] = 0;
    }
    measured_val[i] = 0;
    cell_val[i] = 0;
  }
  for(int i = 0; i < 4; i++){
    faults[i] = 0;
  }

  //set up LED pin and serial communication
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  Serial.begin(38400);
}

void loop()
{
  sample();
  faultCheck();
  report();
  delay(500);
}
