/*
  ==========================================================================
  Proyecto:     Robot SUMO controlado por mando PS5 
  Descripción:  Control de dos motores DC mediante un driver puente H L298N,
                comandados por un ESP32-WROOM-32 que recibe las señales de
                un control PlayStation 5 conectado por Bluetooth
                usando la librería Bluepad32.

  Autor:        Giancarlo Custodio Lara
  Carnet:       24204
  Versión:      1.0
  Fecha:        12/08/2026
  ==========================================================================

  HARDWARE UTILIZADO:
  - 1x Placa ESP32-WROOM-32 (DevKit genérico, 38 pines)
  - 1x Driver puente H L298N
  - 2x Motorreductores DC (con caja amarilla, tipo TT)
  - 1x Control inalámbrico PlayStation 5 
  - 1x Batería de 6 pilas AA (9V) 
  - 1x LED indicador + resistencia de 220 ohms

  DEPENDENCIAS DE SOFTWARE 
  - Arduino IDE
  - Paquete de placas "esp32" de Espressif Systems
  - Paquete de placas "esp32_bluepad32" de Ricardo Quesada (incluye la
    librería Bluepad32.h para la comunicación con el control PS5)

  CONEXIONES (ESP32 -> L298N):
  - GPIO27 -> IN1
  - GPIO26 -> IN2
  - GPIO25 -> IN3
  - GPIO33 -> IN4
  - GPIO14 -> ENA (PWM, velocidad motor izquierdo)
  - GPIO32 -> ENB (PWM, velocidad motor derecho)
  - GND    -> GND (tierra común OBLIGATORIA entre ESP32 y L298N)
  - GPIO2  -> LED indicador 

  ALIMENTACIÓN:
  - El ESP32 se alimenta por su propio cable USB a la compu.
  - El L298N y los motores se alimentan con la batería de 6 pilas AA (9V)
    conectada en su entrada de +12V/GND.
  - NO se debe alimentar el ESP32 desde el pin +5V del L298N si el ESP32
    ya está siendo alimentado por USB al mismo tiempo, y viceversa.
  ==========================================================================
*/

#include <Bluepad32.h>

// ---------------------------------------------------------------------
// Definición de pines
// ---------------------------------------------------------------------
#define IN1 27      // Dirección motor izquierdo (borne A)
#define IN2 26      // Dirección motor izquierdo (borne B)
#define IN3 25      // Dirección motor derecho (borne A)
#define IN4 33      // Dirección motor derecho (borne B)
#define ENA 14      // Habilitación / velocidad PWM motor izquierdo
#define ENB 32      // Habilitación / velocidad PWM motor derecho
#define LED_TEST 2  // LED indicador de estado de conexión del control

// Puntero al control PS5 actualmente conectado 
ControllerPtr myController;

// ---------------------------------------------------------------------
// Callback: se ejecuta automáticamente cuando un control se conecta
void onConnectedController(ControllerPtr ctl) {
  Serial.println("Control conectado");
  myController = ctl;
}

// ---------------------------------------------------------------------
// Callback: se ejecuta automáticamente cuando un control se desconecta
void onDisconnectedController(ControllerPtr ctl) {
  Serial.println("Control desconectado");
  myController = nullptr;
}

// ---------------------------------------------------------------------
// Controla el motor izquierdo.
// velocidad: valor de -255 (reversa máxima) a 255 (adelante máximo)
void motorIzq(int velocidad) {
  digitalWrite(IN1, velocidad >= 0);
  digitalWrite(IN2, velocidad < 0);
  analogWrite(ENA, abs(velocidad));
}

// ---------------------------------------------------------------------
// Controla el motor derecho.
// velocidad: valor de -255 (reversa máxima) a 255 (adelante máximo)
void motorDer(int velocidad) {
  digitalWrite(IN3, velocidad >= 0);
  digitalWrite(IN4, velocidad < 0);
  analogWrite(ENB, abs(velocidad));
}

void setup() {
  Serial.begin(115200);

  // Configuración de pines de salida
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(LED_TEST, OUTPUT);

  // Inicializa Bluepad32 y registra las funciones que se llamarán
  // automáticamente al conectar/desconectar un control.
  BP32.setup(&onConnectedController, &onDisconnectedController);
}

void loop() {
  // Debe llamarse en cada ciclo: procesa eventos de Bluetooth y
  // actualiza el estado del control (botones, sticks, giroscopio, etc.)
  BP32.update();

  if (myController && myController->isConnected()) {
    // Control conectado: enciende el LED 
    digitalWrite(LED_TEST, HIGH);

    // Lectura de los sticks del DualSense.
    // axisY()  -> stick izquierdo, eje vertical (adelante/atrás)
    // axisRX() -> stick derecho,  eje horizontal (giro)
    // Rango aproximado de cada eje: -512 a 512
    int avance = -myController->axisY();
    int giro   =  myController->axisRX();

    // Mezcla tipo "tanque": se combina avance y giro para cada motor,
    // y se reescala el rango a -255..255 que espera analogWrite().
    int velIzq = constrain((avance + giro) / 2, -255, 255);
    int velDer = constrain((avance - giro) / 2, -255, 255);

    motorIzq(velIzq);
    motorDer(velDer);
  } else {
    // Sin control conectado: por seguridad, se apagan los motores
    // y se apaga el LED 
    digitalWrite(LED_TEST, LOW);
    motorIzq(0);
    motorDer(0);
  }

  delay(20); // Pequeña pausa para no saturar el bucle principal
}