#pragma once

#include <string>

namespace settings {
    struct config {
        bool log = true;

        bool applyTimedBlockVFX = true;
        bool applyTimedBlockSFX = true;
        
        bool preventAllDamage = true;
        float additionalDamageReduction = 0.5;
        
        float attackerSlowDownMult = 0.05f;
        float attackerSlowdownDuration = 0.5f;
    };
    
    template <class T>
    struct setting_definition {
        const char* key;
        T settings::config::*member;
    };

    constexpr auto setting_definitions = std::tuple{
        setting_definition<bool>{ "log", &settings::config::log },
        setting_definition<bool>{ "applyTimedBlockVFX", &settings::config::applyTimedBlockVFX },
        setting_definition<bool>{ "applyTimedBlockSFX", &settings::config::applyTimedBlockSFX },
        setting_definition<bool>{ "preventAllDamage", &settings::config::preventAllDamage },
        setting_definition<float>{ "additionalDamageReduction", &settings::config::additionalDamageReduction },
        setting_definition<float>{ "attackerSlowDownMult", &settings::config::attackerSlowDownMult },
        setting_definition<float>{ "attackerSlowdownDuration", &settings::config::attackerSlowdownDuration },
    };

    config Get();
    void Set(const config& value);
    void Load();
    bool Save();

    void __stdcall RenderMenuPage();

    void RegisterMenu();
}
