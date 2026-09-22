#pragma once

namespace form_config {
    inline constexpr auto requirementsPath = "Data/SKSE/Plugins/STBLforms.ini";

    struct PerkRequirement {
        RE::BGSPerk* perk = nullptr;
        bool configured = false;

        [[nodiscard]] bool IsMetBy(const RE::Actor* actor) const {
            return !configured || (actor && actor->HasPerk(perk));
        }
    };

    struct CoreForms {
        RE::SpellItem* parrySpell = nullptr;
        RE::EffectSetting* parryWindow = nullptr;
        RE::SpellItem* staggerSpell = nullptr;
        RE::BGSExplosion* timedBlockExplosion = nullptr;
        RE::BGSSoundDescriptorForm* timedBlockSound = nullptr;
    };

    struct PerkRequirements {
        // Damage-prevention requirements by attack type.
        PerkRequirement melee;
        PerkRequirement spell;
        PerkRequirement arrow;

        // Independent requirement for staggering nearby actors.
        PerkRequirement stagger;
    };

    struct Config {
        CoreForms core;
        PerkRequirements perks;
    };

    // Loads and resolves every configured form. Missing files are created with
    // defaults that preserve Simple Timed Block's current forms.
    bool Load();
    [[nodiscard]] const Config& Get();
}
