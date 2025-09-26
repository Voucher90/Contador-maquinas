
//Saúl Íñiguez Macedo -- Centro Tecnológico del Calzado de La Rioja
//Prog: Contador_velcro   Versión: 1.5   Fecha: 08/04/2019

/*
  Código para programar ciclos en máquinas de ensayo.
  Al iniciar pulsar SET para que aparezca la pantalla de ensayo. Pulsando de nuevo SET se accede a un menú para configurar la consigna de ciclos.
  Al comenzar la cuenta de ciclos, se muestra el tiempo desde el primer ciclo y las RPM actuales. En cualquier momento si se acciona RESET se pone todo a cero.
*/

//Definición de variables, no es necesario inicializar las analógicas en el setup

//Se inicializan las librerías para usar el LCD en I2C
#include <Wire.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>

//Se inicializa la librería para el uso de la memoria eeprom
#include <EEPROM.h>

//Definición de la dirección y pinout del LCD
#define I2C_ADDR 0x27
LiquidCrystal_I2C lcd(I2C_ADDR, 2, 1, 0, 4, 5, 6, 7);

//Variables para consigna
int UNIDAD = 0;              //Unidades de la consigna de ciclos
int DECENA = 0;              //Decenas de la consigna de ciclos
int CENTENA = 0;             //Centenas de la consigna de ciclos
int UMILLAR = 0;             //Unidades de millar de la consigna de ciclos
int DMILLAR = 0;             //Decenas de millar de la consigna de ciclos
long CONSIGNA = 0;           //Consigna total formada por cada digito anterior

//Variables para conteo de ciclos
long CICLOS = 0;             //Ciclos contados
long CICLOSL = 0;            //Ciclos cargados desde la eeprom (Usada para tiempo)
long CICLOSPRINT = 0;         //Variable ciclos a imprimir para que no se modifique con la interrupción
volatile int INTERR = 0;     //Variable para saber cuando ha habido interrupción

//Variables para cálculo de RPM
int CICLORPM = 0;            //Número de ciclos utilizado para el cálculo de RPM
int RPM = 0;                 //Revoluciones por minuto
int RPMPREVIO = 0;           //Número de RPM previas para actualizar el tiempo en la inicial

//Variables para crear menús
int MENU = 6;                //Variable para identificar en que parte del menu de seleccion de ciclos se está
int MENUPREVIO = 0;          //Variable para saber cuando hacer clear en el LCD

//Variables para tiempo de ensayo
int HORAS = 0;               //Horas de ensayo
int MINUTOS = 0;             //Minutos de ensayo
int SEGUNDOS = 0;            //Segundos de ensayo

//Variables de tiempo para temporizar
unsigned long tiemporebote = 0;     //Tiempo para evitar rebotes en los pulsadores
unsigned long tiempointerr = 0;     //Tiempo para evitar rebotes en la interrupción (Distinto de tiemposensor para evitar problemas)
unsigned long tiempoparpadeo = 0;   //Contabilizar tiempo de parpadeo de dígitos
unsigned long tiemporpm = 0;        //Contabilizar el tiempo para promediar RPM
unsigned long tiempopantalla = 0;   //Contabilizar tiempo hasta que aparezca pantalla de reposo
unsigned long tiempoensayo = 0;     //Fijar el tiempo inicial ensayo
unsigned long tiempomillis = 0;     //Contabilizar el tiempo de ensayo
unsigned long tiempogiro = 0;       //Contabilizar el tiempo entre cambios de sentido del giro

/*
  //Variables pulsadores sensores y actuadores
  int SENSOR = 2;     //Sensor para conteo de ciclos
  int PULSADOR = 3;   //Pulsador para cambiar digitos
  int SET = 4;        //Pulsador de set
  int RESET = 5;      //Pulsador de reset
  int RELE = 6;       //Relé NC para cierre de cicrcuito en contactor motor
  int RELEGIRO = 7;   //Relé para control del sentido de giro del motor
*/

//Variables para la gestión de memoria eeprom
int INICIO = 0;     //Comprobación de primera carga (Bytes 0, 1)
int PUNT = 0;       //Puntero para variables de consigna y limpieza de memoria
int PUNTUC = 18;    //Puntero para unidades de ciclo, se encuentra en (Bytes 12, 13) y barre (Bytes 18, 897)
int PUNTDC = 898;   //Puntero para decenas de ciclo, se encuentra en (Bytes 14, 15) y barre (Bytes 898, 995)
int PUNTCC = 986;   //Puntero para centenas de ciclo, se encuentra en (Bytes 16, 18) y barre (Bytes 986, 995)
int UC = 0;         //Unidades de la descomposición de ciclos
int DC = 0;         //Decenas de la descomposición de ciclos
int CC = 0;         //Centenas de la descomposición de ciclos
int UMC = 0;        //Unidades de millar de la descomposición de ciclos
int DMC = 0;        //Decenas de millar de la descomposición de ciclos

/////////////////////////////////////////////////////////////////////////////////////////////
int MEMORIA = 1;    //Define el modo de guardado en eeprom (Activo = 1 o Inactivo = 0)     //
int CONTACTO = 1;   //Variable de definición de contacto (Relé NA = 0 y Relé NC = 1)       //
/////////////////////////////////////////////////////////////////////////////////////////////

byte A[8] = {       //Definición de una variable con la información de un caracter especial
  B00000,
  B11111,
  B00000,
  B11111,
  B00000,
  B11111,
  B00000,
  B00000,
};

void setup() {
  //Definicion de pines como salidas o entradas, Puerto D pines digitales de 7 a 0
  DDRD = B11000000;
  //Se inicia el LCD y se define el brillo
  lcd.begin (16, 2);
  lcd.setBacklightPin(3, POSITIVE);
  lcd.setBacklight(HIGH);
  attachInterrupt( 0, ServicioSensor, RISING);
  lcd.createChar (1, A);  //Creación de un caracter especial con la variable A
  gestionmem(0);  //Se inicia la eeprom o se lee y se calcula consigna
}

void loop() {

  //Se actualiza el tiempo mientras se está a low para empezar a contar desde que se pulsa
  if (!(PIND & (1 << PD3)) && !(PIND & (1 << PD4)) && !(PIND & (1 << PD5))) {
    tiemporebote = millis();
  }

  if (PIND & (1 << PD5) && (millis() - tiemporebote) > 300 && MENU == 0) { //Se hace reset de ciclos y variables de tiempo
    tiemporebote = millis();
    CICLOS = 0;
    HORAS = 0;
    MINUTOS = 0;
    SEGUNDOS = 0;
    gestionmem(2);  //Se actualizan variables y punteros
  }

  if (PIND & (1 << PD4) && (millis() - tiemporebote) > 300) {  //Se define la variable para entrar a los menús
    tiemporebote = millis();
    tiempopantalla = millis();
    if (MENU < 5 && CICLOS == 0) {
      MENU++;
    } else if (MENU == 5) {
      gestionmem(1);  //Se actualiza la eeprom y se calcula consigna
      MENU = 0;
    } else if (MENU == 6) {
      MENU = 0;
    }
  }

  //Si se detecta un ciclo estando en reposo se va a la pantalla de ensayo
  if (MENU == 6 && (millis() - tiempointerr) > 70 && INTERR == 1 && CONSIGNA > 0) {
    MENU = 0;
  }

  //Se cuentan los ciclos si han pasado 70ms desde la interrupción y el pin sigue a high
  //Si en ese tiempo hay otro pulso el tiempo se pone a cero. Si se deja pulsado solo se cuenta el primero
  if (PIND & (1 << PD2) && (millis() - tiempointerr) > 70 && INTERR == 1 && MENU == 0) {
    if (CONSIGNA > 0 && CICLOS != CONSIGNA) {  //Se cuenta el ciclo
      CICLOS++;
      gestionmem(2);  //Se actualizan variables y punteros
    }
    if (CICLOS == 1 || CICLOS == CICLOSL + 1) { //Se fija el tiempo de inicio de ensayo
      tiempoensayo = millis();
      tiempogiro = millis();
    }
    CICLORPM++;
    INTERR = 0;
  } else if (!(PIND & (1 << PD2))) {
    INTERR = 0;
  }

  if (!(PIND & (1 << PD6)) && ((CONTACTO == 0 && CICLOS != CONSIGNA && (MENU == 0 || MENU == 6)) || (CONTACTO == 1 && (CICLOS == CONSIGNA || (MENU > 0 && MENU < 6))))) {  //Activación del relé
    PORTD |= (1 << PB6);  //Se define solamente el pin 6 a HIGH
  } else if (PIND & (1 << PD6) && ((CONTACTO == 0 && (CICLOS == CONSIGNA || (MENU > 0 && MENU < 6))) || (CONTACTO == 1 && CICLOS != CONSIGNA && (MENU == 0 || MENU == 6)))) {
    PORTD &= ~(1 << PB6);  //Se define solamente el pin 6 a LOW
  }

  if (!(PIND & (1 << PD7)) && ((millis() - tiempogiro) > 30000) && CICLOS != 0 && CICLOS != CONSIGNA && (MENU == 0 || MENU == 6)) {  //Activación del relé
    PORTD |= (1 << PB7);  //Se define solamente el pin 7 a HIGH
    tiempogiro = millis();
  } else if (PIND & (1 << PD7) && ((millis() - tiempogiro) > 30000) && CICLOS != 0 && CICLOS != CONSIGNA && (MENU == 0 || MENU == 6)) {
    PORTD &= ~(1 << PB7);  //Se define solamente el pin 7 a LOW
    tiempogiro = millis();
  }

  //RPM se pone a 0 pasados 10s. Respuesta rápida a altas RPM pero no mide menos de 6 RPM sin modificar los 10s
  if (CICLORPM == 1 && RPMPREVIO == 0) {
    tiemporpm = millis();
    RPMPREVIO = 1;
  } else if (CICLORPM >= 5 || (millis() - tiemporpm) > 10000) {
    if (CICLORPM == 0) {
      RPM = 0;
    } else {
      RPM = 60000 / ((millis() - tiemporpm) / (CICLORPM - 1));
    }
    CICLORPM = 0;
    RPMPREVIO = 0;
  }

  if (CICLOS != CONSIGNA && CICLOS != 0 && CICLOS != CICLOSL) { //Se calculan las variables de tiempo
    calculatiempo();
  }

  if (millis() - tiempopantalla > 300000) {  //Se fija la variable de pantalla de inicio
    MENU = 6;
  }

  if (MENU == 0) {  //Se llama a las funciones dependiendo del tipo de menú
    muestraDatos();
  } else if (MENU == 6) {
    pantallaInicio();
  } else {
    Set();
  }
}

//Función para mostrar los datos de consigna, ciclos actuales, RPM y tiempo de ensayo
void muestraDatos() {
  if (MENUPREVIO != MENU) {
    lcd.clear();
  }
  MENUPREVIO = MENU;
  lcd.home ();
  if (CONSIGNA < 10) {
    lcd.print("0000");
  } else if (CONSIGNA < 100) {
    lcd.print("000");
  } else if (CONSIGNA < 1000) {
    lcd.print("00");
  } else if (CONSIGNA < 10000) {
    lcd.print("0");
  }
  lcd.print(CONSIGNA);
  lcd.setCursor ( 0, 1 );
  CICLOSPRINT = CICLOS;
  if (CICLOSPRINT < 10) {
    lcd.print("0000");
  } else if (CICLOSPRINT < 100) {
    lcd.print("000");
  } else if (CICLOSPRINT < 1000) {
    lcd.print("00");
  } else if (CICLOSPRINT < 10000) {
    lcd.print("0");
  }
  lcd.print(CICLOSPRINT);
  lcd.setCursor ( 7, 0);
  lcd.print("RPM:");
  if (RPM < 10) {
    lcd.print("000");
  } else if (RPM < 100) {
    lcd.print("00");
  } else if (RPM < 1000) {
    lcd.print("0");
  }
  lcd.print(RPM);
  lcd.setCursor ( 7, 1);
  if (HORAS < 10) {
    lcd.print("0");
  }
  lcd.print(HORAS);
  lcd.print(":");
  if (MINUTOS < 10) {
    lcd.print("0");
  }
  lcd.print(MINUTOS);
  lcd.print(":");
  if (SEGUNDOS < 10) {
    lcd.print("0");
  }
  lcd.print(SEGUNDOS);
}

//Función para definir los valores de consigna, se muestran por pantalla con un parpadeo
void Set() {
  if (MENUPREVIO == 0) {
    lcd.clear();
  }
  MENUPREVIO = MENU;
  if (PIND & (1 << PD3) && (millis() - tiemporebote) > 300) {
    tiemporebote = millis();
    if (MENU == 1) {
      if (DMILLAR < 0 || DMILLAR > 8) {
        DMILLAR = 0;
      } else if (DMILLAR < 9) {
        DMILLAR++;
      }
    } else if (MENU == 2) {
      if (UMILLAR < 0 || UMILLAR > 8) {
        UMILLAR = 0;
      } else if (UMILLAR < 9) {
        UMILLAR++;
      }
    } else if (MENU == 3) {
      if (CENTENA < 0 || CENTENA > 8) {
        CENTENA = 0;
      } else if (CENTENA < 9) {
        CENTENA++;
      }
    } else if (MENU == 4) {
      if (DECENA < 0 || DECENA > 8) {
        DECENA = 0;
      } else if (DECENA < 9) {
        DECENA++;
      }
    } else if (MENU == 5) {
      if (UNIDAD < 0 || UNIDAD > 8) {
        UNIDAD = 0;
      } else if (UNIDAD < 9) {
        UNIDAD++;
      }
    }
  }
  lcd.home ();
  if ((millis() - tiempoparpadeo) < 500) {
    lcd.print(DMILLAR);
    lcd.print(UMILLAR);
    lcd.print(CENTENA);
    lcd.print(DECENA);
    lcd.print(UNIDAD);
    lcd.setCursor ( 0, 1 );
    lcd.print("Definir ciclos");
    if (PIND & (1 << PD5)) { //Se muestra la versión o se limpia la zona
      lcd.setCursor ( 7, 0 );
      lcd.print("V1.5 2019");
    } else {
      lcd.setCursor ( 7, 0 );
      lcd.print("         ");
    }
  } else if ((millis() - tiempoparpadeo) < 1000) {
    if (MENU == 1) {
      lcd.setCursor ( 0, 0 );
    } else if (MENU == 2) {
      lcd.setCursor ( 1, 0 );
    } else if (MENU == 3) {
      lcd.setCursor ( 2, 0 );
    } else if (MENU == 4) {
      lcd.setCursor ( 3, 0 );
    } else if (MENU == 5) {
      lcd.setCursor ( 4, 0 );
    }
    lcd.print(" ");
  } else if ((millis() - tiempoparpadeo) > 1000) {
    tiempoparpadeo = millis();
  }
}

//Función para mostrar una pantalla inicial y de reposo
void pantallaInicio() {
  if (MENUPREVIO != MENU) {
    lcd.clear();
  }
  MENUPREVIO = MENU;
  lcd.setCursor ( 5, 0);
  lcd.write (byte (1));
  lcd.print("CTCR");
  lcd.setCursor ( 0, 1);
  lcd.print(" Ensayo velcros");
}

//Función para calcular el tiempo de ensayo
void calculatiempo() {
  tiempomillis = millis() - tiempoensayo;
  HORAS = tiempomillis / 3600000;
  MINUTOS = (tiempomillis - (HORAS * 3600000)) / 60000;
  SEGUNDOS = (tiempomillis - (HORAS * 3600000) - (MINUTOS * 60000)) / 1000;
}

//Función para gestionar la memoria eeprom
void gestionmem(int valor1) {
  if (MEMORIA == 1) {
    PUNT = 0;
    if (valor1 == 0) {
      EEPROM.get(PUNT, INICIO);
      if (INICIO == 1) {
        //Leer eeprom para obtener las variables de consigna (Un int son dos bytes, se incrementa la dirección en cada caso)
        EEPROM.get(PUNT += sizeof(int), UNIDAD);
        EEPROM.get(PUNT += sizeof(int), DECENA);
        EEPROM.get(PUNT += sizeof(int), CENTENA);
        EEPROM.get(PUNT += sizeof(int), UMILLAR);
        EEPROM.get(PUNT += sizeof(int), DMILLAR);
        //Leer eeprom para obtener la posición de los punteros
        EEPROM.get(12, PUNTUC);
        EEPROM.get(14, PUNTDC);
        EEPROM.get(16, PUNTCC);
        //Leer eeprom para obtener las variables de ciclo
        EEPROM.get(PUNTUC, UC);
        EEPROM.get(PUNTDC, DC);
        EEPROM.get(PUNTCC, CC);
        EEPROM.get(996, UMC);
        EEPROM.get(998, DMC);
        CICLOS = (DMC * 10000L) + (UMC * 1000L) + (CC * 100L) + (DC * 10L) + UC;
        CICLOSL = CICLOS;
      } else {
        //Definir los bytes que indican que ya está inicializado
        EEPROM.put(PUNT, 1);
        while (PUNT < 998) {  //Actualizar toda la eeprom con los valores a cero (Memoria de 1kB)
          EEPROM.put(PUNT += sizeof(int), 0);
        }
        //Definir los punteros iniciales para el primer ciclo de operación
        EEPROM.put(12, 18);
        EEPROM.put(14, 898);
        EEPROM.put(16, 986);
      }
    } else if (valor1 == 1) {
      //Actualizar eeprom (Se hace de este modo para que se mantengan valores en el menú set)
      EEPROM.update(PUNT += sizeof(int), UNIDAD);
      EEPROM.update(PUNT += sizeof(int), DECENA);
      EEPROM.update(PUNT += sizeof(int), CENTENA);
      EEPROM.update(PUNT += sizeof(int), UMILLAR);
      EEPROM.update(PUNT += sizeof(int), DMILLAR);
    } else if (valor1 == 2) {
      if (CICLOS == CONSIGNA) {
        PUNTUC += sizeof(int);
        PUNTDC += sizeof(int);
        PUNTCC += sizeof(int);
        if (PUNTUC > 897) {
          PUNTUC = 18;
        }
        if (PUNTDC > 985) {
          PUNTDC = 898;
        }
        if (PUNTCC > 995) {
          PUNTCC = 986;
        }
        EEPROM.update(12, PUNTUC);
        EEPROM.update(14, PUNTDC);
        EEPROM.update(16, PUNTCC);
      }
      DMC = CICLOS / 10000;
      UMC = (CICLOS - (DMC * 10000)) / 1000;
      CC = (CICLOS - (DMC * 10000) - (UMC * 1000)) / 1000;
      DC = (CICLOS - (DMC * 10000) - (UMC * 1000) - (CC * 100)) / 10;
      UC = CICLOS - (DMC * 10000) - (UMC * 1000) - (CC * 100) - (DC * 10);
      EEPROM.update(PUNTUC, UC);
      EEPROM.update(PUNTDC, DC);
      EEPROM.update(PUNTCC, CC);
      EEPROM.update(996, UMC);
      EEPROM.update(998, DMC);
    }
  }
  if (valor1 == 0 || valor1 == 1) {  //Cálculo de la variable consigna
    CONSIGNA = (DMILLAR * 10000L) + (UMILLAR * 1000L) + (CENTENA * 100L) + (DECENA * 10L) + UNIDAD;
  }
}

//Interrupción para el conteo de ciclos en cuaqluier instante
void ServicioSensor() {
  tiempointerr = millis();
  tiempopantalla = millis();
  INTERR = 1;
}
