/*
 * =====================================================================
 * PROYECTO: Sumo Bot - Control Inalámbrico por ESP32 y Bluepad32
 * CURSO: Simulación y Fabricación de Circuitos Impresos
 * 
 * REQUISITOS PREVIOS Y CONFIGURACIÓN DEL ENTORNO:
 * 1. Hardware Compatible: Tarjeta de desarrollo ESP32 (ej. ESP32 WROOM)
 *    con soporte Bluetooth y un control compatible (ej. Mando de PS4 - DualShock 4).
 * 2. Librerías de Software:
 *    - Tener instalado el núcleo de ESP32 en el Arduino IDE.
 *    - Descargar e importar la librería "Bluepad32" (desarrollada por Ricardo Quesada).
 * 3. Conexiones físicas del hardware según el esquemático de la PCB:
 *    - Motor A: PWMA (Pin 32), AIN1 (Pin 25), AIN2 (Pin 33)
 *    - Motor B: PWMB (Pin 13), BIN1 (Pin 27), BIN2 (Pin 14)
 *    - Driver TB6612FNG: STBY (Pin 26)
 * =====================================================================
 */

#include <Bluepad32.h>

// ======================================================
//                 CONFIGURACIÓN DE PINES
// ======================================================

// -------------------- MOTOR A -------------------------
const int PWMA = 32;
const int AIN1 = 25;
const int AIN2 = 33;

// -------------------- MOTOR B -------------------------
const int PWMB = 13;
const int BIN1 = 27;
const int BIN2 = 14;

// -------------------- TB6612FNG -----------------------
const int STBY = 26;

// ======================================================
//              CONFIGURACIÓN DEL CONTROL
// ======================================================

// Zona muerta del joystick (filtra ruido y lecturas fantasma en reposo)
const int JOYSTICK_DEADZONE = 40;

// Zona muerta de los gatillos analógicos
const int TRIGGER_DEADZONE = 30;

// ======================================================
//              CONTROLADOR CONECTADO
// ======================================================
ControllerPtr myController = nullptr;

// ======================================================
//               PROTOTIPOS DE FUNCIONES
// ======================================================
void setMotorA(int speed);
void setMotorB(int speed);
void stopMotors();

// ======================================================
//          CALLBACK: CONTROLADOR CONECTADO
// ======================================================
void onConnectedController(ControllerPtr ctl) {
  // Se asegura de aceptar únicamente un mando a la vez
  if (myController == nullptr) {
    myController = ctl;
    Serial.println();
    Serial.println("================================");
    Serial.println("CONTROL PS4 CONECTADO EXITOSAMENTE");
    Serial.println("================================");
    Serial.println();
  } else {
    Serial.println("Aviso: Ya existe un controlador conectado previamente.");
  }
}

// ======================================================
//         CALLBACK: CONTROLADOR DESCONECTADO
// ======================================================
void onDisconnectedController(ControllerPtr ctl) {
  if (myController == ctl) {
    Serial.println();
    Serial.println("================================");
    Serial.println("CONTROL PS4 DESCONECTADO");
    Serial.println("================================");
    Serial.println();

    // Libera la referencia del puntero y frena por seguridad
    myController = nullptr;
    stopMotors();
  }
}

// ======================================================
//                CONTROL DE ACCIONAMIENTO MOTOR A
// ======================================================
void setMotorA(int speed) {
  // Limita los valores al rango físico del PWM (-255 a 255)
  speed = constrain(speed, -255, 255);

  if (speed > 0) {
    // Avance
    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);
    analogWrite(PWMA, speed);
  } 
  else if (speed < 0) {
    // Retroceso (invierte polaridad lógica)
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);
    analogWrite(PWMA, -speed);
  } 
  else {
    // Freno inercial
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, LOW);
    analogWrite(PWMA, 0);
  }
}

// ======================================================
//                CONTROL DE ACCIONAMIENTO MOTOR B
// ======================================================
void setMotorB(int speed) {
  speed = constrain(speed, -255, 255);

  if (speed > 0) {
    // Avance
    digitalWrite(BIN1, HIGH);
    digitalWrite(BIN2, LOW);
    analogWrite(PWMB, speed);
  } 
  else if (speed < 0) {
    // Retroceso
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, HIGH);
    analogWrite(PWMB, -speed);
  } 
  else {
    // Freno inercial
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, LOW);
    analogWrite(PWMB, 0);
  }
}

// ======================================================
//               FUNCIÓN DE EMERGENCIA: FRENAR
// ======================================================
void stopMotors() {
  setMotorA(0);
  setMotorB(0);
}

// ======================================================
//                        SETUP
// ======================================================
void setup() {
  // Inicialización del puerto serial para depuración
  Serial.begin(115200);
  delay(1000);

  // Configuración de los pines del driver de motores como salidas
  pinMode(PWMA, OUTPUT);
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);

  pinMode(PWMB, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);

  pinMode(STBY, OUTPUT);

  // Estado seguro inicial: Desactiva el driver y detiene los motores
  digitalWrite(STBY, LOW);
  stopMotors();

  // Habilita el circuito integrado TB6612FNG
  digitalWrite(STBY, HIGH);

  // Inicialización de la pila de Bluepad32 y callbacks
  BP32.setup(&onConnectedController, &onDisconnectedController);
  BP32.enableVirtualDevice(false);

  Serial.println();
  Serial.println("================================");
  Serial.println("     SUMO BOT - ESP32 READY     ");
  Serial.println("================================");
  Serial.println("Esperando vinculación del control PS4...");
  Serial.println();
}

// ======================================================
//                        LOOP
// ======================================================
void loop() {
  // Actualiza periódicamente el estado de la pila Bluetooth
  bool dataUpdated = BP32.update();

  // Procesa comandos únicamente si hay un control vinculado de forma activa
  if (dataUpdated && myController != nullptr) {
    ControllerPtr ctl = myController;

    // Lectura de los gatillos analógicos traseros (R2 para avance, L2 para retroceso)
    int rawThrottle = ctl->throttle();
    int rawBrake = ctl->brake();

    int throttle = rawThrottle;
    int brake = rawBrake;

    // Aplicación de zona muerta en gatillos
    if (throttle < TRIGGER_DEADZONE) throttle = 0;
    if (brake < TRIGGER_DEADZONE) brake = 0;

    // Mapeo de la señal original del control (0-1023) al rango PWM del driver (0-255)
    throttle = map(throttle, 0, 1023, 0, 255);
    brake = map(brake, 0, 1023, 0, 255);

    // Cálculo del vector de aceleración lineal neta
    int throttleCommand = throttle - brake;

    // Lectura del eje horizontal del joystick derecho para el control de dirección (giros)
    int rawSteering = ctl->axisRX();
    int steering;

    // Filtrado por zona muerta del joystick
    if (abs(rawSteering) < JOYSTICK_DEADZONE) {
      steering = 0;
    } else {
      steering = map(rawSteering, -511, 512, -255, 255);
    }

    // Mezcla de conducción diferencial (Differential Drive)
    // Motor Izquierdo = Componente Lineal + Componente de Giro
    // Motor Derecho  = Componente Lineal - Componente de Giro
    int leftMotor = throttleCommand + steering;
    int rightMotor = throttleCommand - steering;

    // Restricción de seguridad de las señales enviadas a los motores
    leftMotor = constrain(leftMotor, -255, 255);
    rightMotor = constrain(rightMotor, -255, 255);

    // Envío de consignas de velocidad a los actuadores
    setMotorA(leftMotor);
    setMotorB(rightMotor);

    // Salida de telemetría por puerto serial para análisis y depuración
    Serial.print("R2: "); Serial.print(rawThrottle);
    Serial.print(" | L2: "); Serial.print(rawBrake);
    Serial.print(" | RX: "); Serial.print(rawSteering);
    Serial.print(" | CmdVel: "); Serial.print(throttleCommand);
    Serial.print(" | Giro: "); Serial.print(steering);
    Serial.print(" | MotorA: "); Serial.print(leftMotor);
    Serial.print(" | MotorB: "); Serial.println(rightMotor);
  }

  // Pequeña pausa para estabilizar el ciclo del procesador
  delay(5);
}