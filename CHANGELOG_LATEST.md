# Changelog

For more details go to [emsesp.org](https://emsesp.org/).

## [3.8.3]

## Added

- KM300 at address 0x4A [#3084](https://github.com/emsesp/EMS-ESP32/issues/3084)

## Fixed

- signed value for solarInfuence [#3077](https://github.com/emsesp/EMS-ESP32/issues/3077)
- set bin file upload limit to 1M again [#3086](https://github.com/emsesp/EMS-ESP32/issues/3086)
- check arithmetric operations on strings [#3127](https://github.com/emsesp/EMS-ESP32/discussions/3127)
- prevent system crash during WiFi network discovery on ESP32-S3 hardware [#3128](https://github.com/emsesp/EMS-ESP32/pull/3128)
- fix setting date/time on Junkers thermostats
- offset of hpminflowtemp [#3144](https://github.com/emsesp/EMS-ESP32/issues/3144)
- water circulation command [#3149](https://github.com/emsesp/EMS-ESP32/pull/3149)
- start scheduler after customEntities, delay scheduler loop after startup, handling of `system/restart` command [#3146](https://github.com/emsesp/EMS-ESP32/issues/3146)
- possible memory leaks fixed.
- repeated startup schedules compute value

## Changed

- call compute value direct, enlarge TCP stack [#3127](https://github.com/emsesp/EMS-ESP32/discussions/3127)
- Dewtemperature for Easycontrol calculated by ems-esp [#3135](https://github.com/emsesp/EMS-ESP32/issues/3135)
- add option for sync thermostat to ntp, allow different time zones [#3148](https://github.com/emsesp/EMS-ESP32/discussions/3148), **defaults to sync**.
- block too many GET requests [mentioned in #3104](https://github.com/emsesp/EMS-ESP32/issues/3104)
