#include <EEPROM.h>
#include <Stepper.h>
//#define DEBUG

#define CAUDAL 1 // cm^3/miliseg?

#define PIN_V0 4 // PIN VALVULA 0
#define PIN_V1 5
#define PIN_V2 6
#define PIN_V3 7
#define PIN_V4 0
#define PIN_V5 0
#define PIN_V6 0
#define PIN_V7 0

#define POS_V0 100
#define POS_V1 200
#define POS_V2 300
#define POS_V3 400
#define POS_V4 500
#define POS_V5 600
#define POS_V6 700
#define POS_V7 800

#define V0_EEPROM 10 // estado de las valvulas
                     // la bebida de c/ valvula se guarda en V0_EEPROM+NUM_VALVES

#define STP1  9 // PINES DEL MOTOR PAP
#define STP2 10
#define STP3 11
#define STP4 12

#define NUM_VALVES 8
#define NUM_DRINKS 8

#define PIN_FDC 2 //NORMAL ABIERTO DEL FINAL DE CARRERA DEL HOME
#define POS_POR_PASO 0.1 //TODO: ver que pongo aca

String bebidas_s[NUM_DRINKS] = {"FERNET", "VODKA","RON", "TEQUILA", "COCA COLA", "NARANJA", "GANCIA", "SPRITE"}; // ESTO Y LO DE ABAJO DEBERIAN SER IGUALES
enum bebidas {FERNET = 0, VODKA, RON, TEQUILA, COCA_COLA, NARANJA, GANCIA, SPRITE}; //todavia no se para q usar esto <<<<

typedef struct valve_s {
    byte index;
    byte drink;
    byte pin;
    byte active;
    long pos;
} valve_t;

valve_t valve[NUM_VALVES];

Stepper stepper(400, STP1, STP2, STP3, STP4);

char sepChar = ' ';
char subSepChar = ';';
String bufferString = "";
long currentPos = 0;

void setup() {
  Serial.begin(9600);
  stepper.setSpeed(100);
  
  pinSetup();
  posSetup();
  readEEPROM();  
  
  goHome();

  Serial.println("BARDUINO IS READY");
}

void loop() {
  serialEvent();
}

void firstTimeSetUp(){
  int i;
  EEPROM.write(254, 0);
  Serial.println("PERFORMING FIRST TIME SET UP");
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
          //Serial.println(bufferString);
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
  }
  return c;
}

void parse(String str) {
  int i;
  int argCount = stringOcurr(str, sepChar)+1; //busco la cantidad de espacios para no definir arg[] grande al pedo  
  String arg[argCount];
  int numArg = 0;

  for (i = 0; i<str.length(); i++){
    if (str[i] != sepChar) {
      arg[numArg] += str[i];
    } else {
      numArg++;
    }
  }

  cmdToAction(arg, numArg);
}

void readEEPROM(){
  int i;
  //#ifdef DEBUG
    Serial.println("READING VALVE DATA FROM EEPROM.");
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
    if (numArg > 1){
      int drinkArray [numArg-1][2];
      int vol = round(arg[1].toInt() * 0.8); //multiplico por 0.8 para darme un poco de juego
      int sepPos;
      #ifdef DEBUG
        Serial.println("numARG = "+String(numArg)+ ". BEBIDAS = "+String(numArg-1));
      #endif  
      
      for (int i = 2; i <= numArg; i++) {
        sepPos = arg[i].indexOf(subSepChar);
        drinkArray[i-2][0] = arg[i].substring(0,sepPos).toInt(); //TODO: decidir si le paso las bebidas como numero o como string
        drinkArray[i-2][1] = floor(arg[i].substring(sepPos+1).toInt() * (vol / 100.0)); //redondeo para abajo para asegurarme de que no me paso del volumen total 
        #ifdef DEBUG
          Serial.println("DRINK: "+String(drinkArray[i -2][0]));
          Serial.println("VOL: "+String(drinkArray[i-2][1]));
        #endif 
      } 
         
      if (validateInput(vol, drinkArray,numArg-1)) {
        make(drinkArray, numArg -1);
      } else {
        Serial.println("COMPOSITION ERROR");
      }
    } else {
      Serial.println("NOT ENOUGH ARGUMENTS");
    }
  } else if (arg[0]== "SET"){ // SET index cod_bebida
    if (arg[1].toInt()>=0 && arg[1].toInt()<NUM_VALVES){
      setUpValve(arg[1].toInt(), arg[2].toInt()); //TODO: validar el codigo de bebidas.
    } else {
      Serial.println("VALVE DOES NOT EXIST!");  
    }
  } else if (arg[0]== "CLOSE"){ // SET index cod_bebida
    if (numArg >= 1) {
      for (int i = 1; i <= numArg; i++){
        if (arg[i].toInt()>=0 && arg[i].toInt()<NUM_VALVES){
          if (valve[arg[i].toInt()].active){
            closeValve(arg[i].toInt());
          } else {
            Serial.println("VALVE AREADY CLOSED");
          }
        } else {
          Serial.println("VALVE DOES NOT EXIST!");  
        }
      }
    } else if (numArg = 0) {
      Serial.println("NOT ENOUGH ARGUMENTS");
    }
  } else if (arg[0]== "STATUS"){ // STATUS index 
    if (arg[1].toInt()>=0 && arg[1].toInt()<NUM_VALVES){
      if (valve[arg[1].toInt()].active){
        Serial.println("VALVE "+ String(arg[1].toInt()) +" IN USE WITH DRINK "+String(valve[arg[1].toInt()].drink));
      } else {
        Serial.println("VALVE "+ String(arg[1].toInt()) + " NOT IN USE");
      }
    } else {
      Serial.println("VALVE DOES NOT EXIST!");  
    }
  } else if (arg[0]=="HOME"){
    goHome();
  } else if (arg[0]=="CODES"){
    for (int i = 0; i<NUM_DRINKS; i++){
      Serial.println(bebidas_s[i] + ": " + String(i));
    }
  } else {
    Serial.println("COMMAND NOT FOUND");
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
        Serial.println("VALIDATION NOT OK");
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

void make(int d[][2], int dim){
  int i, v_i;
  if (currentPos) {
    #ifdef DEBUG 
      Serial.println("NOT AT HOME WHEN MAKE()");
    #endif
      goHome();
  }

  for (i = 0; i<dim; i++){
      v_i = getValveIndexFromDrink(d[i][0]);
      #ifdef DEBUG 
        Serial.println("getValveFromDrink RETURNS "+ String(v_i));
      #endif
      goTo(valve[v_i].pos);
      pour(valve[v_i].pin, d[i][1]);
  }

  //TODO: revolver bebida ;) ;)
  
  goHome(); 
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

void goHome(){
  #ifdef DEBUG
    Serial.println("GOING HOME, WAITING FOR LIMIT SWITCH");
  #endif
  while (digitalRead(PIN_FDC) == LOW){
    stepper.step(-1);
  }
  currentPos = 0;
  #ifdef DEBUG
    Serial.println("HOME OK");
  #endif
}

