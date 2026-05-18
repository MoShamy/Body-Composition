import 'package:flutter/material.dart';

import '../ble/body_comp_service.dart';
import 'profile_screen.dart';

class ResultsScreen extends StatelessWidget {
  final MeasurementResult result;
  final BodyCompService service;
  const ResultsScreen({super.key, required this.result, required this.service});

  Widget _metric(String label, String value, IconData icon) {
    return Card(
      child: ListTile(
        leading: Icon(icon, size: 32),
        title: Text(label),
        trailing: Text(value,
            style: const TextStyle(fontSize: 18, fontWeight: FontWeight.bold)),
      ),
    );
  }

  double get _bmi {
    final h = result.heightCm / 100.0;
    if (h <= 0) return 0;
    return result.weightKg / (h * h);
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Results')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          _metric('Weight', '${result.weightKg.toStringAsFixed(2)} kg',
              Icons.monitor_weight),
          _metric('Height', '${result.heightCm.toStringAsFixed(1)} cm',
              Icons.height),
          _metric('BMI', _bmi.toStringAsFixed(1), Icons.analytics),
          _metric('Impedance', '${result.zOhms.toStringAsFixed(1)} Ω',
              Icons.electric_bolt),
          _metric('Body Fat', '${result.bodyFatPct.toStringAsFixed(1)} %',
              Icons.opacity),
          _metric('Fat-Free Mass', '${result.ffmKg.toStringAsFixed(2)} kg',
              Icons.fitness_center),
          const SizedBox(height: 24),
          OutlinedButton.icon(
            onPressed: () {
              Navigator.of(context).pushReplacement(MaterialPageRoute(
                builder: (_) => ProfileScreen(service: service),
              ));
            },
            icon: const Icon(Icons.refresh),
            label: const Text('New Measurement'),
          ),
        ],
      ),
    );
  }
}
