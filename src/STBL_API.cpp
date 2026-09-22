#include "PCH.h"
#include "STBL_API.h"
#include "settings.h"
#include "utils.h"
#include "hooks.h"

namespace {
    using namespace STBL_API;

    [[nodiscard]] bool checkFullyBlockedRequirement(AttackType attackType, const RE::Actor* actor) {
        const auto& perks = form_config::Get().perks;

        switch (attackType) {
            case STBL_API::AttackType::Melee:
                return perks.melee.IsMetBy(actor);

            case STBL_API::AttackType::Spell:
                return perks.spell.IsMetBy(actor);

            case STBL_API::AttackType::Arrow:
                return perks.arrow.IsMetBy(actor);

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

    TimedBlockResult TryTrigger(const TimedBlockRequest& request) {
        TimedBlockResult result{};
        if (!request.defender) {
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
        applyTimedBlockEffects(request.defender, request.attacker, request.attackType);
        return result;
    }

    class TimedBlockAPI final : public STBL {
        public:
            STBL_API::TimedBlockResult TryTriggerTimedBlock(const STBL_API::TimedBlockRequest& request) noexcept override {
                return TryTrigger(request);
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
