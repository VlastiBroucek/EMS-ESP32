/*
 * EMS-ESP - https://github.com/emsesp/EMS-ESP
 * Copyright 2020-2025  emsesp.org
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

#include "emsesp.h"
#include "WebCommandService.h"

#include "shuntingYard.h"

namespace emsesp {

WebCommandService::WebCommandService(AsyncWebServer * server, FS * fs, SecurityManager * securityManager)
    : _httpEndpoint(WebCommands::read, WebCommands::update, this, server, EMSESP_COMMANDS_SERVICE_PATH, securityManager, AuthenticationPredicates::IS_AUTHENTICATED)
    , _fsPersistence(WebCommands::read, WebCommands::update, this, fs, EMSESP_COMMANDS_FILE) {
}

void WebCommandService::begin() {
    _fsPersistence.readFromFS();

    EMSESP::webCommandService.read([&](WebCommands & webCommands) { commandItems_ = &webCommands.commandItems; });

    EMSESP::logger().info("Starting Commands service");
    char topic[Mqtt::MQTT_TOPIC_MAX_SIZE];
    snprintf(topic, sizeof(topic), "%s/#", F_(commands));
    Mqtt::subscribe(EMSdevice::DeviceType::COMMAND, topic, nullptr);

#if defined(EMSESP_TEST)
    load_test_data();
#endif
}

void WebCommands::read(WebCommands & webCommands, JsonObject root) {
    JsonArray items   = root["commands"].to<JsonArray>();
    uint8_t   counter = 1;
    for (const CommandItem & ci : webCommands.commandItems) {
        JsonObject obj = items.add<JsonObject>();
        obj["id"]      = counter++;
        obj["cmd"]     = ci.cmd;
        obj["value"]   = ci.value;
        obj["name"]    = (const char *)ci.name;
    }
}

StateUpdateResult WebCommands::update(JsonObject root, WebCommands & webCommands) {
    Command::erase_device_commands(EMSdevice::DeviceType::COMMAND);
    webCommands.commandItems.clear();

    auto items = root["commands"].as<JsonArray>();
    for (const JsonObject item : items) {
        auto ci  = CommandItem();
        ci.cmd   = item["cmd"].as<std::string>();
        ci.value = item["value"].as<std::string>();
        strlcpy(ci.name, item["name"].as<const char *>(), sizeof(ci.name));

        webCommands.commandItems.push_back(ci);
        Command::add(
            EMSdevice::DeviceType::COMMAND,
            webCommands.commandItems.back().name,
            [name = std::string(webCommands.commandItems.back().name)](const char * value, const int8_t id) {
                return EMSESP::webCommandService.executeCommand(name.c_str(), value); // value is optional
            },
            FL_(command_cmd),
            CommandFlag::ADMIN_ONLY);
    }
    return StateUpdateResult::CHANGED;
}

// find a command item by name (case-insensitive)
const CommandItem * WebCommandService::find(const char * name) {
    if (name == nullptr || name[0] == '\0') {
        return nullptr;
    }
    auto lower_name = Helpers::toLower(name);
    for (const CommandItem & ci : *commandItems_) {
        if (ci.name[0] != '\0' && Helpers::toLower(ci.name) == lower_name) {
            return &ci;
        }
    }
    return nullptr;
}

// execute a named command — looks up by name and runs it
bool WebCommandService::executeCommand(const char * name, const char * value) {
    const CommandItem * ci = find(name);
    if (!ci) {
        EMSESP::logger().warning("Command '%s' not found", name ? name : "");
        return false;
    }
    // if there is a value use it, otherwise use the command's default value
    std::string cmd_value = value ? value : ci->value.c_str();
    return executeCommand(ci->name, std::string(ci->cmd.c_str()), cmd_value);
}

// execute a command with explicit cmd and value strings
// handles both HTTP URLs (JSON format) and internal API commands
bool WebCommandService::executeCommand(const char * name, const std::string & command, const std::string & data) {
    std::string cmd = Helpers::toLower(command);

    // handle HTTP commands (JSON with url/method/value)
    JsonDocument doc;
    if (deserializeJson(doc, cmd) == DeserializationError::Ok) {
        std::string url = doc["url"] | "";
        auto        q   = url.find_first_of('?');
        if (q != std::string::npos) {
            auto s = url.substr(q + 1);
            auto l = s.length();
            commands(s, false);
            url.replace(q + 1, l, s);
        }
        std::string value  = doc["value"] | data;
        std::string method = doc["method"] | "GET";
        commands(value, false);
        auto lower_url = Helpers::toLower(url.c_str());
        if (lower_url.starts_with("http://") || lower_url.starts_with("https://")) {
            std::string result;
            int         httpResult = http_request(url, method, value, doc["header"].as<JsonObjectConst>(), result);
            if (httpResult != 200) {
                EMSESP::logger().warning("Command '%s': URL command failed with http code %d", name, httpResult);
                return false;
            }
#if defined(EMSESP_DEBUG)
            EMSESP::logger().debug("Command '%s': URL '%s' successful with http code %d", name, url.c_str(), httpResult);
#endif
            return true;
        }
    }

    // handle internal API commands
    doc.clear();
    JsonObject input = doc.to<JsonObject>();
    if (!data.empty()) {
        input["data"] = data;
    }

    JsonDocument doc_output;
    JsonObject   output = doc_output.to<JsonObject>();

    char command_str[COMMAND_MAX_LENGTH];
    snprintf(command_str, sizeof(command_str), "/api/%s", cmd.c_str());

    uint8_t return_code = Command::process(command_str, true, input, output);
    if (return_code == CommandRet::OK) {
#if defined(EMSESP_DEBUG)
        EMSESP::logger().debug("Command '%s' (%s with data '%s') was successful", name, cmd.c_str(), data.c_str());
#endif
        if (data.empty() && output.size()) {
            Mqtt::queue_publish("response", output);
        }
        return true;
    }

    char error[100];
    if (output.size()) {
        snprintf(error, sizeof(error), "Command '%s': %s", name ? name : "", (const char *)output["message"]);
    } else {
        snprintf(error, sizeof(error), "Command '%s': %s failed with error %s", name, cmd.c_str(), Command::return_code_string(return_code));
    }
    EMSESP::logger().warning(error);
    return false;
}

bool WebCommandService::get_value_info(JsonObject output, const char * cmd) {
    if (commandItems_->empty()) {
        return true;
    }

    if (!strlen(cmd) || !strcmp(cmd, F_(values)) || !strcmp(cmd, F_(info))) {
        for (const CommandItem & ci : *commandItems_) {
            if (ci.name[0] != '\0') {
                output[(const char *)ci.name] = ci.cmd;
            }
        }
        return true;
    }

    if (!strcmp(cmd, F_(entities))) {
        for (const CommandItem & ci : *commandItems_) {
            if (ci.name[0] != '\0') {
                get_value_json(output[ci.name].to<JsonObject>(), ci);
            }
        }
        return true;
    }

    if (!strcmp(cmd, F_(metrics))) {
        std::string metrics = get_metrics_prometheus();
        if (!metrics.empty()) {
            output["api_data"] = metrics;
            return true;
        }
        return false;
    }

    // look up specific command by name
    const char * attribute_s = Command::get_attribute(cmd);
    for (const CommandItem & ci : *commandItems_) {
        if (Helpers::toLower(ci.name) == cmd) {
            get_value_json(output, ci);
            return Command::get_attribute(output, cmd, attribute_s);
        }
    }

    return false;
}

std::string WebCommandService::get_metrics_prometheus() {
    std::string result;
    result.reserve(commandItems_->size() * 100);
    for (const CommandItem & ci : *commandItems_) {
        if (ci.name[0] == '\0') {
            continue;
        }
        result += (std::string) "# HELP emsesp_cmd_" + ci.name + " " + ci.name + "\n";
        result += (std::string) "# TYPE emsesp_cmd_" + ci.name + " gauge\n";
        result += (std::string) "emsesp_cmd_" + ci.name + " 1\n";
    }
    return result;
}

void WebCommandService::get_value_json(JsonObject output, const CommandItem & ci) {
    output["name"] = (const char *)ci.name;
    // output["fullname"]  = (const char *)ci.name;
    // output["type"]     = "command";
    output["command"] = ci.cmd;
    output["value"]   = ci.value;
    // bool hasName       = ci.name[0] != '\0';
    // output["readable"]  = hasName;
    // output["writeable"] = hasName;
    // output["visible"]   = hasName;
}

void WebCommandService::publish(const bool force) {
    if (!Mqtt::enabled() || commandItems_->empty()) {
        return;
    }
    if (force && !EMSESP::mqtt_.get_publish_onchange(EMSdevice::DeviceType::SYSTEM)) {
        return;
    }

    JsonDocument doc(PSRAM_DOC);
    JsonObject   output = doc.to<JsonObject>();
    for (const CommandItem & ci : *commandItems_) {
        if (ci.name[0] != '\0' && !output[ci.name].is<JsonVariantConst>()) {
            output[(const char *)ci.name] = ci.cmd;
        }
    }

    if (!doc.isNull()) {
        char topic[Mqtt::MQTT_TOPIC_MAX_SIZE];
        snprintf(topic, sizeof(topic), "%s_data", F_(commands));
        Mqtt::queue_publish(topic, output);
    }
}

uint8_t WebCommandService::count_entities() {
    return static_cast<uint8_t>(commandItems_ ? commandItems_->size() : 0);
}

#if defined(EMSESP_TEST)
void WebCommandService::load_test_data() {
    Command::erase_device_commands(EMSdevice::DeviceType::COMMAND);
    update([&](WebCommands & webCommands) {
        webCommands.commandItems.clear();

        auto ci  = CommandItem();
        ci.cmd   = "system/fetch";
        ci.value = "10";
        strcpy(ci.name, "fetch_values");
        webCommands.commandItems.push_back(ci);

        ci       = CommandItem();
        ci.cmd   = "system/message";
        ci.value = "hello";
        strcpy(ci.name, "send_message");
        webCommands.commandItems.push_back(ci);

        ci       = CommandItem();
        ci.cmd   = "system/message";
        ci.value = "{\"url\":\"http://emsesp.org/versions.json\"}";
        strcpy(ci.name, "get_versions");
        webCommands.commandItems.push_back(ci);

        // manually add the commands
        for (const auto & item : webCommands.commandItems) {
            if (item.name[0] != '\0') {
                Command::add(
                    EMSdevice::DeviceType::COMMAND,
                    item.name,
                    [name = std::string(item.name)](const char * value, const int8_t id) {
                        return EMSESP::webCommandService.executeCommand(name.c_str(), value);
                    },
                    FL_(command_cmd),
                    CommandFlag::ADMIN_ONLY);
            }
        }

        return StateUpdateResult::CHANGED;
    });
}
#endif

} // namespace emsesp
