import 'package:app_nfc_s3/main.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('muestra la pantalla principal del contador', (tester) async {
    await tester.pumpWidget(const NfcCounterApp());

    expect(find.text('NFC\nContador.'), findsOneWidget);
    expect(find.text('Leer NFC'), findsOneWidget);
    expect(find.text('Definir valor inicial'), findsOneWidget);
    expect(find.text('Contador actual'), findsOneWidget);
  });
}
