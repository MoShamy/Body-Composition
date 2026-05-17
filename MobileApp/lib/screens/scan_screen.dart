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
  final List<ScanResult> _results = [];
  StreamSubscription<List<ScanResult>>? _sub;
  bool _scanning = false;
  String? _error;

  @override
  void initState() {
    super.initState();
    _sub = FlutterBluePlus.scanResults.listen((rs) {
      setState(() {
        _results
          ..clear()
          ..addAll(rs);
      });
    });
  }

  @override
  void dispose() {
    _sub?.cancel();
    FlutterBluePlus.stopScan();
    super.dispose();
  }

  Future<bool> _ensurePermissions() async {
    // iOS only needs the Bluetooth usage prompt (handled automatically when
    // the BLE stack is first touched). No runtime permission_handler call needed.
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

  Future<void> _startScan() async {
    setState(() { _error = null; _results.clear(); });
    if (!await _ensurePermissions()) {
      setState(() => _error = 'Bluetooth permissions denied');
      return;
    }
    setState(() => _scanning = true);
    try {
      await FlutterBluePlus.startScan(
        timeout: const Duration(seconds: 8),
        withServices: [BleIds.service],
      );
      await FlutterBluePlus.isScanning.where((s) => s == false).first;
    } catch (e) {
      setState(() => _error = '$e');
    } finally {
      if (mounted) setState(() => _scanning = false);
    }
  }

  Future<void> _connect(BluetoothDevice d) async {
    showDialog(
      context: context,
      barrierDismissible: false,
      builder: (_) => const Center(child: CircularProgressIndicator()),
    );
    final svc = BodyCompService(d);
    try {
      await svc.connectAndDiscover();
      if (!mounted) return;
      Navigator.of(context).pop();
      Navigator.of(context).push(MaterialPageRoute(
        builder: (_) => ProfileScreen(service: svc),
      ));
    } catch (e) {
      if (!mounted) return;
      Navigator.of(context).pop();
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Connect failed: $e')),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    final filtered = _results
        .where((r) => r.device.platformName == BleIds.deviceName ||
                      r.advertisementData.advName == BleIds.deviceName)
        .toList();

    return Scaffold(
      appBar: AppBar(title: const Text('BodyComp Scanner')),
      body: Column(
        children: [
          if (_error != null)
            Padding(
              padding: const EdgeInsets.all(12),
              child: Text(_error!, style: const TextStyle(color: Colors.red)),
            ),
          Expanded(
            child: filtered.isEmpty
                ? Center(
                    child: Text(_scanning
                        ? 'Scanning…'
                        : 'No BodyComp device found.\nTap Scan to search.'),
                  )
                : ListView.builder(
                    itemCount: filtered.length,
                    itemBuilder: (_, i) {
                      final r = filtered[i];
                      return ListTile(
                        leading: const Icon(Icons.bluetooth),
                        title: Text(r.device.platformName.isEmpty
                            ? r.advertisementData.advName
                            : r.device.platformName),
                        subtitle: Text('${r.device.remoteId}   RSSI ${r.rssi}'),
                        trailing: ElevatedButton(
                          onPressed: () => _connect(r.device),
                          child: const Text('Connect'),
                        ),
                      );
                    },
                  ),
          ),
        ],
      ),
      floatingActionButton: FloatingActionButton.extended(
        onPressed: _scanning ? null : _startScan,
        icon: Icon(_scanning ? Icons.hourglass_top : Icons.search),
        label: Text(_scanning ? 'Scanning' : 'Scan'),
      ),
    );
  }
}
