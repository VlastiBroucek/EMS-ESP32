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

#include <esp32-psram.h>

#ifndef WebCommandService_h
#define WebCommandService_h

#define EMSESP_COMMANDS_FILE "/config/emsespCommands.json"
#define EMSESP_COMMANDS_SERVICE_PATH "/rest/commands" // GET and POST

namespace emsesp {

class CommandItem {
  public:
    stringPSRAM cmd;
    stringPSRAM value;
    char        name[20];
};

class WebCommands {
  public:
    std::list<CommandItem, AllocatorPSRAM<CommandItem>> commandItems;

    static void              read(WebCommands & webCommands, JsonObject root);
    static StateUpdateResult update(JsonObject root, WebCommands & webCommands);
};

class WebCommandService : public StatefulService<WebCommands> {
  public:
    WebCommandService(AsyncWebServer * server, FS * fs, SecurityManager * securityManager);

    void begin();
    void publish(const bool force = false);
    bool get_value_info(JsonObject output, const char * cmd);
    void get_value_json(JsonObject output, const CommandItem & commandItem);

    bool executeCommand(const char * name, const char * value = nullptr);
    bool executeCommand(const char * name, const std::string & cmd, const std::string & value);

    const CommandItem * find(const char * name);

    uint8_t count_entities();

    std::string get_metrics_prometheus();

#if defined(EMSESP_TEST)
    void load_test_data();
#endif

  private:
    HttpEndpoint<WebCommands>  _httpEndpoint;
    FSPersistence<WebCommands> _fsPersistence;

    std::list<CommandItem, AllocatorPSRAM<CommandItem>> * commandItems_;
};

} // namespace emsesp

#endif
