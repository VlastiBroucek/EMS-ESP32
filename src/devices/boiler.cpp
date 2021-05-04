/*
 * EMS-ESP - https://github.com/emsesp/EMS-ESP
 * Copyright 2020  Paul Derbyshire
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "boiler.h"

namespace emsesp {

REGISTER_FACTORY(Boiler, EMSdevice::DeviceType::BOILER)

uuid::log::Logger Boiler::logger_{F_(boiler), uuid::log::Facility::CONSOLE};

Boiler::Boiler(uint8_t device_type, int8_t device_id, uint8_t product_id, const std::string & version, const std::string & name, uint8_t flags, uint8_t brand)
    : EMSdevice(device_type, device_id, product_id, version, name, flags, brand) {
    LOG_DEBUG(F("Adding new Boiler with device ID 0x%02X"), device_id);

    // cascaded heatingsources, only some values per individual heatsource (hs)
    if (device_id != EMSdevice::EMS_DEVICE_ID_BOILER) {
        uint8_t hs = device_id - EMSdevice::EMS_DEVICE_ID_BOILER_1; // heating source id, count from 0
        // Runtime of each heatingsource in 0x06DC, ff
        register_telegram_type(0x6DC + hs, F("CascadeMessage"), false, MAKE_PF_CB(process_CascadeMessage));
        register_device_value(TAG_HS1 + hs, &burnWorkMin_, DeviceValueType::TIME, nullptr, FL_(burnWorkMin), DeviceValueUOM::MINUTES);
        // selBurnpower in D2 and E4
        // register_telegram_type(0xD2, F("CascadePowerMessage"), false, MAKE_PF_CB(process_CascadePowerMessage));
        // individual Flowtemps and powervalues for each heatingsource in E4
        register_telegram_type(0xE4, F("UBAMonitorFastPlus"), false, MAKE_PF_CB(process_UBAMonitorFastPlus));
        register_device_value(TAG_HS1 + hs, &selFlowTemp_, DeviceValueType::UINT, nullptr, FL_(selFlowTemp), DeviceValueUOM::DEGREES);
        register_device_value(TAG_HS1 + hs, &selBurnPow_, DeviceValueType::UINT, nullptr, FL_(selBurnPow), DeviceValueUOM::PERCENT);
        register_device_value(TAG_HS1 + hs, &curFlowTemp_, DeviceValueType::USHORT, FL_(div10), FL_(curFlowTemp), DeviceValueUOM::DEGREES);
        register_device_value(TAG_HS1 + hs, &curBurnPow_, DeviceValueType::UINT, nullptr, FL_(curBurnPow), DeviceValueUOM::PERCENT);
        return;
    }
    // register values for master boiler/cascade module
    reserve_telgram_functions(25); // reserve some space for the telegram registries, to avoid memory fragmentation

    // the telegram handlers...
    // common for all boilers
    register_telegram_type(0x10, F("UBAErrorMessage1"), false, MAKE_PF_CB(process_UBAErrorMessage));
    register_telegram_type(0x11, F("UBAErrorMessage2"), false, MAKE_PF_CB(process_UBAErrorMessage));
    register_telegram_type(0x14, F("UBATotalUptime"), true, MAKE_PF_CB(process_UBATotalUptime));
    register_telegram_type(0x15, F("UBAMaintenanceData"), false, MAKE_PF_CB(process_UBAMaintenanceData));
    register_telegram_type(0x1C, F("UBAMaintenanceStatus"), false, MAKE_PF_CB(process_UBAMaintenanceStatus));
    // EMS1.0 and maybe EMS+?
    register_telegram_type(0x18, F("UBAMonitorFast"), false, MAKE_PF_CB(process_UBAMonitorFast));
    register_telegram_type(0x19, F("UBAMonitorSlow"), true, MAKE_PF_CB(process_UBAMonitorSlow));
    register_telegram_type(0x1A, F("UBASetPoints"), false, MAKE_PF_CB(process_UBASetPoints));
    register_telegram_type(0x35, F("UBAFlags"), false, MAKE_PF_CB(process_UBAFlags));
    // only EMS 1.0
    register_telegram_type(0x16, F("UBAParameters"), true, MAKE_PF_CB(process_UBAParameters));
    register_telegram_type(0x33, F("UBAParameterWW"), true, MAKE_PF_CB(process_UBAParameterWW));
    register_telegram_type(0x34, F("UBAMonitorWW"), false, MAKE_PF_CB(process_UBAMonitorWW));
    // not ems1.0, but HT3
    if (model() != EMSdevice::EMS_DEVICE_FLAG_EMS) {
        register_telegram_type(0x26, F("UBASettingsWW"), true, MAKE_PF_CB(process_UBASettingsWW));
        register_telegram_type(0x2A, F("MC110Status"), false, MAKE_PF_CB(process_MC110Status));
    }
    // only EMS+
    if (model() != EMSdevice::EMS_DEVICE_FLAG_EMS && model() != EMSdevice::EMS_DEVICE_FLAG_HT3) {
        register_telegram_type(0xD1, F("UBAOutdoorTemp"), false, MAKE_PF_CB(process_UBAOutdoorTemp));
        register_telegram_type(0xE3, F("UBAMonitorSlowPlus"), false, MAKE_PF_CB(process_UBAMonitorSlowPlus2));
        register_telegram_type(0xE4, F("UBAMonitorFastPlus"), false, MAKE_PF_CB(process_UBAMonitorFastPlus));
        register_telegram_type(0xE5, F("UBAMonitorSlowPlus"), false, MAKE_PF_CB(process_UBAMonitorSlowPlus));
        register_telegram_type(0xE6, F("UBAParametersPlus"), true, MAKE_PF_CB(process_UBAParametersPlus));
        register_telegram_type(0xE9, F("UBAMonitorWWPlus"), false, MAKE_PF_CB(process_UBAMonitorWWPlus));
        register_telegram_type(0xEA, F("UBAParameterWWPlus"), true, MAKE_PF_CB(process_UBAParameterWWPlus));
    }
    if (model() == EMSdevice::EMS_DEVICE_FLAG_HEATPUMP) {
        register_telegram_type(0x494, F("UBAEnergySupplied"), false, MAKE_PF_CB(process_UBAEnergySupplied));
        register_telegram_type(0x495, F("UBAInformation"), false, MAKE_PF_CB(process_UBAInformation));
        register_telegram_type(0x48D, F("HpPower"), false, MAKE_PF_CB(process_HpPower));
        register_telegram_type(0x48F, F("HpOutdoor"), false, MAKE_PF_CB(process_HpOutdoor));
    }
    // MQTT commands for boiler topic
    register_device_value(TAG_BOILER_DATA, &dummybool_, DeviceValueType::BOOL, nullptr, FL_(wwtapactivated), DeviceValueUOM::NONE, MAKE_CF_CB(set_tapwarmwater_activated));
    register_device_value(TAG_BOILER_DATA, &dummy8u_, DeviceValueType::ENUM, FL_(enum_reset), FL_(reset), DeviceValueUOM::NONE, MAKE_CF_CB(set_reset));

    // add values
    // reserve_device_values(90);

    // main - boiler_data topic
    register_device_value(TAG_BOILER_DATA, &id_, DeviceValueType::UINT, nullptr, FL_(ID), DeviceValueUOM::NONE);
    id_ = product_id;

    register_device_value(TAG_BOILER_DATA, &heatingActive_, DeviceValueType::BOOL, nullptr, FL_(heatingActive), DeviceValueUOM::NONE);
    register_device_value(TAG_BOILER_DATA, &tapwaterActive_, DeviceValueType::BOOL, nullptr, FL_(tapwaterActive), DeviceValueUOM::NONE);
    register_device_value(TAG_BOILER_DATA, &selFlowTemp_, DeviceValueType::UINT, nullptr, FL_(selFlowTemp), DeviceValueUOM::DEGREES, MAKE_CF_CB(set_flow_temp));
    register_device_value(TAG_BOILER_DATA, &selBurnPow_, DeviceValueType::UINT, nullptr, FL_(selBurnPow), DeviceValueUOM::PERCENT, MAKE_CF_CB(set_burn_power));
    register_device_value(TAG_BOILER_DATA, &heatingPumpMod_, DeviceValueType::UINT, nullptr, FL_(heatingPumpMod), DeviceValueUOM::PERCENT);
    register_device_value(TAG_BOILER_DATA, &heatingPump2Mod_, DeviceValueType::UINT, nullptr, FL_(heatingPump2Mod), DeviceValueUOM::PERCENT);
    register_device_value(TAG_BOILER_DATA, &outdoorTemp_, DeviceValueType::SHORT, FL_(div10), FL_(outdoorTemp), DeviceValueUOM::DEGREES);
    register_device_value(TAG_BOILER_DATA, &curFlowTemp_, DeviceValueType::USHORT, FL_(div10), FL_(curFlowTemp), DeviceValueUOM::DEGREES);
    register_device_value(TAG_BOILER_DATA, &retTemp_, DeviceValueType::USHORT, FL_(div10), FL_(retTemp), DeviceValueUOM::DEGREES);
    register_device_value(TAG_BOILER_DATA, &switchTemp_, DeviceValueType::USHORT, FL_(div10), FL_(switchTemp), DeviceValueUOM::DEGREES);
    register_device_value(TAG_BOILER_DATA, &sysPress_, DeviceValueType::UINT, FL_(div10), FL_(sysPress), DeviceValueUOM::BAR);
    register_device_value(TAG_BOILER_DATA, &boilTemp_, DeviceValueType::USHORT, FL_(div10), FL_(boilTemp), DeviceValueUOM::DEGREES);
    register_device_value(TAG_BOILER_DATA, &exhaustTemp_, DeviceValueType::USHORT, FL_(div10), FL_(exhaustTemp), DeviceValueUOM::DEGREES);
    register_device_value(TAG_BOILER_DATA, &burnGas_, DeviceValueType::BOOL, nullptr, FL_(burnGas), DeviceValueUOM::NONE);
    register_device_value(TAG_BOILER_DATA, &flameCurr_, DeviceValueType::USHORT, FL_(div10), FL_(flameCurr), DeviceValueUOM::UA);
    register_device_value(TAG_BOILER_DATA, &heatingPump_, DeviceValueType::BOOL, nullptr, FL_(heatingPump), DeviceValueUOM::PUMP);
    register_device_value(TAG_BOILER_DATA, &fanWork_, DeviceValueType::BOOL, nullptr, FL_(fanWork), DeviceValueUOM::NONE);
    register_device_value(TAG_BOILER_DATA, &ignWork_, DeviceValueType::BOOL, nullptr, FL_(ignWork), DeviceValueUOM::NONE);
    register_device_value(TAG_BOILER_DATA, &heatingActivated_, DeviceValueType::BOOL, nullptr, FL_(heatingActivated), DeviceValueUOM::NONE, MAKE_CF_CB(set_heating_activated));
    register_device_value(TAG_BOILER_DATA, &heatingTemp_, DeviceValueType::UINT, nullptr, FL_(heatingTemp), DeviceValueUOM::DEGREES, MAKE_CF_CB(set_heating_temp));
    register_device_value(TAG_BOILER_DATA, &pumpModMax_, DeviceValueType::UINT, nullptr, FL_(pumpModMax), DeviceValueUOM::PERCENT, MAKE_CF_CB(set_max_pump));
    register_device_value(TAG_BOILER_DATA, &pumpModMin_, DeviceValueType::UINT, nullptr, FL_(pumpModMin), DeviceValueUOM::PERCENT, MAKE_CF_CB(set_min_pump));
    register_device_value(TAG_BOILER_DATA, &pumpDelay_, DeviceValueType::UINT, nullptr, FL_(pumpDelay), DeviceValueUOM::MINUTES, MAKE_CF_CB(set_pump_delay));
    register_device_value(TAG_BOILER_DATA, &burnMinPeriod_, DeviceValueType::UINT, nullptr, FL_(burnMinPeriod), DeviceValueUOM::MINUTES, MAKE_CF_CB(set_burn_period));
    register_device_value(TAG_BOILER_DATA, &burnMinPower_, DeviceValueType::UINT, nullptr, FL_(burnMinPower), DeviceValueUOM::PERCENT, MAKE_CF_CB(set_min_power));
    register_device_value(TAG_BOILER_DATA, &burnMaxPower_, DeviceValueType::UINT, nullptr, FL_(burnMaxPower), DeviceValueUOM::PERCENT, MAKE_CF_CB(set_max_power));
    register_device_value(TAG_BOILER_DATA, &boilHystOn_, DeviceValueType::INT, nullptr, FL_(boilHystOn), DeviceValueUOM::DEGREES, MAKE_CF_CB(set_hyst_on));
    register_device_value(TAG_BOILER_DATA, &boilHystOff_, DeviceValueType::INT, nullptr, FL_(boilHystOff), DeviceValueUOM::DEGREES, MAKE_CF_CB(set_hyst_off));
    register_device_value(TAG_BOILER_DATA, &setFlowTemp_, DeviceValueType::UINT, nullptr, FL_(setFlowTemp), DeviceValueUOM::DEGREES);
    register_device_value(TAG_BOILER_DATA, &setBurnPow_, DeviceValueType::UINT, nullptr, FL_(setBurnPow), DeviceValueUOM::PERCENT);
    register_device_value(TAG_BOILER_DATA, &curBurnPow_, DeviceValueType::UINT, nullptr, FL_(curBurnPow), DeviceValueUOM::PERCENT);
    register_device_value(TAG_BOILER_DATA, &burnStarts_, DeviceValueType::ULONG, nullptr, FL_(burnStarts), DeviceValueUOM::NONE);
    register_device_value(TAG_BOILER_DATA, &burnWorkMin_, DeviceValueType::TIME, nullptr, FL_(burnWorkMin), DeviceValueUOM::MINUTES);
    register_device_value(TAG_BOILER_DATA, &heatWorkMin_, DeviceValueType::TIME, nullptr, FL_(heatWorkMin), DeviceValueUOM::MINUTES);
    register_device_value(TAG_BOILER_DATA, &UBAuptime_, DeviceValueType::TIME, nullptr, FL_(UBAuptime), DeviceValueUOM::MINUTES);
    register_device_value(TAG_BOILER_DATA, &lastCode_, DeviceValueType::TEXT, nullptr, FL_(lastCode), DeviceValueUOM::NONE);
    register_device_value(TAG_BOILER_DATA, &serviceCode_, DeviceValueType::TEXT, nullptr, FL_(serviceCode), DeviceValueUOM::NONE);
    register_device_value(TAG_BOILER_DATA, &serviceCodeNumber_, DeviceValueType::USHORT, nullptr, FL_(serviceCodeNumber), DeviceValueUOM::NONE);
    register_device_value(TAG_BOILER_DATA, &maintenanceMessage_, DeviceValueType::TEXT, nullptr, FL_(maintenanceMessage), DeviceValueUOM::NONE);
    register_device_value(TAG_BOILER_DATA, &maintenanceDate_, DeviceValueType::TEXT, nullptr, FL_(maintenanceDate), DeviceValueUOM::NONE);
    register_device_value(TAG_BOILER_DATA, &maintenanceType_, DeviceValueType::ENUM, FL_(enum_off_time_date), FL_(maintenanceType), DeviceValueUOM::NONE, MAKE_CF_CB(set_maintenance));
    register_device_value(TAG_BOILER_DATA, &maintenanceTime_, DeviceValueType::USHORT, nullptr, FL_(maintenanceTime), DeviceValueUOM::HOURS);
    // heatpump info
    if (model() == EMS_DEVICE_FLAG_HEATPUMP) {
        register_device_value(TAG_BOILER_DATA, &upTimeControl_, DeviceValueType::TIME, FL_(div60), FL_(upTimeControl), DeviceValueUOM::MINUTES);
        register_device_value(TAG_BOILER_DATA, &upTimeCompHeating_, DeviceValueType::TIME, FL_(div60), FL_(upTimeCompHeating), DeviceValueUOM::MINUTES);
        register_device_value(TAG_BOILER_DATA, &upTimeCompCooling_, DeviceValueType::TIME, FL_(div60), FL_(upTimeCompCooling), DeviceValueUOM::MINUTES);
        register_device_value(TAG_BOILER_DATA, &upTimeCompWw_, DeviceValueType::TIME, FL_(div60), FL_(upTimeCompWw), DeviceValueUOM::MINUTES);
        register_device_value(TAG_BOILER_DATA, &heatingStarts_, DeviceValueType::ULONG, nullptr, FL_(heatingStarts), DeviceValueUOM::NONE);
        register_device_value(TAG_BOILER_DATA, &coolingStarts_, DeviceValueType::ULONG, nullptr, FL_(coolingStarts), DeviceValueUOM::NONE);
        register_device_value(TAG_BOILER_DATA, &nrgConsTotal_, DeviceValueType::ULONG, nullptr, FL_(nrgConsTotal), DeviceValueUOM::KWH);
        register_device_value(TAG_BOILER_DATA, &nrgConsCompTotal_, DeviceValueType::ULONG, nullptr, FL_(nrgConsCompTotal), DeviceValueUOM::KWH);
        register_device_value(TAG_BOILER_DATA, &nrgConsCompHeating_, DeviceValueType::ULONG, nullptr, FL_(nrgConsCompHeating), DeviceValueUOM::KWH);
        register_device_value(TAG_BOILER_DATA, &nrgConsCompWw_, DeviceValueType::ULONG, nullptr, FL_(nrgConsCompWw), DeviceValueUOM::KWH);
        register_device_value(TAG_BOILER_DATA, &nrgConsCompCooling_, DeviceValueType::ULONG, nullptr, FL_(nrgConsCompCooling), DeviceValueUOM::KWH);
        register_device_value(TAG_BOILER_DATA, &auxElecHeatNrgConsTotal_, DeviceValueType::ULONG, nullptr, FL_(auxElecHeatNrgConsTotal), DeviceValueUOM::KWH);
        register_device_value(TAG_BOILER_DATA, &auxElecHeatNrgConsHeating_, DeviceValueType::ULONG, nullptr, FL_(auxElecHeatNrgConsHeating), DeviceValueUOM::KWH);
        register_device_value(TAG_BOILER_DATA, &auxElecHeatNrgConsWW_, DeviceValueType::ULONG, nullptr, FL_(auxElecHeatNrgConsWW), DeviceValueUOM::KWH);
        register_device_value(TAG_BOILER_DATA, &nrgSuppTotal_, DeviceValueType::ULONG, nullptr, FL_(nrgSuppTotal), DeviceValueUOM::KWH);
        register_device_value(TAG_BOILER_DATA, &nrgSuppHeating_, DeviceValueType::ULONG, nullptr, FL_(nrgSuppHeating), DeviceValueUOM::KWH);
        register_device_value(TAG_BOILER_DATA, &nrgSuppWw_, DeviceValueType::ULONG, nullptr, FL_(nrgSuppWw), DeviceValueUOM::KWH);
        register_device_value(TAG_BOILER_DATA, &nrgSuppCooling_, DeviceValueType::ULONG, nullptr, FL_(nrgSuppCooling), DeviceValueUOM::KWH);
        register_device_value(TAG_BOILER_DATA, &hpPower_, DeviceValueType::UINT, FL_(div10), FL_(hpPower), DeviceValueUOM::KW);
        register_device_value(TAG_BOILER_DATA, &hpTc0_, DeviceValueType::SHORT, FL_(div10), FL_(hpTc0), DeviceValueUOM::DEGREES);
        register_device_value(TAG_BOILER_DATA, &hpTc1_, DeviceValueType::SHORT, FL_(div10), FL_(hpTc1), DeviceValueUOM::DEGREES);
        register_device_value(TAG_BOILER_DATA, &hpTc3_, DeviceValueType::SHORT, FL_(div10), FL_(hpTc3), DeviceValueUOM::DEGREES);
        register_device_value(TAG_BOILER_DATA, &hpTr3_, DeviceValueType::SHORT, FL_(div10), FL_(hpTr3), DeviceValueUOM::DEGREES);
        register_device_value(TAG_BOILER_DATA, &hpTr4_, DeviceValueType::SHORT, FL_(div10), FL_(hpTr4), DeviceValueUOM::DEGREES);
        register_device_value(TAG_BOILER_DATA, &hpTr5_, DeviceValueType::SHORT, FL_(div10), FL_(hpTr5), DeviceValueUOM::DEGREES);
        register_device_value(TAG_BOILER_DATA, &hpTr6_, DeviceValueType::SHORT, FL_(div10), FL_(hpTr6), DeviceValueUOM::DEGREES);
        register_device_value(TAG_BOILER_DATA, &hpTr7_, DeviceValueType::SHORT, FL_(div10), FL_(hpTr7), DeviceValueUOM::DEGREES);
        register_device_value(TAG_BOILER_DATA, &hpTl2_, DeviceValueType::SHORT, FL_(div10), FL_(hpTl2), DeviceValueUOM::DEGREES);
        register_device_value(TAG_BOILER_DATA, &hpPl1_, DeviceValueType::SHORT, FL_(div10), FL_(hpPl1), DeviceValueUOM::DEGREES);
        register_device_value(TAG_BOILER_DATA, &hpPh1_, DeviceValueType::SHORT, FL_(div10), FL_(hpPh1), DeviceValueUOM::DEGREES);
    }

    // warm water - boiler_data_ww topic
    register_device_value(TAG_DEVICE_DATA_WW, &wWSelTemp_, DeviceValueType::UINT, nullptr, FL_(wWSelTemp), DeviceValueUOM::DEGREES);
    register_device_value(TAG_DEVICE_DATA_WW, &wWSetTemp_, DeviceValueType::UINT, nullptr, FL_(wWSetTemp), DeviceValueUOM::DEGREES, MAKE_CF_CB(set_warmwater_temp));
    register_device_value(TAG_DEVICE_DATA_WW, &wWType_, DeviceValueType::ENUM, FL_(enum_flow), FL_(wWType), DeviceValueUOM::NONE);
    register_device_value(TAG_DEVICE_DATA_WW, &wWComfort_, DeviceValueType::ENUM, FL_(enum_comfort), FL_(wWComfort), DeviceValueUOM::NONE, MAKE_CF_CB(set_warmwater_mode));
    register_device_value(TAG_DEVICE_DATA_WW, &wWFlowTempOffset_, DeviceValueType::UINT, nullptr, FL_(wWFlowTempOffset), DeviceValueUOM::NONE, MAKE_CF_CB(set_wWFlowTempOffset));
    register_device_value(TAG_DEVICE_DATA_WW, &wWMaxPower_, DeviceValueType::UINT, nullptr, FL_(wWMaxPower), DeviceValueUOM::PERCENT, MAKE_CF_CB(set_warmwater_maxpower));
    register_device_value(TAG_DEVICE_DATA_WW, &wWCircPump_, DeviceValueType::BOOL, nullptr, FL_(wWCircPump), DeviceValueUOM::NONE, MAKE_CF_CB(set_warmwater_circulation_pump));
    register_device_value(TAG_DEVICE_DATA_WW, &wWChargeType_, DeviceValueType::BOOL, FL_(enum_charge), FL_(wWChargeType), DeviceValueUOM::NONE);
    register_device_value(TAG_DEVICE_DATA_WW, &wWDisinfectionTemp_, DeviceValueType::UINT, nullptr, FL_(wWDisinfectionTemp), DeviceValueUOM::DEGREES, MAKE_CF_CB(set_disinfect_temp));
    register_device_value(TAG_DEVICE_DATA_WW, &wWCircMode_, DeviceValueType::ENUM, FL_(enum_freq), FL_(wWCircMode), DeviceValueUOM::NONE, MAKE_CF_CB(set_warmwater_circulation_mode));
    register_device_value(TAG_DEVICE_DATA_WW, &wWCirc_, DeviceValueType::BOOL, nullptr, FL_(wWCirc), DeviceValueUOM::NONE, MAKE_CF_CB(set_warmwater_circulation));
    register_device_value(TAG_DEVICE_DATA_WW, &wWCurTemp_, DeviceValueType::USHORT, FL_(div10), FL_(wWCurTemp), DeviceValueUOM::DEGREES);
    register_device_value(TAG_DEVICE_DATA_WW, &wWCurTemp2_, DeviceValueType::USHORT, FL_(div10), FL_(wWCurTemp2), DeviceValueUOM::DEGREES);
    register_device_value(TAG_DEVICE_DATA_WW, &wWCurFlow_, DeviceValueType::UINT, FL_(div10), FL_(wWCurFlow), DeviceValueUOM::LMIN);
    register_device_value(TAG_DEVICE_DATA_WW, &wWStorageTemp1_, DeviceValueType::USHORT, FL_(div10), FL_(wWStorageTemp1), DeviceValueUOM::DEGREES);
    register_device_value(TAG_DEVICE_DATA_WW, &wWStorageTemp2_, DeviceValueType::USHORT, FL_(div10), FL_(wWStorageTemp2), DeviceValueUOM::DEGREES);
    register_device_value(TAG_DEVICE_DATA_WW, &wWActivated_, DeviceValueType::BOOL, nullptr, FL_(wWActivated), DeviceValueUOM::NONE, MAKE_CF_CB(set_warmwater_activated));
    register_device_value(TAG_DEVICE_DATA_WW, &wWOneTime_, DeviceValueType::BOOL, nullptr, FL_(wWOneTime), DeviceValueUOM::NONE, MAKE_CF_CB(set_warmwater_onetime));
    register_device_value(TAG_DEVICE_DATA_WW, &wWDisinfecting_, DeviceValueType::BOOL, nullptr, FL_(wWDisinfecting), DeviceValueUOM::NONE);
    register_device_value(TAG_DEVICE_DATA_WW, &wWCharging_, DeviceValueType::BOOL, nullptr, FL_(wWCharging), DeviceValueUOM::NONE);
    register_device_value(TAG_DEVICE_DATA_WW, &wWRecharging_, DeviceValueType::BOOL, nullptr, FL_(wWRecharging), DeviceValueUOM::NONE);
    register_device_value(TAG_DEVICE_DATA_WW, &wWTempOK_, DeviceValueType::BOOL, nullptr, FL_(wWTempOK), DeviceValueUOM::NONE);
    register_device_value(TAG_DEVICE_DATA_WW, &wWActive_, DeviceValueType::BOOL, nullptr, FL_(wWActive), DeviceValueUOM::NONE);
    register_device_value(TAG_DEVICE_DATA_WW, &wWHeat_, DeviceValueType::BOOL, nullptr, FL_(wWHeat), DeviceValueUOM::NONE);
    register_device_value(TAG_DEVICE_DATA_WW, &wWSetPumpPower_, DeviceValueType::UINT, nullptr, FL_(wWSetPumpPower), DeviceValueUOM::PERCENT);
    register_device_value(TAG_DEVICE_DATA_WW, &mixerTemp_, DeviceValueType::USHORT, FL_(div10), FL_(mixerTemp), DeviceValueUOM::DEGREES);
    register_device_value(TAG_DEVICE_DATA_WW, &tankMiddleTemp_, DeviceValueType::USHORT, FL_(div10), FL_(tankMiddleTemp), DeviceValueUOM::DEGREES);
    register_device_value(TAG_DEVICE_DATA_WW, &wWStarts_, DeviceValueType::ULONG, nullptr, FL_(wWStarts), DeviceValueUOM::NONE);
    register_device_value(TAG_DEVICE_DATA_WW, &wWStarts2_, DeviceValueType::ULONG, nullptr, FL_(wWStarts2), DeviceValueUOM::NONE);
    register_device_value(TAG_DEVICE_DATA_WW, &wWWorkM_, DeviceValueType::TIME, nullptr, FL_(wWWorkM), DeviceValueUOM::MINUTES);

    // fetch some initial data
    EMSESP::send_read_request(0x10, device_id); // read last errorcode on start (only published on errors)
    EMSESP::send_read_request(0x11, device_id); // read last errorcode on start (only published on errors)
    EMSESP::send_read_request(0x15, device_id); // read maintenace data on start (only published on change)
    EMSESP::send_read_request(0x1C, device_id); // read maintenace status on start (only published on change)
}

// publish HA config
bool Boiler::publish_ha_config() {
    StaticJsonDocument<EMSESP_JSON_SIZE_HA_CONFIG> doc;
    doc["uniq_id"] = F_(boiler);

    char stat_t[Mqtt::MQTT_TOPIC_MAX_SIZE];
    snprintf_P(stat_t, sizeof(stat_t), PSTR("%s/%s"), Mqtt::base().c_str(), Mqtt::tag_to_topic(device_type(), DeviceValueTAG::TAG_NONE).c_str());
    doc["stat_t"] = stat_t;

    doc["name"]    = FJSON("ID");
    doc["val_tpl"] = FJSON("{{value_json.id}}");
    JsonObject dev = doc.createNestedObject("dev");
    dev["name"]    = FJSON("EMS-ESP Boiler");
    dev["sw"]      = EMSESP_APP_VERSION;
    dev["mf"]      = brand_to_string();
    dev["mdl"]     = name();
    JsonArray ids  = dev.createNestedArray("ids");
    ids.add("ems-esp-boiler");

    char topic[Mqtt::MQTT_TOPIC_MAX_SIZE];
    snprintf_P(topic, sizeof(topic), PSTR("sensor/%s/boiler/config"), Mqtt::base().c_str());
    Mqtt::publish_ha(topic, doc.as<JsonObject>()); // publish the config payload with retain flag

    return true;
}

// Check if hot tap water or heating is active
// Values will always be posted first time as heatingActive_ and tapwaterActive_ will have values EMS_VALUE_BOOL_NOTSET
void Boiler::check_active(const bool force) {
    if (!Helpers::hasValue(boilerState_)) {
        return;
    }

    bool    b;
    uint8_t val;

    // check if heating is active, bits 2 and 4 must be set
    b   = ((boilerState_ & 0x09) == 0x09);
    val = b ? EMS_VALUE_BOOL_ON : EMS_VALUE_BOOL_OFF;
    if (heatingActive_ != val || force) {
        heatingActive_ = val;
        char s[7];
        Mqtt::publish(F_(heating_active), Helpers::render_boolean(s, b));
    }

    // check if tap water is active, bits 1 and 4 must be set
    // also check if there is a flowsensor and flow-type
    static bool flowsensor = false;
    if (Helpers::hasValue(wWCurFlow_) && (wWCurFlow_ > 0) && (wWType_ == 1)) {
        flowsensor = true;
    }
    if (flowsensor) {
        b = ((wWCurFlow_ > 0) && ((boilerState_ & 0x0A) == 0x0A));
    } else {
        b = ((boilerState_ & 0x0A) == 0x0A);
    }

    val = b ? EMS_VALUE_BOOL_ON : EMS_VALUE_BOOL_OFF;
    if (tapwaterActive_ != val || force) {
        tapwaterActive_ = val;
        char s[7];
        Mqtt::publish(F_(tapwater_active), Helpers::render_boolean(s, b));
        EMSESP::tap_water_active(b); // let EMS-ESP know, used in the Shower class
    }
}

// 0x33
//  Boiler(0x08) -> Me(0x0B), UBAParameterWW(0x33), data: 08 FF 30 FB FF 28 FF 07 46 00 00
void Boiler::process_UBAParameterWW(std::shared_ptr<const Telegram> telegram) {
    // has_update(telegram->read_bitvalue(wwEquipt_,0,3));  //  8=boiler has ww
    has_update(telegram->read_value(wWActivated_, 1)); // 0xFF means on
    has_update(telegram->read_value(wWSelTemp_, 2));
    // has_update(telegram->read_value(wW?_, 3));           // Hyst on (default -5)
    // has_update(telegram->read_value(wW?_, 4));           // (0xFF) Maybe: Hyst off -1?
    has_update(telegram->read_value(wWFlowTempOffset_, 5)); // default 40
    has_update(telegram->read_value(wWCircPump_, 6));       // 0xFF means on
    has_update(telegram->read_value(wWCircMode_, 7));       // 1=1x3min 6=6x3min 7=continuous
    has_update(telegram->read_value(wWDisinfectionTemp_, 8));
    has_update(telegram->read_value(wWChargeType_, 10)); // 0 = charge pump, 0xff = 3-way valve

    telegram->read_value(wWComfort_, 9);
    if (wWComfort_ == 0x00) {
        wWComfort_ = 0; // Hot
    } else if (wWComfort_ == 0xD8) {
        wWComfort_ = 1; // Eco
    } else if (wWComfort_ == 0xEC) {
        wWComfort_ = 2; // Intelligent
    } else {
        wWComfort_ = EMS_VALUE_UINT_NOTSET;
    }
}

// 0x18
void Boiler::process_UBAMonitorFast(std::shared_ptr<const Telegram> telegram) {
    has_update(telegram->read_value(selFlowTemp_, 0));
    has_update(telegram->read_value(curFlowTemp_, 1));
    has_update(telegram->read_value(selBurnPow_, 3)); // burn power max setting
    has_update(telegram->read_value(curBurnPow_, 4));
    has_update(telegram->read_value(boilerState_, 5));

    has_update(telegram->read_bitvalue(burnGas_, 7, 0));
    has_update(telegram->read_bitvalue(fanWork_, 7, 2));
    has_update(telegram->read_bitvalue(ignWork_, 7, 3));
    has_update(telegram->read_bitvalue(heatingPump_, 7, 5));
    has_update(telegram->read_bitvalue(wWHeat_, 7, 6));
    has_update(telegram->read_bitvalue(wWCirc_, 7, 7));

    // warm water storage sensors (if present)
    // wWStorageTemp2 is also used by some brands as the boiler temperature - see https://github.com/emsesp/EMS-ESP/issues/206
    has_update(telegram->read_value(wWStorageTemp1_, 9));  // 0x8300 if not available
    has_update(telegram->read_value(wWStorageTemp2_, 11)); // 0x8000 if not available - this is boiler temp

    has_update(telegram->read_value(retTemp_, 13));
    has_update(telegram->read_value(flameCurr_, 15));

    // system pressure. FF means missing
    has_update(telegram->read_value(sysPress_, 17)); // is *10

    // read the service code / installation status as appears on the display
    if ((telegram->message_length > 18) && (telegram->offset == 0)) {
        serviceCode_[0] = (serviceCode_[0] == '~') ? 0xF0 : serviceCode_[0];
        has_update(telegram->read_value(serviceCode_[0], 18));
        serviceCode_[0] = (serviceCode_[0] == (char)0xF0) ? '~' : serviceCode_[0];
        has_update(telegram->read_value(serviceCode_[1], 19));
        serviceCode_[2] = '\0'; // null terminate string
    }

    has_update(telegram->read_value(serviceCodeNumber_, 20));

    check_active(); // do a quick check to see if the hot water or heating is active
}

/*
 * UBATotalUptime - type 0x14 - total uptime
 * received only after requested (not broadcasted)
 */
void Boiler::process_UBATotalUptime(std::shared_ptr<const Telegram> telegram) {
    has_update(telegram->read_value(UBAuptime_, 0, 3)); // force to 3 bytes
}

/*
 * UBAParameters - type 0x16
 * data: FF 5A 64 00 0A FA 0F 02 06 64 64 02 08 F8 0F 0F 0F 0F 1E 05 04 09 09 00 28 00 3C
 */
void Boiler::process_UBAParameters(std::shared_ptr<const Telegram> telegram) {
    has_update(telegram->read_value(heatingActivated_, 0));
    has_update(telegram->read_value(heatingTemp_, 1));
    has_update(telegram->read_value(burnMaxPower_, 2));
    has_update(telegram->read_value(burnMinPower_, 3));
    has_update(telegram->read_value(boilHystOff_, 4));
    has_update(telegram->read_value(boilHystOn_, 5));
    has_update(telegram->read_value(burnMinPeriod_, 6));
    // has_update(telegram->read_value(pumpType_, 7)); // 0=off, 02=?
    has_update(telegram->read_value(pumpDelay_, 8));
    has_update(telegram->read_value(pumpModMax_, 9));
    has_update(telegram->read_value(pumpModMin_, 10));
}

/*
 * UBASettingsWW - type 0x26 - max power on offset 7, #740
 * Boiler(0x08) -> Me(0x0B), ?(0x26), data: 01 05 00 0F 00 1E 58 5A
 */
void Boiler::process_UBASettingsWW(std::shared_ptr<const Telegram> telegram) {
    has_update(telegram->read_value(wWMaxPower_, 7));
}

/*
 * UBAMonitorWW - type 0x34 - warm water monitor. 19 bytes long
 * received every 10 seconds
 * Boiler(0x08) -> Me(0x0B), UBAMonitorWW(0x34), data: 30 01 BA 7D 00 21 00 00 03 00 01 22 2B 00 19 5B
*/
void Boiler::process_UBAMonitorWW(std::shared_ptr<const Telegram> telegram) {
    has_update(telegram->read_value(wWSetTemp_, 0));
    has_update(telegram->read_value(wWCurTemp_, 1));
    has_update(telegram->read_value(wWCurTemp2_, 3));

    has_update(telegram->read_value(wWType_, 8));
    has_update(telegram->read_value(wWCurFlow_, 9));
    has_update(telegram->read_value(wWWorkM_, 10, 3));  // force to 3 bytes
    has_update(telegram->read_value(wWStarts_, 13, 3)); // force to 3 bytes

    has_update(telegram->read_bitvalue(wWOneTime_, 5, 1));
    has_update(telegram->read_bitvalue(wWDisinfecting_, 5, 2));
    has_update(telegram->read_bitvalue(wWCharging_, 5, 3));
    has_update(telegram->read_bitvalue(wWRecharging_, 5, 4));
    has_update(telegram->read_bitvalue(wWTempOK_, 5, 5));
    has_update(telegram->read_bitvalue(wWActive_, 5, 6));
}

/*
 * UBAMonitorFastPlus - type 0xE4 - central heating monitor EMS+
 * temperatures at 7 and 23 always identical
+ * Bosch Logamax Plus GB122: issue #620
+ * 88 00 E4 00 00 2D 2D 00 00 C9 34 02 21 64 3D 05 02 01 DE 00 00 00 00 03 62 14 00 02 21 00 00 00 00 00 00 00 2B 2B 83
+ * GB125/Logamatic MC110: issue #650: add retTemp & sysPress
+ * 08 00 E4 00 10 20 2D 48 00 C8 38 02 37 3C 27 03 00 00 00 00 00 01 7B 01 8F 11 00 02 37 80 00 02 1B 80 00 7F FF 80 00
 */
void Boiler::process_UBAMonitorFastPlus(std::shared_ptr<const Telegram> telegram) {
    has_update(telegram->read_value(selFlowTemp_, 6));
    has_update(telegram->read_bitvalue(burnGas_, 11, 0));
    // has_update(telegram->read_bitvalue(heatingPump_, 11, 1)); // heating active? see SlowPlus
    has_update(telegram->read_bitvalue(wWHeat_, 11, 2));
    has_update(telegram->read_value(curBurnPow_, 10));
    has_update(telegram->read_value(selBurnPow_, 9));
    has_update(telegram->read_value(curFlowTemp_, 7));
    has_update(telegram->read_value(flameCurr_, 19));
    has_update(telegram->read_value(retTemp_,
                                    17)); // can be 0 if no sensor, handled in export_values
    has_update(telegram->read_value(sysPress_, 21));

    //has_update(telegram->read_value(temperatur_, 13)); // unknown temperature
    //has_update(telegram->read_value(temperatur_, 27)); // unknown temperature

    // read 3 char service code / installation status as appears on the display
    if ((telegram->message_length > 3) && (telegram->offset == 0)) {
        serviceCode_[0] = (serviceCode_[0] == '~') ? 0xF0 : serviceCode_[0];
        has_update(telegram->read_value(serviceCode_[0], 1));
        serviceCode_[0] = (serviceCode_[0] == (char)0xF0) ? '~' : serviceCode_[0];
        has_update(telegram->read_value(serviceCode_[1], 2));
        has_update(telegram->read_value(serviceCode_[2], 3));
        serviceCode_[3] = '\0';
    }
    has_update(telegram->read_value(serviceCodeNumber_, 4));

    // at this point do a quick check to see if the hot water or heating is active
    uint8_t state = EMS_VALUE_UINT_NOTSET;
    if (telegram->read_value(state, 11)) {
        boilerState_ = state & 0x01 ? 0x08 : 0;
        boilerState_ |= state & 0x02 ? 0x01 : 0;
        boilerState_ |= state & 0x04 ? 0x02 : 0;
    }

    check_active(); // do a quick check to see if the hot water or heating is active
}

/*
 * UBAMonitorSlow - type 0x19 - central heating monitor part 2 (27 bytes long)
 * received every 60 seconds
 * e.g. 08 00 19 00 80 00 02 41 80 00 00 00 00 00 03 91 7B 05 B8 40 00 00 00 04 92 AD 00 5E EE 80 00
 *      08 0B 19 00 FF EA 02 47 80 00 00 00 00 62 03 CA 24 2C D6 23 00 00 00 27 4A B6 03 6E 43 
 *                  00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 17 19 20 21 22 23 24
 */
void Boiler::process_UBAMonitorSlow(std::shared_ptr<const Telegram> telegram) {
    has_update(telegram->read_value(outdoorTemp_, 0));
    has_update(telegram->read_value(boilTemp_, 2));
    has_update(telegram->read_value(exhaustTemp_, 4));
    has_update(telegram->read_value(switchTemp_, 25)); // only if there is a mixer module present
    has_update(telegram->read_value(heatingPumpMod_, 9));
    has_update(telegram->read_value(burnStarts_, 10, 3));  // force to 3 bytes
    has_update(telegram->read_value(burnWorkMin_, 13, 3)); // force to 3 bytes
    has_update(telegram->read_value(heatWorkMin_, 19, 3)); // force to 3 bytes
}

/*
 * UBAMonitorSlowPlus2 - type 0xE3
 * 88 00 E3 00 04 00 00 00 00 01 00 00 00 00 00 02 22 2B 64 46 01 00 00 61
 */
void Boiler::process_UBAMonitorSlowPlus2(std::shared_ptr<const Telegram> telegram) {
    has_update(telegram->read_value(heatingPump2Mod_, 13)); // Heat Pump Modulation
}

/*
 * UBAMonitorSlowPlus - type 0xE5 - central heating monitor EMS+
 * Boiler(0x08) -> Me(0x0B), UBAMonitorSlowPlus(0xE5),
 * data: 01 00 20 00 00 78 00 00 00 00 00 1E EB 00 9D 3E 00 00 00 00 6B 5E 00 06 4C 64 00 00 00 00 8A A3
 */
void Boiler::process_UBAMonitorSlowPlus(std::shared_ptr<const Telegram> telegram) {
    has_update(telegram->read_bitvalue(fanWork_, 2, 2));
    has_update(telegram->read_bitvalue(ignWork_, 2, 3));
    has_update(telegram->read_bitvalue(heatingPump_, 2, 5));
    has_update(telegram->read_bitvalue(wWCirc_, 2, 7));
    has_update(telegram->read_value(exhaustTemp_, 6));
    has_update(telegram->read_value(burnStarts_, 10, 3));  // force to 3 bytes
    has_update(telegram->read_value(burnWorkMin_, 13, 3)); // force to 3 bytes
    has_update(telegram->read_value(heatWorkMin_, 19, 3)); // force to 3 bytes
    has_update(telegram->read_value(heatingPumpMod_, 25));
    // temperature measurements at 4, see #620
}

/*
 * UBAParametersPlus - type 0xE6
 * parameters originaly taken from
 * https://github.com/Th3M3/buderus_ems-wiki/blob/master/Einstellungen%20des%20Regelger%C3%A4ts%20MC110.md
 * 88 0B E6 00 01 46 00 00 46 0A 00 01 06 FA 0A 01 02 64 01 00 00 1E 00 3C 01 00 00 00 01 00 9A
 * from: issue #732
 *       data: 01 50 1E 5A 46 12 64 00 06 FA 3C 03 05 64 00 00 00 28 00 41 03 00 00 00 00 00 00 00 00 00
 */
void Boiler::process_UBAParametersPlus(std::shared_ptr<const Telegram> telegram) {
    has_update(telegram->read_value(heatingActivated_, 0));
    has_update(telegram->read_value(heatingTemp_, 1));
    has_update(telegram->read_value(burnMaxPower_, 4));
    has_update(telegram->read_value(burnMinPower_, 5));
    has_update(telegram->read_value(boilHystOff_, 8));
    has_update(telegram->read_value(boilHystOn_, 9));
    has_update(telegram->read_value(burnMinPeriod_, 10));
    // has_update(telegram->read_value(pumpType_, 11));   // guess, RC300 manual: powercontroled, pressurcontrolled 1-4?
    // has_update(telegram->read_value(pumpDelay_, 12));  // guess
    // has_update(telegram->read_value(pumpModMax_, 13)); // guess
    // has_update(telegram->read_value(pumpModMin_, 14)); // guess
}

// 0xEA
void Boiler::process_UBAParameterWWPlus(std::shared_ptr<const Telegram> telegram) {
    has_update(telegram->read_value(wWActivated_, 5)); // 0x01 means on
    has_update(telegram->read_value(wWCircPump_, 10)); // 0x01 means yes
    has_update(telegram->read_value(wWCircMode_, 11)); // 1=1x3min... 6=6x3min, 7=continuous
    // has_update(telegram->read_value(wWDisinfectTemp_, 12)); // settings, status in E9
    // has_update(telegram->read_value(wWSelTemp_, 6));        // settings, status in E9
}

// 0xE9 - WW monitor ems+
// e.g. 08 00 E9 00 37 01 F6 01 ED 00 00 00 00 41 3C 00 00 00 00 00 00 00 00 00 00 00 00 37 00 00 00 (CRC=77) #data=27
void Boiler::process_UBAMonitorWWPlus(std::shared_ptr<const Telegram> telegram) {
    has_update(telegram->read_value(wWSetTemp_, 0));
    has_update(telegram->read_value(wWCurTemp_, 1));
    has_update(telegram->read_value(wWCurTemp2_, 3));

    has_update(telegram->read_value(wWWorkM_, 14, 3));  // force to 3 bytes
    has_update(telegram->read_value(wWStarts_, 17, 3)); // force to 3 bytes

    has_update(telegram->read_bitvalue(wWOneTime_, 12, 2));
    has_update(telegram->read_bitvalue(wWDisinfecting_, 12, 3));
    has_update(telegram->read_bitvalue(wWCharging_, 12, 4));
    has_update(telegram->read_bitvalue(wWRecharging_, 13, 4));
    has_update(telegram->read_bitvalue(wWTempOK_, 13, 5));
    has_update(telegram->read_bitvalue(wWCirc_, 13, 2));

    // has_update(telegram->read_value(wWActivated_, 20)); // Activated is in 0xEA, this is something other 0/100%
    has_update(telegram->read_value(wWSelTemp_, 10));
    has_update(telegram->read_value(wWDisinfectionTemp_, 9));
}

/*
 * UBAInformation - type 0x495
 * all values 32 bit
 * 08 0B FF 00 03 95 01 01 AB 83 00 27 78 EB 00 84 FA 39 FF FF FF 00 00 53 7D 8D 00 00 0F 04 1C
 * 08 00 FF 00 03 95 01 01 AB 83 00 27 78 EB 00 84 FA 39 FF FF FF 00 00 53 7D 8D 00 00 0F 04 63
 * 08 00 FF 18 03 95 00 00 05 84 00 00 07 22 FF FF FF FF 00 00 02 5C 00 00 03 C0 00 00 01 98 64
 * 08 00 FF 30 03 95 00 00 00 D4 FF FF FF FF 00 00 1C 70 FF FF FF FF 00 00 20 30 00 00 0E 06 FB
 * 08 00 FF 48 03 95 00 00 06 C0 00 00 07 66 FF FF FF FF 2E
 */
void Boiler::process_UBAInformation(std::shared_ptr<const Telegram> telegram) {
    has_update(telegram->read_value(upTimeControl_, 0));
    has_update(telegram->read_value(upTimeCompHeating_, 8));
    has_update(telegram->read_value(upTimeCompCooling_, 16));
    has_update(telegram->read_value(upTimeCompWw_, 4));

    has_update(telegram->read_value(heatingStarts_, 28));
    has_update(telegram->read_value(coolingStarts_, 36));
    has_update(telegram->read_value(wWStarts2_, 24));

    has_update(telegram->read_value(nrgConsTotal_, 64));

    has_update(telegram->read_value(auxElecHeatNrgConsTotal_, 40));
    has_update(telegram->read_value(auxElecHeatNrgConsHeating_, 48));
    has_update(telegram->read_value(auxElecHeatNrgConsWW_, 44));

    has_update(telegram->read_value(nrgConsCompTotal_, 56));
    has_update(telegram->read_value(nrgConsCompHeating_, 68));
    has_update(telegram->read_value(nrgConsCompWw_, 72));
    has_update(telegram->read_value(nrgConsCompCooling_, 76));
}

/*
 * UBAEnergy - type 0x494
 * Energy-values all 32bit
 * 08 00 FF 00 03 94 03 31 21 59 00 00 7C 70 00 00 15 B8 00 00 40 E3 00 00 27 23 FF FF FF FF EA
 * 08 00 FF 18 03 94 FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF 00 00 00 00 00 00 00 00 00 7E
 * 08 00 FF 31 03 94 00 00 00 00 00 00 00 38
 */
void Boiler::process_UBAEnergySupplied(std::shared_ptr<const Telegram> telegram) {
    has_update(telegram->read_value(nrgSuppTotal_, 4));
    has_update(telegram->read_value(nrgSuppHeating_, 12));
    has_update(telegram->read_value(nrgSuppWw_, 8));
    has_update(telegram->read_value(nrgSuppCooling_, 16));
}

// Heatpump power - type 0x48D
void Boiler::process_HpPower(std::shared_ptr<const Telegram> telegram){
    has_update(telegram->read_value(hpPower_, 11));

}

// Heatpump outdoor unit - type 0x48F
void Boiler::process_HpOutdoor(std::shared_ptr<const Telegram> telegram){
    has_update(telegram->read_value(hpTc0_, 6));
    has_update(telegram->read_value(hpTc1_, 4));
    has_update(telegram->read_value(hpTc3_, 2));
    has_update(telegram->read_value(hpTr3_, 16));
    has_update(telegram->read_value(hpTr4_, 18));
    has_update(telegram->read_value(hpTr5_, 20));
    has_update(telegram->read_value(hpTr6_, 0));
    has_update(telegram->read_value(hpTr7_, 30));
    has_update(telegram->read_value(hpTl2_, 12));
    has_update(telegram->read_value(hpPl1_, 26));
    has_update(telegram->read_value(hpPh1_, 28));
}

// 0x2A - MC110Status
// e.g. 88 00 2A 00 00 00 00 00 00 00 00 00 D2 00 00 80 00 00 01 08 80 00 02 47 00
// see https://github.com/emsesp/EMS-ESP/issues/397
void Boiler::process_MC110Status(std::shared_ptr<const Telegram> telegram) {
    has_update(telegram->read_value(mixerTemp_, 14));
    has_update(telegram->read_value(tankMiddleTemp_, 18));
}

/*
 * UBAOutdoorTemp - type 0xD1 - external temperature EMS+
 */
void Boiler::process_UBAOutdoorTemp(std::shared_ptr<const Telegram> telegram) {
    has_update(telegram->read_value(outdoorTemp_, 0));
}

// UBASetPoint 0x1A
void Boiler::process_UBASetPoints(std::shared_ptr<const Telegram> telegram) {
    has_update(telegram->read_value(setFlowTemp_, 0));    // boiler set temp from thermostat
    has_update(telegram->read_value(setBurnPow_, 1));     // max json power in %
    has_update(telegram->read_value(wWSetPumpPower_, 2)); // ww pump speed/power?
}

// 0x6DC, ff for cascaded heatsources (hs)
void Boiler::process_CascadeMessage(std::shared_ptr<const Telegram> telegram) {
    // uint8_t  hsActivated;
    // has_update(telegram->read_value(hsActivated, 0));
    telegram->read_value(burnWorkMin_, 3); // this is in seconds
    burnWorkMin_ /= 60;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

// 0x35 - not yet implemented
void Boiler::process_UBAFlags(std::shared_ptr<const Telegram> telegram) {
}

#pragma GCC diagnostic pop

// 0x1C
// 08 00 1C 94 0B 0A 1D 31 08 00 80 00 00 00 -> message for 29.11.2020
// 08 00 1C 94 0B 0A 1D 31 00 00 00 00 00 00 -> message reset
void Boiler::process_UBAMaintenanceStatus(std::shared_ptr<const Telegram> telegram) {
    // 5. byte: Maintenance due (0 = no, 3 = yes, due to operating hours, 8 = yes, due to date)
    uint8_t message_code = maintenanceMessage_[2] - '0';
    has_update(telegram->read_value(message_code, 5));

    // ignore if 0, which means all is ok
    if (Helpers::hasValue(message_code) && message_code > 0) {
        snprintf_P(maintenanceMessage_, sizeof(maintenanceMessage_), PSTR("H%02d"), message_code);
    }
}

// 0x10, 0x11
void Boiler::process_UBAErrorMessage(std::shared_ptr<const Telegram> telegram) {
    if (telegram->offset > 0 || telegram->message_length < 9) {
        return;
    }
    // data: displaycode(2), errornumber(2), year, month, hour, day, minute, duration(2), src-addr
    if (telegram->message_data[4] & 0x80) { // valid date

        static uint32_t lastCodeDate_ = 0; // last code date
        char            code[3];
        uint16_t        codeNo;
        code[0] = telegram->message_data[0];
        code[1] = telegram->message_data[1];
        code[2] = 0;
        telegram->read_value(codeNo, 2);
        uint16_t year  = (telegram->message_data[4] & 0x7F) + 2000;
        uint8_t  month = telegram->message_data[5];
        uint8_t  day   = telegram->message_data[7];
        uint8_t  hour  = telegram->message_data[6];
        uint8_t  min   = telegram->message_data[8];
        uint32_t date  = (year - 2000) * 535680UL + month * 44640UL + day * 1440UL + hour * 60 + min;
        // store only the newest code from telegrams 10 and 11
        if (date > lastCodeDate_) {
            snprintf_P(lastCode_, sizeof(lastCode_), PSTR("%s(%d) %02d.%02d.%d %02d:%02d"), code, codeNo, day, month, year, hour, min);
            lastCodeDate_ = date;
        }
    }
}

// 0x15
void Boiler::process_UBAMaintenanceData(std::shared_ptr<const Telegram> telegram) {
    if (telegram->offset > 0 || telegram->message_length < 5) {
        return;
    }
    // first byte: Maintenance messages (0 = none, 1 = by operating hours, 2 = by date)

    has_update(telegram->read_value(maintenanceType_, 0));

    uint8_t time = (maintenanceTime_ == EMS_VALUE_USHORT_NOTSET) ? EMS_VALUE_UINT_NOTSET : maintenanceTime_ / 100;
    has_update(telegram->read_value(time, 1));
    maintenanceTime_ = (time == EMS_VALUE_UINT_NOTSET) ? EMS_VALUE_USHORT_NOTSET : time * 100;
    // telegram->read_value(maintenanceTime_, 1, 1);
    // maintenanceTime_ = maintenanceTime * 100;

    // date only
    uint8_t day   = telegram->message_data[2];
    uint8_t month = telegram->message_data[3];
    uint8_t year  = telegram->message_data[4];
    if (day > 0 && month > 0) {
        snprintf_P(maintenanceDate_, sizeof(maintenanceDate_), PSTR("%02d.%02d.%04d"), day, month, year + 2000);
    }
}

// Set the warm water temperature 0x33
bool Boiler::set_warmwater_temp(const char * value, const int8_t id) {
    int v = 0;
    if (!Helpers::value2number(value, v)) {
        LOG_WARNING(F("Set boiler warm water temperature: Invalid value"));
        return false;
    }

    LOG_INFO(F("Setting boiler warm water temperature to %d C"), v);
    if (get_toggle_fetch(EMS_TYPE_UBAParametersPlus)) {
        write_command(EMS_TYPE_UBAParameterWWPlus, 6, v, EMS_TYPE_UBAParameterWWPlus);
    } else {
        // some boiler have it in 0x33, some in 0x35
        write_command(EMS_TYPE_UBAFlags, 3, v, 0x34);                          // for i9000, see #397
        write_command(EMS_TYPE_UBAParameterWW, 2, v, EMS_TYPE_UBAParameterWW); // read seltemp back
    }

    return true;
}

// Set the warm water disinfection temperature
bool Boiler::set_disinfect_temp(const char * value, const int8_t id) {
    int v = 0;
    if (!Helpers::value2number(value, v)) {
        LOG_WARNING(F("Set boiler warm water disinfect temperature: Invalid value"));
        return false;
    }

    LOG_INFO(F("Setting boiler warm water disinfect temperature to %d C"), v);
    if (get_toggle_fetch(EMS_TYPE_UBAParametersPlus)) {
        write_command(EMS_TYPE_UBAParameterWWPlus, 12, v, EMS_TYPE_UBAParameterWWPlus);
    } else {
        write_command(EMS_TYPE_UBAParameterWW, 8, v, EMS_TYPE_UBAParameterWW);
    }

    return true;
}
// flow temp
bool Boiler::set_flow_temp(const char * value, const int8_t id) {
    int v = 0;
    if (!Helpers::value2number(value, v)) {
        LOG_WARNING(F("Set boiler flow temperature: Invalid value"));
        return false;
    }

    LOG_INFO(F("Setting boiler flow temperature to %d C"), v);
    write_command(EMS_TYPE_UBASetPoints, 0, v, EMS_TYPE_UBASetPoints);

    return true;
}

// set selected burner power
bool Boiler::set_burn_power(const char * value, const int8_t id) {
    int v = 0;
    if (!Helpers::value2number(value, v)) {
        LOG_WARNING(F("Set burner max. power: Invalid value"));
        return false;
    }

    LOG_INFO(F("Setting burner max. power to %d %"), v);
    write_command(EMS_TYPE_UBASetPoints, 1, v, EMS_TYPE_UBASetPoints);

    return true;
}

// Set the warm water flow temperature offset 0x33
bool Boiler::set_wWFlowTempOffset(const char * value, const int8_t id) {
    int v = 0;
    if (!Helpers::value2number(value, v)) {
        LOG_WARNING(F("Set boiler warm water flow temperature offset: Invalid value"));
        return false;
    }

    LOG_INFO(F("Setting boiler warm water flow temperature offset to %d C"), v);
    write_command(EMS_TYPE_UBAParameterWW, 5, v, EMS_TYPE_UBAParameterWW);

    return true;
}

// set heating activated
bool Boiler::set_heating_activated(const char * value, const int8_t id) {
    bool v = false;
    if (!Helpers::value2bool(value, v)) {
        LOG_WARNING(F("Set boiler heating: Invalid value"));
        return false;
    }

    LOG_INFO(F("Setting boiler heating %s"), v ? "on" : "off");
    if (get_toggle_fetch(EMS_TYPE_UBAParametersPlus)) {
        write_command(EMS_TYPE_UBAParametersPlus, 0, v ? 0x01 : 0, EMS_TYPE_UBAParametersPlus);
    } else {
        write_command(EMS_TYPE_UBAParameters, 0, v ? 0xFF : 0, EMS_TYPE_UBAParameters);
    }

    return true;
}

// set heating maximum temperature
bool Boiler::set_heating_temp(const char * value, const int8_t id) {
    int v = 0;
    if (!Helpers::value2number(value, v)) {
        LOG_WARNING(F("Set boiler heating temperature: Invalid value"));
        return false;
    }

    LOG_INFO(F("Setting boiler heating temperature to %d C"), v);
    if (get_toggle_fetch(EMS_TYPE_UBAParametersPlus)) {
        write_command(EMS_TYPE_UBAParametersPlus, 1, v, EMS_TYPE_UBAParametersPlus);
    } else {
        write_command(EMS_TYPE_UBAParameters, 1, v, EMS_TYPE_UBAParameters);
    }

    return true;
}

// set min boiler output
bool Boiler::set_min_power(const char * value, const int8_t id) {
    int v = 0;
    if (!Helpers::value2number(value, v)) {
        LOG_WARNING(F("Set boiler min power: Invalid value"));
        return false;
    }

    LOG_INFO(F("Setting boiler min power to %d %%"), v);
    if (get_toggle_fetch(EMS_TYPE_UBAParametersPlus)) {
        write_command(EMS_TYPE_UBAParametersPlus, 5, v, EMS_TYPE_UBAParametersPlus);
    } else {
        write_command(EMS_TYPE_UBAParameters, 3, v, EMS_TYPE_UBAParameters);
    }

    return true;
}

// set max boiler output
bool Boiler::set_max_power(const char * value, const int8_t id) {
    int v = 0;
    if (!Helpers::value2number(value, v)) {
        LOG_WARNING(F("Set boiler max power: Invalid value"));
        return false;
    }

    LOG_INFO(F("Setting boiler max power to %d %%"), v);
    if (get_toggle_fetch(EMS_TYPE_UBAParametersPlus)) {
        write_command(EMS_TYPE_UBAParametersPlus, 4, v, EMS_TYPE_UBAParametersPlus);
    } else {
        write_command(EMS_TYPE_UBAParameters, 2, v, EMS_TYPE_UBAParameters);
    }

    return true;
}

// set warm water max power
bool Boiler::set_warmwater_maxpower(const char * value, const int8_t id) {
    int v = 0;
    if (!Helpers::value2number(value, v)) {
        LOG_WARNING(F("Set warm water max power: Invalid value"));
        return false;
    }

    LOG_INFO(F("Setting warm water max power to %d %%"), v);
    write_command(EMS_TYPE_UBASettingsWW, 7, v, EMS_TYPE_UBASettingsWW);

    return true;
}

// set min pump modulation
bool Boiler::set_min_pump(const char * value, const int8_t id) {
    int v = 0;
    if (!Helpers::value2number(value, v)) {
        LOG_WARNING(F("Set pump min: Invalid value"));
        return false;
    }

    LOG_INFO(F("Setting pump min to %d %%"), v);
    if (get_toggle_fetch(EMS_TYPE_UBAParametersPlus)) {
        write_command(EMS_TYPE_UBAParametersPlus, 14, v, EMS_TYPE_UBAParametersPlus);
    } else {
        write_command(EMS_TYPE_UBAParameters, 10, v, EMS_TYPE_UBAParameters);
    }

    return true;
}

// set max pump modulation
bool Boiler::set_max_pump(const char * value, const int8_t id) {
    int v = 0;
    if (!Helpers::value2number(value, v)) {
        LOG_WARNING(F("Set pump max: Invalid value"));
        return false;
    }

    LOG_INFO(F("Setting pump max to %d %%"), v);
    if (get_toggle_fetch(EMS_TYPE_UBAParametersPlus)) {
        write_command(EMS_TYPE_UBAParametersPlus, 13, v, EMS_TYPE_UBAParametersPlus);
    } else {
        write_command(EMS_TYPE_UBAParameters, 9, v, EMS_TYPE_UBAParameters);
    }

    return true;
}

// set boiler on hysteresis
bool Boiler::set_hyst_on(const char * value, const int8_t id) {
    int v = 0;
    if (!Helpers::value2number(value, v)) {
        LOG_WARNING(F("Set boiler hysteresis: Invalid value"));
        return false;
    }

    LOG_INFO(F("Setting boiler hysteresis on to %d C"), v);
    if (get_toggle_fetch(EMS_TYPE_UBAParametersPlus)) {
        write_command(EMS_TYPE_UBAParametersPlus, 9, v, EMS_TYPE_UBAParametersPlus);
    } else {
        write_command(EMS_TYPE_UBAParameters, 5, v, EMS_TYPE_UBAParameters);
    }

    return true;
}

// set boiler off hysteresis
bool Boiler::set_hyst_off(const char * value, const int8_t id) {
    int v = 0;
    if (!Helpers::value2number(value, v)) {
        LOG_WARNING(F("Set boiler hysteresis: Invalid value"));
        return false;
    }

    LOG_INFO(F("Setting boiler hysteresis off to %d C"), v);
    if (get_toggle_fetch(EMS_TYPE_UBAParametersPlus)) {
        write_command(EMS_TYPE_UBAParametersPlus, 8, v, EMS_TYPE_UBAParametersPlus);
    } else {
        write_command(EMS_TYPE_UBAParameters, 4, v, EMS_TYPE_UBAParameters);
    }

    return true;
}

// set min burner period
bool Boiler::set_burn_period(const char * value, const int8_t id) {
    int v = 0;
    if (!Helpers::value2number(value, v)) {
        LOG_WARNING(F("Set burner min. period: Invalid value"));
        return false;
    }

    LOG_INFO(F("Setting burner min. period to %d min"), v);
    if (get_toggle_fetch(EMS_TYPE_UBAParametersPlus)) {
        write_command(EMS_TYPE_UBAParametersPlus, 10, v, EMS_TYPE_UBAParametersPlus);
    } else {
        write_command(EMS_TYPE_UBAParameters, 6, v, EMS_TYPE_UBAParameters);
    }

    return true;
}

// set pump delay
bool Boiler::set_pump_delay(const char * value, const int8_t id) {
    int v = 0;
    if (!Helpers::value2number(value, v)) {
        LOG_WARNING(F("Set boiler pump delay: Invalid value"));
        return false;
    }

    if (get_toggle_fetch(EMS_TYPE_UBAParameters)) {
        LOG_INFO(F("Setting boiler pump delay to %d min"), v);
        write_command(EMS_TYPE_UBAParameters, 8, v, EMS_TYPE_UBAParameters);
        return true;
    }

    return false;
}

// note some boilers do not have this setting, than it's done by thermostat
// on a RC35 it's by EMSESP::send_write_request(0x37, 0x10, 2, &set, 1, 0); (set is 1,2,3) 1=hot, 2=eco, 3=intelligent
bool Boiler::set_warmwater_mode(const char * value, const int8_t id) {
    uint8_t set;
    if (!Helpers::value2enum(value, set, FL_(enum_comfort))) {
        LOG_WARNING(F("Set boiler warm water mode: Invalid value"));
        return false;
    }

    if (!get_toggle_fetch(EMS_TYPE_UBAParameterWW)) {
        return false;
    }

    if (set == 0) {
        LOG_INFO(F("Setting boiler warm water to Hot"));
    } else if (set == 1) {
        LOG_INFO(F("Setting boiler warm water to Eco"));
        set = 0xD8;
    } else if (set == 2) {
        LOG_INFO(F("Setting boiler warm water to Intelligent"));
        set = 0xEC;
    } else {
        return false; // do nothing
    }

    write_command(EMS_TYPE_UBAParameterWW, 9, set, EMS_TYPE_UBAParameterWW);
    return true;
}

// turn on/off warm water
bool Boiler::set_warmwater_activated(const char * value, const int8_t id) {
    bool v = false;
    if (!Helpers::value2bool(value, v)) {
        LOG_WARNING(F("Set boiler warm water active: Invalid value"));
        return false;
    }

    LOG_INFO(F("Setting boiler warm water active %s"), v ? "on" : "off");

    // https://github.com/emsesp/EMS-ESP/issues/268
    uint8_t n;
    if (EMSbus::is_ht3()) {
        n = (v ? 0x08 : 0x00); // 0x08 is on, 0x00 is off
    } else {
        n = (v ? 0xFF : 0x00); // 0xFF is on, 0x00 is off
    }

    if (get_toggle_fetch(EMS_TYPE_UBAParameterWWPlus)) {
        write_command(EMS_TYPE_UBAParameterWWPlus, 1, v ? 1 : 0, EMS_TYPE_UBAParameterWWPlus);
    } else {
        write_command(EMS_TYPE_UBAParameterWW, 1, n, 0x34);
    }

    return true;
}

// Activate / De-activate the Warm Tap Water
// Note: Using the type 0x1D to put the boiler into Test mode. This may be shown on the boiler with a flashing 'T'
bool Boiler::set_tapwarmwater_activated(const char * value, const int8_t id) {
    bool v = false;
    if (!Helpers::value2bool(value, v)) {
        LOG_WARNING(F("Set warm tap water: Invalid value"));
        return false;
    }

    LOG_INFO(F("Setting warm tap water %s"), v ? "on" : "off");
    uint8_t message_data[EMS_MAX_TELEGRAM_MESSAGE_LENGTH];
    for (uint8_t i = 0; i < sizeof(message_data); i++) {
        message_data[i] = 0x00;
    }

    // we use the special test mode 0x1D for this. Setting the first data to 5A puts the system into test mode and
    // a setting of 0x00 puts it back into normal operating mode
    // when in test mode we're able to mess around with the 3-way valve settings
    if (!v) {
        // on
        message_data[0] = 0x5A; // test mode on
        message_data[1] = 0x00; // burner output 0%
        message_data[3] = 0x64; // boiler pump capacity 100%
        message_data[4] = 0xFF; // 3-way valve hot water only
    } else {
        // get out of test mode. Send all zeros.
        // telegram: 0B 08 1D 00 00
    }

    write_command(EMS_TYPE_UBAFunctionTest, 0, message_data, sizeof(message_data), 0);

    return true;
}

// Activate / De-activate One Time warm water 0x35
// true = on, false = off
// See also https://github.com/emsesp/EMS-ESP/issues/341#issuecomment-596245458 for Junkers
bool Boiler::set_warmwater_onetime(const char * value, const int8_t id) {
    bool v = false;
    if (!Helpers::value2bool(value, v)) {
        LOG_WARNING(F("Set warm water OneTime loading: Invalid value"));
        return false;
    }

    LOG_INFO(F("Setting warm water OneTime loading %s"), v ? "on" : "off");
    if (get_toggle_fetch(EMS_TYPE_UBAParameterWWPlus)) {
        write_command(EMS_TYPE_UBAFlags, 0, (v ? 0x22 : 0x02), 0xE9); // not sure if this is in flags
    } else {
        write_command(EMS_TYPE_UBAFlags, 0, (v ? 0x22 : 0x02), 0x34);
    }

    return true;
}

// Activate / De-activate circulation of warm water 0x35
// true = on, false = off
bool Boiler::set_warmwater_circulation(const char * value, const int8_t id) {
    bool v = false;
    if (!Helpers::value2bool(value, v)) {
        LOG_WARNING(F("Set warm water circulation: Invalid value"));
        return false;
    }

    LOG_INFO(F("Setting warm water circulation %s"), v ? "on" : "off");
    if (get_toggle_fetch(EMS_TYPE_UBAParameterWWPlus)) {
        write_command(EMS_TYPE_UBAFlags, 1, (v ? 0x22 : 0x02), 0xE9); // not sure if this is in flags
    } else {
        write_command(EMS_TYPE_UBAFlags, 1, (v ? 0x22 : 0x02), 0x34);
    }

    return true;
}

// configuration of warm water circulation pump
bool Boiler::set_warmwater_circulation_pump(const char * value, const int8_t id) {
    bool v = false;
    if (!Helpers::value2bool(value, v)) {
        LOG_WARNING(F("Set warm water circulation pump: Invalid value"));
        return false;
    }

    LOG_INFO(F("Setting warm water circulation pump %s"), v ? "on" : "off");

    if (get_toggle_fetch(EMS_TYPE_UBAParameterWWPlus)) {
        write_command(EMS_TYPE_UBAParameterWWPlus, 10, v ? 0x01 : 0x00, EMS_TYPE_UBAParameterWWPlus);
    } else {
        write_command(EMS_TYPE_UBAParameterWW, 6, v ? 0xFF : 0x00, EMS_TYPE_UBAParameterWW);
    }

    return true;
}

// Set the mode of circulation, 1x3min, ... 6x3min, continuos
// true = on, false = off
bool Boiler::set_warmwater_circulation_mode(const char * value, const int8_t id) {
    int v = 0;
    if (!Helpers::value2number(value, v)) {
        LOG_WARNING(F("Set warm water circulation mode: Invalid value"));
        return false;
    }

    if (v < 7) {
        LOG_INFO(F("Setting warm water circulation mode %dx3min"), v);
    } else if (v == 7) {
        LOG_INFO(F("Setting warm water circulation mode continuos"));
    } else {
        LOG_WARNING(F("Set warm water circulation mode: Invalid value"));
        return false;
    }

    if (get_toggle_fetch(EMS_TYPE_UBAParameterWWPlus)) {
        write_command(EMS_TYPE_UBAParameterWWPlus, 11, v, EMS_TYPE_UBAParameterWWPlus);
    } else {
        write_command(EMS_TYPE_UBAParameterWW, 7, v, EMS_TYPE_UBAParameterWW);
    }

    return true;
}

// Reset command
// 0 & 1        Reset-Mode (Manual, others)
// 8            reset maintenance message Hxx
// 12 & 13      Reset that Error-memory
bool Boiler::set_reset(const char * value, const int8_t id) {
    uint8_t num;
    if (!Helpers::value2enum(value, num, FL_(enum_reset))) {
        return false;
    }

    if (num == 0) {
        LOG_INFO(F("Reset boiler maintenance message"));
        write_command(0x05, 0x08, 0xFF, 0x1C);
        return true;
    } else if (num == 1) {
        LOG_INFO(F("Reset boiler error message"));
        write_command(0x05, 0x00, 0x5A); // error reset
        return true;
    }
    return false;
}

//maintenance
bool Boiler::set_maintenance(const char * value, const int8_t id) {
    std::string s(12, '\0');
    if (Helpers::value2string(value, s)) {
        if (s == Helpers::toLower(uuid::read_flash_string(F_(reset)))) {
            LOG_INFO(F("Reset boiler maintenance message"));
            write_command(0x05, 0x08, 0xFF, 0x1C);
            return true;
        }
    }

    if (strlen(value) == 10) { // date
        uint8_t day   = (value[0] - '0') * 10 + (value[1] - '0');
        uint8_t month = (value[3] - '0') * 10 + (value[4] - '0');
        uint8_t year  = (uint8_t)(Helpers::atoint(&value[6]) - 2000);
        if (day > 0 && day < 32 && month > 0 && month < 13) {
            LOG_INFO(F("Setting maintenance date to %02d.%02d.%04d"), day, month, year + 2000);
            uint8_t data[5] = {2, (uint8_t)(maintenanceTime_ / 100), day, month, year};
            write_command(0x15, 0, data, 5, 0x15);
        } else {
            LOG_WARNING(F("Setting maintenance: wrong format %d.%d.%d"), day, month, year + 2000);
            return false;
        }
        return true;
    }

    int hrs;
    if (Helpers::value2number(value, hrs)) {
        if (hrs > 99 && hrs < 25600) {
            LOG_INFO(F("Setting maintenance time %d hours"), hrs);
            uint8_t data[2] = {1, (uint8_t)(hrs / 100)};
            write_command(0x15, 0, data, 2, 0x15);
            return true;
        }
    }

    uint8_t num;
    if (Helpers::value2enum(value, num, FL_(enum_off_time_date))) {
        LOG_INFO(F("Setting maintenance type to %s"), value);
        write_command(0x15, 0, num, 0x15);
        return true;
    }

    LOG_WARNING(F("Setting maintenance: wrong format"));
    return false;
}

} // namespace emsesp
