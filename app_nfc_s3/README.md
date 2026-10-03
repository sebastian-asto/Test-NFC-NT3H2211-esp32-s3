# App NFC Contador

Aplicación Flutter para Android que lee y escribe el contador del PoC ESP32-S3 + NT3H2211 mediante NFC/NDEF.

## Funciones

- Comprueba si NFC está disponible y activado.
- Inicia una sesión ISO/IEC 14443 al pulsar **Leer NFC**.
- Lee un NDEF Text Record con formato `contador=<entero>`.
- Valida el tipo de registro, UTF-8, nombre del campo y rango numérico.
- Abre un modal para introducir un valor entre `0` y `2147483647`.
- Escribe el nuevo valor como NDEF Text Record.
- Mantiene activa la sesión NFC hasta retirar el teléfono, evitando que Android
  vuelva a abrir el visor del sistema «Nueva etiqueta recolectada».
- Muestra estados y errores sin simular resultados.

## Interfaz

La pantalla utiliza un estilo minimalista de alto contraste inspirado en la referencia proporcionada: fondo amarillo, tarjetas amplias, bordes negros, sombras sólidas, acentos rosa/verde/violeta y controles grandes para uso en campo.

## Android y permisos

El manifiesto incluye:

```xml
<uses-permission android:name="android.permission.NFC" />
<uses-feature android:name="android.hardware.nfc" android:required="false" />
```

La característica no es obligatoria para instalar la app, de modo que la interfaz puede mostrar un error claro en teléfonos sin NFC. La funcionalidad NFC usa `nfc_manager` 4.2.1 y `NdefAndroid`.

## Ejecución

```bash
flutter pub get
flutter run
```

Compilar APK debug:

```bash
flutter build apk --debug
```

APK generado:

```text
build/app/outputs/flutter-apk/app-debug.apk
```

## Pruebas

```bash
dart analyze
flutter test
```

Flujo de lectura:

1. Pulsar **Leer NFC**.
2. Acercar el teléfono al NT3H2211.
3. Mantenerlo estable durante la lectura y luego retirarlo de la antena.
4. El contador aparece cuando la app confirma que el teléfono fue retirado.

Flujo de escritura:

1. Pulsar **Definir valor inicial**.
2. Introducir el valor y pulsar **Escribir NFC**.
3. Acercar el teléfono y mantenerlo estable durante la escritura.
4. Retirar el teléfono para cerrar la sesión y generar el fin de campo en GPIO18.
5. Confirmar en el monitor serie que el ESP32 guardó el valor en NVS.

## Errores manejados

- NFC ausente o desactivado.
- Tiempo de espera sin detectar etiqueta.
- Etiqueta no compatible con NDEF.
- Etiqueta de solo lectura.
- Mensaje demasiado grande.
- Text Record UTF-16 no soportado.
- Falta del campo `contador`.
- Valor inválido o fuera del rango de 32 bits.

## Limitaciones

- La implementación NFC de esta etapa es Android. Los proyectos iOS generados por Flutter se conservan, pero Core NFC no está configurado ni validado.
- La app confirma la escritura en el NT3H2211; la persistencia en NVS se confirma desde el log del firmware después de retirar el teléfono.
- El warning de migración futura a Built-in Kotlin pertenece a la versión actual del plugin `nfc_manager`; no impide compilar el APK.

Documentación del plugin: [nfc_manager](https://pub.dev/packages/nfc_manager).
