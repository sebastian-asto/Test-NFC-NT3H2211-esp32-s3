# PoC NFC bidireccional con ESP32-S3 y NT3H2211

Prueba de concepto para leer y modificar desde un teléfono Android un contador persistente almacenado en la NVS de un ESP32-S3. El NT3H2211 actúa como etiqueta NFC Forum Type 2 y como memoria compartida accesible por I2C.

```text
Smartphone Android
        ↕
   NFC 13.56 MHz
        ↕
     NT3H2211
        ↕ I2C (0x55)
      ESP32-S3
        ↕
        NVS
```

## Proyectos

- `NFC_S3_NVS/`: firmware ESP-IDF 5.5.4.
- `app_nfc_s3/`: aplicación Flutter para Android.

## Protocolo del PoC

Se utiliza un NDEF Text Record UTF-8 con un único valor:

```text
contador=20
```

Lectura:

```text
NVS -> ESP32-S3 -> EEPROM NDEF del NT3H2211 -> NFC -> aplicación
```

Escritura:

```text
aplicación -> NFC -> EEPROM NDEF del NT3H2211
            -> fin del campo RF / GPIO18
            -> ESP32-S3 -> validación -> NVS
```

El firmware solo escribe NVS cuando el valor recibido es distinto del almacenado. Después de una escritura desde la aplicación es necesario retirar el teléfono de la antena: el flanco de fin de campo permite al ESP32 tomar el bus de memoria y persistir el dato sin competir con RF.

## Estado de validación

- Firmware compilado con ESP-IDF 5.5.4 para ESP32-S3.
- Aplicación analizada sin errores, prueba de widget aprobada y APK debug generado.
- Dirección `0x55` confirmada en el log suministrado.
- Las pruebas eléctricas de GPIO18, lectura/escritura NFC y persistencia tras reset requieren ejecutarse con el hardware físico.

Consulta las instrucciones detalladas en [README del firmware](NFC_S3_NVS/README.md) y [README de la app](app_nfc_s3/README.md).
