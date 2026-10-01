#include "PCH.h"
#include "form_config.h"

#include <SimpleIni.h>

#include <charconv>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace form_config {
    namespace {
        constexpr auto coreSection = "Core";
        constexpr auto perkSection = "PerkRequirements";
        constexpr auto reflectionPerkSection = "ReflectionPerkRequirements";
        constexpr auto soundSection = "SFX";
        constexpr auto vfxSection = "VFX";

        constexpr auto defaultParrySpell = "SimpleTimedBlock.esp ~ 0x802";
        constexpr auto defaultParryWindow = "SimpleTimedBlock.esp ~ 0x801";
        constexpr auto defaultStaggerSpell = "SimpleTimedBlock.esp ~ 0x803";
        constexpr auto defaultTimedBlockBuffSpell = "SimpleTimedBlock.esp ~ 0x80B";
        constexpr auto defaultShieldExplosions = "SimpleTimedBlock.esp ~ 0x808, SimpleTimedBlock.esp ~ 0x809";
        constexpr auto defaultWeaponExplosions = "SimpleTimedBlock.esp ~ 0x805, SimpleTimedBlock.esp ~ 0x806";
        constexpr auto defaultSound = "SimpleTimedBlock.esp ~ 0x807";

        Config activeConfig{};

        std::string_view trim(std::string_view value) {
            constexpr auto whitespace = " \t\r\n";
            const auto first = value.find_first_not_of(whitespace);
            if (first == std::string_view::npos) {
                return {};
            }
            return value.substr(first, value.find_last_not_of(whitespace) - first + 1);
        }

        struct FormReference {
            std::string_view plugin;
            std::uint32_t localFormID = 0;
        };

        bool parseFormReference(std::string_view setting, std::string_view context, FormReference& result) {
            setting = trim(setting);
            const auto separator = setting.rfind('~');
            if (separator == std::string_view::npos) {
                SKSE::log::error("[forms] Invalid {} value '{}'; expected <plugin> ~ 0x<FormID>", context, setting);
                return false;
            }

            const auto plugin = trim(setting.substr(0, separator));
            auto formIDText = trim(setting.substr(separator + 1));
            if (plugin.empty() || formIDText.empty()) {
                SKSE::log::error("[forms] Invalid {} value '{}'; expected <plugin> ~ 0x<FormID>", context, setting);
                return false;
            }
            const bool hasHexPrefix = formIDText.size() >= 2 && formIDText[0] == '0' && (formIDText[1] == 'x' || formIDText[1] == 'X');
            if (hasHexPrefix) {
                formIDText.remove_prefix(2);
            }

            std::uint32_t localFormID = 0;
            const auto [end, error] = std::from_chars(formIDText.data(), formIDText.data() + formIDText.size(), localFormID, 16);
            if (formIDText.empty() || error != std::errc{} ||
                end != formIDText.data() + formIDText.size() || localFormID > 0x00FFFFFF) {
                SKSE::log::error("[forms] Invalid {} FormID in '{}'; use a plugin-local hexadecimal FormID", context, setting);
                return false;
            }

            result = { plugin, localFormID };
            return true;
        }

        template <class T>
        T* loadForm(std::string_view setting, std::string_view context) {
            FormReference reference{};
            if (!parseFormReference(setting, context, reference)) {
                return nullptr;
            }

            auto* dataHandler = RE::TESDataHandler::GetSingleton();
            auto* form = dataHandler ? dataHandler->LookupForm<T>(reference.localFormID, reference.plugin) : nullptr;
            if (!form) {
                SKSE::log::error("[forms] Could not resolve {} from '{}'", context, setting);
                return nullptr;
            }

            SKSE::log::info("[forms] Loaded {} from '{}' as {:08X}", context, setting, form->GetFormID());
            return form;
        }

        std::vector<RE::BGSExplosion*> loadExplosionList(std::string_view setting, std::string_view context) {
            std::vector<RE::BGSExplosion*> forms;
            while (!setting.empty()) {
                const auto separator = setting.find(',');
                auto entry = trim(setting.substr(0, separator));
                if (!entry.empty()) {
                    if (auto* form = loadForm<RE::BGSExplosion>(entry, context)) {
                        forms.push_back(form);
                    } else {
                        SKSE::log::warn("[VFX] Skipping invalid {} entry '{}'", context, entry);
                    }
                }
                if (separator == std::string_view::npos) {
                    break;
                }
                setting.remove_prefix(separator + 1);
            }
            return forms;
        }

        RE::BGSSoundDescriptorForm* loadOptionalSound(std::string_view setting, std::string_view context) {
            setting = trim(setting);
            if (setting.empty()) {
                SKSE::log::info("[SFX] {} is disabled", context);
                return nullptr;
            }

            auto* sound = loadForm<RE::BGSSoundDescriptorForm>(setting, context);
            if (!sound) {
                SKSE::log::warn("[SFX] {} will be skipped because its sound form could not be loaded", context);
            }
            return sound;
        }

        PerkRequirement loadPerkRequirement(std::string_view setting, std::string_view context) {
            setting = trim(setting);
            if (setting.empty()) {
                SKSE::log::info("[perk requirement] {} is unrestricted", context);
                return {};
            }

            auto* perk = loadForm<RE::BGSPerk>(setting, context);
            if (!perk) {
                SKSE::log::error("[perk requirement] Invalid {} requirement; no perk will be required", context);
                return {};
            }

            return { perk, true };
        }

        bool createDefaults() {
            CSimpleIniA ini;
            ini.SetUnicode(false);
            ini.SetValue(coreSection, "ParrySpell", defaultParrySpell);
            ini.SetValue(coreSection, "ParryWindow", defaultParryWindow);
            ini.SetValue(coreSection, "StaggerSpell", defaultStaggerSpell);
            ini.SetValue(coreSection, "TimedBlockBuffSpell", defaultTimedBlockBuffSpell);
            ini.SetValue(soundSection, "shield", defaultSound);
            ini.SetValue(soundSection, "weapons", defaultSound);
            ini.SetValue(soundSection, "else", defaultSound);
            ini.SetValue(vfxSection, "shield", defaultShieldExplosions);
            ini.SetValue(vfxSection, "weapon", defaultWeaponExplosions);
            ini.SetValue(vfxSection, "else", defaultWeaponExplosions);
            ini.SetValue(perkSection, "ShieldMelee", "");
            ini.SetValue(perkSection, "ShieldSpell", "");
            ini.SetValue(perkSection, "ShieldArrow", "");
            ini.SetValue(perkSection, "NonShieldMelee", "");
            ini.SetValue(perkSection, "NonShieldSpell", "");
            ini.SetValue(perkSection, "NonShieldArrow", "");
            ini.SetValue(perkSection, "Stagger", "");
            ini.SetValue(reflectionPerkSection, "ShieldSpell", "");
            ini.SetValue(reflectionPerkSection, "ShieldArrow", "");
            ini.SetValue(reflectionPerkSection, "NonShieldSpell", "");
            ini.SetValue(reflectionPerkSection, "NonShieldArrow", "");

            std::error_code ec;
            std::filesystem::create_directories(std::filesystem::path(requirementsPath).parent_path(), ec);
            if (ec || ini.SaveFile(requirementsPath) < 0) {
                SKSE::log::error("[forms] Could not create {}: {}", requirementsPath, ec.message());
                return false;
            }

            SKSE::log::info("[forms] Created {} with defaults", requirementsPath);
            return true;
        }

        const char* readSetting(
            const CSimpleIniA& ini,
            const char* section,
            const char* key,
            const char* fallback = "") {
            return ini.GetValue(section, key, fallback);
        }

        const char* readPerkSetting(const CSimpleIniA& ini, const char* key, const char* legacyKey) {
            if (const auto* value = ini.GetValue(perkSection, key, nullptr)) {
                return value;
            }
            return ini.GetValue(perkSection, legacyKey, "");
        }
    }

    bool Load() {
        std::error_code ec;
        if (!std::filesystem::exists(requirementsPath, ec) && !ec && !createDefaults()) {
            return false;
        }

        CSimpleIniA ini;
        ini.SetUnicode(false);
        if (ec || ini.LoadFile(requirementsPath) < 0) {
            SKSE::log::error("[forms] Could not read {}", requirementsPath);
            return false;
        }

        Config loaded{};
        loaded.core.parrySpell = loadForm<RE::SpellItem>(readSetting(ini, coreSection, "ParrySpell", defaultParrySpell), "Core/ParrySpell");
        loaded.core.parryWindow = loadForm<RE::EffectSetting>(readSetting(ini, coreSection, "ParryWindow", defaultParryWindow), "Core/ParryWindow");
        loaded.core.staggerSpell = loadForm<RE::SpellItem>(readSetting(ini, coreSection, "StaggerSpell", defaultStaggerSpell), "Core/StaggerSpell");
        loaded.core.timeBlockBuffSpell = loadForm<RE::SpellItem>(readSetting(ini, coreSection, "TimedBlockBuffSpell", defaultTimedBlockBuffSpell), "Core/TimedBlockBuffSpell");
        loaded.sounds.shield = loadOptionalSound(readSetting(ini, soundSection, "shield", defaultSound), "SFX/shield");
        loaded.sounds.weapons = loadOptionalSound(readSetting(ini, soundSection, "weapons", defaultSound), "SFX/weapons");
        loaded.sounds.otherwise = loadOptionalSound(readSetting(ini, soundSection, "else", defaultSound), "SFX/else");

        loaded.vfx.shield = loadExplosionList(readSetting(ini, vfxSection, "shield", defaultShieldExplosions), "VFX/shield");
        loaded.vfx.weapon = loadExplosionList(readSetting(ini, vfxSection, "weapon", defaultWeaponExplosions), "VFX/weapon");
        loaded.vfx.otherwise = loadExplosionList(readSetting(ini, vfxSection, "else", defaultWeaponExplosions), "VFX/else");

        loaded.perks.shield.melee = loadPerkRequirement(readPerkSetting(ini, "ShieldMelee", "Melee"), "shield melee");
        loaded.perks.shield.spell = loadPerkRequirement(readPerkSetting(ini, "ShieldSpell", "Spell"), "shield spell");
        loaded.perks.shield.arrow = loadPerkRequirement(readPerkSetting(ini, "ShieldArrow", "Arrow"), "shield arrow");
        loaded.perks.nonShield.melee = loadPerkRequirement(readPerkSetting(ini, "NonShieldMelee", "Melee"), "non-shield melee");
        loaded.perks.nonShield.spell = loadPerkRequirement(readPerkSetting(ini, "NonShieldSpell", "Spell"), "non-shield spell");
        loaded.perks.nonShield.arrow = loadPerkRequirement(readPerkSetting(ini, "NonShieldArrow", "Arrow"), "non-shield arrow");
        loaded.perks.stagger = loadPerkRequirement(readSetting(ini, perkSection, "Stagger"), "AOE stagger");

        loaded.reflectionPerks.shield.spell = loadPerkRequirement(readSetting(ini, reflectionPerkSection, "ShieldSpell"), "shield spell reflection");
        loaded.reflectionPerks.shield.arrow = loadPerkRequirement(readSetting(ini, reflectionPerkSection, "ShieldArrow"), "shield arrow reflection");
        loaded.reflectionPerks.nonShield.spell = loadPerkRequirement(readSetting(ini, reflectionPerkSection, "NonShieldSpell"), "non-shield spell reflection");
        loaded.reflectionPerks.nonShield.arrow = loadPerkRequirement(readSetting(ini, reflectionPerkSection, "NonShieldArrow"), "non-shield arrow reflection");

        if (!loaded.core.parrySpell || !loaded.core.parryWindow || !loaded.core.staggerSpell ||
            !loaded.core.timeBlockBuffSpell) {
            SKSE::log::critical("[forms] One or more required [Core] forms could not be loaded");
            return false;
        }

        activeConfig = loaded;
        SKSE::log::info("[forms] Loaded {}", requirementsPath);
        return true;
    }

    const Config& Get() {
        return activeConfig;
    }
}
