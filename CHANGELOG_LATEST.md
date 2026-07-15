# Changelog

For more details go to [emsesp.org](https://emsesp.org/).

## [3.9.0]

## Added

- user-requested LED blink [#3063](https://github.com/emsesp/EMS-ESP32/issues/3063)
- KM300 at address 0x4A [#3084](https://github.com/emsesp/EMS-ESP32/issues/3084)
- Commands Service that can be called via MQTT or API or used in the Scheduler Service

## Fixed

- signed value for solarInfuence [#3077](https://github.com/emsesp/EMS-ESP32/issues/3077)
- TLS support with 4MB boards without PSRAM
- prevent system crash during WiFi network discovery on ESP32-S3 hardware [#3128](https://github.com/emsesp/EMS-ESP32/pull/3128)
- check arithmetric operations on strings [#3127](https://github.com/emsesp/EMS-ESP32/discussions/3127)
- fix setting date/time on Junkers thermostats
- offset of hpminflowtemp [#3144](https://github.com/emsesp/EMS-ESP32/issues/3144)
- water circulation command [#3149](https://github.com/emsesp/EMS-ESP32/pull/3149)
- start scheduler after customEntities, delay scheduler loop after startup, handling of `system/restart` command [#3146](https://github.com/emsesp/EMS-ESP32/issues/3146)
- possible memory leaks fixed.
- repeated startup schedules compute value
- shunting yard show json

## Changed

- various memory optimizations [#3083](https://github.com/emsesp/EMS-ESP32/issues/3083)
- Scheduler name is now mandatory
- network fallback to AP only after start [#3090](https://github.com/emsesp/EMS-ESP32/issues/3090)
- replaced Web async-validator with custom validator and toast with native snackbar to reduce bundle size
- Dewtemperature for Easycontrol calculated by ems-esp [#3135](https://github.com/emsesp/EMS-ESP32/issues/3135)
- add option for sync thermostat to ntp, allow different time zones [#3148](https://github.com/emsesp/EMS-ESP32/discussions/3148), **defaults to sync**.
- block too many GET requests [mentioned in #3104](https://github.com/emsesp/EMS-ESP32/issues/3104)
- Gateway and Connect devices are not shown in the Devices page [3126](https://github.com/emsesp/EMS-ESP32/discussions/3126)
