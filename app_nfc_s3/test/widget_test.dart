import 'dart:async';

import 'package:app_nfc_s3/main.dart';
import 'package:app_nfc_s3/nfc_counter_service.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

class _FakeNfcCounterService extends NfcCounterService {
  final readRemoval = Completer<void>();
  final writeRemoval = Completer<void>();

  @override
  Future<int> readCounter({void Function(int counter)? onRead}) async {
    onRead?.call(37);
    await readRemoval.future;
    return 37;
  }

  @override
  Future<void> writeCounter(
    int counter, {
    void Function(int counter)? onWritten,
  }) async {
    onWritten?.call(counter);
    await writeRemoval.future;
  }
}

void main() {
  testWidgets('muestra la pantalla principal del contador', (tester) async {
    await tester.pumpWidget(const NfcCounterApp());

    expect(find.text('NFC\nContador.'), findsOneWidget);
    expect(find.text('Leer NFC'), findsOneWidget);
    expect(find.text('Definir valor inicial'), findsOneWidget);
    expect(find.text('Contador actual'), findsOneWidget);
  });

  testWidgets('avisa inmediatamente cuando la lectura terminó', (tester) async {
    final service = _FakeNfcCounterService();
    await tester.pumpWidget(
      MaterialApp(home: CounterHomePage(service: service)),
    );

    await tester.tap(find.text('Leer NFC'));
    await tester.pump();

    expect(find.text('LECTURA COMPLETADA'), findsOneWidget);
    expect(find.text('RETIRE EL TELÉFONO DE LA ANTENA NFC'), findsOneWidget);
    expect(find.text('37'), findsOneWidget);

    service.readRemoval.complete();
    await tester.pumpAndSettle();
    expect(find.text('Lectura NFC finalizada correctamente'), findsOneWidget);
  });

  testWidgets('avisa inmediatamente cuando la escritura terminó', (
    tester,
  ) async {
    final service = _FakeNfcCounterService();
    await tester.pumpWidget(
      MaterialApp(home: CounterHomePage(service: service)),
    );

    await tester.ensureVisible(find.text('Definir valor inicial'));
    await tester.pumpAndSettle();
    await tester.tap(find.text('Definir valor inicial'));
    await tester.pumpAndSettle();
    await tester.enterText(find.byType(TextField), '100');
    await tester.tap(find.text('Escribir NFC'));
    await tester.pump(const Duration(milliseconds: 500));

    expect(find.text('ESCRITURA COMPLETADA'), findsOneWidget);
    expect(find.text('RETIRE EL TELÉFONO DE LA ANTENA NFC'), findsOneWidget);
    expect(
      find.text('NDEF escrito: contador=100. Retire el teléfono.'),
      findsOneWidget,
    );

    service.writeRemoval.complete();
    await tester.pumpAndSettle();
    expect(
      find.text('Valor enviado. El ESP32 está procesando el cambio.'),
      findsOneWidget,
    );
  });
}
