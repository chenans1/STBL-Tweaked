#pragma once

class hooks {
    public:
        static void Install() {
            SKSE::log::info("Installing Hooks...");
            SKSE::log::info("Installing PlayerCharacter animation graph hook...");

            REL::Relocation<uintptr_t> PlayerCharacter_IAnimationGraphManagerHolderVtbl{RE::VTABLE_PlayerCharacter[3]};
            _PC_NotifyAnimationGraph = PlayerCharacter_IAnimationGraphManagerHolderVtbl.write_vfunc(0x1, PC_NotifyAnimationGraph);

            SKSE::log::info("PlayerCharacter animation graph hook installed successfully");
        }

        static bool LoadForms() {
            auto* dataHandler = RE::TESDataHandler::GetSingleton();
            timedBlockWindowSpell = dataHandler->LookupForm<RE::SpellItem>(0x802, "SimpleTimedBlock.esp");
            timedBlockWindowMGEF = dataHandler->LookupForm<RE::EffectSetting>(0x801, "SimpleTimedBlock.esp");
            timedBlockStaggerSpell = dataHandler->LookupForm<RE::SpellItem>(0x803, "SimpleTimedBlock.esp");
            timeBlockBuffSpell = dataHandler->LookupForm<RE::SpellItem>(0x80B, "SimpleTimedBlock.esp");
            timed_block_explosion = dataHandler->LookupForm<RE::BGSExplosion>(0x805, "SimpleTimedBlock.esp");
            timed_block_counter_glob = dataHandler->LookupForm<RE::TESGlobal>(0x80E, "SimpleTimedBlock.esp");

            if (!timedBlockWindowSpell || !timedBlockWindowMGEF || !timedBlockStaggerSpell || !timeBlockBuffSpell) {
                SKSE::log::error("Failed to load spell forms: timedBlockWindowSpell={}, timedBlockWindowMGEF={}, timedBlockStaggerSpell={}, timeBlockBuffSpell={}", 
                    static_cast<void*>(timedBlockWindowSpell), static_cast<void*>(timedBlockWindowMGEF), static_cast<void*>(timedBlockStaggerSpell), static_cast<void*>(timeBlockBuffSpell));
                return false;
            }
            SKSE::log::info("Correctly loaded spell forms: timedBlockWindowSpell={}, timedBlockWindowMGEF={}, timedBlockStaggerSpell={}, timeBlockBuffSpell={}", 
                    static_cast<void*>(timedBlockWindowSpell), static_cast<void*>(timedBlockWindowMGEF), static_cast<void*>(timedBlockStaggerSpell), static_cast<void*>(timeBlockBuffSpell));
            return true;
        }

    private:
        static inline RE::SpellItem* timedBlockWindowSpell = nullptr;
        static inline RE::EffectSetting* timedBlockWindowMGEF = nullptr;
        static inline RE::SpellItem* timedBlockStaggerSpell = nullptr;
        static inline RE::SpellItem* timeBlockBuffSpell = nullptr;
        static inline RE::BGSExplosion* timed_block_explosion = nullptr;
        static inline RE::TESGlobal* timed_block_counter_glob = nullptr;

        static bool PC_NotifyAnimationGraph(RE::IAnimationGraphManagerHolder* a_this, const RE::BSFixedString& a_eventName);
        inline static REL::Relocation<decltype(PC_NotifyAnimationGraph)> _PC_NotifyAnimationGraph;

};