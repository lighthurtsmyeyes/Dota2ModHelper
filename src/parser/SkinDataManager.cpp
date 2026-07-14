#include "SkinDataManager.h"
#include "../SecurityHardening.h"
#include <cctype>
namespace {
__declspec(noinline) void SH_AD_SkinDataManager() noexcept {
    if (SH_RdtscTiming()) g_integritySeed ^= 0x55667788;
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
}
}


namespace skin_parser {

    SkinDataManager& SkinDataManager::instance() {
    SH_AD_SkinDataManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        static SkinDataManager mgr;
        return mgr;
    }

    std::unordered_map<std::string, Hero>& SkinDataManager::heroes() {
    SH_AD_SkinDataManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        return heroes_;
    }

    std::unordered_map<int, DefaultSlot>& SkinDataManager::defaultSlots() {
    SH_AD_SkinDataManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        return default_slots_;
    }

    std::unordered_map<int, Skin>& SkinDataManager::skins() {
    SH_AD_SkinDataManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        return skins_;
    }

    std::vector<Skin>& SkinDataManager::unresolvedSkins() {
    SH_AD_SkinDataManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        return unresolved_skins_;
    }

    std::unordered_map<std::string, DecompiledModel>& SkinDataManager::decompiledModels() {
    SH_AD_SkinDataManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        return decompiled_models_;
    }

    std::unordered_map<std::string, WorldSlotDefinition>& SkinDataManager::worldSlotDefinitions() {
    SH_AD_SkinDataManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        return world_slot_definitions_;
    }

    std::unordered_map<std::string, WorldCategory>& SkinDataManager::worldCategories() {
    SH_AD_SkinDataManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        return world_categories_;
    }

    std::unordered_map<std::string, std::map<std::string, std::vector<Skin>>>& SkinDataManager::worldCategorySkins() {
    SH_AD_SkinDataManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        return world_category_skins_;
    }

    std::unordered_map<std::string, CustomSkinData>& SkinDataManager::customSkinsData() {
    SH_AD_SkinDataManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        return custom_skins_data_;
    }

    std::map<std::string, std::string>& SkinDataManager::rarityColors() {
    SH_AD_SkinDataManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        return rarity_colors_;
    }

    std::unordered_map<std::string, HeroPersonas>& SkinDataManager::heroPersonas() {
    SH_AD_SkinDataManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        return hero_personas_;
    }

    void SkinDataManager::setModelDecompiler(std::unique_ptr<ModelDecompiler> decompiler) {
    SH_AD_SkinDataManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        model_decompiler_ = std::move(decompiler);
    }

    ModelDecompiler* SkinDataManager::getModelDecompiler() const {
    SH_AD_SkinDataManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        return model_decompiler_.get();
    }

    bool SkinDataManager::hasModelDecompiler() const {
    SH_AD_SkinDataManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        return model_decompiler_ != nullptr;
    }


    void SkinDataManager::initializeWorldSlotDefinitions() {
    SH_AD_SkinDataManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        world_categories_ = {
            {OBF_CSTR("heroes"),   {OBF_CSTR("heroes"),   OBF_CSTR("Heroes"),   OBF_CSTR("file:resources/images/category_heroes.png")}},
            {OBF_CSTR("landscape"),{OBF_CSTR("landscape"),OBF_CSTR("Landscape"),OBF_CSTR("file:resources/images/category_landscape.png")}},
            {OBF_CSTR("hud"),      {OBF_CSTR("hud"),      OBF_CSTR("HUD"),      OBF_CSTR("file:resources/images/category_hud.png")}},
            {OBF_CSTR("sounds"),   {OBF_CSTR("sounds"),   OBF_CSTR("Sounds"),   OBF_CSTR("file:resources/images/category_sounds.png")}}
        };

        world_slot_definitions_ = {
            {OBF_CSTR("radiantcreeps"), {
                OBF_CSTR("radiantcreeps"), OBF_CSTR("Radiant Creeps"), OBF_CSTR("landscape"), OBF_CSTR(""),
                OBF_CSTR("radiantcreeps"), {}, 660,
                OBF_CSTR("econ/tools/default_radiant_creeps"),
                OBF_CSTR("#DOTA_Item_Default_Radiant_Creeps"),
                OBF_CSTR("#DOTA_Item_Default_Radiant_Creeps")
            }},
            {OBF_CSTR("direcreeps"), {
                OBF_CSTR("direcreeps"), OBF_CSTR("Dire Creeps"), OBF_CSTR("landscape"), OBF_CSTR(""),
                OBF_CSTR("direcreeps"), {}, 661,
                OBF_CSTR("econ/tools/default_dire_creeps"),
                OBF_CSTR("#DOTA_Item_Default_Dire_Creeps"),
                OBF_CSTR("#DOTA_Item_Default_Dire_Creeps")
            }},
            {OBF_CSTR("ward"), {
                OBF_CSTR("ward"), OBF_CSTR("Ward"), OBF_CSTR("landscape"), OBF_CSTR("panorama/images/econ/items/wards/ward_default_png.vtex_c"),
                OBF_CSTR("ward"), {}, 596,
                OBF_CSTR("econ/items/wards/ward_default"),
                OBF_CSTR("#DOTA_Item_Default_Ward"),
                OBF_CSTR("#DOTA_Item_Default_Ward")
            }},
            {OBF_CSTR("courier"), {
                OBF_CSTR("courier"), OBF_CSTR("Courier"), OBF_CSTR("landscape"), OBF_CSTR(""),
                OBF_CSTR("courier"), {}, 595,
                OBF_CSTR("econ/courier/donkey_radiant_default"),
                OBF_CSTR("#DOTA_Item_Default_Courier"),
                OBF_CSTR("#DOTA_Item_Default_Courier")
            }},
            {OBF_CSTR("weather"), {
                OBF_CSTR("weather"), OBF_CSTR("Weather"), OBF_CSTR("landscape"), OBF_CSTR(""),
                OBF_CSTR(""), {OBF_CSTR("weather")}, 555,
                OBF_CSTR("econ/tools/weather_default"),
                OBF_CSTR("#DOTA_Item_Default_Weather"),
                OBF_CSTR("#DOTA_Item_Default_Weather")
            }},
            {OBF_CSTR("radiantsiegecreeps"), {
                OBF_CSTR("radiantsiegecreeps"), OBF_CSTR("Radiant Siege Creeps"), OBF_CSTR("landscape"), OBF_CSTR("file:resources/images/radiant_siege.png"),
                OBF_CSTR("radiantsiegecreeps"), {}, 34462,
                OBF_CSTR("econ/default_no_item"),
                OBF_CSTR("#DOTA_Item_Default_Radiant_Siege_Creeps"),
                OBF_CSTR("#DOTA_Item_Default_Radiant_Siege_Creeps")
            }},
            {OBF_CSTR("diresiegecreeps"), {
                OBF_CSTR("diresiegecreeps"), OBF_CSTR("Dire Siege Creeps"), OBF_CSTR("landscape"), OBF_CSTR("file:resources/images/dire_siege.png"),
                OBF_CSTR("diresiegecreeps"), {}, 34463,
                OBF_CSTR("econ/default_no_item"),
                OBF_CSTR("#DOTA_Item_Default_Dire_Siege_Creeps"),
                OBF_CSTR("#DOTA_Item_Default_Dire_Siege_Creeps")
            }},
            {OBF_CSTR("radianttowers"), {
                OBF_CSTR("radianttowers"), OBF_CSTR("Radiant Towers"), OBF_CSTR("landscape"), OBF_CSTR("file:resources/images/radiant_tower.png"),
                OBF_CSTR("radianttowers"), {}, 677,
                OBF_CSTR("econ/tools/default_radiant_towers"),
                OBF_CSTR("#DOTA_Item_Default_Radiant_Towers"),
                OBF_CSTR("#DOTA_Item_Default_Radiant_Towers")
            }},
            {OBF_CSTR("diretowers"), {
                OBF_CSTR("diretowers"), OBF_CSTR("Dire Towers"), OBF_CSTR("landscape"), OBF_CSTR("file:resources/images/dire_tower.png"),
                OBF_CSTR("diretowers"), {}, 678,
                OBF_CSTR("econ/tools/default_dire_towers"),
                OBF_CSTR("#DOTA_Item_Default_Dire_Towers"),
                OBF_CSTR("#DOTA_Item_Default_Dire_Towers")
            }},
            {OBF_CSTR("roshan"), {
                OBF_CSTR("roshan"), OBF_CSTR("Roshan"), OBF_CSTR("landscape"), OBF_CSTR(""),
                OBF_CSTR("roshan"), {}, 801,
                OBF_CSTR("econ/roshan/dota_item_roshan"),
                OBF_CSTR("#DOTA_WearableType_Default_Item"),
                OBF_CSTR("#DOTA_WearableType_Default_Item")
            }},
            {OBF_CSTR("terrain"), {
                OBF_CSTR("terrain"), OBF_CSTR("Terrain"), OBF_CSTR("landscape"), OBF_CSTR(""),
                OBF_CSTR("terrain"), {}, 590,
                OBF_CSTR("econ/terrain/default_terrain"),
                OBF_CSTR("#DOTA_Item_Default_Terrain"),
                OBF_CSTR("#DOTA_Item_Default_Terrain")
            }},
            {OBF_CSTR("streak_effect"), {
                OBF_CSTR("streak_effect"), OBF_CSTR("Kill Streak Effect"), OBF_CSTR("hud"), OBF_CSTR("file:resources/images/kill_streak_effects.png"),
                OBF_CSTR("streak_effect"), {}, 14912,
                OBF_CSTR("econ/default_no_item"),
                OBF_CSTR("#DOTA_WearableType_Streak_Effect"),
                OBF_CSTR("#DOTA_WearableType_Streak_Effect")
            }},
            {OBF_CSTR("multikill_banner"), {
                OBF_CSTR("multikill_banner"), OBF_CSTR("Multikill Banner"), OBF_CSTR("hud"), OBF_CSTR("file:resources/images/multikill_banner.png"),
                OBF_CSTR("misc"), {OBF_CSTR("multikill_banner")}, 14912,
                OBF_CSTR("econ/default_no_item"),
                OBF_CSTR("#DOTA_Item_Battle_Glory_Kill_Banner"),
                OBF_CSTR("#DOTA_Item_Battle_Glory_Kill_Banner")
            }},
            {OBF_CSTR("emblem"), {
                OBF_CSTR("emblem"), OBF_CSTR("Emblem"), OBF_CSTR("hud"), OBF_CSTR("file:resources/images/emblem.png"),
                OBF_CSTR("emblem"), {}, -1,
                OBF_CSTR("econ/default_no_item"),
                OBF_CSTR("#DOTA_Item_Emblem"),
                OBF_CSTR("#DOTA_Item_Emblem")
            }},
            {OBF_CSTR("hud_skin"), {
                OBF_CSTR("hud_skin"), OBF_CSTR("Hud Skin"), OBF_CSTR("hud"), OBF_CSTR(""),
                OBF_CSTR("hud_skin"), {}, 587,
                OBF_CSTR("econ/huds/hud_default"),
                OBF_CSTR("#DOTA_Item_Default_Hud_Skin"),
                OBF_CSTR("#DOTA_Item_Default_Hud_Skin")
            }},
            {OBF_CSTR("versus_screen"), {
                OBF_CSTR("versus_screen"), OBF_CSTR("Versus Screen"), OBF_CSTR("hud"), OBF_CSTR(""),
                OBF_CSTR("versus_screen"), {}, 12970,
                OBF_CSTR("econ/tools/default_versus"),
                OBF_CSTR("#DOTA_Item_Default_Versus_Screen"),
                OBF_CSTR("#DOTA_Item_Default_Versus_Screen")
            }},
            {OBF_CSTR("announcer"), {
                OBF_CSTR("announcer"), OBF_CSTR("Announcer"), OBF_CSTR("sounds"), OBF_CSTR(""),
                OBF_CSTR("announcer"), {OBF_CSTR("announcer")}, 11173,
                OBF_CSTR("econ/announcer/announcer_default"),
                OBF_CSTR("#DOTA_Item_Default_Announcer"),
                OBF_CSTR("#DOTA_Item_Default_Announcer")
            }},
            {OBF_CSTR("mega_kills"), {
                OBF_CSTR("mega_kills"), OBF_CSTR("Mega-Kill Announcer"), OBF_CSTR("sounds"), OBF_CSTR(""),
                OBF_CSTR("announcer"), {OBF_CSTR("mega_kills")}, 586,
                OBF_CSTR("econ/announcer/announcer_default_megakill"),
                OBF_CSTR("#DOTA_Item_Default_MegaKill_Announcer"),
                OBF_CSTR("#DOTA_Item_Default_MegaKill_Announcer")
            }},
            {OBF_CSTR("tormentor"), {
                OBF_CSTR("tormentor"), OBF_CSTR("Tormentor"), OBF_CSTR("landscape"), OBF_CSTR("file:resources/images/tormentor.png"),
                OBF_CSTR("tormentor"), {}, -1,
                OBF_CSTR("econ/default_no_item"),
                OBF_CSTR("#DOTA_Item_10th_Anniversary_Tormentor"),
                OBF_CSTR("#DOTA_Item_10th_Anniversary_Tormentor")
            }},
            {OBF_CSTR("cursor_pack"), {
                OBF_CSTR("cursor_pack"), OBF_CSTR("Cursor Pack"), OBF_CSTR("hud"), OBF_CSTR(""),
                OBF_CSTR("cursor_pack"), {}, 202,
                OBF_CSTR("econ/cursor_pack/cursor_pack_default"),
                OBF_CSTR("#DOTA_Item_Default_Cursor_Pack"),
                OBF_CSTR("#DOTA_Item_Default_Cursor_Pack")
            }},
        };

        // Auto-register any categories referenced by slot definitions that are not predefined.
        for (const auto& kv : world_slot_definitions_) {
            const std::string& catKey = kv.second.category;
            if (catKey.empty()) continue;
            if (world_categories_.find(catKey) == world_categories_.end()) {
                WorldCategory cat;
                cat.key = catKey;
                cat.display_name = catKey;
                if (!cat.display_name.empty()) {
                    cat.display_name[0] = static_cast<char>(std::toupper(cat.display_name[0]));
                }
                world_categories_[catKey] = std::move(cat);
            }
        }
    }

    void SkinDataManager::clear() {
    SH_AD_SkinDataManager();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        heroes_.clear();
        default_slots_.clear();
        skins_.clear();
        unresolved_skins_.clear();
        decompiled_models_.clear();
        world_slot_definitions_.clear();
        world_categories_.clear();
        world_category_skins_.clear();
        custom_skins_data_.clear();
        rarity_colors_.clear();
        hero_personas_.clear();
        model_decompiler_.reset();
    }

} // namespace skin_parser