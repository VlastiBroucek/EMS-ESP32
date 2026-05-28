# Changelog

For more details go to [emsesp.org](https://emsesp.org/).

## [3.9.0]

## Added

- user-requested LED blink [#3063](https://github.com/emsesp/EMS-ESP32/issues/3063)
- KM300 at address 0x4A [#3084](https://github.com/emsesp/EMS-ESP32/issues/3084)

## Fixed

- signed value for solarInfuence [#3077](https://github.com/emsesp/EMS-ESP32/issues/3077)
- TLS support with 4MB boards without PSRAM

## Changed

- various memory optimizations [#3083](https://github.com/emsesp/EMS-ESP32/issues/3083)
- Scheduler name is mandatory
- Scheduler with type "Immediate", click Execute does not run the command [#3092](https://github.com/emsesp/EMS-ESP32/issues/3092)
- network fallback to AP only after start [#3090](https://github.com/emsesp/EMS-ESP32/issues/3090)
