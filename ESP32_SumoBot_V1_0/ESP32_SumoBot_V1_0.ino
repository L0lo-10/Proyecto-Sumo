/*
  Proyecto:     Robot SUMO controlado por mando PS5
  Descripción:  Control de dos motores DC N20 mediante un driver puente H
                TB6612FNG, comandados por un ESP32-WROOM-32

  Autor:        Giancarlo Custodio Lara
  Carnet:       24204
  Versión:      1.3 (inversión de sentido de giro de los motores)
  Fecha:        02/11/2026


  DEPENDENCIAS DE SOFTWARE
  - Arduino IDE
  - Paquete de placas "esp32" de Espressif Systems
  - Paquete de placas "esp32_bluepad32" de Ricardo Quesada (incluye la
    librería Bluepad32.h para la comunicación con el control PS5)

  CONEXIONES (ESP32 -> TB6612FNG):
  (El número de pin es el del símbolo del ESP32 en el esquemático)

    Pin símbolo | GPIO   | TB6612FNG
    ------------+--------+----------
        11      | GPIO14 | PWMA  (PWM, velocidad motor izquierdo)
        10      | GPIO27 | AIN2
         9      | GPIO26 | AIN1
        13      | GPIO13 | STBY  (debe estar en HIGH para operar)
         8      | GPIO25 | BIN1
         7      | GPIO33 | BIN2
         6      | GPIO32 | PWMB  (PWM, velocidad motor derecho)
        16      | 3V3    | VCC   (lógica del TB6612FNG)
        14      | GND    | GND   (tierra común OBLIGATORIA)
         -      | GPIO2  | LED indicador (con resistencia de 220 ohms)

  CONTROLES:
  - Botón X (Cross)     -> ACTIVA el robot (los joysticks mueven los motores)
  - Botón Cuadrado      -> DESACTIVA el robot (motores detenidos)
  - Stick izquierdo (eje vertical)   -> avance / reversa
  - Stick derecho (eje horizontal)   -> giro
  - El robot siempre inicia DESACTIVADO, y se desactiva si el control
    se desconecta.

  LED INDICADOR (GPIO2):
  - Apagado       -> sin control conectado
  - Parpadeando   -> control conectado, robot DESACTIVADO
  - Encendido fijo-> control conectado, robot ACTIVO

*/

#include <Bluepad32.h>

// ---------------------------------------------------------------------
// Definición de pines (GPIO del ESP32)
// ---------------------------------------------------------------------
// Motor A (izquierdo)
#define AIN1 26     // Dirección motor izquierdo
#define AIN2 27     // Dirección motor izquierdo
#define PWMA 14     // Velocidad PWM motor izquierdo

// Motor B (derecho)
#define BIN1 25     // Dirección motor derecho
#define BIN2 33     // Dirección motor derecho
#define PWMB 32     // Velocidad PWM motor derecho

// Control general
#define STBY 13     // Standby del TB6612FNG (HIGH = driver activo)
#define LED_TEST 2  // LED indicador de estado

//iNVERTIR MOTORES
#define INVERTIR_MOTOR_IZQ true
#define INVERTIR_MOTOR_DER true

// Puntero al control PS5 actualmente conectado
ControllerPtr myController;

// Estado del robot: false = desactivado (motores detenidos), true = activo
bool robotActivo = false;

// ---------------------------------------------------------------------
// Botones usados para activar/desactivar el robot.

bool botonActivar()    { return myController->a(); }  // X (Cross)
bool botonDesactivar() { return myController->x(); }  // Cuadrado

// ---------------------------------------------------------------------
// Callback
void onConnectedController(ControllerPtr ctl) {
  Serial.println("Control conectado");
  myController = ctl;
  robotActivo = false;  // siempre inicia desactivado
}

// se ejecuta automáticamente cuando un control se desconecta
void onDisconnectedController(ControllerPtr ctl) {
  Serial.println("Control desconectado");
  myController = nullptr;
  robotActivo = false;
}

// ---------------------------------------------------------------------
// Controla el motor izquierdo (canal A del TB6612FNG).

void motorIzq(int velocidad) {
  if (INVERTIR_MOTOR_IZQ) velocidad = -velocidad;
  digitalWrite(AIN1, velocidad >= 0);
  digitalWrite(AIN2, velocidad < 0);
  analogWrite(PWMA, abs(velocidad));
}

// ---------------------------------------------------------------------
// Controla el motor derecho (canal B del TB6612FNG).

void motorDer(int velocidad) {
  if (INVERTIR_MOTOR_DER) velocidad = -velocidad;
  digitalWrite(BIN1, velocidad >= 0);
  digitalWrite(BIN2, velocidad < 0);
  analogWrite(PWMB, abs(velocidad));
}

void setup() {
  Serial.begin(115200);

  // Configuración de pines de salida
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(PWMA, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);
  pinMode(PWMB, OUTPUT);
  pinMode(STBY, OUTPUT);
  pinMode(LED_TEST, OUTPUT);

  // Habilita el driver TB6612FNG. Si STBY queda en LOW los motores
  
  digitalWrite(STBY, HIGH);

  // Inicializa Bluepad32 y registra las funciones que se llamarán
  // automáticamente al conectar/desconectar un control.
  BP32.setup(&onConnectedController, &onDisconnectedController);
}

void loop() {
  // Debe llamarse en cada ciclo: procesa eventos de Bluetooth y
  // actualiza el estado del control (botones, sticks, giroscopio, etc.)
  BP32.update();

  if (myController && myController->isConnected()) {

    // Lectura de botones de activación/desactivación.
    if (botonActivar())    robotActivo = true;
    if (botonDesactivar()) robotActivo = false;

    if (robotActivo) {
      // Robot ACTIVO: LED fijo y los joysticks controlan los motores
      digitalWrite(LED_TEST, HIGH);

      // Lectura de los sticks del DualSense.
      int avance = -myController->axisY();
      int giro   =  myController->axisRX();

      // Mezcla tipo "tanque": se combina avance y giro para cada motor,
      // y se reescala el rango a -255..255 que espera analogWrite().
      int velIzq = constrain((avance + giro) / 2, -255, 255);
      int velDer = constrain((avance - giro) / 2, -255, 255);

      motorIzq(velIzq);
      motorDer(velDer);
    } else {
      // Robot DESACTIVADO: motores detenidos y LED parpadeando
      motorIzq(0);
      motorDer(0);

      static unsigned long ultimoParpadeo = 0;
      static bool estadoLed = false;
      if (millis() - ultimoParpadeo >= 300) {
        ultimoParpadeo = millis();
        estadoLed = !estadoLed;
        digitalWrite(LED_TEST, estadoLed);
      }
    }
  } else {
    // Sin control conectado: se apagan los motores
    // y se apaga el LED
    digitalWrite(LED_TEST, LOW);
    motorIzq(0);
    motorDer(0);
  }

  delay(20); // Pequeña pausa para no saturar el bucle principal
}