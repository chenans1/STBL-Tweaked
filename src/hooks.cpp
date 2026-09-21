#include "PCH.h"
#include "hooks.h"
#include "utils.h"
#include "settings.h"

//doing this allows for native dual wield block key compat. 
bool hooks::PC_NotifyAnimationGraph(RE::IAnimationGraphManagerHolder* a_this, const RE::BSFixedString& a_eventName) {
    const bool result = _PC_NotifyAnimationGraph(a_this, a_eventName);
    if (!result) return result;
    if (a_eventName == "blockStart") {
        //apply timedblock MGEF
        // SKSE::log::info("APPLYING TIMED BLOCK MGEF!");
        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            if (auto* caster = player->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant)) {
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
                // SKSE::log::info("[processHit] Player has window MGEF, blocking all damage. percentBlocked = {}", hitData.percentBlocked);
                if (cfg.preventAllDamage) {
                    hitData.totalDamage = 0.0f;
                    hitData.criticalDamageMult = 0.0f;
                    hitData.physicalDamage = 0.0f;
                } else {
                    hitData.totalDamage *= cfg.additionalDamageReduction;
                    hitData.criticalDamageMult *= cfg.additionalDamageReduction;
                    hitData.physicalDamage *= cfg.additionalDamageReduction;
                }
                
                hitData.percentBlocked = 1.0f;
                hitData.stagger = 0.0f;
                //apply sfx/vfx
                if (cfg.applyTimedBlockSFX) {
                    player->PlaceObjectAtMe(hooks::timed_block_explosion, false);
                }
                if (cfg.applyTimedBlockVFX) {
                    utils::play_sound(actor, hooks::timedBlockSFX);
                }
            }
        }
    }
    return _ProcessHit(actor, hitData);
}