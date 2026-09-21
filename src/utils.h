#pragma once

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
}