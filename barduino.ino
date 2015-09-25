#include <EEPROM.h>
#include <Stepper.h>

#define CAUDAL 1

#define PIN_V0 4 // PIN VALVULA 0
#define PIN_V1 5
#define PIN_V2 6
#define PIN_V3 7
#define PIN_V4 0
#define PIN_V5 0
#define PIN_V6 0
#define PIN_V7 0

#define V0_EEPROM_b 10 // codigo de bebidas
#define V0_EEPROM_a 20 // estado de las valvulas

#define STP1  9 // PINES DEL MOTOR PAP
#define STP2 10
#define STP3 11
#define STP4 12

#define NUM_VALVES 4

#define PIN_FDC 8 //NORMAL ABIERTO DEL FINAL DE CARRERA DEL HOME
#define POS_POR_PASO 1.0 //TODO: ver que pongo aca

String bebidas_s[] = {"FERNET", "VODKA","RON", "TEQUILA", "COCA COLA", "NARANJA"};
enum bebidas {FERNET = 0, VODKA, RON, TEQUILA, COCA_COLA, NARANJA};

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
  // put your setup code here, to run once:
  Serial.begin(9600);
  stepper.setSpeed(100);
  readEEPROM();
  valve[0].pin = PIN_V0;
  valve[1].pin = PIN_V1;
  valve[2].pin = PIN_V2;
  valve[3].pin = PIN_V3;
  pinMode(PIN_V0, OUTPUT);
  pinMode(PIN_V1, OUTPUT);
  pinMode(PIN_V2, OUTPUT);
  pinMode(PIN_V3, OUTPUT);
  goHome(); // hago que el motor valla a home.
}

void loop() {
  // put your main code here, to run repeatedly:
  serialEvent();
}

void serialEvent(){
  char inputChar;
  while (Serial.available()) {
    inputChar = (char)Serial.read();
    if ((inputChar >= 33 && inputChar <= 126) || inputChar == 10 || inputChar == 32) {
      if (inputChar != '\n'){
        bufferString += inputChar;
      } else {
        parse(bufferString);
        bufferString = "";
      }
    }
  }  
}

void setUpValve (int index, int drinkCode){
  valve[index].active = 1;
  EEPROM.update(V0_EEPROM_a + index, 1);
  valve[index].drink = drinkCode;
  EEPROM.update(V0_EEPROM_b + index, drinkCode);  
}

void closeValve (int index){
  valve[index].active = 0;
  EEPROM.update(V0_EEPROM_a + index, 0);
}

void parse(String str) {
  int i;
  String arg[10] = {"","","","","","","","","",""};
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
  for (i = 0; i < NUM_VALVES; i++){
    valve[i].active = EEPROM.read(V0_EEPROM_a +i);
    if (valve[i].active) {
      valve[i].drink = EEPROM.read(V0_EEPROM_b + i);
    }
  }
}

void cmdToAction(String arg[],int numArg){
  int drinkArray [numArg-1][2];
  arg[0].toUpperCase();
  
  
  //EJEMPLO DE BEBIDAS PERSONALIZADAS
  if (arg[0] == "MAKE"){ // MAKE volumen_del_vaso cod_bebida;porcentaje cod_bebida;porcentaje ...
    int volumen = round(arg[1].toInt() * 0.8); //multiplico por 0.8 para darme un poco de juego
    
    for (int i = 2; i <= numArg; i++) {        
      drinkArray[i-2][0] = arg[1].toInt(); //TODO: decidir si le paso las bebidas como numero o como string
      drinkArray[i-2][1] = floor(arg[2].toInt() * volumen / 100.0); //redondeo para abajo para asegurarme de que no me paso del volumen total 
    }
       
    if (validateInput(volumen, drinkArray,numArg-2)) {
      make(drinkArray, numArg -2);
    } else {
      delay(1); 
    }
  }

  if (arg[0]== "SET"){ // SET index cod_bebida
    if (arg[1].toInt()>=0 && arg[1].toInt()<=7){
      setUpValve(arg[1].toInt(), arg[2].toInt()); //TODO: validar el codigo de bebidas.
    }
  }

  if (arg[0]== "CLOSE"){ // SET index cod_bebida
    if (arg[1].toInt()>=0 && arg[1].toInt()<=7){
      if (valve[arg[1].toInt()].active){
        closeValve(arg[1].toInt());
      }        
    }
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
    porcentajeSum += d[i][1]; //TODO: validar los codigos de bebidas
  }

  return (porcentajeSum <= v);  
}

void pour(int pin, int cant) {
  long t;
  
  t = round((float)cant/CAUDAL); //TODO: ver tema de unidades.

  digitalWrite(pin, HIGH);
  delay(t);
  digitalWrite(pin, LOW);  
}

byte getValveIndexFromDrink(int d){
  int i;
  
  for (i = 0; i < NUM_VALVES; i++){
    if (valve[i].drink == d){
      return i;
    }
  }
  
  return -1;
}

void make(int d[][2], int dim){
  int i, v_i;
  if (currentPos) goHome();

  for (i = 0; i<dim; i++){
      v_i = getValveIndexFromDrink(d[i][0]);
      goTo(valve[v_i].pos);
      pour(valve[v_i].pin, d[i][1]);
  }

  //TODO: revolver bebida ;) ;)
  
  goHome();  
}

long distanceToSteps(long pos) {
  return round(pos/POS_POR_PASO);
}

void goTo(long pos){
  stepper.step(distanceToSteps(currentPos - pos)); // diferencia entre la posición actual y la de destino.
  currentPos = pos;                                // hay que configurar los cables del stepper para que step(1) valla hacia afuera
                                                   // y step(-1) valla hacia adentro.
}

void goHome(){
  while (digitalRead(PIN_FDC) == LOW){
    stepper.step(-1);
  }
  currentPos = 0;
}

