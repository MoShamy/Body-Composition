import 'package:flutter/material.dart';

import 'screens/scan_screen.dart';

void main() => runApp(const BodyCompApp());

class BodyCompApp extends StatelessWidget {
  const BodyCompApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Body Composition Analyzer',
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.teal),
        useMaterial3: true,
      ),
      home: const ScanScreen(),
    );
  }
}
