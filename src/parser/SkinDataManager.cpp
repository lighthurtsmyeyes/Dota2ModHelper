#include "SkinDataManager.h"
#include <cctype>


namespace skin_parser {

    SkinDataManager& SkinDataManager::instance() {
        static SkinDataManager mgr;
        return mgr;
    }

    std::unordered_map<std::string, Hero>& SkinDataManager::heroes() {
        return heroes_;
    }

    std::unordered_map<int, DefaultSlot>& SkinDataManager::defaultSlots() {
        return default_slots_;
    }

    std::unordered_map<int, Skin>& SkinDataManager::skins() {
        return skins_;
    }

    std::vector<Skin>& SkinDataManager::unresolvedSkins() {
        return unresolved_skins_;
    }

    std::unordered_map<std::string, DecompiledModel>& SkinDataManager::decompiledModels() {
        return decompiled_models_;
    }

    std::unordered_map<std::string, WorldSlotDefinition>& SkinDataManager::worldSlotDefinitions() {
        return world_slot_definitions_;
    }

    std::unordered_map<std::string, WorldCategory>& SkinDataManager::worldCategories() {
        return world_categories_;
    }

    std::unordered_map<std::string, std::map<std::string, std::vector<Skin>>>& SkinDataManager::worldCategorySkins() {
        return world_category_skins_;
    }

    std::unordered_map<std::string, CustomSkinData>& SkinDataManager::customSkinsData() {
        return custom_skins_data_;
    }

    std::map<std::string, std::string>& SkinDataManager::rarityColors() {
        return rarity_colors_;
    }

    std::unordered_map<std::string, HeroPersonas>& SkinDataManager::heroPersonas() {
        return hero_personas_;
    }

    void SkinDataManager::setModelDecompiler(std::unique_ptr<ModelDecompiler> decompiler) {
        model_decompiler_ = std::move(decompiler);
    }

    ModelDecompiler* SkinDataManager::getModelDecompiler() const {
        return model_decompiler_.get();
    }

    bool SkinDataManager::hasModelDecompiler() const {
        return model_decompiler_ != nullptr;
    }


    void SkinDataManager::initializeWorldSlotDefinitions() {
        world_categories_ = {
            {"heroes",   {"heroes",   "Heroes",   "file:resources/images/category_heroes.png"}},
            {"landscape",{"landscape","Landscape","file:resources/images/category_landscape.png"}},
            {"hud",      {"hud",      "HUD",      "file:resources/images/category_hud.png"}},
            {"sounds",   {"sounds",   "Sounds",   "file:resources/images/category_sounds.png"}}
        };

        world_slot_definitions_ = {
            {"radiantcreeps", {
                "radiantcreeps", "Radiant Creeps", "landscape", "",
                "radiantcreeps", {}, 660,
                "econ/tools/default_radiant_creeps",
                "#DOTA_Item_Default_Radiant_Creeps",
                "#DOTA_Item_Default_Radiant_Creeps"
            }},
            {"direcreeps", {
                "direcreeps", "Dire Creeps", "landscape", "",
                "direcreeps", {}, 661,
                "econ/tools/default_dire_creeps",
                "#DOTA_Item_Default_Dire_Creeps",
                "#DOTA_Item_Default_Dire_Creeps"
            }},
            {"ward", {
                "ward", "Ward", "landscape", "panorama/images/econ/items/wards/ward_default_png.vtex_c",
                "ward", {}, 596,
                "econ/items/wards/ward_default",
                "#DOTA_Item_Default_Ward",
                "#DOTA_Item_Default_Ward"
            }},
            {"courier", {
                "courier", "Courier", "landscape", "",
                "courier", {}, 595,
                "econ/courier/donkey_radiant_default",
                "#DOTA_Item_Default_Courier",
                "#DOTA_Item_Default_Courier"
            }},
            {"weather", {
                "weather", "Weather", "landscape", "",
                "", {"weather"}, 555,
                "econ/tools/weather_default",
                "#DOTA_Item_Default_Weather",
                "#DOTA_Item_Default_Weather"
            }},
            {"radiantsiegecreeps", {
                "radiantsiegecreeps", "Radiant Siege Creeps", "landscape", "file:resources/images/radiant_siege.png",
                "radiantsiegecreeps", {}, 34462,
                "econ/default_no_item",
                "#DOTA_Item_Default_Radiant_Siege_Creeps",
                "#DOTA_Item_Default_Radiant_Siege_Creeps"
            }},
            {"diresiegecreeps", {
                "diresiegecreeps", "Dire Siege Creeps", "landscape", "file:resources/images/dire_siege.png",
                "diresiegecreeps", {}, 34463,
                "econ/default_no_item",
                "#DOTA_Item_Default_Dire_Siege_Creeps",
                "#DOTA_Item_Default_Dire_Siege_Creeps"
            }},
            {"radianttowers", {
                "radianttowers", "Radiant Towers", "landscape", "file:resources/images/radiant_tower.png",
                "radianttowers", {}, 677,
                "econ/tools/default_radiant_towers",
                "#DOTA_Item_Default_Radiant_Towers",
                "#DOTA_Item_Default_Radiant_Towers"
            }},
            {"diretowers", {
                "diretowers", "Dire Towers", "landscape", "file:resources/images/dire_tower.png",
                "diretowers", {}, 678,
                "econ/tools/default_dire_towers",
                "#DOTA_Item_Default_Dire_Towers",
                "#DOTA_Item_Default_Dire_Towers"
            }},
            {"roshan", {
                "roshan", "Roshan", "landscape", "",
                "roshan", {}, 801,
                "econ/roshan/dota_item_roshan",
                "#DOTA_WearableType_Default_Item",
                "#DOTA_WearableType_Default_Item"
            }},
            {"terrain", {
                "terrain", "Terrain", "landscape", "",
                "terrain", {}, 590,
                "econ/terrain/default_terrain",
                "#DOTA_Item_Default_Terrain",
                "#DOTA_Item_Default_Terrain"
            }},
            {"streak_effect", {
                "streak_effect", "Kill Streak Effect", "hud", "file:resources/images/kill_streak_effects.png",
                "streak_effect", {}, 14912,
                "econ/default_no_item",
                "#DOTA_WearableType_Streak_Effect",
                "#DOTA_WearableType_Streak_Effect"
            }},
            {"multikill_banner", {
                "multikill_banner", "Multikill Banner", "hud", "file:resources/images/multikill_banner.png",
                "misc", {"multikill_banner"}, 14912,
                "econ/default_no_item",
                "#DOTA_Item_Battle_Glory_Kill_Banner",
                "#DOTA_Item_Battle_Glory_Kill_Banner"
            }},
            {"emblem", {
                "emblem", "Emblem", "hud", "file:resources/images/emblem.png",
                "emblem", {}, -1,
                "econ/default_no_item",
                "#DOTA_Item_Emblem",
                "#DOTA_Item_Emblem"
            }},
            {"hud_skin", {
                "hud_skin", "Hud Skin", "hud", "",
                "hud_skin", {}, 587,
                "econ/huds/hud_default",
                "#DOTA_Item_Default_Hud_Skin",
                "#DOTA_Item_Default_Hud_Skin"
            }},
            {"versus_screen", {
                "versus_screen", "Versus Screen", "hud", "",
                "versus_screen", {}, 12970,
                "econ/tools/default_versus",
                "#DOTA_Item_Default_Versus_Screen",
                "#DOTA_Item_Default_Versus_Screen"
            }},
            {"announcer", {
                "announcer", "Announcer", "sounds", "",
                "announcer", {"announcer"}, 11173,
                "econ/announcer/announcer_default",
                "#DOTA_Item_Default_Announcer",
                "#DOTA_Item_Default_Announcer"
            }},
            {"mega_kills", {
                "mega_kills", "Mega-Kill Announcer", "sounds", "",
                "announcer", {"mega_kills"}, 586,
                "econ/announcer/announcer_default_megakill",
                "#DOTA_Item_Default_MegaKill_Announcer",
                "#DOTA_Item_Default_MegaKill_Announcer"
            }},
            {"tormentor", {
                "tormentor", "Tormentor", "landscape", "file:resources/images/tormentor.png",
                "tormentor", {}, -1,
                "econ/default_no_item",
                "#DOTA_Item_10th_Anniversary_Tormentor",
                "#DOTA_Item_10th_Anniversary_Tormentor"
            }},
            {"cursor_pack", {
                "cursor_pack", "Cursor Pack", "hud", "",
                "cursor_pack", {}, 202,
                "econ/cursor_pack/cursor_pack_default",
                "#DOTA_Item_Default_Cursor_Pack",
                "#DOTA_Item_Default_Cursor_Pack"
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