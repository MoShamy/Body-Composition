import 'dart:async';
import 'dart:typed_data';

import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import 'ble_constants.dart';

class MeasurementResult {
  final double weightKg;
  final double heightCm;
  final double zOhms;
  final double bodyFatPct;
  final double ffmKg;

  MeasurementResult({
    required this.weightKg,
    required this.heightCm,
    required this.zOhms,
    required this.bodyFatPct,
    required this.ffmKg,
  });

  factory MeasurementResult.fromBytes(List<int> bytes) {
    final bd = ByteData.sublistView(Uint8List.fromList(bytes));
    return MeasurementResult(
      weightKg:   bd.getFloat32(0,  Endian.little),
      heightCm:   bd.getFloat32(4,  Endian.little),
      zOhms:      bd.getFloat32(8,  Endian.little),
      bodyFatPct: bd.getFloat32(12, Endian.little),
      ffmKg:      bd.getFloat32(16, Endian.little),
    );
  }
}

class StatusUpdate {
  final MeasurementStatus status;
  final int errorCode;
  final int progressPercent;
  StatusUpdate(this.status, this.errorCode, this.progressPercent);

  factory StatusUpdate.fromBytes(List<int> b) {
    final st = b.isNotEmpty ? statusFromByte(b[0]) : MeasurementStatus.unknown;
    final err = b.length > 1 ? b[1] : 0;
    final prog = b.length > 2 ? b[2] : 0;
    return StatusUpdate(st, err, prog);
  }
}

/// High-level wrapper around the BodyComp GATT service.
class BodyCompService {
  final BluetoothDevice device;
  BluetoothCharacteristic? _userProfile;
  BluetoothCharacteristic? _start;
  BluetoothCharacteristic? _status;
  BluetoothCharacteristic? _result;

  final _statusCtrl = StreamController<StatusUpdate>.broadcast();
  final _resultCtrl = StreamController<MeasurementResult>.broadcast();

  Stream<StatusUpdate> get statusStream => _statusCtrl.stream;
  Stream<MeasurementResult> get resultStream => _resultCtrl.stream;

  BodyCompService(this.device);

  Future<void> connectAndDiscover() async {
    await device.connect(autoConnect: false, timeout: const Duration(seconds: 15));
    final services = await device.discoverServices();
    final svc = services.firstWhere((s) => s.uuid == BleIds.service,
        orElse: () => throw Exception('BodyComp service not found on device'));

    for (final c in svc.characteristics) {
      if (c.uuid == BleIds.userProfile)  _userProfile = c;
      else if (c.uuid == BleIds.measureStart) _start = c;
      else if (c.uuid == BleIds.status)  _status = c;
      else if (c.uuid == BleIds.result)  _result = c;
    }

    if (_status != null) {
      await _status!.setNotifyValue(true);
      _status!.lastValueStream.listen((v) {
        if (v.isNotEmpty) _statusCtrl.add(StatusUpdate.fromBytes(v));
      });
    }
    if (_result != null) {
      await _result!.setNotifyValue(true);
      _result!.lastValueStream.listen((v) {
        if (v.length >= 20) _resultCtrl.add(MeasurementResult.fromBytes(v));
      });
    }
  }

  Future<void> writeProfile({required int age, required Sex sex}) async {
    if (_userProfile == null) throw Exception('Profile characteristic missing');
    await _userProfile!.write([age & 0xFF, sex == Sex.male ? 1 : 0],
        withoutResponse: false);
  }

  Future<void> startMeasurement() async {
    if (_start == null) throw Exception('Start characteristic missing');
    await _start!.write([0x01], withoutResponse: false);
  }

  Future<void> disconnect() async {
    await _statusCtrl.close();
    await _resultCtrl.close();
    await device.disconnect();
  }
}
