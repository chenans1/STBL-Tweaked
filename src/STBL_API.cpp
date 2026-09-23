#include "PCH.h"
#include "STBL_API.h"
#include "settings.h"
#include "utils.h"
#include "hooks.h"
#include <random>

namespace {
    using namespace STBL_API;

    [[nodiscard]] bool isSupportedAttackType(AttackType attackType) {
        switch (attackType) {
            case AttackType::Melee:
            case AttackType::Spell:
            case AttackType::Arrow:
                return true;
            default:
                return false;
        }
    }

    [[nodiscard]] bool isUsingShield(const RE::Actor* actor) {
        if (!actor) {
            return false;
        }

        const auto* leftHand = actor->GetEquippedObject(true);
        const auto* armor = leftHand ? leftHand->As<RE::TESObjectARMO>() : nullptr;
        return armor && armor->IsShield();
    }

    static bool evalutateInterruption(RE::Actor* blocker, RE::Actor* attacker, AttackType attackType, const settings::config& cfg) {
        if (!attacker || !blocker) return false;
        if (attackType==AttackType::Melee && !cfg.meleeInterruptEnabled) return false;
        if (attackType!=AttackType::Melee && !cfg.rangedInterruptEnabled) return false;
        if (!utils::passesInterruptConditions(blocker, attacker)) {
            if (cfg.log) {SKSE::log::info("[evalutateInterruption] blocker={:08X} attacker={:08X} does not pass conditions", 
                    blocker ? blocker->GetFormID() : 0, attacker ? attacker->GetFormID() : 0);}
            return false;
        }
        const float blockSkill = (std::max)(0.0f, blocker->AsActorValueOwner()->GetActorValue(RE::ActorValue::kBlock));
        float chance = cfg.baseInterruptChance * (1.0f + blockSkill * cfg.blockSkillFactor/100.0f);
        if (isUsingShield(blocker)) chance *= cfg.shieldInterruptMult;
        if (attackType != AttackType::Melee) chance *= cfg.rangedInterruptMult;
        chance = std::clamp(chance, 0.0f, std::clamp(cfg.maxInterruptChance, 0.0f, 1.0f));
        thread_local std::mt19937 generator{ std::random_device{}() };
        std::uniform_real_distribution<float> distribution{ 0.0f, 1.0f };
        const float dist = distribution(generator);
        if (dist >= chance){ 
            if (cfg.log) {SKSE::log::info("[evalutateInterruption] blocker={:08X} chance failed dist={} chance={}", 
                    blocker ? blocker->GetFormID() : 0, dist, chance);}
            return false;}
        
        if (cfg.interruptStagger) {
            utils::overrideSTBLStagger(cfg.staggerMagnitudeOverride);
            utils::ApplySpell(blocker, attacker, hooks::STBLTweakedStaggerSpell);
            return true;
        } else {
            //technically not needed for the spells, the conditions will apply.
            // if (!utils::passesInterruptConditions(blocker, attacker)) {
            //     return false;
            // }
            switch (attackType) {
                case STBL_API::AttackType::Melee:
                    attacker->NotifyAnimationGraph("recoilLargeStart");
                    return true;
                case STBL_API::AttackType::Spell:
                    attacker->NotifyAnimationGraph("InterruptCast");
                    return true;
                case STBL_API::AttackType::Arrow:
                    attacker->NotifyAnimationGraph("recoilLargeStart");
                    return true;
                default:
                    return false;
            }
        }
    }   

    [[nodiscard]] bool checkFullyBlockedRequirement(AttackType attackType, const RE::Actor* actor) {
        const auto& perks = form_config::Get().perks;
        const auto& attackPerks = isUsingShield(actor) ? perks.shield : perks.nonShield;

        switch (attackType) {
            case STBL_API::AttackType::Melee:
                return attackPerks.melee.IsMetBy(actor);

            case STBL_API::AttackType::Spell:
                return attackPerks.spell.IsMetBy(actor);

            case STBL_API::AttackType::Arrow:
                return attackPerks.arrow.IsMetBy(actor);

            default:
                return false;
        }
    }

    [[nodiscard]] bool checkReflectionRequirement(AttackType attackType, RE::Actor* defender, const settings::config& config) {
        if (!defender) return false;
        const auto& requirements = form_config::Get().reflectionPerks;
        const auto& equippedRequirements = isUsingShield(defender) ? requirements.shield : requirements.nonShield;
        switch (attackType) {
            case AttackType::Spell:
                return config.reflectSpells && equippedRequirements.spell.IsMetBy(defender);

            case AttackType::Arrow:
                return config.reflectArrows && equippedRequirements.arrow.IsMetBy(defender);

            default:
                return false;
        }
    }

    // Rolls reflection after the setting and equipment-specific perk gate pass.
    [[nodiscard]] bool handleReflection(AttackType attackType, RE::Actor* defender, const settings::config& config) {
        if (!checkReflectionRequirement(attackType, defender, config)) {
            return false;
        }

        const float blockSkill = (std::max)(0.0f, defender->AsActorValueOwner()->GetActorValue(RE::ActorValue::kBlock));
        float chance = config.baseReflectionChance * (1.0f + blockSkill * config.reflectionSkillFactor/100.0f);
        switch (attackType) {
            case AttackType::Spell:
                chance *= config.spellReflectionMult;
                chance *= utils::handlePEPE(defender, "STBLReflectionChanceSpell");
                break;
            case AttackType::Arrow:
                chance *= config.arrowReflectionMult;
                chance *= utils::handlePEPE(defender, "STBLReflectionChanceArrow");
                break;
            default:
                return false;
        }

        chance = std::clamp(chance, 0.0f, 1.0f);
        thread_local std::mt19937 generator{ std::random_device{}() };
        std::uniform_real_distribution<float> distribution{ 0.0f, 1.0f };
        const float dist = distribution(generator);
        const bool succeeded = dist < chance;
        if (config.log) {
            SKSE::log::info("[handleReflection] blocker={:08X} succeeded={} roll={} chance={}",
                defender->GetFormID(), succeeded, dist, chance);
        }
        return succeeded;
    }

    // Returns only the extra reflection portion. The consumer remains
    // responsible for adding this to its normal block resource cost.
    [[nodiscard]] float getReflectionCostMultiplier(AttackType attackType, RE::Actor* defender, const settings::config& config) {
        if (!defender) {
            return 0.0f;
        }

        switch (attackType) {
            case AttackType::Spell:
                return (std::max)(0.0f, config.spellCostReflectionMult) *
                       utils::handlePEPE(defender, "STBLReflectionCostSpell");
            case AttackType::Arrow:
                return (std::max)(0.0f, config.arrowReflectionCostMult) *
                       utils::handlePEPE(defender, "STBLReflectionCostArrow");
            default:
                return 0.0f;
        }
    }

    struct DamageSettings {
        bool preventAllDamage = false;
        float additionalDamageMultiplier = 1.0f;
    };

    [[nodiscard]] DamageSettings getDamageSettings(AttackType attackType, const settings::config& config) {
        switch (attackType) {
            case AttackType::Melee:
                return { config.preventAllDamage, config.additionalDamageReduction };
            case AttackType::Spell:
                return { config.preventAllDamageSpells, config.additionalDamageReductionSpell };
            case AttackType::Arrow:
                return { config.preventAllDamageArrows, config.additionalDamageReductionArrow };
            default:
                return {};
        }
    }

    [[nodiscard]] RE::SpellItem* getAttackerSpell(AttackType attackType) {
        switch (attackType) {
            case AttackType::Melee:
                return hooks::timedBlockMeleeAttackerSpell;
            case AttackType::Spell:
                return hooks::timedBlockSpellAttackerSpell;
            case AttackType::Arrow:
                return hooks::timedBlockArrowAttackerSpell;
            default:
                return nullptr;
        }
    }

    void applyTimedBlockEffects(RE::Actor* defender, RE::Actor* attacker, AttackType attackType) {
        if (!defender) {
            return;
        }

        utils::incrementGlobalTBCounter();
        utils::ApplySpell(defender, defender, hooks::timeBlockBuffSpell);

        const auto cfg = settings::Get();
        if (cfg.applyTimedBlockVFX) {
            defender->PlaceObjectAtMe(hooks::timed_block_explosion, false);
        }

        if (cfg.applyTimedBlockSFX) {
            utils::play_sound(defender, hooks::timedBlockSFX);
        }

        const bool hasStaggerPerk = form_config::Get().perks.stagger.IsMetBy(defender);
        if (cfg.AOEStaggerEnabled && hasStaggerPerk) {
            utils::StaggerNearby(defender, cfg.AOEStaggerRadius);
        }

        if (!attacker) {
            return;
        }

        if (cfg.enableInterrupt) {
            evalutateInterruption(defender, attacker, attackType, cfg);
        }

        utils::ApplySpell(defender, attacker, getAttackerSpell(attackType));

        if (cfg.attackerHistopEnabled) {
            utils::applyHitstopSpell(attacker, defender, cfg.attackerSlowdownDuration);
        }
        utils::SendTBModEvent(defender, attacker);
    }

    TimedBlockResult evaluateTimedBlock(const TimedBlockRequest& request) {
        TimedBlockResult result{};
        if (!request.defender || !isSupportedAttackType(request.attackType)) {
            return result;
        }

        if (!utils::hasMGEF(request.defender, hooks::timedBlockWindowMGEF)) {
            return result;
        }

        const auto config = settings::Get();
        const auto damageSettings = getDamageSettings(request.attackType, config);
        const bool hasRequiredPerk = checkFullyBlockedRequirement(request.attackType, request.defender);

        if (damageSettings.preventAllDamage && hasRequiredPerk) {
            result.outcome = TimedBlockOutcome::FullyBlocked;
            result.damageMultiplier = 0.0F;
        } else {
            result.outcome = TimedBlockOutcome::Reduced;
            result.damageMultiplier = std::clamp(damageSettings.additionalDamageMultiplier, 0.0f, 1.0f);
        }

        result.reflectProjectile = handleReflection(request.attackType, request.defender, config);
        if (result.reflectProjectile) {
            result.reflectionCostMultiplier = getReflectionCostMultiplier(request.attackType, request.defender, config);
        }
        return result;
    }

    class TimedBlockAPI final : public STBL {
        public:
            STBL_API::TimedBlockResult CanTimedBlock(const STBL_API::TimedBlockRequest& request) noexcept override {
                return evaluateTimedBlock(request);
            }

            bool TriggerTimedBlock(const STBL_API::TimedBlockRequest& request) noexcept override {
                if (!evaluateTimedBlock(request).Triggered()) {
                    return false;
                }

                applyTimedBlockEffects(request.defender, request.attacker, request.attackType);
                return true;
            }

            STBL_API::TimedBlockResult TryTriggerTimedBlock(const STBL_API::TimedBlockRequest& request) noexcept override {
                auto result = evaluateTimedBlock(request);
                if (result.Triggered()) {
                    applyTimedBlockEffects(request.defender, request.attacker, request.attackType);
                }
                return result;
            }
    };

    TimedBlockAPI api;

}

extern "C" __declspec(dllexport)
void* RequestPluginAPI(const STBL_API::InterfaceVersion version) noexcept {
    switch (version) {
        case STBL_API::InterfaceVersion::V1:
            return &api;
        default:
            return nullptr;
    }
}
