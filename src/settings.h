#pragma once

namespace settings {
    struct config {
        bool log = true;

        bool applyTimedBlockVFX = true;
        bool applyTimedBlockSFX = true;
        
        bool preventAllDamage = true;

        //bypasses the blockcap limit intentionally
        float additionalDamageReduction = 0.5f;
        float additionalDamageReductionArrow = 0.7f;
        float additionalDamageReductionSpell = 0.7f;

        float timedBlockWindow = 0.33f;

        bool AOEStaggerEnabled = true;
        float AOEStaggerRadius = 256.0f;
        
        //interrupt -> recoil, spellcaster-> interrupt, arrow = recoil (with behavior patch)
        //alternatively: just uses the stagger spell
        bool enableInterrupt = true;
        bool interruptStagger = true; //off = recoil, on = stagger
        bool meleeInterruptEnabled = true;
        bool rangedInterruptEnabled = true;
        float baseInterruptChance = 0.2f; //interrupt chance = base * blockskill*blockSkillFactor * (shieldInterruptMult * rangedInterruptMult)
        float maxInterruptChance = 1.0f;
        float blockSkillFactor = 0.5f; //additional chance per block skill factor
        float shieldInterruptMult = 1.5f;
        float rangedInterruptMult = 0.5f;
        float staggerMagnitudeOverride = 1.0f; //overrides the magnitude of SimpleTimedBlockTweaked.esp~0x809's 0x808 MGEF

        bool attackerHistopEnabled = true;
        float attackerSlowDownMult = 0.05f;
        float attackerSlowdownDuration = 0.5f;

        bool preventAllDamageArrows = true;
        bool preventAllDamageSpells = true;
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
        setting_definition<float>{ "additionalDamageReductionArrow", &settings::config::additionalDamageReductionArrow },
        setting_definition<float>{ "additionalDamageReductionSpell", &settings::config::additionalDamageReductionSpell },
        
        setting_definition<float>{ "timedBlockWindow", &settings::config::timedBlockWindow },

        setting_definition<bool>{ "AOEStaggerEnabled", &settings::config::AOEStaggerEnabled },
        setting_definition<float>{ "AOEStaggerRadius", &settings::config::AOEStaggerRadius },

        setting_definition<bool>{ "enableInterrupt", &settings::config::enableInterrupt },
        setting_definition<bool>{ "interruptStagger", &settings::config::interruptStagger },
        setting_definition<bool>{ "meleeInterruptEnabled", &settings::config::meleeInterruptEnabled },
        setting_definition<bool>{ "rangedInterruptEnabled", &settings::config::rangedInterruptEnabled },
        setting_definition<float>{ "baseInterruptChance", &settings::config::baseInterruptChance },
        setting_definition<float>{ "maxInterruptChance", &settings::config::maxInterruptChance },
        setting_definition<float>{ "blockSkillFactor", &settings::config::blockSkillFactor },
        setting_definition<float>{ "shieldInterruptMult", &settings::config::shieldInterruptMult },
        setting_definition<float>{ "rangedInterruptMult", &settings::config::rangedInterruptMult },
        setting_definition<float>{ "staggerMagnitudeOverride", &settings::config::staggerMagnitudeOverride },

        setting_definition<bool>{ "attackerHistopEnabled", &settings::config::attackerHistopEnabled },
        setting_definition<float>{ "attackerSlowDownMult", &settings::config::attackerSlowDownMult },
        setting_definition<float>{ "attackerSlowdownDuration", &settings::config::attackerSlowdownDuration },

        setting_definition<bool>{ "preventAllDamageArrows", &settings::config::preventAllDamageArrows },
        setting_definition<bool>{ "preventAllDamageSpells", &settings::config::preventAllDamageSpells },
    };

    config Get();
    void Set(const config& value);
    void Load();
    bool Save();

    void __stdcall RenderMenuPage();

    void RegisterMenu();
}
