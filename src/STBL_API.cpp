#include "PCH.h"
#include "STBL_API.h"
#include "settings.h"
#include "utils.h"
#include "hooks.h"

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

        const auto hasRequiredPerk = checkFullyBlockedRequirement(request.attackType, request.defender);

        const auto config = settings::Get();
        if (config.preventAllDamage && hasRequiredPerk) {
            result.outcome = TimedBlockOutcome::FullyBlocked;
            result.damageMultiplier = 0.0F;
        } else {
            result.outcome = TimedBlockOutcome::Reduced;
            result.damageMultiplier = std::clamp(config.additionalDamageReduction, 0.0f, 1.0f);
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
