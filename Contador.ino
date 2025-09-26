
//Saúl Íñiguez Macedo -- Centro Tecnológico del Calzado de La Rioja
//Prog: Contador   Versión: 1.1   Fecha: 08/04/2019

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
volatile int CICLOS = 0;     //Ciclos contados
int CICLOSPRINT = 0;         //Variable ciclos a imprimir para que no se modifique con la interrupción
volatile int INTERR = 0;     //Variable para saber cuando ha habido interrupción

//Variables para cálculo de RPM
volatile int CICLORPM = 0;   //Número de ciclos utilizado para el cálculo de RPM
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

/*
  //Variables pulsadores sensores y actuadores
  int SENSOR = 2;     //Sensor para conteo de ciclos
  int PULSADOR = 3;   //Pulsador para cambiar digitos
  int SET = 4;        //Pulsador de set
  int RESET = 5;      //Pulsador de reset
  int RELE = 6;       //Relé NC para cierre de cicrcuito en contactor motor
*/

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
  DDRD = B01000000;
  //Se inicia el LCD y se define el brillo
  lcd.begin (16, 2);
  lcd.setBacklightPin(3, POSITIVE);
  lcd.setBacklight(HIGH);
  attachInterrupt( 0, ServicioSensor, RISING);
  lcd.createChar (1, A);   //Creación de un caracter especial con la variable A
}

void loop() {

  //Se actualiza el tiempo mientras se está a low para empezar a contar desde que se pulsa
  if (!(PIND & (1 << PD3)) && !(PIND & (1 << PD4)) && !(PIND & (1 << PD5))) {
    tiemporebote = millis();
  }

  if (MENU == 0) {  //Cálculo de la variable consigna
    CONSIGNA = (DMILLAR * 10000L) + (UMILLAR * 1000L) + (CENTENA * 100L) + (DECENA * 10L) + UNIDAD;
  }

  if (CICLOS == CONSIGNA && CONSIGNA != 0) {  //Activación del relé (Pasa a abierto hasta hacer reset)
    PORTD |= (1 << PB6);  //Se define solamente el pin 6 a HIGH
  }

  if (CICLOS != CONSIGNA && CICLOS != 0) {  //Se calculan las variables de tiempo
    calculatiempo();
  }

  //RPM se pone a 0 pasados 10s. Respuesta rápida a altas RPM pero no mide menos de 6 RPM sin modificar los 10s
  if (CICLORPM == 1 && RPMPREVIO == 0) {
    tiemporpm = millis();
    RPMPREVIO = 1;
  } else if (CICLORPM >= 5 || (millis() - tiemporpm) > 10000) {
    if (CICLORPM == 0) {
      RPM = 0;
    } else {
      RPM = 60000 / ((millis() - tiemporpm) / CICLORPM);
    }
    CICLORPM = 0;
    RPMPREVIO = 0;
  }

  if (PIND & (1 << PD5) && (millis() - tiemporebote) > 300) {  //Se hace reset de ciclos y variables de tiempo
    tiemporebote = millis();
    CICLOS = 0;
    HORAS = 0;
    MINUTOS = 0;
    SEGUNDOS = 0;
    PORTD &= ~(1 << PB6);  //Se define solamente el pin 6 a LOW
  }

  if (PIND & (1 << PD4) && (millis() - tiemporebote) > 300) {  //Se define la variable para entrar a los menús
    tiemporebote = millis();
    if (MENU < 5 && CICLOS == 0) {
      MENU++;
    } else if (MENU == 5 || MENU == 6) {
      MENU = 0;
      tiempopantalla = millis();
    }
  }

  //Se cuentan los ciclos si han pasado 100ms desde la interrupción y el pin sigue a high
  //Si en ese tiempo hay otro pulso el tiempo se pone a cero. Si se deja pulsado solo se cuenta el primero
  if (PIND & (1 << PD2) && (millis() - tiempointerr) > 100 && INTERR == 1 && MENU == 0) {
    if (CONSIGNA > 0 && CICLOS != CONSIGNA) {  //Se cuenta el ciclo
      CICLOS++;
    }
    if (CICLOS == 1) {  //Se fija el tiempo de inicio de ensayo
      tiempoensayo = millis();
    }
    CICLORPM++;
    INTERR = 0;
  } else if (!(PIND & (1 << PD2))) {
    INTERR = 0;
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
      if (DMILLAR < 9) {
        DMILLAR++;
      } else if (DMILLAR == 9) {
        DMILLAR = 0;
      }
    } else if (MENU == 2) {
      if (UMILLAR < 9) {
        UMILLAR++;
      } else if (UMILLAR == 9) {
        UMILLAR = 0;
      }
    } else if (MENU == 3) {
      if (CENTENA < 9) {
        CENTENA++;
      } else if (CENTENA == 9) {
        CENTENA = 0;
      }
    } else if (MENU == 4) {
      if (DECENA < 9) {
        DECENA++;
      } else if (DECENA == 9) {
        DECENA = 0;
      }
    } else if (MENU == 5) {
      if (UNIDAD < 9) {
        UNIDAD++;
      } else if (UNIDAD == 9) {
        UNIDAD = 0;
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
      lcd.print("V1.1 2019");
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
  lcd.print(" Ensayo flexion");
}

//Función para calcular el tiempo de ensayo
void calculatiempo() {
  tiempomillis = millis() - tiempoensayo;
  HORAS = tiempomillis / 3600000;
  MINUTOS = (tiempomillis - (HORAS * 3600000)) / 60000;
  SEGUNDOS = (tiempomillis - (HORAS * 3600000) - (MINUTOS * 60000)) / 1000;
}

//Interrupción para el conteo de ciclos en cuaqluier instante
void ServicioSensor() {
  tiempointerr = millis();
  tiempopantalla = millis();
  INTERR = 1;
}
