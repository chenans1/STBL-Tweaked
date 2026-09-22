#include "PCH.h"
#include "form_config.h"

#include <SimpleIni.h>

#include <charconv>
#include <cstdint>
#include <filesystem>
#include <string>

namespace form_config {
    namespace {
        constexpr auto coreSection = "Core";
        constexpr auto perkSection = "PerkRequirements";

        constexpr auto defaultParrySpell = "SimpleTimedBlock.esp ~ 0x802";
        constexpr auto defaultParryWindow = "SimpleTimedBlock.esp ~ 0x801";
        constexpr auto defaultStaggerSpell = "SimpleTimedBlock.esp ~ 0x803";
        constexpr auto defaultExplosion = "SimpleTimedBlock.esp ~ 0x805";
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

        PerkRequirement loadPerkRequirement(std::string_view setting, std::string_view context) {
            setting = trim(setting);
            if (setting.empty()) {
                SKSE::log::info("[perk requirement] {} timed blocking is unrestricted", context);
                return {};
            }

            auto* perk = loadForm<RE::BGSPerk>(setting, context);
            if (!perk) {
                SKSE::log::error(
                    "[perk requirement] Invalid {} requirement; no perk will be required",
                    context);
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
            ini.SetValue(coreSection, "TimedBlockExplosion", defaultExplosion);
            ini.SetValue(coreSection, "TimedBlockSound", defaultSound);
            ini.SetValue(perkSection, "Melee", "");
            ini.SetValue(perkSection, "Spell", "");
            ini.SetValue(perkSection, "Projectile", "");

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
        loaded.core.timedBlockExplosion = loadForm<RE::BGSExplosion>(readSetting(ini, coreSection, "TimedBlockExplosion", defaultExplosion), "Core/TimedBlockExplosion");
        loaded.core.timedBlockSound = loadForm<RE::BGSSoundDescriptorForm>(readSetting(ini, coreSection, "TimedBlockSound", defaultSound), "Core/TimedBlockSound");

        loaded.perks.melee = loadPerkRequirement(readSetting(ini, perkSection, "Melee"), "melee");
        loaded.perks.spell = loadPerkRequirement(readSetting(ini, perkSection, "Spell"), "spell");
        loaded.perks.projectile = loadPerkRequirement(readSetting(ini, perkSection, "Projectile"), "projectile");

        if (!loaded.core.parrySpell || !loaded.core.parryWindow || !loaded.core.staggerSpell ||
            !loaded.core.timedBlockExplosion || !loaded.core.timedBlockSound) {
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
