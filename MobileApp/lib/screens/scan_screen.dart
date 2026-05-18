import 'dart:async';
import 'dart:io' show Platform;

import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:permission_handler/permission_handler.dart';

import '../ble/ble_constants.dart';
import '../ble/body_comp_service.dart';
import 'profile_screen.dart';

class ScanScreen extends StatefulWidget {
  const ScanScreen({super.key});
  @override
  State<ScanScreen> createState() => _ScanScreenState();
}

class _ScanScreenState extends State<ScanScreen> {
  StreamSubscription<List<ScanResult>>? _sub;
  bool _scanning = false;
  bool _connecting = false;
  bool _stopped = false;
  String _statusMsg = 'Starting…';
  String? _error;

  @override
  void initState() {
    super.initState();
    _sub = FlutterBluePlus.scanResults.listen((rs) {
      if (_connecting || _stopped) return;
      for (final r in rs) {
        final name = r.device.platformName.isEmpty
            ? r.advertisementData.advName
            : r.device.platformName;
        if (name == BleIds.deviceName) {
          _connecting = true;
          FlutterBluePlus.stopScan();
          _connect(r.device);
          break;
        }
      }
    });
    WidgetsBinding.instance.addPostFrameCallback((_) => _autoConnectLoop());
  }

  @override
  void dispose() {
    _stopped = true;
    _sub?.cancel();
    FlutterBluePlus.stopScan();
    super.dispose();
  }

  Future<bool> _ensurePermissions() async {
    if (Platform.isIOS) {
      final s = await Permission.bluetooth.request();
      return s.isGranted || s.isLimited || s.isRestricted == false;
    }
    final statuses = await [
      Permission.bluetoothScan,
      Permission.bluetoothConnect,
      Permission.locationWhenInUse,
    ].request();
    return statuses.values.every((s) => s.isGranted || s.isLimited);
  }

  Future<void> _autoConnectLoop() async {
    if (!await _ensurePermissions()) {
      setState(() => _error = 'Bluetooth permissions denied');
      return;
    }
    while (!_stopped && !_connecting) {
      setState(() {
        _scanning = true;
        _statusMsg = 'Scanning for BodyComp…';
        _error = null;
      });
      try {
        await FlutterBluePlus.startScan(
          timeout: const Duration(seconds: 6),
          withServices: [BleIds.service],
        );
        await FlutterBluePlus.isScanning.where((s) => s == false).first;
      } catch (e) {
        if (!mounted) return;
        setState(() => _error = '$e');
      }
      if (_connecting || _stopped) break;
      if (!mounted) return;
      setState(() {
        _scanning = false;
        _statusMsg = 'Device not found, retrying…';
      });
      await Future.delayed(const Duration(seconds: 1));
    }
    if (mounted && !_connecting) {
      setState(() => _scanning = false);
    }
  }

  Future<void> _connect(BluetoothDevice d) async {
    setState(() {
      _scanning = false;
      _statusMsg = 'Connecting to BodyComp…';
    });
    final svc = BodyCompService(d);
    try {
      await svc.connectAndDiscover();
      if (!mounted) return;
      _stopped = true;
      Navigator.of(context).pushReplacement(MaterialPageRoute(
        builder: (_) => ProfileScreen(service: svc),
      ));
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _error = 'Connect failed: $e';
        _connecting = false;
      });
      _autoConnectLoop();
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('BodyComp Scanner')),
      body: Center(
        child: Padding(
          padding: const EdgeInsets.all(24),
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              const Icon(Icons.bluetooth_searching, size: 72),
              const SizedBox(height: 24),
              if (_scanning || _connecting)
                const CircularProgressIndicator()
              else
                const SizedBox(height: 4),
              const SizedBox(height: 24),
              Text(
                _statusMsg,
                textAlign: TextAlign.center,
                style: const TextStyle(fontSize: 16),
              ),
              if (_error != null) ...[
                const SizedBox(height: 16),
                Text(_error!,
                    textAlign: TextAlign.center,
                    style: const TextStyle(color: Colors.red)),
              ],
            ],
          ),
        ),
      ),
    );
  }
}
