#include "PCH.h"
#include "settings.h"
#include <SimpleIni.h>
#include <SKSEMenuFramework.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <mutex>
#include <tuple>
#include <utility>


namespace settings {
    constexpr auto iniPath = "Data/SKSE/Plugins/STBLtweaked.ini";
    constexpr auto general = "General";

    std::mutex configMutex;
    settings::config activeConfig{};
    bool unsavedChanges = false;

    float readFloat(const CSimpleIniA& ini, const char* section, const char* key, const float fallback) {
        const float value = static_cast<float>(ini.GetDoubleValue(section, key, fallback));
        return std::isfinite(value) ? value : fallback;
    }

    bool readValue(const CSimpleIniA& ini, const char* section, const char* key, const bool fallback) {
        return ini.GetBoolValue(section, key, fallback);
    }

    float readValue(const CSimpleIniA& ini, const char* section, const char* key, const float fallback) {
        return readFloat(ini, section, key, fallback);
    }

    void writeValue(CSimpleIniA& ini, const char* section, const char* key, const bool value) {
        ini.SetBoolValue(section, key, value);
    }

    void writeValue(CSimpleIniA& ini, const char* section, const char* key, const float value) {
        ini.SetDoubleValue(section, key, value, nullptr, false);
    }

    template <class T>
    void loadSetting(const CSimpleIniA& ini, settings::config& config, const setting_definition<T>& definition) {
        auto& value = config.*(definition.member);
        value = readValue(ini, general, definition.key, value);
    }

    template <class T>
    void saveSetting(CSimpleIniA& ini, const settings::config& config, const setting_definition<T>& definition) {
        writeValue(ini, general, definition.key, config.*(definition.member));
    }

    template <class Function>
    void forEachSetting(Function&& function) {
        std::apply(
            [&function](const auto&... definition) {
                (function(definition), ...);
            },
            setting_definitions);
    }

    config Get() {
        std::scoped_lock lock(configMutex);
        return activeConfig;
    }

    void Set(const config& value) {
        std::scoped_lock lock(configMutex);
        activeConfig = value;
    }

    void Load() {
        config loaded{};
        CSimpleIniA ini;
        ini.SetUnicode(false);

        std::error_code ec;
        if (!std::filesystem::exists(iniPath, ec) && !ec) {
            Set(loaded);
            if (Save()) {
                SKSE::log::info("[settings] Created {} with defaults", iniPath);
            }
            return;
        }
        if (ec || ini.LoadFile(iniPath) < 0) {
            SKSE::log::error("[settings] Could not read {}; using defaults without replacing it", iniPath);
            Set(loaded);
            return;
        }

        forEachSetting([&ini, &loaded](const auto& definition) {
            loadSetting(ini, loaded, definition);
        });

        Set(loaded);
        SKSE::log::info("[settings] Loaded {}", iniPath);
    }

    bool Save() {
        const config current = Get();
        CSimpleIniA ini;
        ini.SetUnicode(false);
        (void)ini.LoadFile(iniPath);  // Preserve unrecognized keys from newer versions.

        forEachSetting([&ini, &current](const auto& definition) {
            saveSetting(ini, current, definition);
        });

        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(iniPath).parent_path(), ec);
        if (ec || ini.SaveFile(iniPath) < 0) {
            SKSE::log::error("[settings] Could not save {}: {}", iniPath, ec.message());
            return false;
        }

        SKSE::log::info("[settings] Saved {}", iniPath);
        return true;
    }

    static void FinishMenuPage(const config& current, bool changed) {
        if (changed) {
            Set(current);  // Apply slider and checkbox changes immediately.
            unsavedChanges = true;
        }
        ImGuiMCP::Separator();
        if (ImGuiMCP::Button("Save")) {
            if (Save()) {
                unsavedChanges = false;
            }
        }
        ImGuiMCP::SameLine();
        if (ImGuiMCP::Button("Revert")) {
            Load();
            unsavedChanges = false;
        }
        if (unsavedChanges) {
            ImGuiMCP::TextUnformatted("Unsaved changes");
        }
    }

    void __stdcall RenderMenuPage() {
        config current = Get();
        bool changed = false;
        changed |= ImGuiMCP::SliderFloat("Timed Block Window", &current.timedBlockWindow, 0.0f, 1.0f, "%.2f");

        changed |= ImGuiMCP::Checkbox("Enable Sound Effects", &current.applyTimedBlockSFX);
        changed |= ImGuiMCP::Checkbox("Enable Visual Effects", &current.applyTimedBlockVFX);
        changed |= ImGuiMCP::Checkbox("Prevent All Melee Damage", &current.preventAllDamage);
        changed |= ImGuiMCP::Checkbox("Prevent All Spell Damage", &current.preventAllDamageSpells);
        changed |= ImGuiMCP::Checkbox("Prevent All Arrow Damage", &current.preventAllDamageArrows);
        changed |= ImGuiMCP::SliderFloat("Timed Block Damage Mult", &current.additionalDamageReduction, 0.0f, 1.0f, "%.2f");
        changed |= ImGuiMCP::SliderFloat("Timed Block Arrow Damage Mult", &current.additionalDamageReductionArrow, 0.0f, 1.0f, "%.2f");
        changed |= ImGuiMCP::SliderFloat("Timed Block Spell Damage Mult", &current.additionalDamageReductionSpell, 0.0f, 1.0f, "%.2f");

        changed |= ImGuiMCP::Checkbox("AOE Stagger Enabled (legacy)", &current.AOEStaggerEnabled);
        changed |= ImGuiMCP::SliderFloat("AOE Stagger Radius", &current.AOEStaggerRadius, 0.0f, 2048.0f, "%1.0f");

        changed |= ImGuiMCP::Checkbox("Attacker Histop Enabled", &current.attackerHistopEnabled);
        changed |= ImGuiMCP::SliderFloat("Attacker Animation Slowdown", &current.attackerSlowDownMult, 0.0f, 1.0f, "%.2f");
        changed |= ImGuiMCP::SliderFloat("Attacker Animation Slowdown Duration", &current.attackerSlowdownDuration, 0.0f, 1.0f, "%.2f");
        ImGuiMCP::Separator();
        changed |= ImGuiMCP::Checkbox("Enable diagnostic logging", &current.log);
        FinishMenuPage(current, changed);
    }

    void RegisterMenu() { 
        if (!SKSEMenuFramework::IsInstalled()) {
            SKSE::log::info("[settings] SKSE Menu Framework is not installed; INI settings remain available");
            return;
        }

        menuFramework = GetModuleHandleW(L"SKSEMenuFramework.dll");
        if (!menuFramework) {
            SKSE::log::warn("[settings] SKSE Menu Framework DLL exists but is not loaded");
            return;
        }
        SKSEMenuFramework::SetSection("Simple Timed Block Tweaked");
        SKSEMenuFramework::AddSectionItem("Settings", RenderMenuPage);
        SKSE::log::info("[settings] Registered SKSE Menu Framework page");
    }
}
