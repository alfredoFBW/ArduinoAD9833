#include <Arduino.h>
#include <SPI.h>
#include <math.h> 
#include <LiquidCrystal.h>

//En arduino uno, nano.. Int = 16 Bits, Long = 32 Bits
//FUNCIONA

//Ajustes del SPI
SPISettings ajusteSPI(10000000, MSBFIRST, SPI_MODE2);
//Ajustes pantalla LCD
LiquidCrystal LCD(5, 6, 7, 8, 9, A5);                   //(RS, EN, D4, D5, D6, D7) de la pantalla lcd

//Variables NO CONSTANTES globales
const long OSCEXT          = 25000000;                  //Frecuencia oscilador externo a 25MHz resolucion maxima = 0.1
int        POS_ROTARY      = 0;                         //Contador del rotaryEncoder
int        ACTUAL_CLK;                                  //Valor Actual CLK   (global por interrupt)
int        ANTERIOR_CLK;                                //Valor anterior CLK
long       CONT_ULT_PULS   = 0;                         //Tiempo ultima pulsacion SW
long       CONT_ULT_PB     = 0;                         //Tiempo ultima pulsacion PB
int        FREQ_ROTARY[8]  = {0, 0, 0, 0, 0, 0, 0, 0};  //Digitos de la frecuencia de rotary(el ultimo son 2 decimales)
int        FASE_ROTARY[3]  = {0, 0, 0};                 //Digitos de la fase del rotary
int        POS_DIGITO_FASE = 0;                         //Del 0 al 2, donde el 2 son las unidades
int        POS_DIGITO_FREQ = 1;                         //Del 1 al 8, donde el 8 son las unidades
int        MODO_OPERACION  = 0;                         //Modo 0 Seleccion de F, Modo 1 Seleccion de Fase, Modo 2 Onda, Modo 3 Stdby
int        ONDA_DESEADA    = 0;                         //0 Seno, 1 Triang, 2 SQRMSB, 3 SQRMSB/2
unsigned int ONDA_ESCRIBIR;                             //Onda que escribiremos en el AD        
 
//Pines
const uint8_t FSYNC = 10; //Pin chipSelect
const uint8_t CLK   =  2; //Pin CLK rotary Encoder Con intterrupción;
const uint8_t DT    =  3; //Pin DT Rotary Encoder Con interrupción
const uint8_t SW    =  4; //Pin SW Rotary Encoder para cambiar el digito de freq
const uint8_t PB    = A0; //Pin del push button para seleccionar entre freq, fase u onda;

//Formas de Onda
const unsigned int SINE        = 0x2000;
const unsigned int TRIANG      = 0x2002;
const unsigned int SQUARE_MSB  = 0x2028;
const unsigned int SQUARE_MSB2 = 0x2020;//Divide entre 2 la FREQ


//EScribimos en el registro del AD9837
void escribirRegistro(const int &data);

//Escribimos la frecuencia, fase(pedida en grados) y onda deseada, hay que llamarla y se la llama en cambiarModoOperacionYEscribirDato
void escribirAD9837(const float &freqDeseada, const unsigned int &faseDeseada, const unsigned int &onda);

//Transforma el array en frecuencia
float freqSeleccionada();

//Transforma el array de la fase en fase
int faseSeleccionada();

//Cambiamos el modo de operacion y escribe el dato EN EL AD <---------
void cambiarModoOperacionYEscribirDato();

//Cambiamos la posicion digito de la freq y de la fase
void cambiarPosicionDigitoFreqYFase();

//Seleccionamos la onda
void seleccionarOnda();

//Decodificamos el Encoder y ponemos la Frecuencia,Fase u onda dependiendo dle modo donde estemos;
//REloj de 25MHz, reslucion de 0.1Hz
void encoderCuadratura()
{
  ACTUAL_CLK = digitalRead(CLK);

    if(ANTERIOR_CLK != ACTUAL_CLK && ACTUAL_CLK == 1)
    {
      if(MODO_OPERACION == 0)   //FREQ
      {
         //Sentido agujas del reloj, incrementamos
        if(ACTUAL_CLK == digitalRead(DT))
        {
          //Si estamos en la de las décimas y no hemos llegado al MAX (0.9)
          if(POS_DIGITO_FREQ == 1 && FREQ_ROTARY[7] < 9 && FREQ_ROTARY[0] < 8)
            FREQ_ROTARY[7] += 1;
          else if(POS_DIGITO_FREQ == 2 && FREQ_ROTARY[6] < 9 && FREQ_ROTARY[0] < 8)
            FREQ_ROTARY[6]++;
          else if(POS_DIGITO_FREQ == 3 && FREQ_ROTARY[5] < 9 && FREQ_ROTARY[0] < 8)
            FREQ_ROTARY[5]++;
          else if(POS_DIGITO_FREQ == 4 && FREQ_ROTARY[4] < 9 && FREQ_ROTARY[0] < 8)
            FREQ_ROTARY[4]++;
          else if(POS_DIGITO_FREQ == 5 && FREQ_ROTARY[3] < 9 && FREQ_ROTARY[0] < 8)
            FREQ_ROTARY[3]++;
          else if(POS_DIGITO_FREQ == 6 && FREQ_ROTARY[2] < 9 && FREQ_ROTARY[0] < 8)
            FREQ_ROTARY[2]++;
          else if(POS_DIGITO_FREQ == 7 && FREQ_ROTARY[1] < 9 && FREQ_ROTARY[0] < 8)
            FREQ_ROTARY[1]++;
          else if(POS_DIGITO_FREQ == 8 && FREQ_ROTARY[0] < 8)
          {
            FREQ_ROTARY[0]++;
            if(FREQ_ROTARY[0] == 8)  //Hemos llegado al maximo de 8 Mhz
            {
              for(int i = 7; i > 0; i--)
                FREQ_ROTARY[i] = 0.0;                         
            }
          }
        }
        else
        {
          //Si estamos en la de las décimas y no hemos llegado al MIN
          if(POS_DIGITO_FREQ == 1 && FREQ_ROTARY[7] > 0)
            FREQ_ROTARY[7] -= 1;
          else if(POS_DIGITO_FREQ == 2 && FREQ_ROTARY[6] > 0)
            FREQ_ROTARY[6]--;
          else if(POS_DIGITO_FREQ == 3 && FREQ_ROTARY[5] > 0)
            FREQ_ROTARY[5]--;
          else if(POS_DIGITO_FREQ == 4 && FREQ_ROTARY[4] > 0)
            FREQ_ROTARY[4]--;
          else if(POS_DIGITO_FREQ == 5 && FREQ_ROTARY[3] > 0)
            FREQ_ROTARY[3]--;
          else if(POS_DIGITO_FREQ == 6 && FREQ_ROTARY[2] > 0)
            FREQ_ROTARY[2]--;
          else if(POS_DIGITO_FREQ == 7 && FREQ_ROTARY[1] > 0)
            FREQ_ROTARY[1]--;
          else if(POS_DIGITO_FREQ == 8 && FREQ_ROTARY[0] > 0)
            FREQ_ROTARY[0]--;
        }       
      }
      else if(MODO_OPERACION == 1) //FASE
      {
        if(ACTUAL_CLK == digitalRead(DT))
        {
          if(POS_DIGITO_FASE == 0)
          {
            if(FASE_ROTARY[2] < 9 && FASE_ROTARY[1] < 6 && FASE_ROTARY[0] <= 3)
              FASE_ROTARY[2]++;
            else if(FASE_ROTARY[1] >= 6 && FASE_ROTARY[0] == 3)
              FASE_ROTARY[2] = 0;
          }
          else if(POS_DIGITO_FASE == 1)
          {
            if(FASE_ROTARY[1] < 9 && FASE_ROTARY[0] != 3)
              FASE_ROTARY[1]++;
            else if(FASE_ROTARY[0] == 3 && FASE_ROTARY[1] < 6)
              FASE_ROTARY[1]++;
          }
          else
          {
            if(FASE_ROTARY[0] < 3)
              FASE_ROTARY[0]++;
          }
            
        }
        else
        {
          if(POS_DIGITO_FASE == 0)
          {
            if(FASE_ROTARY[2] > 0)
              FASE_ROTARY[2]--;    
          }
          else if(POS_DIGITO_FASE == 1)
          {
            if(FASE_ROTARY[1] > 0)
              FASE_ROTARY[1]--;
          }
          else
          {
            if(FASE_ROTARY[0] > 0)
              FASE_ROTARY[0]--;
          }
        }
      }
      else if(MODO_OPERACION == 2) //ONDA
      {
        if(ACTUAL_CLK == digitalRead(DT))
        {
          if(ONDA_DESEADA < 3)
            ONDA_DESEADA++;
        }
        else
        {
          if(ONDA_DESEADA > 0)
            ONDA_DESEADA--;
        }
      }
    }  
    ANTERIOR_CLK = ACTUAL_CLK;
}

void setup() 
{
    Serial.begin(9600);
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
    digitalWrite(FSYNC, HIGH); //Lo inicializamos a 1

    ANTERIOR_CLK = digitalRead(CLK);
    
    attachInterrupt(digitalPinToInterrupt(CLK), encoderCuadratura, CHANGE);
    attachInterrupt(digitalPinToInterrupt(DT), encoderCuadratura, CHANGE);

}

void loop() 
{
  
   cambiarPosicionDigitoFreqYFase(); 
   seleccionarOnda();
   pantallaModosOp();
   cambiarModoOperacionYEscribirDato();
  
}

void seleccionarOnda()
{
  switch(ONDA_DESEADA)
  {
    case 0:ONDA_ESCRIBIR = SINE;      break;
    case 1:ONDA_ESCRIBIR = TRIANG;    break;
    case 2:ONDA_ESCRIBIR = SQUARE_MSB;break;
    case 3:ONDA_ESCRIBIR = SQUARE_MSB2;break;
  }
}

void pantallaModosOp()
{
  if(MODO_OPERACION == 0) //FREQ,
  {
    LCD.cursor();
    LCD.setCursor(12, 0);
    LCD.print(" LDR");      //LOAD(CARGANDO DATOS)
    switch(POS_DIGITO_FREQ)
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
  else if(MODO_OPERACION == 1) //FASE
  {
    LCD.cursor();
    switch(POS_DIGITO_FASE)
    {
      case 0: 
        if(FASE_ROTARY[0] == 3 && FASE_ROTARY[1] == 6)
        {
          LCD.setCursor(3, 1);
          LCD.print("360");
          LCD.setCursor(5, 1);
        }
        else
        {
          LCD.setCursor(5, 1);
          LCD.print(FASE_ROTARY[2]);
          LCD.setCursor(5, 1);          
        }
        break;

      case 1:
        if(FASE_ROTARY[0] == 3 && FASE_ROTARY[1] >= 6)
        {
          LCD.setCursor(3, 1);
          LCD.print("360");
          LCD.setCursor(4, 1);
        }
        else
        {
          LCD.setCursor(4, 1);
          LCD.print(FASE_ROTARY[1]);
          LCD.setCursor(4, 1);          
        }
        break;

      case 2:
        if(FASE_ROTARY[0] == 3 && FASE_ROTARY[1] >= 6)
        {
          LCD.setCursor(3, 1);
          LCD.print("360");
          LCD.setCursor(3, 1);
        }
        else
        {
        LCD.setCursor(3, 1);
        LCD.print(FASE_ROTARY[0]);
        LCD.setCursor(3, 1);
        }
        break;
    }
/*    LCD.noCursor();
    if(FASE_DESEADA < 10)
    {
      LCD.setCursor(5, 1);
      LCD.print(FASE_DESEADA);
      LCD.setCursor(5, 1);
    }
    else if(FASE_DESEADA < 100)
    {
      LCD.setCursor(4, 1);
      LCD.print(FASE_DESEADA);
      LCD.setCursor(5, 1); //Para que parezca que siempre esta en las unidades
    }
    else
    {
      LCD.setCursor(3, 1);
      LCD.print(FASE_DESEADA);
      LCD.setCursor(5, 1);
    }
    LCD.cursor();
*/
  }
  else if(MODO_OPERACION == 2) //ONDA
  {
    LCD.noCursor();
    LCD.setCursor(11, 1);
    switch(ONDA_DESEADA)
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

void cambiarPosicionDigitoFreqYFase()
{
   int botonActual = digitalRead(SW);
  //Si se pulsa el botón del encoder aumentamos el digito de la freq
  if(botonActual == 0)
  {
    //Si se ha mantenido mas de 200 ms
    if( millis() - CONT_ULT_PULS > 200)
    {
      if(MODO_OPERACION == 0)  //Si estamos en frecuencia
      {
       if(POS_DIGITO_FREQ < 8)
        POS_DIGITO_FREQ++;
       else
        POS_DIGITO_FREQ = 1;
      }
      else if(MODO_OPERACION == 1) //Si estamos en fase  
      {
        if(POS_DIGITO_FASE < 3)
          POS_DIGITO_FASE++;
        else
          POS_DIGITO_FASE = 0;
      }
    }

    CONT_ULT_PULS = millis();
  }
}

void cambiarModoOperacionYEscribirDato()
{
   int botonActual = digitalRead(PB);
  //Si se pulsa el botón(lo ponemos a masa) aumentamos el modo de operacion
  if(botonActual == 0)
  {
    //Si se ha mantenido mas de 200 ms
    if( millis() - CONT_ULT_PB > 200)
    {
      if(MODO_OPERACION < 3)
      {
        MODO_OPERACION++;
        
        if(MODO_OPERACION == 3)
          escribirAD9837(freqSeleccionada(), faseSeleccionada(), ONDA_ESCRIBIR); //ESCRIBIMOS AQUI PARA SOLO ESCRIBIR UNA VEZ DEL PASO DEL 2 AL 3
      }
      else
      {
        MODO_OPERACION = 0;
      }
    }
    CONT_ULT_PB = millis();
  }
}

float freqSeleccionada()
{
  //Curiosamente con un for no sale igual de preciso
  return FREQ_ROTARY[0]*pow(10, 6) + FREQ_ROTARY[1]*pow(10, 5) + FREQ_ROTARY[2]*pow(10, 4) + 
  FREQ_ROTARY[3]*pow(10, 3) + FREQ_ROTARY[4]*pow(10, 2) + FREQ_ROTARY[5]*pow(10, 1) + FREQ_ROTARY[6] + FREQ_ROTARY[7]/10.00;
}

int faseSeleccionada()
{
  if(FASE_ROTARY[0] == 3 && FASE_ROTARY[1] >= 6)
    return 360;
  else
    return FASE_ROTARY[0]*pow(10, 2) + FASE_ROTARY[1]*pow(10,1) + FASE_ROTARY[2];
}

void escribirRegistro(const int &data)
{
  SPI.beginTransaction(ajusteSPI);
  digitalWrite(FSYNC, LOW);
  //delayMicroseconds(5); Minimo hay que esperar 5ns, cada instruccion con clock de 16Mhz es en cada 62.5 ns, no necesario
  SPI.transfer16(data);
  digitalWrite(FSYNC, HIGH);
  SPI.endTransaction();
}

void escribirAD9837(const float &freqDeseada, const unsigned int &faseDeseada, const unsigned int &onda)
{
  
  unsigned long int freqReg = (freqDeseada * pow(2,28))/(OSCEXT); //2^28
  /*Desplazamos 14 msb bits, hacemos nand para dejar D15 = 0, D14 = 1 PARA
  REG0 y OR con 0x4000 en caso de perderlos(D15, D14) (Freq Alta)*/
  int msbFreq = ((freqReg >> 14) & 0x7FFF) | 0x4000;
  //Igual que MSB pero no hay que desplazar nada ya que cogemos los LSB directamente
  int lsbFreq = (freqReg & 0x7FFF) | 0x4000;
  
  unsigned int faseReg = (((faseDeseada*PI)/180) * 4096)/(2*PI);
  int bitsFase = (0xC000 | faseReg);

  escribirRegistro(0x2100);   //Preparacion para  poner registros a 0
  escribirRegistro(lsbFreq);
  escribirRegistro(msbFreq);
  escribirRegistro(bitsFase); //Registro de fase (AN1017)
  escribirRegistro(onda);     //ExitReset con forma de onda
}
