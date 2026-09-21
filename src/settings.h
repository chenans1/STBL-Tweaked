#pragma once

#include <string>

namespace settings {
    bool log = true;

    struct config {
        bool log = true;

        bool applyTimedBlockVFX = true;
        bool applyTimedBlockSFX = true;
        
    };
    
    config Get();
    void Set(const config& value);
    void Load();
    bool Save();
    void RegisterMenu();
}