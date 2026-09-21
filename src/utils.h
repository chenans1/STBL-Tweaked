#pragma once
#include "hooks.h"


namespace utils {
    inline static bool ApplySpell(RE::Actor* a_caster, RE::Actor* a_target, RE::SpellItem* a_spell) {
        if (!a_caster || !a_target || !a_spell) {
            return false;
        }

        if (auto* caster = a_caster->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant)) {
            caster->CastSpellImmediate(a_spell, false, a_target, 1.0f, false, 0.0f, a_caster);
            // if (const auto cfg = settings::Get().log) { 
            SKSE::log::info("[ApplySpell]: Cast spell={} on target={} caster={}",  
                static_cast<void*>(a_spell), static_cast<void*>(a_target), static_cast<void*>(a_caster));
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

    inline static void StaggerNearby(RE::Actor* a_defender, RE::Actor* a_attacker, float radius) {
        auto* cell = a_defender ? a_defender->GetParentCell() : nullptr;
        if (!cell || !cell->IsAttached() || radius <= 0.0f) {
            return;
        }
        radius = std::min(radius, 4095.0f);
        cell->ForEachReferenceInRange(a_defender->GetPosition(), radius, [&](RE::TESObjectREFR* ref){
            auto* actor = ref ? ref->As<RE::Actor>() : nullptr;
            if (!actor ||
                // actor->IsDead() ||
                actor->IsDisabled() ||
                !actor->Is3DLoaded()) {
                return RE::BSContainer::ForEachResult::kContinue;
            }
            SKSE::log::info( "[Utils] Staggering={:08X} ", actor ? actor->GetFormID() : 0);
            ApplySpell(a_defender, actor, hooks::timedBlockStaggerSpell);
            return RE::BSContainer::ForEachResult::kContinue;
        });
    }   
}