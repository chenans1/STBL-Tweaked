#include "PCH.h"
#include "hooks.h"
#include "utils.h"
#include "settings.h"

void applyWindowDuration() {
    if (hooks::timedBlockWindowMGEF) {
        const auto config = settings::Get();
        hooks::timedBlockWindowMGEF->data.taperDuration = std::clamp(config.timedBlockWindow, 0.0f, 1.0f);
    }
}

void applyHitstopSpell(RE::Actor* attacker, float duration) {
    if (!attacker) {
        return;
    }
    SKSE::log::info("[applyHitstopSpell] applying hitstop spell to attacker");
    if (hooks::attackerHitStopMGEF) {
        hooks::attackerHitStopMGEF->data.taperDuration = std::clamp(duration, 0.0f, 1.0f);
    }
    utils::ApplySpell(attacker, attacker, hooks::attackerHitstopSpell);
}

//doing this allows for native dual wield block key compat. 
bool hooks::PC_NotifyAnimationGraph(RE::IAnimationGraphManagerHolder* a_this, const RE::BSFixedString& a_eventName) {
    const bool result = _PC_NotifyAnimationGraph(a_this, a_eventName);
    if (!result) return result;
    if (a_eventName == "blockStart") {
        //apply timedblock MGEF
        // SKSE::log::info("APPLYING TIMED BLOCK MGEF!");
        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            if (auto* caster = player->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant)) {
                applyWindowDuration();
                caster->CastSpellImmediate(hooks::timedBlockWindowSpell, true, player, 1.0f, false, 0.0f, player);
                return result;
            }
        }
    }
    return result;
}

void hooks::processHit(RE::Actor* actor, RE::HitData& hitData) {
    auto player =  RE::PlayerCharacter::GetSingleton();
    if (actor == player) {
        if (!hitData.flags.any(RE::HitData::Flag::kBlocked)) {
            // SKSE::log::info("[processHit] Non-blocked hitData");
            return _ProcessHit(actor, hitData);
        }
        if (auto* magicTarget = player->GetMagicTarget()) {
            if (magicTarget->HasMagicEffect(hooks::timedBlockWindowMGEF)) {
                const auto cfg = settings::Get();
                // utils::SendTBModEvent()
                if (cfg.preventAllDamage) {
                    hitData.totalDamage = 0.0f;
                    hitData.criticalDamageMult = 0.0f;
                    hitData.physicalDamage = 0.0f;
                    // SKSE::log::info("[processHit] Player has window MGEF, prevent all damage enabled = {}", hitData.percentBlocked);
                } else {
                    hitData.totalDamage *= cfg.additionalDamageReduction;
                    hitData.criticalDamageMult *= cfg.additionalDamageReduction;
                    hitData.physicalDamage *= cfg.additionalDamageReduction;
                }
                
                hitData.percentBlocked = 1.0f;
                hitData.stagger = 0.0f;
                if (cfg.AOEStaggerEnabled) {
                    utils::StaggerNearby(actor, cfg.AOEStaggerRadius);
                }

                //apply sfx/vfx
                if (cfg.applyTimedBlockVFX) {
                    player->PlaceObjectAtMe(hooks::timed_block_explosion, false);
                }

                if (cfg.applyTimedBlockSFX) {
                    utils::play_sound(actor, hooks::timedBlockSFX);
                }

                if (cfg.attackerHistopEnabled) {
                    auto* attacker = hitData.aggressor.get().get();
                    if (attacker) {
                        applyHitstopSpell(attacker, cfg.attackerSlowdownDuration);
                    }
                }
            }
        }
    }
    return _ProcessHit(actor, hitData);
}
//from asrak's magicutils: -0xC0 pointer offset
[[nodiscard]] inline RE::BShkbAnimationGraph* GraphFromCharacter(RE::hkbCharacter* chr) noexcept {
    if (!chr) {
        return nullptr;
    }
    //from BShkbAnimationGraph.h: hkbCharacter characterInstance;// 0C0
    return SKSE::stl::adjust_pointer<RE::BShkbAnimationGraph>(chr, -0xC0);
}

//hook to check if the actor has the hitstop mgef, if so, slow down animations.
void hooks::UpdateClip(RE::hkbClipGenerator* self, const RE::hkbContext& a_context, float a_timestep) {
    if (!self) {
        //log::warn("[hkbHook::Update] no self");
        return _originalUpdate(self, a_context, a_timestep);
    }
    auto* graph = GraphFromCharacter(a_context.character);
    if (!graph) {
        SKSE::log::warn("[UpdateClip]: No graph");
        return _originalUpdate(self, a_context, a_timestep);
    }
    
    auto* actor = graph->holder;
    if (!actor) {
        SKSE::log::warn("[UpdateClip]: No actor");
        return _originalUpdate(self, a_context, a_timestep);
    }
    const auto cfg = settings::Get();
    if (utils::hasMGEF(actor, hooks::attackerHitStopMGEF)) {
        if (cfg.log) {
            SKSE::log::info("[UpdateClip]: Setting Playback speed = {}", cfg.attackerSlowDownMult);
        }
        self->playbackSpeed = cfg.attackerSlowDownMult;
    }
    return _originalUpdate(self, a_context, a_timestep);
}