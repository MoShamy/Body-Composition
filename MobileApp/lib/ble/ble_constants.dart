import 'package:flutter_blue_plus/flutter_blue_plus.dart';

class BleIds {
  static const String deviceName = 'BodyComp';

  static final Guid service        = Guid('12345678-1234-5678-1234-567812345678');
  static final Guid userProfile    = Guid('12345678-1234-5678-1234-567812345601');
  static final Guid measureStart   = Guid('12345678-1234-5678-1234-567812345602');
  static final Guid status         = Guid('12345678-1234-5678-1234-567812345603');
  static final Guid result         = Guid('12345678-1234-5678-1234-567812345604');
}

enum MeasurementStatus { idle, ready, measuring, done, error, unknown }

MeasurementStatus statusFromByte(int b) {
  switch (b) {
    case 0: return MeasurementStatus.idle;
    case 1: return MeasurementStatus.ready;
    case 2: return MeasurementStatus.measuring;
    case 3: return MeasurementStatus.done;
    case 4: return MeasurementStatus.error;
    default: return MeasurementStatus.unknown;
  }
}

enum Sex { female, male }
