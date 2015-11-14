//TODO LIST:
// + TEST NEW ACC CURVE WITH POLOLU A4988
// + TEST AND DEBUG LCD MENU
// + ADD A MASTER PRINT FUNCTION, lcd_master(byte index)
//   EACH INDEX REPRESENTS ONE MESSAGE AND CAN BE STORED 
//   INSIDE FLASH MEMORY VS SRAM WITH F() 
// + LOAD MORE DRINKS AND MORE COCKTAILS
// + TRY TO OPTIMIZE MEMORY USAGE
// + 1st FLOW TEST: 8,064 ML/SEG = 0,008064 ML/MILLISEG
// + 2nd FLOW TEST:

//LIB INCLUDE
#include <LiquidCrystal.h>
#include <EEPROM.h>
//#include <Stepper.h>

//DEBUG DEFINITIONS
#define DEBUG //SHOW TONS OF DEBUG MESSAGES VIA SERIAL
#define ACCEL //USE GOTOSMOOTH INSTEAD OF GOTO
#define LCD

//S-CURVE GENERATION
#define SMOOTHSTEP(x) ((x) * (x) * (3 - 2 * (x))) // << SE PUEDE APLICAR VARIAS VECES SOBRE SI MISMA
#define SMOOTHERSTEP(x) x*x*x*(x*(x*6 - 15) + 10) // << UN POCO MAS SUAVE QUE LA ANTERIOR

//VALVE RELATED VARIABLES
#define PIN_V0 21 // PIN VALVULA 0
#define PIN_V1 22
#define PIN_V2 23
#define PIN_V3 24
#define PIN_V4 25
#define PIN_V5 26
#define PIN_V6 27
#define PIN_V7 28

#define POS_V0 100
#define POS_V1 200
#define POS_V2 300
#define POS_V3 400
#define POS_V4 500
#define POS_V5 600
#define POS_V6 650
#define POS_V7 699

#define V0_EEPROM 10

//STEPPER RELATED VARIABLES
#define STP_STEP 11 // PINES DEL MOTOR PAP
#define STP_DIR 12
#define STP_ENA 13
#define STP_MODE 1 //1 = FULL STEP, 2 = HALF-STEP, 4 = 1/4 STEP, 8 = 1/8 STEP
#define STP_ACC 500 //STEPS/SEG/seg
#define STP_MAX 200 //RPM
#define STP_MIN 30 //RPM
#define STP_DEFAULTSPEED 30 //RPM
#define STP_HOMESPEED 30 //RPM
#define STP_STEPS 200 //RPM
#define STP_MAXPOS 700 // mm desde HOME
#define STP_PULLEY 0.05 // cm, para que v_lin = [cm/s]. << si resulta muy chico pasar a mm, um, pm, etc

//LCD RELATED VARIABLES
#define LCD_RS 8
#define LCD_EN 9
#define LCD_D4 4
#define LCD_D5 5
#define LCD_D6 6
#define LCD_D7 7

//ACCELERATION CURVE CONSTANTS
#define ACC_N 0.075
#define ACC_S 0.05

//LIMIT SWITCH PIN
#define PIN_FDC 2 //N(ormally)O(pen)

//CUP SENSOR PIN
#define CUP_SENSOR 24 // INTERNAL PULLUP, NORMALLY OPEN ,TIRAR A GND CUANDO HAY UN VASO

//ANALOG VALUES OF EACH BUTTON
#define A_default 1000
#define A_sel 638
#define A_left 410
#define A_down 257
#define A_up 101
#define A_right 0

//NUMBER OF...
#define NUM_VALVES 8
#define NUM_DRINKS 10
#define NUM_PRESET 6

//LINEAR TRANSFORMATION VARIABLES
#define CAUDAL 0.008
#define STEPS_X_MM 10 //STEPS X MILIMETRO // POLEA:STEPS 20:5 16:6.25

//DRINK NAME DECLARATION. DEFINE EVERY POSIBLE DRINK HERE.
//---------------------------------------------------------
//  * both arrays must be in the same order!
//---------------------------------------------------------
String bebidas_s[NUM_DRINKS] = {"FERNET", "VODKA","RON", "TEQUILA", "COCA COLA", "NARANJA", "GANCIA", "SPRITE", "WHISKEY", "CAMPARI"};
enum bebidas {FERNET = 0, VODKA, RON, TEQUILA, COCA_COLA, NARANJA, GANCIA, SPRITE, WHISKEY, CAMPARI};

//PRE-CONFIGURED COCKTAILS GO HERE
//---------------------------------------------------------
//  * this string holds the names shown in the lcd
//  * array with drink and % is made in menuPer().
//  * in_preset holds a pseudo truth table where each cocktail
//  declares what drinks it needs
//---------------------------------------------------------
String bebidas_preset_s[NUM_PRESET] = {"FERNET-COLA", "DESTORNILLADOR", "WHISK-COLA", "CUBA LIBRE", "CAMPARI C/NARJ", "GANCIA C/SPRITE"};

byte in_preset [NUM_PRESET][NUM_DRINKS] {
   {1, 0, 0, 0, 1, 0, 0, 0, 0, 0}, //FERNET CON COCA
   {0, 1, 0, 0, 0, 1, 0, 0, 0, 0}, //VODKA CN NARANJA
   {0, 0, 0, 0, 1, 0, 0, 0, 1, 0}, //WHISKCOLA 
   {0, 0, 1, 0, 1, 0, 0, 0, 0, 0}, //CUBA LIBRE
   {0, 0, 0, 0, 0, 1, 0, 0, 0, 1}, //CAMPARI CON NARANJA
   {0, 0, 0, 0, 0, 0, 1, 1, 0, 0} //GANCIA C/ SPRITE
};


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
#ifdef LCD
LiquidCrystal lcd (LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
#endif

char sepChar = ' ';
char subSepChar = ';';
String bufferString = "";
long currentPos = 0;

void setup() {
  Serial.begin(9600);
  
  #ifdef LCD
  lcd.begin(16,2);
  lcd.print("barduino");  //TODO: funcion para imprimir por lcd
  #endif
  
  pinSetup();
  posSetup();
  readEEPROM();  
  goHome();
  
  Serial.println(F("BARDUINO IS READY"));
}

void loop() {
#ifdef LCD
  menuPpal();
#endif
#ifndef LCD
  serialEvent();
#endif
}

int readButtons(){
  byte i = 0;
  int analog = analogRead(A0);
  int error = 50;
  delay(5);

  if (analog > (A_default - error)) return DEF;
  if (analog < (A_right + error))   return DERECHA;
  if (analog < (A_up + error))      return ARRIBA;
  if (analog < (A_down + error))    return ABAJO;
  if (analog < (A_left + error))    return IZQUIERDA;  
  if (analog < (A_sel + error))     return SELECT;  

  return DEF;
}

#ifdef LCD
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
  lcd.print(String(("%4.2f",s1)) + " RPM");
  lcd.setCursor(0,1);
  lcd.print(String(("%4.2f",s2)) + " cm/s");
}

void lcd_print2l(String l1, String l2){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(l1);
  lcd.setCursor(0,1);
  lcd.print(l2);
}

#endif

byte canIDo(byte index){
  int i;
  for (i = 0; i<NUM_DRINKS; i++){
    Serial.println(i);
    if (in_preset[index][i]){
      if (getValveIndexFromDrink(i)== -1){
        Serial.println("NO PUEDO HACER");
        return 0;
      }
    }
  }  
  Serial.println("PUEDO HACER");
  return 1;
}

#ifdef LCD
void menuPpal(){
  int i=0;
  int li=-1;
  int b;
  int last=DEF;  
  Serial.println(F(">menuPpal"));
  do {
    serialEvent();
    b = readButtons();
    delay(5);
    
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
    
    if (i != li){
      li = i;
      switch (i){
        case 0:
          lcd_print2l("BEBIDAS","PRE-CONFIGURADAS");
          break;  
        case 1:
          lcd_print2l("BEBIDA","PERSONALIZADA");
          break;  
        case 2:
          lcd_print("CONFIGURACION");
          break;  
        default:
          break;
      }   
    }  
  } while ((b != SELECT));

  while (readButtons());
  switch (i) {
    case 0:      
      menuTam(i);      
      break;
    case 1:
      menuTam(i);      
      break;
    case 2:
      menuConf();
      break;      
    default:
      break;
  }
}

void menuTam(int next){
  int j=0;
  int lj=-1;
  int b;
  int last=SELECT;  
  Serial.println(F(">menuTam"));

  do {
    b = readButtons();
    delay(5);
    
    if (b != last) {
      last = b;
      switch (b){
        case ABAJO:   
        j-=25;   
        Serial.println(j);
        break;
        case ARRIBA:
        j+=25;
        Serial.println(j);
        break;
        case IZQUIERDA:
        j-=25; 
        Serial.println(j);
        break;
        case DERECHA:
        j+=25;
        Serial.println(j);
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
      lcd_print2l("CAPACIDAD VASO:", String(j)+" ml");
    }  
  } while (b != SELECT);

  
  if (j) {
    switch (next) {
      case 0:
      while (readButtons());
        menuPre(j);
        break;
      case 1:
      while (readButtons());
        menuPer(j);
        break;
      default:
        break;
    }
  } else {
    lcd_print2l("TAMANIO","INVALIDO");
    delay(3000);
  }
}

void menuPre(int v){
  int j=0;
  int lj=-1;
  int p[NUM_PRESET] = {0};
  int lp[NUM_PRESET] = {0};
  int b;
  int last=SELECT;
  int canI = 0;

  Serial.println(F(">menuPre"));

  for (int x = 0; x <NUM_PRESET; x++){
    if (canIDo(x)) {
      canI = 1;
      break;
    }
  }

  if (canI){     
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
          do {
            j--;
            if (j < 0) break;        
          } while (canIDo(j)==0);
          break;
          case DERECHA:
          do {
            j++;       
            if (j>=NUM_PRESET) break;   
          } while (canIDo(j)==0);
          break;
          case SELECT:
          break;
          default:
          break;
        }
      } else {
        b = DEF;
      }
  
      if (j>=NUM_PRESET) {
        j = 0;
        while (canIDo(j) == 0) j++;
      }
      if (j<0) {
        j = NUM_PRESET - 1;
        while (canIDo(j) == 0) j--;
      }
      switch (j) { // POR SI QUIERO AGREGAR CONCENTRACIONES MAXIMAS Y MINIMAS.
        case 0: // FERNET
        case 1: // DESTORNILLADOR
        case 2:
        case 3:
        case 4:
        case 5:
          if (p[j]<15) p[j]=15;
          if (p[j]>50) p[j]=50;
          break;
        default:
          if (p[j]<5) p[j]=5;
          if (p[j]>95) p[j]=95;
        break;        
      }
  
      if (j != lj || p[j] != lp[j]){
        lj = j;
        lp[j] = p[j];
        switch (j) { // POR SI QUIERO AGREGAR CONCENTRACIONES MAXIMAS Y MINIMAS.
        case 0: //FERNET
        case 1: //DESTORNILLADOR
        case 2: //WISK-COLA
        case 3: //CUBA LIBRE
        case 4: //CAMPARI
        case 5: //GANCIA SPRITE
          lcd_print1p(bebidas_preset_s[j], String(p[j])+"% - "+String(100-p[j])+"%");
          break; 
        default:
          lcd_print(bebidas_preset_s[j]); //en otro caso solo muestro el nombre;
          break;        
      }
      }  
    } while (b != SELECT);
  
    while (readButtons());

    int d[NUM_VALVES][2]; //DECLARO ACA EL MAXIMO DE BEBUDAS INDIVIDUALES Q PUEDE HABER
    int cant = 0;


    // DRINK MATRIX GENERATION
    switch (j) {     
      case 0: //FERNET
        cant = 2;
        d[0][0] = FERNET;
        d[1][0] = COCA_COLA;
        d[0][1] = floor((float)v*p[j]/100);
        d[1][1] = floor((float)v*(100 - p[j])/100);
        break;
      case 1: //DESTORNILLADOR
        cant = 2;
        d[0][0] = VODKA;
        d[1][0] = NARANJA;
        d[0][1] = floor((float)v*p[j]/100);
        d[1][1] = floor((float)v*(100 - p[j])/100);
        break;
      case 2: //WHISKCOLA
        cant = 2;
        d[0][0] = WHISKEY;
        d[1][0] = COCA_COLA;
        d[0][1] = floor((float)v*p[j]/100);
        d[1][1] = floor((float)v*(100 - p[j])/100);        
        break;
      case 3:
        cant = 2;
        d[0][0] = RON;
        d[1][0] = COCA_COLA;
        d[0][1] = floor((float)v*p[j]/100);
        d[1][1] = floor((float)v*(100 - p[j])/100);    
        break;
      case 4:
        cant = 2;
        d[0][0] = CAMPARI;
        d[1][0] = NARANJA;
        d[0][1] = floor((float)v*p[j]/100);
        d[1][1] = floor((float)v*(100 - p[j])/100);
        break;   
      case 5:
        cant = 2;
        d[0][0] = GANCIA;
        d[1][0] = SPRITE;
        d[0][1] = floor((float)v*p[j]/100);
        d[1][1] = floor((float)v*(100 - p[j])/100);
        break; 
      default:
        break;
    }

    //CASO GENERAL DE VALIDACIÓN Y LUEGO HAGO EL TRAGO
    if (validateInput(v, d, cant)) {
      lcd_print2l("HACIENDO","BEBIDA...");
      make(d, cant);
    } else {
      lcd_print("ERROR");
      delay(2500);
    }
  } else {
    lcd_print2l("BEBIDAS", "INSUFICIENTES");
    delay(2500);
  }
}

void menuPer(int v){
  int j=0;
  int lj=-1;
  byte lp[NUM_VALVES]={0};
  byte porcentaje[NUM_VALVES]={0};
  int canI = 0;
  int b;
  int last=SELECT;

  Serial.println(F(">menuPer"));

  for (int x = 0; x < NUM_VALVES; x++) {
    if (valve[x].active){
          j=x;
          Serial.println(x);
          canI = 1;
          break;
      }
    }    

  if (canI) {     
    do {
      b = readButtons();
      delay(10);
        
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
          do {
            j--;
            if (j < 0) break;
          } while (valve[j].active == 0);
          break;
          case DERECHA:
          do {
            j++;
            if (j >= NUM_VALVES) break;
          } while (valve[j].active == 0);
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
      
      if (j>=NUM_VALVES) {
        j = 0;
        while(valve[j].active == 0) j++;
      }
      if (j<0) {
        j = NUM_VALVES -1;
        while(valve[j].active == 0) j--;
      }
      if (porcentaje[j] > 100) porcentaje[j] = 0;
      if (porcentaje[j]<0) porcentaje[j]=100;
  
  
      if (j != lj || porcentaje[j] != lp[j]){
        lj = j;
        lp[j]=porcentaje[j];
        lcd_print1p(bebidas_s[valve[j].drink], String(porcentaje[j])+"%");
       }   
    } while (b != SELECT);
  
    while (readButtons());
  
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
  } else {
    lcd_print("NO HAY BEBIDAS!");
    delay(2500);
  }
}

void menuConf(){
  int j=0;
  int lj=-1;
  int b;
  int last=SELECT;

  Serial.println(F(">menuConf"));

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
      lj = j;
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

  while (readButtons());
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
  int cerrados=0;
  int b;
  int last=SELECT;

  Serial.println(F(">menuSet"));

  for (int x = 0; x < NUM_VALVES; x++){
    if (valve[x].active == 0) cerrados++;
  }

  //Serial.println(cerrados);
  

  if (cerrados != 0){

    while (valve[v].active) { // me adelanto hasta la primera valvula cerrada
      v++;
    }
  
    do {
      b = readButtons();
      delay(5);
  
      if (b != last) {
        last = b;
        switch (b){
          case ABAJO:
          do {
            v--;
            if (v < 0) break;
          } while (valve[v].active);
          break;
          case ARRIBA:
          do {
            v++;
            if (v>=NUM_VALVES) break;
          } while (valve[v].active);
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
      if (j<0) j = NUM_DRINKS-1;
      if (v >= NUM_VALVES) {
        v=0;
        while(valve[v].active == 1) v++;
      }
      if (v < 0) {
        v=NUM_VALVES-1;
        while (valve[v].active == 1) v--;
      }
  
      if (j != lj || v != lv){
        lj = j;
        lv = v;
  
        lcd_print1p1s("VALVULA:", String(v), bebidas_s[j]);      
      }  
    } while (b != SELECT);
  
    while (readButtons());
    setUpValve(v,j);
  } else {
    lcd_print2l("NO HAY VALVULAS","CERRADAS");
    delay(2500);
  }
}

void menuClose(){
  int j=0;
  int lj=-1;
  int b;
  int last=SELECT;
  int abiertos = 0;

  Serial.println(F("menuClose"));
  
  for (int x = 0; x < NUM_VALVES; x++){
    if (valve[x].active) abiertos++;
  }
  
  if (abiertos) {

    while (valve[j].active == 0) {
      j++;
    }
    
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
          do {
            j--;
            if (j<0) break;
          } while (!(valve[j].active));
          break;
          case DERECHA:
          do {
            j++;
            if (j>=NUM_DRINKS) break;
          } while (!(valve[j].active));
          break;
          case SELECT:
          break;
          default:
          break;
        }
      } else {
        b = DEF;
      }
  
      if (j>= NUM_VALVES) {
        j = 0;
        while (valve[j].active == 0) j++;
      }
      
      if (j<0) {
        j = NUM_VALVES -1;
        while (valve[j].active == 0) j--;
      }
  
      if (j != lj){
        lj = j;
        lcd_print1p("CERRAR VALVULA:", String(j));
      }  
    } while (b != SELECT);
    
    while (readButtons());
    closeValve(j);
  } else {
    lcd_print2l("NO HAY VALVULAS", "ABIERTAS");
  }
  
}

void menuDebug(){
  int j=0;
  int lj=-1;
  int b;
  int last=SELECT;
  int arg[2] = {0};
  int larg[2] = {0};

  Serial.println(">menuDebug");

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

  while (readButtons());
  Serial.println(j);
  switch (j){
        case 0:
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

#endif

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
  //ELECTROVALVULAS
  pinMode(PIN_V0, OUTPUT);
  pinMode(PIN_V1, OUTPUT);
  pinMode(PIN_V2, OUTPUT);
  pinMode(PIN_V3, OUTPUT);
  pinMode(PIN_V4, OUTPUT);
  pinMode(PIN_V5, OUTPUT);
  pinMode(PIN_V6, OUTPUT);
  pinMode(PIN_V7, OUTPUT);

  //STEPPER
  pinMode(STP_STEP, OUTPUT);
  pinMode(STP_DIR, OUTPUT);
  pinMode(STP_ENA, OUTPUT);
  digitalWrite(STP_STEP, LOW);
  digitalWrite(STP_DIR, LOW);
  digitalWrite(STP_ENA, HIGH);

  //SENSORES
  pinMode(PIN_FDC, INPUT_PULLUP);
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
  
  Serial.println("VALVE: "+String(index)+" -> DRINK: "+String(drinkCode)+ " (" + bebidas_s[valve[index].drink] + ")");
}

void closeValve (int index){
  valve[index].active = 0;
  EEPROM.update(V0_EEPROM + index, 0);
  
  Serial.println("VALVE: "+String(index)+" -> CLOSED");
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
    Serial.println(F("EEPROM -> READ"));
  //#endif

  if (EEPROM.read(254)==255) { 
    firstTimeSetUp();
  } else {
    for (i = 0; i < NUM_VALVES; i++){
      valve[i].active = EEPROM.read(V0_EEPROM +i);
      if (valve[i].active) {
        valve[i].drink = EEPROM.read(V0_EEPROM + NUM_VALVES + i);
        #ifdef DEBUG
          Serial.println("VALVE: "+ String(i)+" -> DRINK: "+ String(valve[i].drink)+" (" + bebidas_s[valve[i].drink] + ")");
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
    int pos = arg[1].toInt();
    pos = (pos <= STP_MAXPOS)?pos:STP_MAXPOS;
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
      //#ifdef DEBUG
        Serial.println(F("VALIDATION -> FAIL"));
      //#endif
      return 0;
    }
    porcentajeSum += d[i][1];
  }

  //#ifdef DEBUG
    Serial.print("VALIDATION ->");
    Serial.println((porcentajeSum <= v)?"PASS":"FAIL");
  //#endif
  
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
    if (valve[i].active) {      
      if (valve[i].drink == d){
        #ifdef DEBUG
          Serial.println("DRINK "+String(d)+" FOUND IN VALVE "+String(i));
        #endif
        return i;
      }
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
  Serial.println(">make");
  if (currentPos != 0) {
    #ifdef DEBUG 
      Serial.println("NOT AT HOME WHEN MAKE()");
    #endif
      goHome();
  }

  #ifdef LCD
    lcd_print2l("COLOQUE", "EL VASO!");
   #endif
  
  if (!cupPresent()){
    while (!cupPresent());
  }

  
  for (int z = 5; z > 0; z--){
    #ifdef LCD
    lcd_print1p("COMENZANDO EN:", "00:0"+String(z));
    #endif
    delay(1000);
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
      #ifdef LCD
      lcd_print2l(bebidas_s[valve[v_i].drink], String(d[i][1])+" ml");
      #endif
      pour(valve[v_i].pin, d[i][1]);
  }

  if (currentPos>10) goToSmooth(10);
  goHome();

  #ifdef LCD
  lcd_print("RETIRE EL VASO!"); 
  #endif 

  if (cupPresent()){
    while (cupPresent());
  }
  
  #ifdef LCD
  lcd_print2l("DISFRUTE","SU BEBIDA!");
  delay(2500);
  #endif
  
  #ifdef DEBUG
    Serial.println("ENJOY YOUR DRINK!!");
  #endif 
}

long distanceToSteps(long pos) {
  #ifdef DEBUG
    Serial.println("DISTANCE: "+String(pos)+" -> STEPS: "+String(round(pos*STEPS_X_MM)));
  #endif
  return round(pos*STEPS_X_MM);
}

void goTo(long pos){
  #ifdef DEBUG
    Serial.println("GOING TO POS "+String(pos));
  #endif

  int steps = distanceToSteps(pos - currentPos);
  int dir = (steps>0)?1:0;
  int i;
  int p = 1/(STP_MIN*STP_STEPS/60);
  steps = abs(steps);

  digitalWrite(STP_ENA, LOW);
  digitalWrite(STP_DIR, (dir)?HIGH:LOW);

  for (i = 0; i<steps; i++) {
    stepPulse(STP_STEP, 1);
    stepDelay(p, 2);
  }
  currentPos = pos;                            
}

void goToSmooth1(long pos){
  long steps = distanceToSteps(pos - currentPos)*STP_MODE;
  int dir = (steps >= 0)?1:0;  
  int i;
  long stepsdone=0;
  //Nuevo
  int stepsToVLim;
  float v_min,v_max, tt=0;
  float p,p_0, p_max;
  unsigned long thelp, tiempo;

  digitalWrite(STP_ENA, LOW);
 
  steps = abs(steps); // valor absoluto
  Serial.println("STPMAX: "+String(STP_MAX)+". STPMIN: "+String(STP_MIN)+". STEPS: "+String(STP_STEPS) +"\n");
  v_min = STP_MIN*STP_STEPS/60;
  v_max = STP_MAX*STP_STEPS/60;
  stepsToVLim=ceil((v_max*v_max-v_min*v_min)/(2*STP_ACC));

  digitalWrite(STP_DIR, (dir)?HIGH:LOW);
  Serial.println("DIR SET TO "+ String((dir)?HIGH:LOW));

  if (steps<(2*stepsToVLim)) {
    v_max = sqrt(2* STP_ACC *steps/2.25+v_min*v_min); //VOY HASTA UN POCO MAS LENTO DE LO MAXIMO QUE PUEDO IR, POR LAS DUDAS.
    stepsToVLim=ceil((v_max*v_max-v_min*v_min)/(2*STP_ACC));
  }

  p_0 = 1/sqrt(v_min+v_min+2*STP_ACC);
  p=p_0;
  p_max=1/v_max;

  #ifdef DEBUG
    Serial.println("STEPS: "+String(steps)+". STEPS TO VLIM: "+String(stepsToVLim)+". VMAX: "+String(v_max)+". Vmin: "+String(v_min)+"\n");
    Serial.println("p_0: "+String(p_0, DEC)+". p_max: "+String(p_max,DEC)+".\n");
  #endif
  tiempo = millis();
  //FASE ACELERACIÓN
  for (i = 0; i < stepsToVLim; i++)
  {
    thelp=micros();
    Serial.print(i);
    stepPulse(STP_STEP, 1);
    if (i) {
      p = p*(1-STP_ACC*p*p+1.5*STP_ACC*STP_ACC*p*p*p*p);
    } else {
      p = p_0;
    }
    if (p<p_max) p = p_max; //Si el delay es mas lento que el delay de v_max.
    stepDelay(p, micros()-thelp);
  }
  //FASE VELOCIDAD MAXIMA
  for (int i = 0; i < (steps-2*stepsToVLim); i++)
  {
    thelp=micros();
    Serial.print(i);
    stepPulse(STP_STEP, 1);
    p=p_max;
    stepDelay(p, micros()-thelp);
  }
  //FASE DESACELERACIÓN
  for (int i = 0; i < stepsToVLim; i++)
  {
    thelp=micros();
    Serial.print(i);
    stepPulse(STP_STEP, 1);
    p = p*(1+STP_ACC*p*p+1.5*STP_ACC*STP_ACC*p*p*p*p);
    if (p>p_0) p = p_0; //Si el delay es mas lento que el delay de v_max.
    stepDelay(p, micros()-thelp);
  }
  tiempo=millis()-tiempo;
  Serial.println("STEPSCOMPUTADOS: "+String(stepsdone));
  Serial.println(tiempo);
  Serial.println(tt);
  currentPos=pos;
}

void goToSmooth(long pos){
  long steps = distanceToSteps(pos - currentPos)*STP_MODE;
  int dir = (steps >= 0)?1:0;  
  int i;
  long stepsdone=0;
  //Nuevo
  int stepsToVLim;
  double v_min,v_max, tt=0;
  float p,p_0, p_max;
  unsigned long thelp, tiempo;

  digitalWrite(STP_ENA, LOW);
  
  steps = abs(steps); // valor absoluto  
  v_min = STP_MIN*(STP_STEPS/60.0);
  v_max = STP_MAX*(STP_STEPS/60.0);
  stepsToVLim=ceil((v_max*v_max-v_min*v_min)/(2*STP_ACC));
  
  if (steps<(2*stepsToVLim)) {
    v_max = sqrt(2* STP_ACC *(steps/2.2)+v_min*v_min); //VOY HASTA UN POCO MAS LENTO DE LO MAXIMO QUE PUEDO IR, POR LAS DUDAS.
    stepsToVLim=ceil((v_max*v_max-v_min*v_min)/(2*STP_ACC));
  }

  digitalWrite(STP_DIR, (dir)?HIGH:LOW);
  Serial.println("DIR SET TO "+ String((dir)?HIGH:LOW));

  p_0 = 1/sqrt(v_min+v_min+2*STP_ACC);
  p=p_0;
  p_max=1/v_max;

  #ifdef DEBUG
    Serial.println("STPMAX: "+String(STP_MAX)+". STPMIN: "+String(STP_MIN)+". STEPS: "+String(STP_STEPS) +"\n");
    Serial.println("STEPS: "+String(steps)+". STEPS TO VLIM: "+String(stepsToVLim)+". VMAX: "+String(v_max)+". Vmin: "+String(v_min)+"\n");
    Serial.println("tcuentas: "+String(thelp)+"p_0: "+String(p_0, DEC)+". p_max: "+String(p_max,DEC)+".\n");
  #endif
  tiempo = millis();
  for (i = 1; i <= steps; i++)
  {
    thelp=micros();
    stepPulse(STP_STEP, 1);
    if (i != 1) {      
      if (i<stepsToVLim){
        p = p*(1-STP_ACC*p*p+1.5*STP_ACC*STP_ACC*p*p*p*p);
      } else if (i>(steps-stepsToVLim)){
        p = p*(1+STP_ACC*p*p+1.5*STP_ACC*STP_ACC*p*p*p*p);
      }
    } else {
      p = p_0;
    }
    if (p<p_max) p = p_max; //Si el delay es mas lento que el delay de v_max.
    if (p>p_0) p = p_0; //Si el delay es mas lento que el delay de v_max.
    stepDelay(p, micros()-thelp);
  }
  
  tiempo=millis()-tiempo;
  Serial.println("TIEMPO: (MS): "+String(tiempo));
  currentPos=pos;
}

void goHome(){
  float p = 1/(STP_MIN*STP_STEPS/60.0);
  
  #ifdef DEBUG
    Serial.println(F("GOING HOME, WAITING FOR LIMIT SWITCH"));    
  #endif
  digitalWrite(STP_DIR, LOW);
  Serial.println("DIR SET TO LOW");
  while (digitalRead(PIN_FDC) == HIGH){
    
    stepPulse(STP_STEP, 1);
    stepDelay(p, 2);
  }
  currentPos = 0;
  digitalWrite(STP_ENA, HIGH);
  
  #ifdef DEBUG
    Serial.println(F("HOME OK"));
  #endif
}

void stepPulse(int pin, int delayy){
  //Serial.print("STEP ");
  digitalWrite(pin, HIGH);
  delayMicroseconds(delayy);
  digitalWrite(pin, LOW);
  delayMicroseconds(delayy);
}

void stepDelay(float p, long resta){
  int milisegundos, microsegundos;
  long pp=p*1000000;
  long r = resta;
  pp -= r;
  //Serial.print("delay "+ String(resta));
  if (pp>0){
  milisegundos = (int)(pp/1000);
  microsegundos = pp%1000;
  //Serial.println(" p = "+String(p, DEC)+" r= "+String(resta, DEC)+". uS = "+String(microsegundos)+". mili: "+String(milisegundos));
  //Serial.println("si");
  delayMicroseconds(microsegundos);
  delay(milisegundos);
  }
}
