//LIB INCLUDE
#include <LiquidCrystal.h>
#include <EEPROM.h>
#include <Stepper.h>

//DEBUG DEFINITIONS
#define DEBUG //SHOW TONS OF DEBUG MESSAGES VIA SERIAL
#define ACCEL //USE GOTOSMOOTH INSTEAD OF GOTO

//S-CURVE GENERATION
#define SMOOTHSTEP(x) ((x) * (x) * (3 - 2 * (x))) // << SE PUEDE APLICAR VARIAS VECES SOBRE SI MISMA
#define SMOOTHERSTEP(X) x*x*x*(x*(x*6 - 15) + 10) // << UN POCO MAS SUAVE QUE LA ANTERIOR

//VALVE RELATED VARIABLES
#define PIN_V0 4 // PIN VALVULA 0
#define PIN_V1 5
#define PIN_V2 6
#define PIN_V3 7
#define PIN_V4 8
#define PIN_V5 13
#define PIN_V6 14
#define PIN_V7 15

#define POS_V0 100
#define POS_V1 200
#define POS_V2 300
#define POS_V3 400
#define POS_V4 500
#define POS_V5 600
#define POS_V6 700
#define POS_V7 800

#define V0_EEPROM 10

//STEPPER RELATED VARIABLES
#define STP1  9 // PINES DEL MOTOR PAP
#define STP2 10 
#define STP3 11
#define STP4 12
#define STP_MAX 200
#define STP_MIN 10
#define STP_DEFAULTSPEED 75
#define STP_HOMESPEED 25
#define STP_STEPS 200
#define STP_MAXPOS 1000
#define STP_PULLEY 0.008

//LCD RELATED VARIABLES
#define LCD_RS 46
#define LCD_EN 48
#define LCD_D4 38
#define LCD_D5 40
#define LCD_D6 42
#define LCD_D7 44

//LIMIT SWITCH PIN
#define PIN_FDC 2 //N(ormally)O(pen)

//CUP SENSOR PIN
#define CUP_SENSOR 16 // INTERNAL PULLUP, TIRAR A GND CUANDO HAY UN VASO

//ANALOG VALUES OF EACH BUTTON
#define A_default 1000
#define A_sel 638
#define A_left 410
#define A_down 257
#define A_up 101
#define A_right 0

//NUMBER OF...
#define NUM_VALVES 8
#define NUM_DRINKS 8
#define NUM_PRESET 2

//LINEAR TRANSFORMATION VARIABLES
#define CAUDAL 0.01
#define POS_POR_PASO 0.1

//DRINK NAME DECLARATION. DEFINE EVERY POSIBLE DRINK HERE.
//---------------------------------------------------------
//  both arrays must be in the same order!
//  alcoholic drinks should go first, then sodas and mixers
//---------------------------------------------------------
String bebidas_s[NUM_DRINKS] = {"FERNET", "VODKA","RON", "TEQUILA", "COCA COLA", "NARANJA", "GANCIA", "SPRITE"};
enum bebidas {FERNET = 0, VODKA, RON, TEQUILA, COCA_COLA, NARANJA, GANCIA, SPRITE};

//PRE-CONFIGURED COCKTAILS GO HERE
//---------------------------------------------------------
//  this string holds the names shown in the lcd
//  array with drink and % is made in menuPer().
//---------------------------------------------------------
String bebidas_preset_s[NUM_PRESET] = {"FERNET-COLA", "DESTORNILLADOR"};

//BUTTON ENUM
enum BOTONES {DEF, SELECT,  ARRIBA,  ABAJO,  IZQUIERDA,  DERECHA};

typedef struct valve_s {
    byte index;
    byte drink;
    byte pin;
    byte active;
    long pos;
} valve_t;

valve_t valve[NUM_VALVES];

//OBJECTS DECLARATION
LiquidCrystal lcd (LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
Stepper stepper(STP_STEPS, STP1, STP2, STP3, STP4);

char sepChar = ' ';
char subSepChar = ';';
String bufferString = "";
long currentPos = 0;

void setup() {
  Serial.begin(9600);
  stepper.setSpeed(STP_DEFAULTSPEED);
  lcd.begin(16,2);
  lcd.print("barduino");  //TODO: funcion para imprimir por lcd
  
  pinSetup();
  posSetup();
  readEEPROM();  
  
  goHome();
  
  Serial.println(F("BARDUINO IS READY"));
}

void loop() {
  serialEvent();
  menuPpal();
}

int readButtons(){
  byte i = 0;
  int lastAnalog = analogRead(A0);
  int analog = lastAnalog;
  int error = 50;

  if (analog > (A_default - error)) return DEF;
  if (analog < (A_right + error))   return DERECHA;
  if (analog < (A_up + error))      return ARRIBA;
  if (analog < (A_down + error))    return ABAJO;
  if (analog < (A_left + error))    return IZQUIERDA;  
  if (analog < (A_sel + error))     return SELECT;  

  return DEF;
}

void lcd_printBreak(String s, byte i) {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(s.substring(0,i));
  lcd.setCursor(0,1);
  lcd.print(s.substring(i+1));
}

void lcd_print(String s) {
  lcd.clear();
  lcd.setCursor(0,0);
  if (s.length()<16) {
    lcd.print(s);
  } else if (s.length()<32) {
    lcd.print(s.substring(0,15));
    lcd.setCursor(0,1);
    lcd.print(s.substring(16));    
  } else {
    lcd.autoscroll();
    lcd.print(s);
    lcd.noAutoscroll();    
  }
}

void lcd_print1p(String s, String arg){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(s);
  lcd.setCursor(0,1);
  lcd.print(arg);
}

void lcd_print1p1s(String s1, String arg1, String s2){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(s1+" "+arg1);
  lcd.setCursor(0,1);
  lcd.print(s2);
}

void lcd_printSpeed(float s1, float s2){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(String(("%4.2f",s1)) + " m/s");
  lcd.setCursor(0,1);
  lcd.print(String(("%4.2f",s1)) + " RPM");
}

void lcd_print2l(String l1, String l2){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(l1);
  lcd.setCursor(0,1);
  lcd.print(l2);
}

void menuPpal(){
  int i=0,j=0;
  int li=1, lj=1;
  int b;
  int last=0;  

  do {
    b = readButtons();
    delay(5);
    #ifdef DEBUG
      Serial.println("i: "+String(i)+"\nj: "+String(j)+"\nb: "+String(b));
    #endif
    
    if (b != last) {
      last = b;
      switch (b){
        case ABAJO:      
        break;
        case ARRIBA:
        break;
        case IZQUIERDA:
        i--;
        break;
        case DERECHA:
        i++;
        break;
        case SELECT:
        break;
        default:
        break;
      }
    }

    if (i>2) i = 0;
    if (i<0) i = 2;
    


    if (i != li || j != lj){
      li = i;
      lj = j;
      switch (i){
        case 0:
          lcd_printBreak("BEBIDASPRECONFIGURADAS", 6);
          break;  
        case 1:
          lcd_printBreak("BEBIDAPERSONALIZADA", 5);
          break;  
        case 2:
          lcd_print("CONFIGURACION");
          break;  
        default:
          break;
      }   
    }  
  } while (b != SELECT);

  switch (i) {
    case 0:
      menuTam(0);
      break;
    case 1:
      menuTam(1);
      break;
    default:
      break;
  }
}

void menuTam(int next){
  int j=0;
  int lj=1;
  int b;
  int last=0;  

  do {
    b = readButtons();
    delay(5);
    
    if (b != last) {
      last = b;
      switch (b){
        case ABAJO:   
        j-=25;   
        break;
        case ARRIBA:
        j+=25;
        break;
        case IZQUIERDA:
        j-=25; 
        break;
        case DERECHA:
        j+=25;
        break;
        case SELECT:
        break;
        default:
        break;
      }
    }

    if (j>500) j = 500;
    if (j<0) j = 0;
    


    if (j != lj){
      lj = j;      
      lcd_print1p("CAPACIDAD VASO:", String(j));
    }  
  } while (b != SELECT);

  switch (next) {
    case 0:
      menuPre(j);
      break;
    case 1:
      menuPer(j);
      break;
    default:
      break;
  }
}

void menuPre(int v){
  int j=0;
  int lj=-1;
  int p[NUM_PRESET] = {0};
  int lp[NUM_PRESET] = {0};
  int b;
  int last=SELECT;

  do {
    b = readButtons();
    delay(5); // SI NO ANDA PROBAR CON 10

    if (b != last) {
      last = b;
      switch (b){
        case ABAJO:
        p[j]-=5;    
        break;
        case ARRIBA:
        p[j]+=5;  
        break;
        case IZQUIERDA:
        j--;
        break;
        case DERECHA:
        j++;
        break;
        case SELECT:
        break;
        default:
        break;
      }
    } else {
      b = DEF;
    }

    if (j>=NUM_PRESET) j = 0;
    if (j<0) j = NUM_PRESET - 1;
    switch (j) { // POR SI QUIERO AGREGAR CONCENTRACIONES MAXIMAS Y MINIMAS.
      case 0: // FERNET
        if (p[j]<15) p[j]=15;
        if (p[j]>40) p[j]=40;
        break;
      case 1: // DESTORNILLADOR
        if (p[j]<15) p[j]=15;
        if (p[j]>40) p[j]=40;
        break;
      default:
      break;        
    }

    if (j != lj){
      lj = j;
      switch (j) { // POR SI QUIERO AGREGAR CONCENTRACIONES MAXIMAS Y MINIMAS.
      case 0: // FERNET
        lcd_print1p(bebidas_preset_s[j], String(p[j]));
        break;
      case 1: // DESTORNILLADOR
        lcd_print1p(bebidas_preset_s[j], String(p[j]));
        break;      
      default:
        lcd_print(bebidas_preset_s[j]); //en otro caso solo muestro el nombre;
        break;        
    }
    }  
  } while (b != SELECT);

  switch (j) {
    int d[NUM_VALVES][2]; //DECLARO ACA EL MAXIMO DE BEBUDAS INDIVIDUALES Q PUEDE HABER
    
    case 0: //FERNET
      d[0][0] = FERNET;
      d[1][0] = COCA_COLA;
      d[0][1] = floor((float)v*p[j]/100);
      d[1][1] = floor((float)v*(100 - p[j])/100);

      if (validateInput(v, d, 2)) {
        make(d, 2);
      } else {
        lcd_printBreak("ERROR DECOMPOSICION",7);
      }
      break;
    case 1: //DESTORNILLADOR
      d[0][0] = VODKA;
      d[1][0] = NARANJA;
      d[0][1] = floor((float)v*p[j]/100);
      d[1][1] = floor((float)v*(100 - p[j])/100);

      if (validateInput(v, d, 2)) {
        lcd_printBreak("HACIENDOBEBIDA...", 7);
        make(d, 2);
      } else {
        lcd_print("ERROR");
      }
      break;
    default:
      break;
  }
}

void menuPer(int v){
  int j=0;
  int lj=-1;
  byte lp[NUM_VALVES]={0};
  byte porcentaje[NUM_VALVES]={0};
  int b;
  int last=SELECT;
 
  do {
    b = readButtons();
    delay(10);
    //Serial.println("j: "+String(j)+"\nb: "+String(b));

    if (b != last) {
      last = b;
      switch (b){
        case ABAJO:   
        porcentaje[j]-=5;     
        break;
        case ARRIBA:
        porcentaje[j]+=5;
        break;
        case IZQUIERDA:
        j--;
        break;
        case DERECHA:
        j++;
        break;
        case SELECT:
        break;
        default:
        break;
      }
    } else {
      b = DEF;
    }

    if (!(valve[j].active)) j++; // SI NO  ESTA LLENA LA VALVULA ME SALTEO.
    
    if (j>=NUM_VALVES) j = 0;
    if (j<0) j = NUM_VALVES -1;
    if (porcentaje[j] > 100) porcentaje[j] = 0;
    if (porcentaje[j]<0) porcentaje[j]=100;


    if (j != lj || porcentaje[j] != lp[j]){
      lj = j;
      lp[j]=porcentaje[j];
      lcd_print1p(bebidas_s[valve[j].drink], String(porcentaje[j])+"%");
     }   
  } while (b != SELECT);

  int d[NUM_VALVES][2];
  int cant=0;
  for (int i = 0; i<NUM_VALVES; i++){
    if (valve[i].active) {
      if (porcentaje[i]) {        
        d[cant][0] = valve[i].drink;
        d[cant][1] = floor((float)v*porcentaje[i]/100);

        cant++;        
      }
    }    
  }

  if (validateInput(v, d, cant)) {
    make(d,cant);
  } else {
    lcd_print("ERROR");
  }
}

void menuConf(){
  int j=0;
  int lj=-1;
  int b;
  int last=SELECT;

  do {
    b = readButtons();
    delay(5); // SI NO ANDA PROBAR CON 10

    if (b != last) {
      last = b;
      switch (b){
        case ABAJO:    
        break;
        case ARRIBA:
        break;
        case IZQUIERDA:
        j--;
        break;
        case DERECHA:
        j++;
        break;
        case SELECT:
        break;
        default:
        break;
      }
    } else {
      b = DEF;
    }

    if (j>2) j = 0;
    if (j<0) j = 2;

    if (j != lj){
      switch (j){
        case 0:
          lcd_print("ABRIR VALVULA");
        break;
        case 1:
          lcd_print("CERRAR VALVULA");
        break;
        case 2:
          lcd_print("DEBUG");
        break;
      }
    }  
  } while (b != SELECT);

  switch (j){
        case 0:
          menuSet();
        break;
        case 1:
          menuClose();
        break;
        case 2:
          menuDebug();
        break;
      }
}

void menuSet(){
  int j=0;
  int lj=-1;
  int v=0;
  int lv=-1;
  int b;
  int last=SELECT;

  do {
    b = readButtons();
    delay(5); // SI NO ANDA PROBAR CON 10

    if (b != last) {
      last = b;
      switch (b){
        case ABAJO:
        v--;  
        break;
        case ARRIBA:
        v++;
        break;
        case IZQUIERDA:
        j--;
        break;
        case DERECHA:
        j++;
        break;
        case SELECT:
        break;
        default:
        break;
      }
    } else {
      b = DEF;
    }

    if (j>=NUM_DRINKS) j = 0;
    if (j<0) j = NUM_DRINKS;
    if (v >= NUM_VALVES) v = 0; //OJO, LAS VALVULAS VAN DEL 0 AL 7, NUM_VALVES = 8!!
    if (v < 0) v = NUM_VALVES -1;

    if (j != lj || v != lv){
      lj = j;
      lv = v;

      lcd_print1p1s("VALVULA:", String(v), bebidas_s[j]);      
    }  
  } while (b != SELECT);

  setUpValve(v,j);  
}

void menuClose(){
  int j=0;
  int lj=-1;
  int b;
  int last=SELECT;

  do {
    b = readButtons();
    delay(5); // SI NO ANDA PROBAR CON 10

    if (b != last) {
      last = b;
      switch (b){
        case ABAJO:    
        break;
        case ARRIBA:
        break;
        case IZQUIERDA:
        j--;
        break;
        case DERECHA:
        j++;
        break;
        case SELECT:
        break;
        default:
        break;
      }
    } else {
      b = DEF;
    }

    if (j>= NUM_VALVES) j = 0;
    if (j<0) j = NUM_VALVES -1;

    if (j != lj){
      lcd_print1p("CERRAR VALVULA:", String(j));
    }  
  } while (b != SELECT);

  closeValve(j);
}

void menuDebug(){
  int j=0;
  int lj=-1;
  int b;
  int last=SELECT;
  int arg[2] = {0};
  int larg[2] = {0};

  do {
    b = readButtons();
    delay(5); // SI NO ANDA PROBAR CON 10

    if (b != last) {
      last = b;
      switch (b){
        case ABAJO:   
        arg[j] -= 10; 
        break;
        case ARRIBA:
        arg[j] += 10; 
        break;
        case IZQUIERDA:
        j--;
        break;
        case DERECHA:
        j++;
        break;
        case SELECT:
        break;
        default:
        break;
      }
    } else {
      b = DEF;
    }

    if (j>2) j = 0;
    if (j<0) j = 2;
    if (arg[j] > STP_MAXPOS) arg[j]=STP_MAXPOS;
    if (arg[j] < 10) arg[j]=10; //NO QUIERO Q LO PUEDAN HACER HOME DESDE EL GOTO PORQUE NO PUSE UNA INTERRUPCION EN EL FDC

    if (j != lj || arg[j] != larg[j]){
      lj = j;
      larg[j]=arg[j];
      
      switch (j){
        case 0:
          lcd_print1p("IR A (V CTE)",String(arg[j]));
          break;
        case 1:
          lcd_print1p("IR A (ACCEL)",String(arg[j]));
          break;
        case 2:
          lcd_print("IR A HOME");
        break;
      }
    }  
  } while (b != SELECT);

  switch (j){
        case 0:
          stepper.setSpeed(STP_MIN);
          goTo(arg[0]);
          break;
        case 1:
          goToSmooth(arg[1]);
          break;
        case 2:
          goHome();
          break;
      }
}

void firstTimeSetUp(){
  int i;
  EEPROM.write(254, 0);
  Serial.println(F("PERFORMING FIRST TIME SET UP"));
  for (i = 0; i < NUM_VALVES; i++){
    valve[i].active = 0;
    EEPROM.update(V0_EEPROM + i, 0);
  }
}

void posSetup(){
  valve[0].pos = POS_V0;
  valve[1].pos = POS_V1;
  valve[2].pos = POS_V2;
  valve[3].pos = POS_V3;
  valve[4].pos = POS_V4;
  valve[5].pos = POS_V5;
  valve[6].pos = POS_V6;
  valve[7].pos = POS_V7;
}

void pinSetup(){
  pinMode(PIN_V0, OUTPUT);
  pinMode(PIN_V1, OUTPUT);
  pinMode(PIN_V2, OUTPUT);
  pinMode(PIN_V3, OUTPUT);
  pinMode(PIN_V4, OUTPUT);
  pinMode(PIN_V5, OUTPUT);
  pinMode(PIN_V6, OUTPUT);
  pinMode(PIN_V7, OUTPUT);
  pinMode(PIN_FDC, INPUT);
  pinMode(STP1, OUTPUT);
  pinMode(STP2, OUTPUT);
  pinMode(STP3, OUTPUT);
  pinMode(STP4, OUTPUT);
  pinMode(CUP_SENSOR, INPUT_PULLUP);

  valve[0].pin = PIN_V0;
  valve[1].pin = PIN_V1;
  valve[2].pin = PIN_V2;
  valve[3].pin = PIN_V3;
  valve[4].pin = PIN_V4;
  valve[5].pin = PIN_V5;
  valve[6].pin = PIN_V6;
  valve[7].pin = PIN_V7;
}

void serialEvent(){
  char inputChar;
  while (Serial.available()) {
    inputChar = (char)Serial.read();
    if ((inputChar >= 33 && inputChar <= 126) || inputChar == 10 || inputChar == 32) {
      if (inputChar != '\n'){
        bufferString += inputChar;
      } else {
        #ifdef DEBUG
          Serial.println(bufferString);
        #endif
        parse(bufferString);
        bufferString = "";
      }
    }
  }  
}

void setUpValve (int index, int drinkCode){
  valve[index].active = 1;
  EEPROM.update(V0_EEPROM + index, 1);
  valve[index].drink = drinkCode;
  EEPROM.update(V0_EEPROM + NUM_VALVES + index, drinkCode);
  #ifdef DEBUG
    Serial.println("VALVE "+String(index)+" SET TO DRINK "+String(drinkCode));
  #endif
}

void closeValve (int index){
  valve[index].active = 0;
  EEPROM.update(V0_EEPROM + index, 0);
  #ifdef DEBUG
    Serial.println("VALVE "+String(index)+" NO LONGER AVAILABLE");
  #endif
}

int stringOcurr(String str, char subS){
  int i,  last=0, c=0;
  while (str.indexOf(subS, last) != -1){
    c++;
    last = str.indexOf(subS, last);
    last++;
  }
  return c;
}

void parse(String str) {
  int i;
  int argCount = stringOcurr(str, sepChar)+1; //busco la cantidad de espacios para no definir arg[] grande al pedo  
  String arg[argCount]; // make 500 1;50 2;50
  int numArg = 0;

  for (i = 0; i<str.length(); i++){
    if (str[i] != sepChar) {
      arg[numArg] += str[i];
    } else {
      numArg++;
      //Serial.println(numArg);
    }
  }
  //Serial.println("parse");
  cmdToAction(arg, argCount);
}

void readEEPROM(){
  int i;
  //#ifdef DEBUG
    Serial.println(F("READING VALVE DATA FROM EEPROM."));
  //#endif

  if (EEPROM.read(254)==255) { 
    firstTimeSetUp();
  } else {
    for (i = 0; i < NUM_VALVES; i++){
      valve[i].active = EEPROM.read(V0_EEPROM +i);
      if (valve[i].active) {
        valve[i].drink = EEPROM.read(V0_EEPROM + NUM_VALVES + i);
        #ifdef DEBUG
          Serial.println("VALVE "+ String(i)+" ACTIVE WITH DRINK "+ String(valve[i].drink));
        #endif
      }
    }
  }
    
}

void cmdToAction(String arg[],int numArg){  
  arg[0].toUpperCase();
  #ifdef DEBUG
    Serial.println("COMMAND: "+arg[0]);
  #endif
  
  
  //EJEMPLO DE BEBIDAS PERSONALIZADAS
  if (arg[0] == "MAKE"){ // MAKE volumen_del_vaso cod_bebida;porcentaje cod_bebida;porcentaje ...
    if (numArg > 2){
      int drinkArray [numArg-2][2];
      int vol = round(arg[1].toInt() * 0.8); //multiplico por 0.8 para darme un poco de juego
      int sepPos;
      #ifdef DEBUG
        Serial.println("numARG = "+String(numArg)+ ". BEBIDAS = "+String(numArg-1));
      #endif  
      
      for (int i = 2; i < numArg; i++) {
        sepPos = arg[i].indexOf(subSepChar);
        drinkArray[i-2][0] = arg[i].substring(0,sepPos).toInt(); //TODO: decidir si le paso las bebidas como numero o como string
        drinkArray[i-2][1] = floor(arg[i].substring(sepPos+1).toInt() * (vol / 100.0)); //redondeo para abajo para asegurarme de que no me paso del volumen total 
        #ifdef DEBUG
          Serial.println("DRINK: "+String(drinkArray[i -2][0]));
          Serial.println("VOL: "+String(drinkArray[i-2][1]));
        #endif 
      } 
         
      if (validateInput(vol, drinkArray,numArg-2)) {
        make(drinkArray, numArg -2);
      } else {
        Serial.println(F("COMPOSITION ERROR"));
      }
    } else {
      Serial.println(F("NOT ENOUGH ARGUMENTS"));
    }
  } else if (arg[0]== "SET"){ // SET index cod_bebida
    if (arg[1].toInt()>=0 && arg[1].toInt()<NUM_VALVES){
      setUpValve(arg[1].toInt(), arg[2].toInt()); //TODO: validar el codigo de bebidas.
    } else {
      Serial.println(F("VALVE DOES NOT EXIST!"));  
    }
  } else if (arg[0]== "SPEED"){ // SET index cod_bebida
    if (numArg = 2) {
      if (arg[1].toInt() <= STP_MAX && arg[1].toInt() >= STP_MIN) {
        stepper.setSpeed(arg[1].toInt());
        //#ifdef DEBUG
          Serial.println("SET SPEED TO: "+String(arg[1].toInt()));
        //#endif
      } else {
        Serial.println(F("SPEED MUST BE LOWER THAN 100"));
      }
      
    } else {
      Serial.println(F("NOT ENOUGH ARGUMENTS"));
    }
  } else if (arg[0]== "STEP"){ // SET index cod_bebida
    if (numArg = 2) {
      Serial.println("DOING "+ String(arg[1].toInt())+" STEPS");      
        stepper.step(arg[1].toInt());        
    } else {
      Serial.println(F("NOT ENOUGH ARGUMENTS"));
    }
  } else if (arg[0]== "CLOSE"){ // SET index cod_bebida
    if (numArg >= 2) {
      for (int i = 1; i < numArg; i++){
        if (arg[i].toInt()>=0 && arg[i].toInt()<NUM_VALVES){
          if (valve[arg[i].toInt()].active){
            closeValve(arg[i].toInt());
          } else {
            Serial.println(F("VALVE AREADY CLOSED"));
          }
        } else {
          Serial.println(F("VALVE DOES NOT EXIST!"));  
        }
      }
    } else {
      Serial.println(F("NOT ENOUGH ARGUMENTS"));
    }
  } else if (arg[0]== "STATUS"){ // STATUS index 
    if (arg[1].toInt()>=0 && arg[1].toInt()<NUM_VALVES){
      if (valve[arg[1].toInt()].active){
        Serial.println("VALVE "+ String(arg[1].toInt()) +" IN USE WITH DRINK "+String(valve[arg[1].toInt()].drink));
      } else {
        Serial.println("VALVE "+ String(arg[1].toInt()) + " NOT IN USE");
      }
    } else {
      Serial.println(F("VALVE DOES NOT EXIST!"));  
    }
  } else if (arg[0]=="HOME"){
    goHome();
  } else if (arg[0]=="GOTO"){
    #ifdef ACCEL
      goToSmooth(arg[1].toInt());
    #else
      goTo(arg[1].toInt());
    #endif       
  } else if (arg[0]=="CODES"){
    for (int i = 0; i<NUM_DRINKS; i++){
      Serial.println(bebidas_s[i] + ": " + String(i));
    }
  } else {
    Serial.println(F("COMMAND NOT FOUND"));
  }

//EJEMPLO DE BEBIDAS PRE-ESTABLECIDAS
//  if (arg[0] == "FERNET"){ // FERNET volumen_del_vaso porcentaje
//    if (numArg == 2){//volumen = arg[1]
//      //int drinkArray [numArg-1][2];
//      drinkArray[0][0] = 11111; //codigo del fernet
//      drinkArray[0][1] = arg[2].toInt();
//      drinkArray[1][0] = 11111; //codigo de la coca
//      drinkArray[1][1] = 100 - arg[2].toInt();
//
//      if (validateInput(drinkArray,numArg-2)) {
//        delay(1);// pasar a otra funcion
//      } else {
//        delay(1); 
//      }
//    } else {
//      delay(1); //error
//    }   
//  }
}

boolean validateInput(int v,int d[][2], int dim){
  int i;
  int porcentajeSum = 0;

  for (i = 0; i<dim; i++){
    if (getValveIndexFromDrink(d[i][0])== -1){ //si no esta disponible la bebida, tiro error
      #ifdef DEBUG
        Serial.println(F("VALIDATION NOT OK"));
      #endif
      return 0;
    }
    porcentajeSum += d[i][1];
  }

  #ifdef DEBUG
    Serial.print("VALIDATION ");
    Serial.println((porcentajeSum <= v)?"OK":"NOT OK");
  #endif
  
  return (porcentajeSum <= v);  
}

void pour(int pin, int cant) {
  long t;
  
  t = round((float)cant/CAUDAL); //TODO: ver tema de unidades.
  
  #ifdef DEBUG
    Serial.println("OPEN VALVE ON PIN "+String(pin)+" FOR "+String(t)+" MILLIS");
  #endif
  
  digitalWrite(pin, HIGH);
  delay(t);
  digitalWrite(pin, LOW);
  
  #ifdef DEBUG
    Serial.println("VALVE ON PIN "+String(pin)+" CLOSED");
  #endif
}

int getValveIndexFromDrink(int d){
  int i;

  #ifdef DEBUG
    Serial.println("getValveFromDrink GETS "+ String(d) +" AS ARGUMENT");
  #endif
  
  for (i = 0; i < NUM_VALVES; i++){
    if (valve[i].drink == d){
      #ifdef DEBUG
        Serial.println("DRINK "+String(d)+" FOUND IN VALVE "+String(i));
      #endif
      return i;
    }
  }

  #ifdef DEBUG
    Serial.println("DRINK "+ String(d) +" NOT FOUND");
  #endif
  
  return -1;
}

bool cupPresent(){
  return !digitalRead(CUP_SENSOR);
}

void make(int d[][2], int dim){
  int i, v_i;
  if (currentPos != 0) {
    #ifdef DEBUG 
      Serial.println("NOT AT HOME WHEN MAKE()");
    #endif
      goHome();
  }

  if (!cupPresent()){
    while (!cupPresent());
  }

  for (i = 0; i<dim; i++){
      v_i = getValveIndexFromDrink(d[i][0]);
      #ifdef DEBUG 
        Serial.println("getValveFromDrink RETURNS "+ String(v_i));
      #endif
      #ifdef ACCEL
        goToSmooth(valve[v_i].pos);
      #else
        goTo(valve[v_i].pos);
      #endif
      lcd_print2l(bebidas_s[valve[v_i].drink], String(d[i][1])+" ml");
      pour(valve[v_i].pin, d[i][1]);
  }

  //TODO: revolver bebida ;) ;)
  if (currentPos>10) goToSmooth(10);
  goHome();

  lcd_print("RETIRE EL VASO!");  

  if (cupPresent()){
    while (cupPresent());
  }

  lcd_printBreak("DISFRUTESU BEBIDA!",7);
  delay(2500);
  
  #ifdef DEBUG
    Serial.println("ENJOY YOUR DRINK!!");
  #endif 
}

long distanceToSteps(long pos) {
  #ifdef DEBUG
    Serial.println("DISTANCE: "+String(pos)+" -> STEPS: "+String(round(pos/POS_POR_PASO)));
  #endif
  return round(pos/POS_POR_PASO);
}

void goTo(long pos){
  #ifdef DEBUG
    Serial.println("GOING TO POS "+String(pos));
  #endif
  stepper.step(distanceToSteps(pos - currentPos)); // diferencia entre la posición de destino y la actual.
  currentPos = pos;                                // hay que configurar los cables del stepper para que step(1) valla hacia afuera
                                                   // y step(-1) valla hacia adentro.
}

void goToSmooth(long pos){
  //Serial.println(pos);
  long steps = distanceToSteps(pos - currentPos);
  int dir = (steps >= 0)?1:-1;
  int stepGap;
  int i;
  int N;
  float v,x,vmax;
  long stepsdone=0;
  
  steps = dir*steps; // valor absoluto
  N = floor(steps*0.05);  
  stepGap=floor(N*0.025);
  if (!stepGap) stepGap = 1;
  #ifdef DEBUG
    Serial.println("STEPS: "+String(steps)+"\nN: "+String(N)+"\nstepGap: "+String(stepGap));
  #endif

  for (i = 0; i < N; i+=stepGap)
  {
    v = (float)i / N;
    v=SMOOTHSTEP(v);
    x = (STP_MAX * v) + (STP_MIN * (1 - v)); //VELOCIDAD INICAL != 0
    stepper.setSpeed(x);
    lcd_printSpeed(x, STP_PULLEY*x*3.14/30);
    Serial.println("VEL "+String(x));
    if (i+stepGap<=N) {
      stepper.step(dir*stepGap);
      stepsdone += stepGap;
    } else {
      stepper.step(dir*((i+stepGap)-N));
      stepsdone += ((i+stepGap)-N);
    }
  }
  
  Serial.println("VEL MAX= "+String(x));
  
  stepper.step(dir*(steps-2*stepsdone));
  stepsdone += steps-2*stepsdone;
  
  vmax = x; //arranco a desacelerar desde la velocidad a la que iba.
  
  for (i = 0; i < N; i+=stepGap)
  {
    v = 1.0*i/N;
    v = SMOOTHSTEP(v);
    x = (STP_MIN * v) + (vmax * (1 - v));
    stepper.setSpeed(x);
    lcd_printSpeed(x, STP_PULLEY*x*3.14/30);
    Serial.println("VEL "+String(x));
    if (i+stepGap<=N) {
      stepper.step(dir*stepGap);
      stepsdone += stepGap;
    } else {
      stepper.step(dir*((i+stepGap)-N));
      stepsdone += ((i+stepGap)-N);
    }
  }
  Serial.println("VEL FINAL= "+String(x)+"\nSTEPSCOMPUTADOS: "+String(stepsdone));
  currentPos=pos;
}

void goHome(){
  #ifdef DEBUG
    Serial.println(F("GOING HOME, WAITING FOR LIMIT SWITCH"));
  #endif

  stepper.setSpeed(STP_HOMESPEED);
  while (digitalRead(PIN_FDC) == LOW){
    stepper.step(-1);
  }
  currentPos = 0;
  #ifdef DEBUG
    Serial.println(F("HOME OK"));
  #endif
}

