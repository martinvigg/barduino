#include <Stepper.h>
#define CAUDAL 1

//enum bebidas {FERNET = 1, VODKA, RON, TEQUILA, COCA_COLA, };

char sepChar = ' ';
char subSepChar = ';';
String bufferString = "";

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  
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

void cmdToAction(String arg[],int numArg){
  int drinkArray [numArg-1][2];
  arg[0].toUpperCase();
  
  
  //EJEMPLO DE BEBIDAS PERSONALIZADAS
  if (arg[0] == "MAKE"){ // MAKE volumen_del_vaso cod_bebida;porcentaje cod_bebida;porcentaje ...
    //volumen = arg[1]
    
    for (int i = 2; i <= numArg; i++) {        
      drinkArray[i-2][0] = arg[1].toInt(); //TODO: decidir si le paso las bebidas como numero o como string
      drinkArray[i-2][1] = arg[2].toInt(); //TODO: conviene usar float?   
    }
       
    if (validateInput(drinkArray,numArg-2)) {
      delay(1);// pasar a otra funcion
    } else {
      delay(1); 
    }
  }

  //EJEMPLO DE BEBIDAS PRE-ESTABLECIDAS
  if (arg[0] == "FERNET"){ // FERNET volumen_del_vaso porcentaje
    if (numArg == 2){//volumen = arg[1]
      //int drinkArray [numArg-1][2];
      drinkArray[0][0] = 11111; //codigo del fernet
      drinkArray[0][1] = arg[2].toInt();
      drinkArray[1][0] = 11111; //codigo de la coca
      drinkArray[1][1] = 100 - arg[2].toInt();

      if (validateInput(drinkArray,numArg-2)) {
        delay(1);// pasar a otra funcion
      } else {
        delay(1); 
      }
    } else {
      delay(1); //error
    }   
  }
}

boolean validateInput(int d[][2], int dim){
  int i;
  int porcentajeSum = 0;

  for (i = 0; i<dim; i++){
    porcentajeSum += d[i][1]; //TODO: validar los codigos de bebidas
  }

  return (porcentajeSum == 100);  
}

void pour(int valNum, int cant) {
  long t;
  
  t = round((float)cant/CAUDAL); //TODO: ver tema de unidades.

  digitalWrite(valNum, HIGH);
  delay(t);
  digitalWrite(valNum, LOW);  
}
