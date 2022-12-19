#include <Arduino.h>
#include <SPI.h>
#include <math.h> 
#include <LiquidCrystal.h>


//SPI Settings
SPISettings SPIsettings(10000000, MSBFIRST, SPI_MODE2);
//LCD 16x2 Settings
LiquidCrystal LCD(5, 6, 7, 8, 9, A5);                   //(RS, EN, D4, D5, D6, D7) 

//Non Constant Global variables
const long OSCEXT          = 25000000;                  //External Oscilator Frequency 25MHz
int        POS_ROTARY      = 0;                         //Rotary Encoder Counter
int        ACTUAL_CLK;                                  //Actual CLK value
int        LAST_CLK;                                    //Last CLK value
long       CONT_LAST_SW    = 0;                         //Variable to store the time since the SW button was pressed
long       CONT_LAST_PB    = 0;                         //Variable to store the time since the Push Button was pressed
int        FREQ_ROTARY[8]  = {0, 0, 0, 0, 0, 0, 0, 0};  //Frequency Digits
int        PHASE_ROTARY[3] = {0, 0, 0};                 //Phase Digits
int        POS_DIGIT_PHASE = 0;                         //From 0 to 2, where 2 are the units
int        POS_DIGIT_FREQ = 1;                          //From 1 to 8 where 8 are the units
int        OPERATION_MODE  = 0;                         //Mode 0: Frequency, Mode 1: Phase, Mode 2: Wave (Selection of those variables)
int        DESIRED_WAVE    = 0;                         //0 Sine, 1 Triang, 2 SQRMSB, 3 SQRMSB/2
unsigned int WAVE_TO_WRITE;                             //Wave that it is going to be written on the AD      
 
//Pins
const uint8_t FSYNC = 10; //ChipSelect Pin
const uint8_t CLK   =  2; //CLK Rotary Pin;
const uint8_t DT    =  3; //DT Rotary Pin
const uint8_t SW    =  4; //SW Rotary Pin, to change between digits
const uint8_t PB    = A0; //PB pin, to switch between operation modes

//Waveforms
const unsigned int SINE        = 0x2000;
const unsigned int TRIANG      = 0x2002;
const unsigned int SQUARE_MSB  = 0x2028;
const unsigned int SQUARE_MSB2 = 0x2020;//Divide by 2 the FREQ


//Write AD9833's Registers
void writeRegister(const int &data);

//Writes the frequency, phase(asked in degrees) and the desired wave. It has to be called using ChangeOperationModeAndWriteData
void writeAD9837(const float &desiredFreq, const unsigned int &desiredPhase, const unsigned int &wave);

//Transforms the array into our frequency and returns it
float selectedFreq();

//Transforms the array into our phase and returns it
int selectedPhase();

//Changes the operation mode and writes data into the AD9833 <-------------
void changeOperationModeAndWriteData();

//Changes the position of the frequency and phase
void changeDigitFreqPhase();

//Obvious
void selectWave();

//Prints everything in the lcd
void lcdOperationMode();

//Decodes the encoder and select the value of the different arrays (freq and phase);
//25MHz clock => 0.1Hz Resolution
void cuadratureEncoder()
{
  ACTUAL_CLK = digitalRead(CLK);

    if(LAST_CLK != ACTUAL_CLK && ACTUAL_CLK == 1)
    {
      if(OPERATION_MODE == 0)   //FREQ
      {
         //Clockwise, increment
        if(ACTUAL_CLK == digitalRead(DT))
        {
          
          if(POS_DIGIT_FREQ == 1 && FREQ_ROTARY[7] < 9 && FREQ_ROTARY[0] < 8) //Decimal(aaa.00)
            FREQ_ROTARY[7] += 1;
          else if(POS_DIGIT_FREQ == 2 && FREQ_ROTARY[6] < 9 && FREQ_ROTARY[0] < 8) //Units
            FREQ_ROTARY[6]++;
          else if(POS_DIGIT_FREQ == 3 && FREQ_ROTARY[5] < 9 && FREQ_ROTARY[0] < 8)
            FREQ_ROTARY[5]++;
          else if(POS_DIGIT_FREQ == 4 && FREQ_ROTARY[4] < 9 && FREQ_ROTARY[0] < 8)
            FREQ_ROTARY[4]++;
          else if(POS_DIGIT_FREQ == 5 && FREQ_ROTARY[3] < 9 && FREQ_ROTARY[0] < 8)
            FREQ_ROTARY[3]++;
          else if(POS_DIGIT_FREQ == 6 && FREQ_ROTARY[2] < 9 && FREQ_ROTARY[0] < 8)
            FREQ_ROTARY[2]++;
          else if(POS_DIGIT_FREQ == 7 && FREQ_ROTARY[1] < 9 && FREQ_ROTARY[0] < 8)
            FREQ_ROTARY[1]++;
          else if(POS_DIGIT_FREQ == 8 && FREQ_ROTARY[0] < 8)
          {
            FREQ_ROTARY[0]++;
            if(FREQ_ROTARY[0] == 8)  //8MHz MAximum reached
            {
              for(int i = 7; i > 0; i--)
                FREQ_ROTARY[i] = 0;                         
            }
          }
        }
        else
        {
     
          if(POS_DIGIT_FREQ == 1 && FREQ_ROTARY[7] > 0)     //Decimal(aa.00)
            FREQ_ROTARY[7] -= 1;
          else if(POS_DIGIT_FREQ == 2 && FREQ_ROTARY[6] > 0)  //Units
            FREQ_ROTARY[6]--;
          else if(POS_DIGIT_FREQ == 3 && FREQ_ROTARY[5] > 0) 
            FREQ_ROTARY[5]--;
          else if(POS_DIGIT_FREQ == 4 && FREQ_ROTARY[4] > 0)
            FREQ_ROTARY[4]--;
          else if(POS_DIGIT_FREQ == 5 && FREQ_ROTARY[3] > 0)
            FREQ_ROTARY[3]--;
          else if(POS_DIGIT_FREQ == 6 && FREQ_ROTARY[2] > 0)
            FREQ_ROTARY[2]--;
          else if(POS_DIGIT_FREQ == 7 && FREQ_ROTARY[1] > 0)
            FREQ_ROTARY[1]--;
          else if(POS_DIGIT_FREQ == 8 && FREQ_ROTARY[0] > 0)
            FREQ_ROTARY[0]--;
        }       
      }
      else if(OPERATION_MODE == 1) //PHASE
      {
        if(ACTUAL_CLK == digitalRead(DT))
        {
          if(POS_DIGIT_PHASE == 0)
          {
            if(PHASE_ROTARY[2] < 9 && PHASE_ROTARY[1] < 6 && PHASE_ROTARY[0] <= 3)
              PHASE_ROTARY[2]++;
            else if(PHASE_ROTARY[1] >= 6 && PHASE_ROTARY[0] == 3)
              PHASE_ROTARY[2] = 0;
          }
          else if(POS_DIGIT_PHASE == 1)
          {
            if(PHASE_ROTARY[1] < 9 && PHASE_ROTARY[0] != 3)
              PHASE_ROTARY[1]++;
            else if(PHASE_ROTARY[0] == 3 && PHASE_ROTARY[1] < 6)
              PHASE_ROTARY[1]++;
          }
          else
          {
            if(PHASE_ROTARY[0] < 3)
              PHASE_ROTARY[0]++;
          }
            
        }
        else
        {
          if(POS_DIGIT_PHASE == 0)
          {
            if(PHASE_ROTARY[2] > 0)
              PHASE_ROTARY[2]--;    
          }
          else if(POS_DIGIT_PHASE == 1)
          {
            if(PHASE_ROTARY[1] > 0)
              PHASE_ROTARY[1]--;
          }
          else
          {
            if(PHASE_ROTARY[0] > 0)
              PHASE_ROTARY[0]--;
          }
        }
      }
      else if(OPERATION_MODE == 2) //WAVE
      {
        if(ACTUAL_CLK == digitalRead(DT))
        {
          if(DESIRED_WAVE < 3)
            DESIRED_WAVE++;
        }
        else
        {
          if(DESIRED_WAVE > 0)
            DESIRED_WAVE--;
        }
      }
    }  
    LAST_CLK = ACTUAL_CLK;
}

void setup() 
{
    SPI.begin();
    LCD.begin(16, 2);
    LCD.cursor();
    LCD.print("0000000.00Hz LDR");
    LCD.setCursor(0, 1);
    LCD.print("PH:000Deg");
    LCD.setCursor(11, 1);
    LCD.print("W:SIN");
    pinMode(FSYNC, OUTPUT);
    pinMode(CLK, INPUT);
    pinMode(DT, INPUT);
    pinMode(SW, INPUT_PULLUP);
    pinMode(PB, INPUT);
    digitalWrite(FSYNC, HIGH); //Inizialitez to 1

    LAST_CLK = digitalRead(CLK);
    
    attachInterrupt(digitalPinToInterrupt(CLK), cuadratureEncoder, CHANGE);
    attachInterrupt(digitalPinToInterrupt(DT), cuadratureEncoder, CHANGE);

}

void loop() 
{
  
   changeDigitFreqPhase(); 
   selectWave();
   lcdOperationMode();
   changeOperationModeAndWriteData();
  
}

void selectWave()
{
  switch(DESIRED_WAVE)
  {
    case 0:WAVE_TO_WRITE = SINE;      break;
    case 1:WAVE_TO_WRITE = TRIANG;    break;
    case 2:WAVE_TO_WRITE = SQUARE_MSB;break;
    case 3:WAVE_TO_WRITE = SQUARE_MSB2;break;
  }
}

void lcdOperationMode()
{
  if(OPERATION_MODE == 0) //FREQ,
  {
    LCD.cursor();
    LCD.setCursor(12, 0);
    LCD.print(" LDR");      //LOAD
    switch(POS_DIGIT_FREQ)
    {
      case 1:
        LCD.setCursor(8,0);
        LCD.print(FREQ_ROTARY[7]);
        LCD.setCursor(8,0);
        break;

      case 2:
        LCD.setCursor(6,0);
        LCD.print(FREQ_ROTARY[6]);
        LCD.setCursor(6,0);
        break;     

      case 3:
        LCD.setCursor(5,0);
        LCD.print(FREQ_ROTARY[5]);
        LCD.setCursor(5,0);
        break;
        
      case 4: 
        LCD.setCursor(4,0);
        LCD.print(FREQ_ROTARY[4]);
        LCD.setCursor(4,0);
        break;

      case 5:
        LCD.setCursor(3,0);
        LCD.print(FREQ_ROTARY[3]);
        LCD.setCursor(3,0);
        break;     

      case 6:
        LCD.setCursor(2,0);
        LCD.print(FREQ_ROTARY[2]);
        LCD.setCursor(2,0);
        break;  
        
      case 7:   
        LCD.setCursor(1,0);
        LCD.print(FREQ_ROTARY[1]);
        LCD.setCursor(1,0);
        break;
        
      case 8:
        LCD.setCursor(0,0);
        LCD.print(FREQ_ROTARY[0]);
        LCD.setCursor(0,0);
        if(FREQ_ROTARY[0] == 8)
        {
          LCD.print("8000000.00Hz");
        }
        LCD.setCursor(0,0);
        break;                  
    }
    
  }
  else if(OPERATION_MODE == 1) //PHASE
  {
    LCD.cursor();
    switch(POS_DIGIT_PHASE)
    {
      case 0: 
        if(PHASE_ROTARY[0] == 3 && PHASE_ROTARY[1] == 6)
        {
          LCD.setCursor(3, 1);
          LCD.print("360");
          LCD.setCursor(5, 1);
        }
        else
        {
          LCD.setCursor(5, 1);
          LCD.print(PHASE_ROTARY[2]);
          LCD.setCursor(5, 1);          
        }
        break;

      case 1:
        if(PHASE_ROTARY[0] == 3 && PHASE_ROTARY[1] >= 6)
        {
          LCD.setCursor(3, 1);
          LCD.print("360");
          LCD.setCursor(4, 1);
        }
        else
        {
          LCD.setCursor(4, 1);
          LCD.print(PHASE_ROTARY[1]);
          LCD.setCursor(4, 1);          
        }
        break;

      case 2:
        if(PHASE_ROTARY[0] == 3 && PHASE_ROTARY[1] >= 6)
        {
          LCD.setCursor(3, 1);
          LCD.print("360");
          LCD.setCursor(3, 1);
        }
        else
        {
        LCD.setCursor(3, 1);
        LCD.print(PHASE_ROTARY[0]);
        LCD.setCursor(3, 1);
        }
        break;
    }
  }
  else if(OPERATION_MODE == 2) //WAVE
  {
    LCD.noCursor();
    LCD.setCursor(11, 1);
    switch(DESIRED_WAVE)
    {
      case 0:
        LCD.print("W:SIN");
        break;
      case 1:
        LCD.print("W:TNG");
        break;
      case 2:
        LCD.print("W:SQ1");
        break;
      case 3:
        LCD.print("W:SQ2");
        break;
    }
    LCD.setCursor(15, 1);
    LCD.cursor();
  }
  else
  {
    LCD.noCursor();
    LCD.setCursor(13, 0);
    LCD.print("OUT");
    LCD.setCursor(13, 0);
  }
  delay(100);
}

void changeDigitFreqPhase()
{
   int buttonActual = digitalRead(SW);
  //If the encoder button is pressed
  if(buttonActual == 0)
  {
    //If it was held for 200 mS
    if( millis() - CONT_LAST_SW > 200)
    {
      if(OPERATION_MODE == 0)  //Frequency op mode
      {
       if(POS_DIGIT_FREQ < 8)
        POS_DIGIT_FREQ++;
       else
        POS_DIGIT_FREQ = 1;
      }
      else if(OPERATION_MODE == 1) //Phase op mode 
      {
        if(POS_DIGIT_PHASE < 3)
          POS_DIGIT_PHASE++;
        else
          POS_DIGIT_PHASE = 0;
      }
    }

    CONT_LAST_SW = millis();
  }
}

void changeOperationModeAndWriteData()
{
   int buttonActual = digitalRead(PB);
  //If the button is pressed we are changing the operation mode
  if(buttonActual == 0)
  {
    //Si se ha mantenido mas de 200 ms
    if( millis() - CONT_LAST_PB > 200)
    {
      if(OPERATION_MODE < 3)
      {
        OPERATION_MODE++;
        
        if(OPERATION_MODE == 3)
          writeAD9837(selectedFreq(), selectedPhase(), WAVE_TO_WRITE); //WRITTEN HERE, BECAUSE SINCE WE ARE CHANGING FROM 2 TO 3, THE AD IS WRITTEN ONLY ONCE, HENCE NOT AFFECTIN LOW FREQ
      }
      else
      {
        OPERATION_MODE = 0;
      }
    }
    CONT_LAST_PB = millis();
  }
}

float selectedFreq()
{
  return FREQ_ROTARY[0]*pow(10, 6) + FREQ_ROTARY[1]*pow(10, 5) + FREQ_ROTARY[2]*pow(10, 4) + 
  FREQ_ROTARY[3]*pow(10, 3) + FREQ_ROTARY[4]*pow(10, 2) + FREQ_ROTARY[5]*pow(10, 1) + FREQ_ROTARY[6] + FREQ_ROTARY[7]/10.00;
}

int selectedPhase()
{
  if(PHASE_ROTARY[0] == 3 && PHASE_ROTARY[1] >= 6)
    return 360;
  else
    return PHASE_ROTARY[0]*pow(10, 2) + PHASE_ROTARY[1]*pow(10,1) + PHASE_ROTARY[2];
}

void writeRegister(const int &data)
{
  SPI.beginTransaction(SPIsettings);
  digitalWrite(FSYNC, LOW);
  SPI.transfer16(data);
  digitalWrite(FSYNC, HIGH);
  SPI.endTransaction();
}

void writeAD9837(const float &desiredFreq, const unsigned int &desiredPhase, const unsigned int &wave)
{
  
  unsigned long int freqReg = (desiredFreq * pow(2,28))/(OSCEXT); //2^28
  int msbFreq = ((freqReg >> 14) & 0x7FFF) | 0x4000;
  int lsbFreq = (freqReg & 0x7FFF) | 0x4000;
  
  unsigned int faseReg = (((desiredPhase*PI)/180) * 4096)/(2*PI);
  int bitsPhase = (0xC000 | faseReg);

  writeRegister(0x2100);  
  writeRegister(lsbFreq);
  writeRegister(msbFreq);
  writeRegister(bitsPhase); 
  writeRegister(wave);     
}
