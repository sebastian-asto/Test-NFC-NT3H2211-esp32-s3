import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import 'nfc_counter_service.dart';

enum _NfcAction { read, write }

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(const NfcCounterApp());
}

class NfcCounterApp extends StatelessWidget {
  const NfcCounterApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'NFC Contador',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(
          seedColor: const Color(0xFFFFD51F),
          brightness: Brightness.light,
        ),
        scaffoldBackgroundColor: const Color(0xFFFFD51F),
        useMaterial3: true,
      ),
      home: const CounterHomePage(),
    );
  }
}

class CounterHomePage extends StatefulWidget {
  const CounterHomePage({super.key, this.service});

  final NfcCounterService? service;

  @override
  State<CounterHomePage> createState() => _CounterHomePageState();
}

class _CounterHomePageState extends State<CounterHomePage> {
  late final NfcCounterService _nfc = widget.service ?? NfcCounterService();
  int? _counter;
  bool _busy = false;
  bool _readyToRemove = false;
  _NfcAction? _activeAction;
  String _status = 'Listo para acercar el teléfono al NT3H2211';

  void _signalNfcCompleted() {
    HapticFeedback.heavyImpact();
    SystemSound.play(SystemSoundType.alert);
  }

  void _onReadCompleted(int counter) {
    if (!mounted) return;
    setState(() {
      _counter = counter;
      _readyToRemove = true;
      _status = 'Dato leído: contador=$counter. Retire el teléfono.';
    });
    _signalNfcCompleted();
  }

  void _onWriteCompleted(int counter) {
    if (!mounted) return;
    setState(() {
      _counter = counter;
      _readyToRemove = true;
      _status = 'NDEF escrito: contador=$counter. Retire el teléfono.';
    });
    _signalNfcCompleted();
  }

  Future<void> _readCounter() async {
    setState(() {
      _busy = true;
      _readyToRemove = false;
      _activeAction = _NfcAction.read;
      _status =
          'Acerque el teléfono, espere la lectura y luego retírelo de la antena…';
    });
    try {
      final counter = await _nfc.readCounter(onRead: _onReadCompleted);
      if (!mounted) return;
      setState(() {
        _counter = counter;
        _readyToRemove = false;
        _status = 'Lectura NFC finalizada correctamente';
      });
    } catch (error) {
      if (!mounted) return;
      setState(() {
        _readyToRemove = false;
        _status = error.toString();
      });
    } finally {
      if (mounted) {
        setState(() {
          _busy = false;
          _readyToRemove = false;
          _activeAction = null;
        });
      }
    }
  }

  Future<void> _requestNewCounter() async {
    final value = await showDialog<int>(
      context: context,
      builder: (context) => const _CounterDialog(),
    );
    if (value == null || !mounted) return;

    setState(() {
      _busy = true;
      _readyToRemove = false;
      _activeAction = _NfcAction.write;
      _status =
          'Acerque el teléfono para escribir contador=$value y retírelo al terminar…';
    });
    try {
      await _nfc.writeCounter(value, onWritten: _onWriteCompleted);
      if (!mounted) return;
      setState(() {
        _counter = value;
        _readyToRemove = false;
        _status = 'Valor enviado. El ESP32 está procesando el cambio.';
      });
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Valor enviado correctamente por NFC')),
      );
    } catch (error) {
      if (!mounted) return;
      setState(() {
        _readyToRemove = false;
        _status = error.toString();
      });
    } finally {
      if (mounted) {
        setState(() {
          _busy = false;
          _readyToRemove = false;
          _activeAction = null;
        });
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: SafeArea(
        child: Stack(
          children: [
            const Positioned(
              left: -62,
              top: 82,
              child: _DecorativeCircle(color: Color(0xFFFF4785), size: 180),
            ),
            Positioned(
              right: -100,
              bottom: -70,
              child: Transform.rotate(
                angle: -0.35,
                child: const SizedBox(
                  width: 260,
                  height: 320,
                  child: ColoredBox(color: Color(0xFF7B5CF5)),
                ),
              ),
            ),
            Center(
              child: SingleChildScrollView(
                padding: const EdgeInsets.symmetric(
                  horizontal: 22,
                  vertical: 26,
                ),
                child: ConstrainedBox(
                  constraints: const BoxConstraints(maxWidth: 440),
                  child: _PhonePanel(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.stretch,
                      children: [
                        Row(
                          children: [
                            const _SquareIcon(icon: Icons.nfc_rounded),
                            const Spacer(),
                            Container(
                              padding: const EdgeInsets.symmetric(
                                horizontal: 10,
                                vertical: 6,
                              ),
                              decoration: BoxDecoration(
                                color: const Color(0xFFB7FF2A),
                                border: Border.all(
                                  color: Colors.black,
                                  width: 2,
                                ),
                                borderRadius: BorderRadius.circular(12),
                              ),
                              child: const Text(
                                'ESP32-S3',
                                style: TextStyle(fontWeight: FontWeight.w900),
                              ),
                            ),
                          ],
                        ),
                        const SizedBox(height: 30),
                        const Text(
                          'NFC\nContador.',
                          style: TextStyle(
                            height: 0.9,
                            fontSize: 46,
                            fontWeight: FontWeight.w900,
                            letterSpacing: -2,
                          ),
                        ),
                        const SizedBox(height: 12),
                        const Text(
                          'Lee y configura el valor persistente del equipo sin cables.',
                          style: TextStyle(
                            fontSize: 16,
                            height: 1.35,
                            fontWeight: FontWeight.w600,
                          ),
                        ),
                        const SizedBox(height: 28),
                        _PrimaryButton(
                          label: _readyToRemove
                              ? 'Retire el teléfono'
                              : _busy
                              ? 'Esperando NFC…'
                              : 'Leer NFC',
                          icon: _readyToRemove
                              ? Icons.check_circle_rounded
                              : Icons.contactless_rounded,
                          color: _readyToRemove
                              ? const Color(0xFFB7FF2A)
                              : const Color(0xFFFFD51F),
                          onPressed: _busy ? null : _readCounter,
                        ),
                        AnimatedSwitcher(
                          duration: const Duration(milliseconds: 250),
                          child: _readyToRemove
                              ? Padding(
                                  key: const ValueKey('nfc-success-banner'),
                                  padding: const EdgeInsets.only(top: 16),
                                  child: _NfcSuccessBanner(
                                    action: _activeAction!,
                                  ),
                                )
                              : const SizedBox.shrink(),
                        ),
                        const SizedBox(height: 24),
                        const Text(
                          'DATOS LEÍDOS DEL DISPOSITIVO',
                          style: TextStyle(
                            fontSize: 12,
                            fontWeight: FontWeight.w900,
                            letterSpacing: 0.7,
                          ),
                        ),
                        const SizedBox(height: 10),
                        _CounterCard(
                          counter: _counter,
                          status: _status,
                          busy: _busy,
                          readyToRemove: _readyToRemove,
                        ),
                        const SizedBox(height: 22),
                        _PrimaryButton(
                          label: 'Definir valor inicial',
                          icon: Icons.edit_rounded,
                          color: const Color(0xFFFF4785),
                          onPressed: _busy ? null : _requestNewCounter,
                        ),
                        const SizedBox(height: 14),
                        const Text(
                          'NT3H2211 · I²C 0x55 · NDEF',
                          textAlign: TextAlign.center,
                          style: TextStyle(
                            fontSize: 12,
                            fontWeight: FontWeight.w700,
                          ),
                        ),
                      ],
                    ),
                  ),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _PhonePanel extends StatelessWidget {
  const _PhonePanel({required this.child});
  final Widget child;

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        color: Colors.black,
        borderRadius: BorderRadius.circular(31),
      ),
      padding: const EdgeInsets.only(right: 9, bottom: 11),
      child: Container(
        padding: const EdgeInsets.fromLTRB(22, 22, 22, 25),
        decoration: BoxDecoration(
          color: const Color(0xFFFFFBF4),
          border: Border.all(color: Colors.black, width: 3),
          borderRadius: BorderRadius.circular(28),
        ),
        child: child,
      ),
    );
  }
}

class _CounterCard extends StatelessWidget {
  const _CounterCard({
    required this.counter,
    required this.status,
    required this.busy,
    required this.readyToRemove,
  });
  final int? counter;
  final String status;
  final bool busy;
  final bool readyToRemove;

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        color: Colors.black,
        borderRadius: BorderRadius.circular(18),
      ),
      padding: const EdgeInsets.only(right: 6, bottom: 7),
      child: Container(
        padding: const EdgeInsets.all(18),
        decoration: BoxDecoration(
          color: readyToRemove
              ? const Color(0xFFDFFFAD)
              : const Color(0xFFE8E1FF),
          border: Border.all(color: Colors.black, width: 2.5),
          borderRadius: BorderRadius.circular(16),
        ),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                const Icon(Icons.memory_rounded),
                const SizedBox(width: 8),
                const Text(
                  'Contador actual',
                  style: TextStyle(fontWeight: FontWeight.w800),
                ),
                if (readyToRemove) ...[
                  const Spacer(),
                  const Icon(Icons.check_circle_rounded, size: 26),
                ] else if (busy) ...[
                  const Spacer(),
                  const SizedBox.square(
                    dimension: 18,
                    child: CircularProgressIndicator(
                      strokeWidth: 3,
                      color: Colors.black,
                    ),
                  ),
                ],
              ],
            ),
            const SizedBox(height: 10),
            Text(
              counter?.toString() ?? '—',
              style: const TextStyle(
                fontSize: 50,
                height: 1,
                fontWeight: FontWeight.w900,
              ),
            ),
            const SizedBox(height: 12),
            Text(
              status,
              style: TextStyle(
                fontSize: readyToRemove ? 15 : 13,
                height: 1.35,
                fontWeight: readyToRemove ? FontWeight.w900 : FontWeight.normal,
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _NfcSuccessBanner extends StatelessWidget {
  const _NfcSuccessBanner({required this.action});

  final _NfcAction action;

  @override
  Widget build(BuildContext context) {
    final title = action == _NfcAction.read
        ? 'LECTURA COMPLETADA'
        : 'ESCRITURA COMPLETADA';

    return Semantics(
      liveRegion: true,
      label: '$title. Retire el teléfono de la antena NFC.',
      child: Container(
        width: double.infinity,
        padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 14),
        decoration: BoxDecoration(
          color: const Color(0xFFB7FF2A),
          border: Border.all(color: Colors.black, width: 2.5),
          borderRadius: BorderRadius.circular(14),
          boxShadow: const [
            BoxShadow(color: Colors.black, offset: Offset(5, 5)),
          ],
        ),
        child: Column(
          children: [
            const Icon(Icons.check_circle_rounded, size: 34),
            const SizedBox(height: 4),
            Text(
              title,
              textAlign: TextAlign.center,
              style: const TextStyle(fontSize: 17, fontWeight: FontWeight.w900),
            ),
            const SizedBox(height: 2),
            const Text(
              'RETIRE EL TELÉFONO DE LA ANTENA NFC',
              textAlign: TextAlign.center,
              style: TextStyle(fontSize: 14, fontWeight: FontWeight.w900),
            ),
          ],
        ),
      ),
    );
  }
}

class _PrimaryButton extends StatelessWidget {
  const _PrimaryButton({
    required this.label,
    required this.icon,
    required this.color,
    required this.onPressed,
  });
  final String label;
  final IconData icon;
  final Color color;
  final VoidCallback? onPressed;

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        color: Colors.black,
        borderRadius: BorderRadius.circular(14),
      ),
      padding: const EdgeInsets.only(right: 5, bottom: 6),
      child: FilledButton.icon(
        onPressed: onPressed,
        icon: Icon(icon),
        label: Padding(
          padding: const EdgeInsets.symmetric(vertical: 15),
          child: Text(
            label,
            style: const TextStyle(fontSize: 16, fontWeight: FontWeight.w900),
          ),
        ),
        style: FilledButton.styleFrom(
          foregroundColor: Colors.black,
          backgroundColor: color,
          disabledBackgroundColor: color,
          disabledForegroundColor: Colors.black,
          shape: RoundedRectangleBorder(
            side: const BorderSide(color: Colors.black, width: 2.5),
            borderRadius: BorderRadius.circular(12),
          ),
        ),
      ),
    );
  }
}

class _SquareIcon extends StatelessWidget {
  const _SquareIcon({required this.icon});
  final IconData icon;

  @override
  Widget build(BuildContext context) {
    return Container(
      width: 48,
      height: 48,
      decoration: BoxDecoration(
        color: Colors.white,
        border: Border.all(color: Colors.black, width: 2.5),
        borderRadius: BorderRadius.circular(12),
        boxShadow: const [BoxShadow(color: Colors.black, offset: Offset(3, 3))],
      ),
      child: Icon(icon, size: 28),
    );
  }
}

class _DecorativeCircle extends StatelessWidget {
  const _DecorativeCircle({required this.color, required this.size});
  final Color color;
  final double size;

  @override
  Widget build(BuildContext context) {
    return Container(
      width: size,
      height: size,
      decoration: BoxDecoration(color: color, shape: BoxShape.circle),
    );
  }
}

class _CounterDialog extends StatefulWidget {
  const _CounterDialog();

  @override
  State<_CounterDialog> createState() => _CounterDialogState();
}

class _CounterDialogState extends State<_CounterDialog> {
  final _controller = TextEditingController();
  String? _error;

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  void _submit() {
    final value = int.tryParse(_controller.text);
    if (value == null || value < 0 || value > 2147483647) {
      setState(() => _error = 'Ingrese un entero entre 0 y 2147483647');
      return;
    }
    Navigator.of(context).pop(value);
  }

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      backgroundColor: const Color(0xFFFFFBF4),
      shape: RoundedRectangleBorder(
        side: const BorderSide(color: Colors.black, width: 3),
        borderRadius: BorderRadius.circular(20),
      ),
      title: const Text(
        'Definir valor inicial del contador',
        style: TextStyle(fontWeight: FontWeight.w900),
      ),
      content: TextField(
        controller: _controller,
        autofocus: true,
        keyboardType: TextInputType.number,
        inputFormatters: [FilteringTextInputFormatter.digitsOnly],
        decoration: InputDecoration(
          labelText: 'valor_contador',
          errorText: _error,
          filled: true,
          fillColor: Colors.white,
          border: OutlineInputBorder(borderRadius: BorderRadius.circular(12)),
          focusedBorder: OutlineInputBorder(
            borderRadius: BorderRadius.circular(12),
            borderSide: const BorderSide(color: Colors.black, width: 2.5),
          ),
        ),
        onSubmitted: (_) => _submit(),
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.of(context).pop(),
          child: const Text('Cancelar'),
        ),
        FilledButton.icon(
          onPressed: _submit,
          style: FilledButton.styleFrom(
            backgroundColor: const Color(0xFFFFD51F),
            foregroundColor: Colors.black,
            side: const BorderSide(color: Colors.black, width: 2),
          ),
          icon: const Icon(Icons.nfc_rounded),
          label: const Text(
            'Escribir NFC',
            style: TextStyle(fontWeight: FontWeight.w800),
          ),
        ),
      ],
    );
  }
}
