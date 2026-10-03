# Firmware NFC_S3_NVS

Firmware ESP-IDF 5.5.4 para validar comunicación bidireccional entre un ESP32-S3 y un smartphone mediante un NT3H2211 (NTAG I2C plus 2k).

## Hardware y conexiones

| Señal | ESP32-S3 | NT3H2211 |
|---|---:|---|
| SDA | GPIO8 | SDA |
| SCL | GPIO9 | SCL |
| Field Detect | GPIO18 | FD |
| Alimentación | 3.3 V | VCC |
| Tierra | GND | VSS |

El dispositivo usa la dirección I2C de 7 bits `0x55` y el bus opera a 100 kHz. SDA y SCL deben llevar pull-ups externas, típicamente 4.7 kΩ a 3.3 V. FD es open-drain y debe tener una pull-up externa mayor de 2 kΩ; el firmware habilita además la pull-up interna de GPIO18, pero no debe considerarse sustituto de la externa para un montaje final.

## Arquitectura

- `i2c_manager`: bus I2C nuevo de ESP-IDF (`driver/i2c_master.h`), exclusión mutua y diagnóstico.
- `nt3h2211`: bloques EEPROM de 16 bytes, Session Registers, estado de arbitraje y Field Detect.
- `nvs_manager`: namespace `contador_nvs`, clave `contador`, valor inicial `20`.
- `nfc_manager`: NDEF, ISR mínima de GPIO18 y tarea de procesamiento NFC.
- `contador_monitor_task`: lee e imprime NVS cada 5 segundos sin escribirla.

La ISR solo envía una notificación FreeRTOS. La lectura I2C, el análisis NDEF y las operaciones NVS ocurren en `nfc_event_task`.

## Decisión NDEF frente a SRAM/pass-through

Este PoC utiliza un NDEF Text Record almacenado en EEPROM. Es la alternativa mínima interoperable con las APIs NDEF estándar de Android y permite validar ambos sentidos sin implementar comandos NFC-A propietarios en la aplicación.

No se usa pass-through en esta etapa. Pass-through ofrece SRAM de 64 bytes y handshake mediante `SRAM_I2C_READY`/`SRAM_RF_READY`, pero exige:

- comandos NFC-A crudos sobre páginas `F0h` a `FFh`;
- configurar `TRANSFER_DIR` desde I2C;
- terminar cada transferencia en la página/bloque final;
- un protocolo propio de fragmentación y confirmación.

Para mensajes pequeños y cambios poco frecuentes, NDEF en EEPROM simplifica la validación. El firmware nunca reescribe NVS durante el monitor y solo actualiza la EEPROM al arrancar o después de un cambio real.

## Inicialización NDEF

El datasheet indica que el Capability Container viene en cero de fábrica. El firmware verifica los bytes CC y, si es necesario, configura `E1 10 6D 00`. Como el mismo bloque I2C contiene la dirección del esclavo, al escribirlo conserva explícitamente `0x55` mediante el byte codificado `0xAA`. El dispositivo se vuelve a sondear después de esa operación.

## Arbitraje RF/I2C

El NT3H2211 usa arbitraje first-come, first-served:

- `RF_LOCKED=1`: el ESP32 no accede a EEPROM; los Session Registers siguen disponibles.
- `EEPROM_WR_BUSY=1`: el firmware espera antes de leer o escribir.
- Al finalizar el campo RF, GPIO18 vuelve a nivel alto y la tarea NFC procesa el NDEF.
- Las transacciones I2C se serializan con mutex.
- Las lecturas respetan la secuencia indicada por NXP: dirección de bloque, STOP, pausa mínima de 50 µs y lectura de 16 bytes.
- Tras una escritura EEPROM se esperan al menos 5 ms.

## Flujo de arranque

1. Inicializar NVS.
2. Crear `contador=20` solo si la clave no existe.
3. Inicializar I2C y verificar `0x55`.
4. Inicializar NT3H2211 y el Capability Container.
5. Configurar FD para detección simple de campo.
6. Publicar `contador=<valor NVS>` como NDEF.
7. Iniciar las tareas de eventos y monitor.

## Compilación y carga

Desde una terminal ESP-IDF 5.5.4:

```bash
idf.py build
idf.py -p COMx flash
idf.py -p COMx monitor
```

Para compilar, flashear y abrir monitor:

```bash
idf.py -p COMx flash monitor
```

## Logs esperados

```text
I (...) NVS: Contador cargado: 20
I (...) I2C: Dispositivo detectado en 0x55
I (...) NFC: NDEF publicado: contador=20
I (...) NFC: Campo RF detectado; sesion NFC iniciada
I (...) NFC: Sesion NFC finalizada
I (...) NFC: Nuevo contador recibido: 1000
I (...) NVS: Contador actualizado correctamente: 1000
I (...) NVS: Contador actual: 1000
```

## Prueba física recomendada

1. Flashear y comprobar el sondeo de `0x55`.
2. Confirmar que GPIO18 está alto sin teléfono y bajo con campo RF.
3. Leer desde la app y comprobar `20`.
4. Escribir `1000`, mantener el teléfono hasta que la app confirme y luego retirarlo.
5. Esperar el log de actualización NVS.
6. Reiniciar el ESP32-S3 y confirmar que sigue mostrando `1000`.
7. Leer nuevamente desde la app.

## Limitaciones conocidas

- La confirmación de la app indica que Android escribió el NDEF. La confirmación definitiva de persistencia ocurre al retirar el teléfono y observar el log del ESP32.
- No hay autenticación ni protección por contraseña; es un PoC abierto.
- No se implementa pass-through SRAM ni transferencia de mensajes mayores de 32 bytes.
- El hardware no puede validarse automáticamente durante la compilación.
- La configuración actual declara flash de 16 MB; debe coincidir con el módulo físico.

Referencia técnica: [datasheet oficial NT3H2111/NT3H2211, Rev. 3.6](https://www.nxp.com/docs/en/data-sheet/NT3H2111_2211.pdf).
