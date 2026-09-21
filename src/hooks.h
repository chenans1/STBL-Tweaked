#pragma once

class hooks {
    public:
        static inline RE::SpellItem* timedBlockWindowSpell = nullptr;
        static inline RE::EffectSetting* timedBlockWindowMGEF = nullptr;
        static inline RE::SpellItem* timedBlockStaggerSpell = nullptr;
        static inline RE::SpellItem* timeBlockBuffSpell = nullptr;
        static inline RE::BGSExplosion* timed_block_explosion = nullptr;
        static inline RE::TESGlobal* timed_block_counter_glob = nullptr;
        static inline RE::BGSSoundDescriptorForm* timedBlockSFX = nullptr;

        static void Install() {
            SKSE::log::info("Installing Hooks...");
            SKSE::log::info("Installing PlayerCharacter notifyanimationgraph hook...");

            {
                REL::Relocation<uintptr_t> PlayerCharacter_IAnimationGraphManagerHolderVtbl{RE::VTABLE_PlayerCharacter[3]};
                _PC_NotifyAnimationGraph = PlayerCharacter_IAnimationGraphManagerHolderVtbl.write_vfunc(0x1, PC_NotifyAnimationGraph);
            }
           
            SKSE::log::info("PlayerCharacter animation graph hook installed successfully");

            //hitdata hook from valhalla combat & stbl
            SKSE::log::info("Installing Attempting to install processHit hook...");
            {
                auto& trampoline = SKSE::GetTrampoline();
			    _ProcessHit = trampoline.write_call<5>(REL::RelocationID(37673, 38627).address()+ REL::Relocate(0x3C0, 0x4A8), processHit);
            }
            
            SKSE::log::info("Installed processHit hook. ");
            
            //anim speed hooks from simple timed block addons

            SKSE::log::info("Finished Installing Hooks. ");
        }

        static bool LoadForms() {
            auto* dataHandler = RE::TESDataHandler::GetSingleton();
            timedBlockWindowSpell = dataHandler->LookupForm<RE::SpellItem>(0x802, "SimpleTimedBlock.esp");
            timedBlockWindowMGEF = dataHandler->LookupForm<RE::EffectSetting>(0x801, "SimpleTimedBlock.esp");
            timedBlockStaggerSpell = dataHandler->LookupForm<RE::SpellItem>(0x803, "SimpleTimedBlock.esp");
            timeBlockBuffSpell = dataHandler->LookupForm<RE::SpellItem>(0x80B, "SimpleTimedBlock.esp");
            timed_block_explosion = dataHandler->LookupForm<RE::BGSExplosion>(0x805, "SimpleTimedBlock.esp");
            timed_block_counter_glob = dataHandler->LookupForm<RE::TESGlobal>(0x80E, "SimpleTimedBlock.esp");
            timedBlockSFX = dataHandler->LookupForm<RE::BGSSoundDescriptorForm>(0x807, "SimpleTimedBlock.esp");
            
            if (!timedBlockWindowSpell || !timedBlockWindowMGEF || !timedBlockStaggerSpell || !timeBlockBuffSpell) {
                SKSE::log::error("Failed to load spell forms: timedBlockWindowSpell={}, timedBlockWindowMGEF={}, timedBlockStaggerSpell={}, timeBlockBuffSpell={}", 
                    static_cast<void*>(timedBlockWindowSpell), static_cast<void*>(timedBlockWindowMGEF), static_cast<void*>(timedBlockStaggerSpell), static_cast<void*>(timeBlockBuffSpell));
                return false;
            }
            if (!timed_block_explosion|| !timed_block_counter_glob || !timedBlockSFX) {
                SKSE::log::error("Failed to load effect forms: timed_block_explosion={}, timed_block_counter_glob={}, timedBlockSFX={}", 
                    static_cast<void*>(timed_block_explosion), static_cast<void*>(timed_block_counter_glob), static_cast<void*>(timedBlockSFX));
                return false;
            }
            SKSE::log::info("Correctly loaded spell forms: timedBlockWindowSpell={:08X}, timedBlockWindowMGEF={:08X}, timedBlockStaggerSpell={:08X}, timeBlockBuffSpell={:08X}", 
                    timedBlockWindowSpell->GetFormID(), timedBlockWindowMGEF->GetFormID(), timedBlockStaggerSpell->GetFormID(), timeBlockBuffSpell->GetFormID());
            SKSE::log::info("Correctly loaded effect forms: timed_block_explosion={:08X}, timed_block_counter_glob={:08X}, timedBlockSFX={:08X}", 
                   timed_block_explosion->GetFormID(), timed_block_counter_glob->GetFormID(), timedBlockSFX->GetFormID());
            return true;
        }

    private:
        // static inline RE::SpellItem* timedBlockWindowSpell = nullptr;
        // static inline RE::EffectSetting* timedBlockWindowMGEF = nullptr;
        // static inline RE::SpellItem* timedBlockStaggerSpell = nullptr;
        // static inline RE::SpellItem* timeBlockBuffSpell = nullptr;
        // static inline RE::BGSExplosion* timed_block_explosion = nullptr;
        // static inline RE::TESGlobal* timed_block_counter_glob = nullptr;
        // static inline RE::BGSSoundDescriptorForm* timedBlockSFX = nullptr;

        static bool PC_NotifyAnimationGraph(RE::IAnimationGraphManagerHolder* a_this, const RE::BSFixedString& a_eventName);
        static void processHit(RE::Actor* actor, RE::HitData& hitData);
        inline static REL::Relocation<decltype(PC_NotifyAnimationGraph)> _PC_NotifyAnimationGraph;
        inline static REL::Relocation<decltype(processHit)> _ProcessHit;

};