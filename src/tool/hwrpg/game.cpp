#include "save_data.h"
#include <json/convert_json.h>
#include <json/json_export.h>

struct PhaseData {
    std::string name;
    std::string script;

    bool from_json(const futils::json::JSON& json, futils::json::FromFlag flag) {
        JSON_PARAM_BEGIN(*this, json);
        FROM_JSON_PARAM(name, "name", flag);
        FROM_JSON_PARAM(script, "script", flag);
        JSON_PARAM_END();
    }
};

struct GameConfig {
    std::string version;
    std::vector<PhaseData> phases;

    bool from_json(const futils::json::JSON& json, futils::json::FromFlag flag) {
        JSON_PARAM_BEGIN(*this, json);
        FROM_JSON_PARAM(version, "version", flag);
        FROM_JSON_PARAM(phases, "phases", flag);
        JSON_PARAM_END();
    }
};

struct GameMain {
    void game_start() {
    }
};