#pragma once
#include "hooks.h"
#include "settings.h"
#include "extern/PerkEntryPointExtenderAPI.h"

namespace utils {
    inline static bool ApplySpell(RE::Actor* a_caster, RE::Actor* a_target, RE::SpellItem* a_spell) {
        if (!a_caster || !a_target || !a_spell) {
            return false;
        }

        if (auto* caster = a_caster->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant)) {
            caster->CastSpellImmediate(a_spell, false, a_target, 1.0f, false, 0.0f, a_caster);
            // if (const auto cfg = settings::Get().log) { 
            //     SKSE::log::info("[ApplySpell]: Cast spell={:08X} on target={:08X} caster={:08X}",  
            //         a_spell->GetFormID(), a_target ? a_target->GetFormID() : 0, a_caster ? a_caster->GetFormID() : 0);
            // }
            return true;
        }
        return false;
    }

    inline static void SendTBModEvent(RE::Actor* a_defender, RE::Actor* a_attacker) {
        const auto attacker_ID = a_attacker ? a_attacker->GetFormID() : 0x0;
        const auto level = a_attacker ? a_attacker->GetLevel() : 0x0;
        const auto level_arg = static_cast<float>(level);

        const SKSE::ModCallbackEvent modEvent{ .eventName = RE::BSFixedString("STBL_OnTimedBlockDefender"), .strArg = RE::BSFixedString(std::to_string(attacker_ID)), .numArg = level_arg, .sender = a_defender };
        const SKSE::ModCallbackEvent modEventATK{ .eventName = RE::BSFixedString("STBL_OnTimedBlockAttacker"), .strArg = RE::BSFixedString(), .numArg = level_arg, .sender = a_attacker };
        SKSE::GetModCallbackEventSource()->SendEvent(&modEvent);
        SKSE::GetModCallbackEventSource()->SendEvent(&modEventATK);
    }

    // plays sound if it exists on the actor. adapted from DTRY's payload and spell hotbar2
    inline static RE::BSSoundHandle play_sound(RE::Actor* actor, RE::BGSSoundDescriptorForm* sound_form) {
        RE::BSSoundHandle handle;
        handle.soundID = static_cast<uint32_t>(-1);
        handle.assumeSuccess = false;
        handle.state = RE::BSSoundHandle::AssumedState::kInitialized;
        if (!actor || !sound_form) {
            return handle;
        }
        /*assumption: not doing this causes the game to crash if the audio engine is paused,
        eg: always active, mute on focus loss mod installed, tab out during casting*/
        auto audio_manager = RE::BSAudioManager::GetSingleton();
        if (audio_manager) {
            // 16 is used by payload & spellhotbar 2
            audio_manager->BuildSoundDataFromDescriptor(handle, sound_form, 16);
            if (handle.SetPosition(actor->data.location)) {
                handle.SetObjectToFollow(actor->Get3D());
                handle.Play();
            }
        }
        return handle;
    }

    inline static bool isMelee(RE::Actor* actor) {
       if (!actor) {
            return false;
        }
        int rightHand = 0;
        actor->GetGraphVariableInt("iRightHandType", rightHand);
        return (rightHand <= 6);
    }

    inline void applyHitstopSpell(RE::Actor* attacker, RE::Actor* blocker, float duration) {
        if (!attacker) {
            return;
        }
        // SKSE::log::info("[applyHitstopSpell] applying hitstop spell to attacker");
        if (hooks::attackerHitStopMGEF) {
            hooks::attackerHitStopMGEF->data.taperDuration = std::clamp(duration, 0.0f, 1.0f);
        }
        utils::ApplySpell(blocker, attacker, hooks::attackerHitstopSpell);
    }

    inline static void ApplySpellRadius(RE::Actor* a_defender, float radius, RE::SpellItem* a_spell, RE::Actor* excluded = nullptr) {
        auto* cell = a_defender ? a_defender->GetParentCell() : nullptr;
        if (!cell || !cell->IsAttached() || radius <= 0.0f) {
            return;
        }
        radius = std::min(radius, 4095.0f);
        cell->ForEachReferenceInRange(a_defender->GetPosition(), radius, [&](RE::TESObjectREFR* ref){
            auto* actor = ref ? ref->As<RE::Actor>() : nullptr;
            if (!actor || actor->IsDisabled() || !actor->Is3DLoaded() || actor == a_defender || (excluded && actor == excluded)) {
                return RE::BSContainer::ForEachResult::kContinue;
            }
            SKSE::log::info("[Utils] Staggering={:08X} ", actor ? actor->GetFormID() : 0);

            if (a_spell == hooks::attackerHitstopSpell) {
                const auto cfg = settings::Get();
                applyHitstopSpell(actor, a_defender, isMelee(actor) ? cfg.attackerSlowdownDuration : cfg.rangedHitStopDuration);
            } else {
                ApplySpell(a_defender, actor, a_spell);
            }
            return RE::BSContainer::ForEachResult::kContinue;
        });
    }

    inline static bool hasMGEF(RE::Actor* actor, RE::EffectSetting* a_effect) {
        if (!actor || !a_effect) {
            return false;
        }
        auto* magicTarget = actor->GetMagicTarget();
        if (!magicTarget) {
            return false;
        }

        if (magicTarget->HasMagicEffect(a_effect)) {
            return true;
        }
        return false;
    }   

    inline static float safelyAddWithCap(const float value, const float increment, const float cap) {
        return std::min(value + increment, cap);
    }

    inline void incrementGlobalTBCounter(){
        if (hooks::timed_block_counter_glob) {
            hooks::timed_block_counter_glob->value = safelyAddWithCap(hooks::timed_block_counter_glob->value, 1.0f, 5000.0f);
        }
    }

    inline static RE::Effect* FindEffect(RE::SpellItem* a_spell, RE::EffectSetting* a_mgef) {
        if (!a_spell || !a_mgef) {
            return nullptr;
        }
        for (auto* effect : a_spell->effects) {
            if (effect && effect->baseEffect == a_mgef) {
                return effect;
            }
        }
        return nullptr;
    }

    //overrides the magnitude of SimpleTimedBlockTweaked.esp~0x809's 0x808 MGEF (first instance). 0.0f means disabled.
    inline static void overrideSTBLStagger(float overrideMag) {
        if (overrideMag <= 0.0f) {
            return;
        }
        auto* effect = FindEffect(hooks::STBLTweakedStaggerSpell, hooks::STBLTweakedStaggerMGEF);
        effect->SetMagnitude(overrideMag);
    }
    
    // Handles Perk Entry Point Extender.
    // GROUP__STBLReflectionChanceArrow
    // GROUP__STBLReflectionChanceSpell
    // GROUP__STBLReflectionCostArrow
    // GROUP__STBLReflectionCostSpell
    inline static float handlePEPE(RE::Actor* actor, std::string_view category, bool fromMenu = false) {
        float multiplier = 1.0f;
        const auto result = RE::HandleEntryPoint(RE::PerkEntryPoint::kModTelekinesisDistance, actor, multiplier, category);
        if (settings::Get().log && !fromMenu) {
            SKSE::log::info("[PEPE] category={} result={} value={}", category, static_cast<int>(result), multiplier);
        }
        return (std::max)(0.0f, multiplier);
    }
}
