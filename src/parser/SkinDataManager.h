#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <nlohmann/json.hpp>
#include "ModelDecompiler.h"
#include "../Structures.h"
#include <set>

namespace skin_parser {

    using json = nlohmann::json;
    using ordered_json = nlohmann::ordered_json;

    // ------------------------------------------------------------
    // Структуры данных
    // ------------------------------------------------------------

    struct HeroSlot {
        std::string name;
        std::string display_name;
        std::string slot_name;
        std::string slot_text;
        bool is_real_slot = false;
        bool is_virtual = false;
        std::string image_vpk_path;
    };

    struct DefaultSlot {
        int id = -1;
        std::string name;
        std::string prefab;
        std::string item_slot;
        std::string item_type_name;
        std::string hero_name;
        std::string slot_type;
        std::vector<std::string> used_by_heroes;
        std::string image_inventory;
        std::string image_vpk_path;
        std::string item_description;
        std::string item_name;
        std::string model_player;
        std::vector<int> skin_ids;
        bool is_virtual = false;
        // Raw `visuals` block from items_game.txt (e.g. the asset_modifier
        // entries that ship with persona-specific default items like
        // Invoker Kid's Forge Spirits, id 13047). Populated by
        // ItemsParser::parseDefaultSlot. Used by
        // StyleProcessor::processPersonaDefaultSlotVisuals to surface the
        // default item's persona-specific visuals into the parser's
        // normal modification/replacement pipeline so they show up in
        // dota_skins.json and are applied at VPK build time even when no
        // wearable skin is selected for the persona's default slot.
        json visuals;
        // Raw text of each asset_modifier entry inside the default
        // item's visuals block. Populated by
        // ItemsParser::extractOriginalAssetModifiers. Used to generate
        // exact-text `remove_block` modifications at process time
        // (mirrors the way wearable skins carry their own
        // original_asset_modifiers). Keyed "modifier_<index>".
        std::unordered_map<std::string, std::string> original_asset_modifiers;
    };

    struct Skin {
        int id = -1;
        std::string name;
        std::string prefab;
        std::string item_slot;
        std::string item_type_name;
        std::vector<std::string> used_by_heroes;
        std::string image_inventory;
        std::string image_vpk_path;
        std::string item_description;
        std::string item_name;
        std::string model_player;
        std::string rarity;
        std::string rarity_color;
        std::vector<ordered_json> modifications;
        std::vector<ordered_json> replacements;
        json visuals;
	int default_slot_id = -1;
	// C-suffixed model_player paths of every default_item slot for the
	// hero this skin belongs to. Used by StyleProcessor to mark
	// asset_modifier model replacements that target a default slot model
	// as low_priority, so equipped skins can override arcana refits.
	std::unordered_set<std::string> default_item_models;
	// For persona_selector wearables: the items_game.txt id of the
        // matching default_item (prefab=default_item, item_slot=persona_selector)
        // that this selector should swap with at VPK build time. -1 for
        // every other skin. The SwapSlots call in the VPK worker reads
        // this id; without it the worker only applies modifications/
        // replacements and leaves the default persona_selector in place,
        // so the persona visual never actually engages.
        int persona_default_slot_id = -1;
        std::vector<std::string> additional_models;
        std::vector<std::string> skip;
        std::vector<ordered_json> combo;
        // hero_scale asset_modifier entries harvested from this
        // skin's visuals block. Each entry targets an NPC (asset
        // field, e.g. "npc_dota_hero_axe") and carries scale values
        // to apply at VPK build time. The VPK worker rewrites the
        // matching keys inside scripts/npc/npc_units.txt (fallback
        // npc_heroes.txt) — not items_game.txt. Authored by
        // StyleProcessor::processVisualsSection /
        // processStyles, serialized by OutputGenerator::buildSkinJson,
        // rehydrated by IGParser::ParseSkin, consumed by
        // GUI::VPKWorkerThread::hero_scale pre-pass.
        std::vector<HeroScalePayload> hero_scales;
        std::unordered_map<std::string, std::string> original_asset_modifiers;
        // Индекс способности, для которой этот скин является эффектом (AbilityX)
        std::string ability_effect_for_name;
        // Признак, что скин может быть использован как эффект способности (имеет can_equip_as_ability_effects)
        bool can_be_ability_effect = false;
        std::set<int> ability_effect_indices;
        // True if this skin has an activity asset_modifier (undefined behaviour)
        bool is_undefined = false;

        // Visual styles for this skin. Populated by StyleProcessor::processStyles().
        // When non-empty (size > 1), the base skin's modifications/replacements are
        // ignored at apply time; each entry carries the full payload for one style.
        struct StyleVariant {
            int index = 0;
            std::string name;
            std::string image_vpk_path;
            std::string image_inventory;
            std::string model_player;
            std::vector<ordered_json> modifications;
            std::vector<ordered_json> replacements;
            std::vector<std::string> skip;
            std::vector<ordered_json> combos;
            // hero_scale asset_modifier entries. Each one targets an
            // NPC (asset) and rewrites the scale keys in
            // scripts/npc/npc_units.txt (fallback npc_heroes.txt) at
            // VPK build time. Mirrors Skin::hero_scales for the
            // base skin.
            std::vector<HeroScalePayload> hero_scales;
        };
        std::vector<StyleVariant> styles;
        // Default style index written to the JSON. The runtime worker falls
        // back to this when no user-selected style is persisted.
        int default_style_index = 0;

        struct PendingAbilityMod {
            std::string ability_name;
            ordered_json add_mod;
            ordered_json replacement;
            std::string mod_type;
        };
        std::vector<PendingAbilityMod> pending_ability_effects;

        // ---- Persona AE/AI routing -----------------------------------
        // When a default hero skin is included in a persona's Ability
        // Effects / Ability Icons virtual slots, the source skin's
        // modifications target the DEFAULT hero's slot id (e.g. head
        // for Invoker's regular "head" slot). For the persona to
        // actually see the effect in-game, those modifications have
        // to land on the PERSONA's matching slot (e.g. head_persona_1
        // with id 13043) — otherwise the engine writes the change to
        // the wrong item block and the persona ends up unchanged.
        //
        // Set by SlotDistributor::buildAbilityEffectsSlots when it
        // surfaces default-hero skins into a non-default persona's
        // virtual slots. Consumed by GUI::VPKWorkerThread before
        // ItemManager::ModifyItemsGame, which rewrites every mod's
        // `id` field to persona_target_slot_id.
        //
        // -1 (default) means: no rerouting, the modification ids
        // stay as-authored and target the source skin's own slot.
        int persona_target_slot_id = -1;
        // For logging / debug: the human-readable name of the
        // target slot ("head_persona_1", "persona_selector", ...).
        // Empty when persona_target_slot_id == -1.
        std::string persona_target_slot_name;
    };

    struct VirtualSlot {
        std::string name;
        std::string display_name;
        std::string id = "-1";
        bool is_virtual = true;
        std::string image_vpk_path;
        std::string ability_name;
        std::string ability_display_name;
        std::vector<VirtualSlot> subslots;
        std::vector<Skin> skins;
    };

    struct Hero {
        std::string name;
        std::string display_name;
        std::string code_name;
        std::string image_vpk_path;
        std::vector<HeroSlot> slots;
        std::map<std::string, DefaultSlot> default_slots;
        std::map<std::string, std::vector<Skin>> slot_skins;
        std::vector<std::string> abilities;
        std::vector<VirtualSlot> virtual_slots;
        std::map<int, std::string> ability_effects_slots;
    };

    // One persona entry. Mirrors the regular hero slot/skin shape but lives
    // on a separate model. The `default_slot` is a hidden persona_selector
    // whose modifications/replacements get applied at VPK build time when
    // the user picks a non-default persona.
    struct Persona {
        std::string code;                  // e.g. "npc_dota_hero_invoker_persona1"
        std::string name;                  // raw name (e.g. "Acolyte of the Lost Arts")
        std::string display_name;          // localized name
        std::string image_vpk_path;        // e.g. "panorama/images/heroes/npc_dota_hero_invoker_persona1_png.vtex_c"
        bool is_default = false;           // true for the implicit default persona
        std::vector<HeroSlot> slots;       // persona-specific slots (head_persona_1, ...)
        std::map<std::string, std::vector<Skin>> slot_skins;
        // Virtual slots generated for this persona from its skins'
        // pending_ability_effects (mirrors Hero::virtual_slots for the
        // default persona). Populated by SlotDistributor::
        // buildAbilityEffectsSlots. Always empty for the default persona,
        // which shares the hero's own virtual_slots instead. Serialized
        // to dota_skins.json by OutputGenerator::buildPersonaSlotsJson
        // so the GUI can render "Ability Effects" / "Ability Icons" for
        // persona skins.
        std::vector<VirtualSlot> virtual_slots;
        // The wearable item with item_slot=persona_selector for this persona.
        // Empty for the default persona (no application needed).
        Skin persona_selector_skin;
        // IDs of the default items associated with the persona's slots.
        // Keyed by normalized slot name (e.g. "head_persona_1" -> "head").
        std::map<std::string, DefaultSlot> default_slots;
        // The items_game.txt id of the default_item this persona_selector
        // replaces (prefab=default_item, item_slot=persona_selector for
        // the same hero, e.g. 683 for Invoker). -1 for the default
        // persona (no swap needed) and for any persona where the
        // matching default couldn't be located.
        int default_slot_id = -1;
        // Per-default-slot "always-on" visual payloads. Each entry is
        // built from the default item's `visuals.asset_modifier` block
        // (e.g. the kid-Invoker forge spirit particles that ship with
        // default slot 13047) by StyleProcessor::processPersonaDefaultSlotVisuals.
        // They run alongside the persona_selector pre-pass at VPK
        // build time so the persona's loadout visuals are always
        // applied, regardless of whether the user picked a wearable
        // for that slot. Serialized to dota_skins.json by
        // OutputGenerator::buildPersonaSlotsJson and rehydrated by
        // IGParser::ParseSlot. Without this pass, the persona's
        // default-slot visuals sit in items_game.txt but never flow
        // through our normal modifications/replacements pipeline —
        // they cannot be inspected, overridden by mods, or reflected
        // in the build log.
        struct DefaultSlotVisualPayload {
            int slot_id = -1;                    // items_game.txt id of the default slot
            std::string slot_name;               // e.g. "summon_persona_1"
            std::string display_name;            // localized slot name (best-effort)
            std::vector<ordered_json> modifications; // remove_block + add replacements
            std::vector<ordered_json> replacements;   // entity_model / particle / etc.
            // hero_scale asset_modifier entries harvested from this
            // default slot's visuals block. Same semantics as
            // Skin::hero_scales but for the always-on persona default
            // visual payload. Applied unconditionally when the
            // persona is active (no user selection required for the
            // slot) by the persona default slot visual pre-pass.
            std::vector<HeroScalePayload> hero_scales;
        };
        std::vector<DefaultSlotVisualPayload> default_slot_visual_payloads;
    };

    // Extended Hero with persona support. The original Hero struct above
    // stays untouched; we add an optional personas vector at the parser
    // level via a parallel map keyed by hero code.
    struct HeroPersonas {
        std::vector<Persona> personas; // always at least one entry when populated (index 0 = default)
    };

    struct Style {
        int index = 0;
        std::string name;
        int skin_id = -1;
        int alternate_icon = -1;
        std::string model_player;
        json unlock;
    };

    struct AlternateIcon {
        int index = 0;
        std::string icon_path;
    };

    struct MaterialGroup {
        std::string name;
        std::vector<std::string> materials;
    };

    struct DecompiledModel {
        std::string model_path;
        std::string data_content;
        std::vector<MaterialGroup> material_groups;
        bool is_loaded = false;
    };

    struct CustomSkinData {
        std::vector<ordered_json> modifications;
        std::vector<ordered_json> replacements;
        std::vector<std::string> skip;
        std::vector<ordered_json> combo;
    };

    struct WorldCategory {
        std::string key;
        std::string display_name;
        std::string image_path; // optional file: override, e.g. "file:resources/images/landscape.png"
    };

    struct WorldSlotDefinition {
        std::string slot_id;
        std::string display_name;
        std::string category;    // category key, e.g. "landscape"
        std::string image_path;  // optional file: override, e.g. "file:resources/images/ward.png"
        std::string prefab;
        std::vector<std::string> item_slots;
        int default_slot_id = -1;
        std::string default_image;
        std::string default_description;
        std::string default_name;

        bool matches(const std::string& prefab_value, const std::string& item_slot_value) const {
            bool prefab_ok = prefab.empty() || prefab_value == prefab;
            bool item_slot_ok = item_slots.empty();
            if (!item_slot_ok) {
                for (const auto& s : item_slots) {
                    if (!s.empty() && item_slot_value == s) { item_slot_ok = true; break; }
                }
            }
            return prefab_ok && item_slot_ok;
        }
    };

    struct SkinWithOriginalText : Skin {
        std::unordered_map<std::string, std::string> original_asset_modifiers;
        std::string original_skin_block;
    };


    // ------------------------------------------------------------
    // Класс-менеджер для хранения всех глобальных данных
    // ------------------------------------------------------------
    class SkinDataManager {
    public:
        static SkinDataManager& instance();

        // Доступ к контейнерам
        std::unordered_map<std::string, Hero>& heroes();
        std::unordered_map<int, DefaultSlot>& defaultSlots();
        std::unordered_map<int, Skin>& skins();
        std::vector<Skin>& unresolvedSkins();
        std::unordered_map<std::string, DecompiledModel>& decompiledModels();
        std::unordered_map<std::string, WorldSlotDefinition>& worldSlotDefinitions();
        std::unordered_map<std::string, WorldCategory>& worldCategories();
        std::unordered_map<std::string, std::map<std::string, std::vector<Skin>>>& worldCategorySkins();
        std::unordered_map<std::string, CustomSkinData>& customSkinsData();
        std::map<std::string, std::string>& rarityColors();
        // Per-hero personas. Keyed by hero code (e.g. "npc_dota_hero_invoker").
        // Empty entry means the hero has no personas.
        std::unordered_map<std::string, HeroPersonas>& heroPersonas();

        // Управление декомпилятором
        void setModelDecompiler(std::unique_ptr<ModelDecompiler> decompiler);
        ModelDecompiler* getModelDecompiler() const;
        bool hasModelDecompiler() const;

        // Инициализация world slot definitions
        void initializeWorldSlotDefinitions();

        // Проверить, подходит ли скин под любой world-слот (по prefab/item_slot)
        bool isWorldItem(const std::string& prefab, const std::string& item_slot) const;
        // Получить определение world-слота по prefab или item_slot
        const WorldSlotDefinition* findWorldSlot(const std::string& prefab, const std::string& item_slot) const;

        // Очистка всех данных
        void clear();

    private:
        SkinDataManager() = default;
        ~SkinDataManager() = default;
        SkinDataManager(const SkinDataManager&) = delete;
        SkinDataManager& operator=(const SkinDataManager&) = delete;

        std::unordered_map<std::string, Hero> heroes_;
        std::unordered_map<int, DefaultSlot> default_slots_;
        std::unordered_map<int, Skin> skins_;
        std::vector<Skin> unresolved_skins_;
        std::unordered_map<std::string, DecompiledModel> decompiled_models_;
        std::unordered_map<std::string, WorldSlotDefinition> world_slot_definitions_;
        std::unordered_map<std::string, WorldCategory> world_categories_;
        // category key -> (slot_id -> skins) for world items
        std::unordered_map<std::string, std::map<std::string, std::vector<Skin>>> world_category_skins_;
        std::unordered_map<std::string, CustomSkinData> custom_skins_data_;
        std::map<std::string, std::string> rarity_colors_;
        std::unordered_map<std::string, HeroPersonas> hero_personas_;
        std::unique_ptr<ModelDecompiler> model_decompiler_;
    };

    inline bool SkinDataManager::isWorldItem(const std::string& prefab, const std::string& item_slot) const {
        if (findWorldSlot(prefab, item_slot) != nullptr) return true;
        return false;
    }

    inline const WorldSlotDefinition* SkinDataManager::findWorldSlot(const std::string& prefab, const std::string& item_slot) const {
        for (const auto& kv : world_slot_definitions_) {
            if (kv.second.matches(prefab, item_slot)) return &kv.second;
        }
        return nullptr;
    }

} // namespace skin_parser