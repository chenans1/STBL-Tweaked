#pragma once

namespace form_config {
    inline constexpr auto requirementsPath = "Data/SKSE/Plugins/STBLforms.ini";

    struct PerkRequirement {
        RE::BGSPerk* perk = nullptr;
        bool configured = false;

        bool IsMetBy(const RE::Actor* actor) const {
            if (!configured) {
                // SKSE::log::info("[IsMetBy] unrestricted");
                return true;
            }

            const bool hasPerk = actor && perk && actor->HasPerk(perk);
            // SKSE::log::info("[IsMetBy] required perk {:08X}, hasPerk={}", perk ? perk->GetFormID() : 0, hasPerk);
            return hasPerk;
        }
    };

    struct CoreForms {
        RE::SpellItem* parrySpell = nullptr;
        RE::EffectSetting* parryWindow = nullptr;
        RE::SpellItem* staggerSpell = nullptr;
        RE::SpellItem* timeBlockBuffSpell = nullptr;
        RE::BGSExplosion* timedBlockExplosion = nullptr;
        RE::BGSSoundDescriptorForm* timedBlockSound = nullptr;

    };

    struct AttackTypePerkRequirements {
        PerkRequirement melee;
        PerkRequirement spell;
        PerkRequirement arrow;
    };

    struct PerkRequirements {
        AttackTypePerkRequirements shield;
        AttackTypePerkRequirements nonShield;

        // Independent requirement for staggering nearby actors.
        PerkRequirement stagger;
    };

    struct RangedPerkRequirements {
        PerkRequirement spell;
        PerkRequirement arrow;
    };

    struct ReflectionPerkRequirements {
        RangedPerkRequirements shield;
        RangedPerkRequirements nonShield;
    };

    struct Config {
        CoreForms core;
        PerkRequirements perks;
        ReflectionPerkRequirements reflectionPerks;
    };

    // Loads and resolves every configured form. Missing files are created with
    // defaults that preserve Simple Timed Block's current forms.
    bool Load();
    const Config& Get();
}
