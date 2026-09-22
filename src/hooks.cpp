#include "PCH.h"
#include "hooks.h"
#include "STBL_API.h"
#include "utils.h"
#include "settings.h"

void applyWindowDuration() {
    if (hooks::timedBlockWindowMGEF) {
        const auto config = settings::Get();
        hooks::timedBlockWindowMGEF->data.taperDuration = std::clamp(config.timedBlockWindow, 0.0f, 1.0f);
    }
}

//doing this allows for native dual wield block key compat. 
bool hooks::PC_NotifyAnimationGraph(RE::IAnimationGraphManagerHolder* a_this, const RE::BSFixedString& a_eventName) {
    static const RE::BSFixedString blockStartEvent{ "blockStart" }; //should optimize it a bit, point check instead of str compare?

    const bool result = _PC_NotifyAnimationGraph(a_this, a_eventName);
    if (!result) return result;
    if (a_eventName == blockStartEvent) {
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
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (actor != player || !hitData.flags.any(RE::HitData::Flag::kBlocked)) {
        return _ProcessHit(actor, hitData);
    }

    static auto* timedBlockAPI = STBL_API::RequestInterface();
    if (!timedBlockAPI) {
        SKSE::log::error("[processHit] Could not acquire the local STBL API");
        return _ProcessHit(actor, hitData);
    }

    auto* attacker = hitData.aggressor ? hitData.aggressor.get().get() : nullptr;
    const auto result = timedBlockAPI->TryTriggerTimedBlock({STBL_API::AttackType::Melee, attacker, player});

    if (result.Triggered()) {
        hitData.totalDamage *= result.damageMultiplier;
        hitData.criticalDamageMult *= result.damageMultiplier;
        hitData.physicalDamage *= result.damageMultiplier;
        hitData.percentBlocked = 1.0f;
        hitData.stagger = 0.0f;

        // if (result.FullyBlocked()) {
        //     hitData.totalDamage = 0.0f;
        //     hitData.criticalDamageMult = 0.0f;
        //     hitData.physicalDamage = 0.0f;
        // }
    }

    return _ProcessHit(actor, hitData);
}

// This modifies only the animation routine's local delta time. It does not
void applyHitstopToAnimationDelta(RE::Actor* actor, float& deltaTime) {
    const auto config = settings::Get();
    if (!config.attackerHistopEnabled || !utils::hasMGEF(actor, hooks::attackerHitStopMGEF)) {
        return;
    }
    
    deltaTime *= std::clamp(config.attackerSlowDownMult, 0.0f, 1.0f);
}

void hooks::UpdateAnimation(RE::Actor* a_this, float a_deltaTime) {
    applyHitstopToAnimationDelta(a_this, a_deltaTime);
    _originalUpdate(a_this, a_deltaTime);
}
