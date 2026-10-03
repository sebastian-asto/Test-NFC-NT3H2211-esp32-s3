import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';

import 'package:nfc_manager/ndef_record.dart';
import 'package:nfc_manager/nfc_manager.dart';
import 'package:nfc_manager/nfc_manager_android.dart';

class NfcCounterException implements Exception {
  const NfcCounterException(this.message);

  final String message;

  @override
  String toString() => message;
}

class NfcCounterService {
  static const Duration _sessionTimeout = Duration(seconds: 20);

  Future<void> _ensureAvailable() async {
    final availability = await NfcManager.instance.checkAvailability();
    if (availability != NfcAvailability.enabled) {
      throw const NfcCounterException(
        'NFC no está disponible o está desactivado en este teléfono.',
      );
    }
  }

  Future<int> readCounter() async {
    await _ensureAvailable();
    final result = Completer<int>();

    await NfcManager.instance.startSession(
      pollingOptions: const {NfcPollingOption.iso14443},
      noPlatformSoundsAndroid: false,
      onDiscovered: (tag) async {
        NdefAndroid? ndef;
        int? value;
        Object? operationError;
        StackTrace? operationStackTrace;
        try {
          ndef = NdefAndroid.from(tag);
          if (ndef == null) {
            throw const NfcCounterException(
              'La etiqueta detectada no contiene datos NDEF.',
            );
          }
          final message = await ndef.getNdefMessage();
          if (message == null) {
            throw const NfcCounterException('No se encontró un mensaje NDEF.');
          }
          value = _counterFromMessage(message);
        } catch (error, stackTrace) {
          operationError = error;
          operationStackTrace = stackTrace;
        } finally {
          if (ndef != null) await _waitForTagRemoval(ndef);
          await _stopSessionQuietly();
        }

        if (!result.isCompleted) {
          if (operationError != null) {
            result.completeError(operationError, operationStackTrace!);
          } else {
            result.complete(value!);
          }
        }
      },
    );

    try {
      return await result.future.timeout(_sessionTimeout);
    } on TimeoutException {
      await _stopSessionQuietly();
      throw const NfcCounterException(
        'Tiempo agotado. Acerque el teléfono y retírelo después de la lectura.',
      );
    }
  }

  Future<void> writeCounter(int counter) async {
    await _ensureAvailable();
    if (counter < -2147483648 || counter > 2147483647) {
      throw const NfcCounterException(
        'El valor está fuera del rango de 32 bits.',
      );
    }

    final result = Completer<void>();
    final message = _messageForCounter(counter);

    await NfcManager.instance.startSession(
      pollingOptions: const {NfcPollingOption.iso14443},
      noPlatformSoundsAndroid: false,
      onDiscovered: (tag) async {
        NdefAndroid? ndef;
        Object? operationError;
        StackTrace? operationStackTrace;
        try {
          ndef = NdefAndroid.from(tag);
          if (ndef == null) {
            throw const NfcCounterException(
              'La etiqueta detectada no es compatible con NDEF.',
            );
          }
          if (!ndef.isWritable) {
            throw const NfcCounterException(
              'La etiqueta NFC es de solo lectura.',
            );
          }
          if (message.byteLength > ndef.maxSize) {
            throw const NfcCounterException(
              'El mensaje excede la capacidad NFC.',
            );
          }
          await ndef.writeNdefMessage(message);
        } catch (error, stackTrace) {
          operationError = error;
          operationStackTrace = stackTrace;
        } finally {
          if (ndef != null) await _waitForTagRemoval(ndef);
          await _stopSessionQuietly();
        }

        if (!result.isCompleted) {
          if (operationError != null) {
            result.completeError(operationError, operationStackTrace!);
          } else {
            result.complete();
          }
        }
      },
    );

    try {
      await result.future.timeout(_sessionTimeout);
    } on TimeoutException {
      await _stopSessionQuietly();
      throw const NfcCounterException(
        'Tiempo agotado. Acerque el teléfono y retírelo después de la escritura.',
      );
    }
  }

  Future<void> _waitForTagRemoval(NdefAndroid ndef) async {
    while (true) {
      await Future<void>.delayed(const Duration(milliseconds: 200));
      try {
        await ndef.getNdefMessage();
      } catch (_) {
        return;
      }
    }
  }

  Future<void> _stopSessionQuietly() async {
    try {
      await NfcManager.instance.stopSession();
    } catch (_) {
      // La sesión también puede haber sido cerrada por el timeout.
    }
  }

  int _counterFromMessage(NdefMessage message) {
    for (final record in message.records) {
      final isText =
          record.typeNameFormat == TypeNameFormat.wellKnown &&
          record.type.length == 1 &&
          record.type.first == 0x54;
      if (!isText || record.payload.isEmpty) continue;

      final status = record.payload.first;
      if ((status & 0x80) != 0) {
        throw const NfcCounterException('El texto UTF-16 no está soportado.');
      }
      final languageLength = status & 0x3F;
      if (1 + languageLength >= record.payload.length) continue;
      final text = utf8.decode(record.payload.sublist(1 + languageLength));
      final match = RegExp(r'^contador=(-?\d+)$').firstMatch(text.trim());
      if (match == null) continue;
      final value = int.tryParse(match.group(1)!);
      if (value == null || value < -2147483648 || value > 2147483647) {
        throw const NfcCounterException(
          'El contador NFC no es un entero válido.',
        );
      }
      return value;
    }
    throw const NfcCounterException(
      'No se encontró el campo contador en el mensaje NFC.',
    );
  }

  NdefMessage _messageForCounter(int counter) {
    final text = utf8.encode('contador=$counter');
    final payload = Uint8List.fromList([0x02, 0x65, 0x73, ...text]);
    return NdefMessage(
      records: [
        NdefRecord(
          typeNameFormat: TypeNameFormat.wellKnown,
          type: Uint8List.fromList([0x54]),
          identifier: Uint8List(0),
          payload: payload,
        ),
      ],
    );
  }
}
