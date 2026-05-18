import 'dart:async';

import 'package:flutter/material.dart';

import '../ble/ble_constants.dart';
import '../ble/body_comp_service.dart';
import 'results_screen.dart';

class ProfileScreen extends StatefulWidget {
  final BodyCompService service;
  const ProfileScreen({super.key, required this.service});

  @override
  State<ProfileScreen> createState() => _ProfileScreenState();
}

class _ProfileScreenState extends State<ProfileScreen> {
  final _ageCtrl = TextEditingController(text: '25');
  Sex _sex = Sex.male;
  StatusUpdate? _last;
  StreamSubscription<StatusUpdate>? _statusSub;
  StreamSubscription<MeasurementResult>? _resultSub;
  bool _busy = false;

  @override
  void initState() {
    super.initState();
    _statusSub = widget.service.statusStream.listen((s) {
      setState(() => _last = s);
    });
    _resultSub = widget.service.resultStream.listen((res) {
      if (!mounted) return;
      Navigator.of(context).pushReplacement(MaterialPageRoute(
        builder: (_) => ResultsScreen(result: res, service: widget.service),
      ));
    });
  }

  @override
  void dispose() {
    _ageCtrl.dispose();
    _statusSub?.cancel();
    _resultSub?.cancel();
    super.dispose();
  }

  Future<void> _go() async {
    final age = int.tryParse(_ageCtrl.text.trim());
    if (age == null || age <= 0 || age > 120) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Enter a valid age (1-120)')),
      );
      return;
    }
    setState(() => _busy = true);
    try {
      await widget.service.writeProfile(age: age, sex: _sex);
      await widget.service.startMeasurement();
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Error: $e')),
      );
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  String _statusLabel(MeasurementStatus s) => switch (s) {
        MeasurementStatus.idle => 'Idle',
        MeasurementStatus.ready => 'Ready',
        MeasurementStatus.measuring => 'Measuring…',
        MeasurementStatus.done => 'Done',
        MeasurementStatus.error => 'Error',
        MeasurementStatus.unknown => 'Unknown',
      };

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Your Profile')),
      body: Padding(
        padding: const EdgeInsets.all(20),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            TextField(
              controller: _ageCtrl,
              keyboardType: TextInputType.number,
              decoration: const InputDecoration(
                labelText: 'Age',
                border: OutlineInputBorder(),
              ),
            ),
            const SizedBox(height: 16),
            const Text('Sex'),
            SegmentedButton<Sex>(
              segments: const [
                ButtonSegment(value: Sex.male, label: Text('Male')),
                ButtonSegment(value: Sex.female, label: Text('Female')),
              ],
              selected: {_sex},
              onSelectionChanged: (s) => setState(() => _sex = s.first),
            ),
            const SizedBox(height: 24),
            ElevatedButton.icon(
              onPressed: _busy ? null : _go,
              icon: const Icon(Icons.play_arrow),
              label: const Text('Start Measurement'),
            ),
            const SizedBox(height: 24),
            if (_last != null) ...[
              Text('Status: ${_statusLabel(_last!.status)}',
                  style: Theme.of(context).textTheme.titleMedium),
              const SizedBox(height: 8),
              LinearProgressIndicator(
                value: _last!.progressPercent / 100.0,
              ),
              if (_last!.errorCode != 0)
                Padding(
                  padding: const EdgeInsets.only(top: 8),
                  child: Text('Error code: ${_last!.errorCode}',
                      style: const TextStyle(color: Colors.red)),
                ),
            ],
            const Spacer(),
            const Text(
              'Stand on the scale and grip the electrodes once measurement starts.',
              textAlign: TextAlign.center,
              style: TextStyle(fontStyle: FontStyle.italic),
            ),
          ],
        ),
      ),
    );
  }
}
