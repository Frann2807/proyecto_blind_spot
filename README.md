# Blind Spot – Bastón para no videntes

Proyecto con 2 x **ESP32-C3 Super Mini** comunicadas por **ESP-NOW**, con
debug inalámbrico por **BLE** (NimBLE, servicio NUS), siguiendo el estilo
del firmware `balancin` de ProyectoCyS (BLE + intérprete de comandos).

## Arquitectura

- **Muñeca** (`firmware/muneca`): pulsador en GPIO3 -> envía `"btn"` por
  ESP-NOW; vibrador por transistor en GPIO10 cuando el bastón avisa.
- **Bastón** (`firmware/baston`): sensor IR GPIO0, fotoresistencia GPIO1,
  LEDs GPIO5/6 y buzzer activo GPIO4. Responde al `"btn"` de la muñeca
  con el buzzer (localizador) y avisa obstáculos/oscuridad a la muñeca.

## Mapa de pines (sin conflictos: todo puede convivir en UNA sola ESP)

| Pin  | Dispositivo     | Uso                                   | Modo |
|------|-----------------|---------------------------------------|------|
| GPIO0  | Bastón | Sensor IR analógico (ADC1_CH0)        | entrada analógica |
| GPIO1  | Bastón | Fotoresistencia (ADC1_CH1)            | entrada analógica |
| GPIO3  | Muñeca | **Pulsador** (botón entre 3V3 y GPIO3, pulldown interno) | INPUT_PULLDOWN |
| GPIO4  | Bastón | **Buzzer activo**                     | salida |
| GPIO5  | Bastón | LED 1 (oscuridad nivel 1)             | salida |
| GPIO6  | Bastón | LED 2 (oscuridad nivel 2)             | salida |
| GPIO10 | Muñeca | Transistor -> motor vibrador          | salida (PWM) |

Recomendaciones: GPIO3 para el pulsador (el C3 tiene pulldown interno en
todos los GPIO) y GPIO4 para el buzzer. Se evitan GPIO2/8/9 (strapping) y
GPIO20/21 (UART), y GPIO8 suele ser el LED onboard.

## Protocolo ESP-NOW

Ambas placas en modo `WIFI_STA`, canal 1, peer **broadcast**
(`FF:FF:FF:FF:FF:FF`), por lo que no hay que conocer la MAC del otro.
Si se quiere dirigir solo a una placa, reemplazar `macPeer` en el .ino
por la MAC de la otra (el comando `mac` la muestra).

```cpp
typedef struct {
  char tipo[8];   // "btn", "obst", "libre", "oscuro", "claro", "ping"
  int  valor;     // dato del sensor (opcional)
} Mensaje;
```

- Muñeca -> Bastón: `"btn"` (buzzer de localización, 3 beeps), `"ping"` (1 beep).
- Bastón -> Muñeca: `"obst"`/`"libre"` (sensor IR, se repite `"obst"`
  cada `cdobs` ms mientras siga el obstáculo), `"oscuro"`/`"claro"`
  (fotoresistencia).

## Estructura del repositorio

```
firmware/
  muneca/     muneca.ino + espnow_cb.h   (firmware final de la muñeca)
  baston/     baston.ino + espnow_cb.h   (firmware final del bastón)
test/         (cada test prueba UNA cosa; todos usan pines distintos
               de los de arriba, así que pueden probarse en la misma ESP)
  test01_boton/           pulsador GPIO3
  test02_transistor/      transistor/vibrador GPIO10
  test03_buzzer/          buzzer activo GPIO4
  test04_sensor_ir/       sensor IR GPIO0
  test05_fotoresistencia/ LDR GPIO1
  test06_leds/            LEDs GPIO5/6
  test07_espnow/          ESP-NOW broadcast (ping cada 5 s)
  test08_ble/             BLE NUS con ECO
```

> `espnow_cb.h` está duplicado a propósito en cada carpeta: el IDE de
> Arduino solo compila archivos dentro de la carpeta del sketch, y ese
> header adapta el callback de recepción de ESP-NOW a las firmas
> distintas de arduino-esp32 2.x y 3.x.

## Librerías y placa

- Placa en el IDE: **ESP32C3 Dev Module** (o ESP32-C3 Super Mini).
- Librería externa: **NimBLE-Arduino** (h2zero), desde el Gestor de
  Librerías. WiFi/esp_now/Preferences vienen con el núcleo ESP32.
- Si el sketch no entra, elegir esquema de particiones "Huge APP".
- Los tests 01-06 solo usan Serial (monitor a 115200), no requieren BLE.

## Pruebas con una sola ESP32-C3

Los firmwares traen comandos que **simulan las acciones a distancia**:

1. **Probar el bastón** (una sola placa): flashear `firmware/baston` y
   conectar por BLE (`nRF Connect` o `Serial Bluetooth Terminal`,
   dispositivo `BastonBS`, enviar con `\n`):
   - `simmuneca btn` -> buzzer de localización (lo mismo que recibir el botón de la muñeca).
   - `buzz 500`, `led1 1`, `led2 1`, `auto`, `ir`, `foto`, `stream 1`.
   - Ajustar umbrales reales: `umbral 2500`, `dirir 1`, `luz1 2000`, `luz2 3000`, `fdir 1`, `guardar`.
2. **Probar la muñeca** (una sola placa): flashear `firmware/muneca`
   (dispositivo `MunecaBS`):
   - `btn` -> envía `"btn"` (simula pulsación), `ping`, `vibra 3`.
   - `simbaston obst|ping|oscuro|claro` -> simula mensajes entrantes del bastón.
3. **Enlace real ESP-NOW**: con las dos placas finales, o flasheando
   `test07_espnow` en ambas para ver los pings mutuos.
   Importante: ambos firmwares fuerzan el canal 1.

## Comandos BLE de la muñeca

| Comando | Descripción |
|---|---|
| `help` / `mostrar` | ayuda y valores actuales |
| `btn` | simula el pulsador: envía `"btn"` al bastón + vibra |
| `ping` | envía `"ping"` al bastón |
| `vibra [n]` | vibra n veces (default 1) |
| `simbaston <obst\|ping\|oscuro\|claro>` | simula mensaje entrante del bastón |
| `pwm N` / `pulso N` / `pausa N` / `npul N` | intensidad, ms por pulso, pausa, pulsos en `obst` |
| `guardar` | persiste los parámetros en flash |
| `mac` | muestra la MAC (para configurar el peer) |

## Comandos BLE del bastón

| Comando | Descripción |
|---|---|
| `help` / `mostrar` | ayuda y valores actuales |
| `buzz [ms]` | prueba el buzzer (default 500 ms) |
| `led1 0/1`, `led2 0/1`, `auto` | LEDs manuales o automáticos por umbrales |
| `ir` / `foto` | lectura única de cada sensor |
| `stream 0/1` | stream de sensores cada 250 ms por BLE |
| `simmuneca <btn\|ping>` | simula mensaje entrante de la muñeca |
| `umbral N`, `dirir 1/-1`, `cdobs N`, `nprom N` | configuración del sensor IR |
| `luz1 N`, `luz2 N`, `fdir 1/-1` | configuración de la fotoresistencia |
| `guardar` / `mac` | persistir en flash / mostrar MAC |

## Notas de hardware

- **Pulsador**: entre 3V3 y GPIO3; el pulldown interno mantiene LOW en
  reposo (presionado = HIGH).
- **Transistor/motor vibrador**: NPN o MOSFET en GPIO10 (PWM 0-255).
  Si es un motor, agregar diodo flyback en paralelo.
- **Buzzer activo**: usar un módulo con driver o un transistor; no
  colgar más de ~20 mA directamente del GPIO.
- **Sensor IR y LDR**: divisores alimentados a 3V3; las lecturas van de
  0 a 4095. La dirección de los umbrales (`dirir`, `fdir`) se calibra
  con los tests 04/05 y se configura por BLE.
