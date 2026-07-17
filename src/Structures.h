// Structures.h (универсальный слот)

#pragma once
#include <string>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <map>
#include <algorithm>
#include <vector>
#include <any>
#include <functional>
#include <optional>

namespace fs = std::filesystem;

typedef std::optional<std::vector<std::byte>> FileData;
typedef nlohmann::json json;

struct FileEntry
{
	std::string vpkPath;
	FileData data;
};

struct Modification {
	std::string id;
	std::vector<std::string> key_path;
	std::string value;
	std::string new_value;
	std::string type;

	std::map<std::string, std::string> match_conditions = {};
	bool match_all_conditions = true;

	std::string get_last_key() const {
		return key_path.empty() ? "" : key_path.back();
	}

	std::vector<std::string> get_parent_keys() const {
		if (key_path.size() <= 1) return {};
		return std::vector<std::string>(key_path.begin(), key_path.end() - 1);
	}

	bool has_conditions() const {
		return !match_conditions.empty();
	}

	void add_condition(const std::string& key, const std::string& expected_value) {
		match_conditions[key] = expected_value;
	}
};

struct Replacement
{
	std::string type;
	std::string what;
	std::string to;
	bool custom = false;
	std::string mod_folder;
};

struct ComboItem
{
	std::string name;
	int style_index = -1;
};

struct Combo
{
	std::vector<ComboItem> items;
	std::vector<Replacement> replacements;
};

// Forward declaration
struct UniversalSlot;

// Context passed to action handlers (for future mod script support)
struct ActionContext {
	FileData& items_game;
	std::string& items_game_content;
	std::map<std::string, std::string>& sounds;
	std::map<std::string, std::string>& npcModels;
	std::function<void(const std::string&)> log;
};

// One hero_scale asset_modifier payload. Authored by the parser from
// items_game.txt entries like:
//   "asset_modifier"
//   {
//       "type"          "hero_scale"
//       "asset"         "npc_dota_hero_axe"
//       "ModelScale"            "1.030000"
//       "VersusScale"           "0.916700"
//       "LoadoutScale"          "0.978500"
//       "SpectatorLoadoutScale" "0.875500"
//   }
//
// The VPK worker applies these to scripts/npc/npc_units.txt (fallback
// npc_heroes.txt) by locating the NPC block named `asset` and rewriting
// the matching scale keys. The keys are optional — only present keys
// are written; missing keys are left untouched. A payload with an empty
// `asset` is treated as invalid and skipped.
struct HeroScalePayload {
    std::string asset;
    std::string model_scale;
    std::string versus_scale;
    std::string loadout_scale;
    std::string spectator_loadout_scale;

    bool isEmpty() const { return asset.empty(); }
};

// One style variant of a multi-style skin. Carries the same payload shape as
// a regular leaf skin, but represents an alternate visual (alternate icon,
// model_player, style-specific modifications, etc.) of a single cosmetic item.
struct SkinStyleVariant {
	int index = 0;                    // 0..N-1, matches Valve's style index
	std::string name;                 // "Style 0", "Style 1", ...
	std::string image_vpk_path;       // icon shown when this style is active
	std::string image_inventory;      // raw inventory icon path (for image_inventory modification)
	std::string model_player;         // model_player override for this style
	std::vector<Modification> modifications;
	std::vector<Replacement> replacements;
	std::vector<std::string> skip;
	std::vector<Combo> combos;
	std::vector<HeroScalePayload> hero_scales;
};

// Style-specific override payload authored by a mod. Has the same fields as
// ModSkinOverride plus the active style index this override applies to.
struct ModStyleOverride {
	std::vector<Modification> modifications;
	std::vector<Replacement> replacements;
	std::vector<std::string> skip;
	std::vector<Combo> combos;
	std::vector<HeroScalePayload> hero_scales;
	std::optional<bool> is_undefined;
	int style_index = 0;
};

// Default slot override payload authored by a mod. Modifies a target skin's
// payload with extra modifications, replacements, skip rules, combos and
// style-specific overrides.
struct ModSkinOverride {
    std::vector<Modification> modifications;
    std::vector<Replacement> replacements;
    std::vector<std::string> skip;
    std::vector<Combo> combos;
    // hero_scale asset_modifier entries that get appended to the
    // target skin's payload (or to a persona's selector/default
    // slot payload, keyed by the same name). Mod authors can use
    // this to add NPC scale changes that the source skin did not
    // ship with — for example, a balance mod that bumps a hero's
    // LoadoutScale globally.
    std::vector<HeroScalePayload> hero_scales;
    std::optional<bool> is_undefined;
    std::vector<ModStyleOverride> styles;
};

// New skin definition authored by a mod. Creates a virtual skin entry
// in the slot tree when the mod is active.
struct ModNewSkin {
    std::string name;
    std::vector<std::string> path; // full path including "Heroes" root
    std::vector<int> default_slot_ids;
    std::vector<int> swap_slot_ids;
    std::vector<std::vector<std::string>> disables;
    std::vector<Modification> modifications;
    std::vector<Replacement> replacements;
    std::vector<std::string> skip;
    std::vector<Combo> combos;
    std::vector<HeroScalePayload> hero_scales;
};

// The single universal recursive slot. Can be either:
// - A navigation node (children non-empty, action empty)
// - A final leaf (children empty, action set)
// - A final leaf with multiple style variants (HasStyles() == true)
struct UniversalSlot {
	std::string id;
	std::string name;
	std::string display_name;
	std::vector<UniversalSlot> children;
	std::vector<std::pair<std::string, std::any>> data;
	std::function<void(const UniversalSlot&, ActionContext&)> action;

	// When populated (size > 0), the slot represents a single cosmetic with
	// multiple visual styles. The base slot's modifications/replacements
	// carry the style-independent payload (notably the model swap
	// skin.model_player -> default_slot.model_player from
	// processReplacementsAndModifications); the variant's payload carries
	// style-specific modifications/replacements. At apply time in
	// GUI::VPKWorkerThread the base is layered first and the active variant
	// is layered on top (variant wins on type+what+to collision). Without
	// this layering the base model file would never land in the mod VPK for
	// style skins (e.g. storm_hat.vmdl_c was missing for Storm Spirit's
	// "Lightning Orchid of Eminent Revival" retro immortal). GUI should
	// render a style switcher when styles.size() > 1.
	std::vector<SkinStyleVariant> styles;
	int default_style_index = 0;

	// True when this slot was injected by an active mod via new_skins.
	// Used to remove mod skins when the mod is deactivated or rescanned.
	bool isModSkin = false;

	bool isFinal() const { return !!action; }
	// True when this slot has at least one style variant. At apply time the
	// active variant's payload is layered on top of the base's payload
	// (see `styles` comment above for rationale).
	bool HasStyles() const { return !styles.empty(); }

	// True when this slot is a category node (e.g. "Heroes", "Landscape").
	// Used by the GUI to distinguish root categories from heroes/world slots.
	bool isCategory = false;
	// True when this category contains heroes (as opposed to world items).
	bool isHeroCategory = false;

	// True when this slot is a "persona entry" — the auxiliary navigation
	// node the OutputGenerator emits inside a hero that has personas
	// (one per persona, e.g. "Default" and "Acolyte of the Lost Arts"
	// for Invoker). Persona entries exist purely so the GUI can render
	// the persona-specific loadout slots in their own subtree; the user
	// cannot navigate into them as a separate level and they are hidden
	// from the breadcrumb. The active persona is stored in
	// ThreadSafeDataManager and selected via dots on the hero card.
	bool isPersonaEntry = false;
	int StyleCount() const { return styles.empty() ? 1 : static_cast<int>(styles.size()); }

	// Returns the active style variant (caller resolves index externally) or
	// nullptr if this slot has no styles.
	const SkinStyleVariant* GetStyle(int index) const {
		if (styles.empty()) return nullptr;
		if (index < 0 || index >= static_cast<int>(styles.size())) index = 0;
		return &styles[index];
	}

	const std::string& GetDisplayName() const { return display_name.empty() ? name : display_name; }

    std::string imagePath;
    bool darkenImage = true;
    bool showText = true;
    std::string image_vpk_path;

    // ------------------------------------------------------------------
    // Personas (hero-level alternate views)
    // ------------------------------------------------------------------
    // When populated, the slot represents a hero that has one or more
    // personas. Index 0 is always the default (the original hero), and
    // indices 1..N are custom personas. The active persona index lives
    // outside the tree in ThreadSafeDataManager.
    //
    // The persona entries follow the same recursive shape as children,
    // but we keep them separate so:
    //   - the style-switcher dots render with persona names as tooltips
    //   - the active persona's children are swapped in at click time
    //   - persona slots/skins don't mix with default hero slots
    std::vector<UniversalSlot> personas;
    std::vector<std::string> persona_names; // display names per persona, parallel to personas
    // Optional key (e.g. hero code name) for persisting persona selection
    // across saves/loads. Empty when the slot is not a hero persona switcher.
    std::string persona_key;

    bool HasPersonas() const { return !personas.empty(); }
    int PersonaCount() const { return personas.empty() ? 1 : static_cast<int>(personas.size()); }

    // The persona's effective image (e.g. for thumbnail display in the GUI).
    // Index -1 or out-of-range falls back to this slot's image_vpk_path.
    const std::string& GetPersonaImage(int index) const {
        if (index < 0 || index >= static_cast<int>(personas.size())) return image_vpk_path;
        const std::string& img = personas[index].image_vpk_path;
        return img.empty() ? image_vpk_path : img;
    }

    const std::string& GetPersonaDisplayName(int index) const {
        if (index < 0 || index >= static_cast<int>(persona_names.size())) return name;
        return persona_names[index];
    }
};

// Default marker action for skins parsed from dota_skins.json.
// Does nothing by itself; the VPK worker applies logic based on slot.data.
inline const std::function<void(const UniversalSlot&, ActionContext&)> kDefaultSkinAction =
	[](const UniversalSlot&, ActionContext&) {};

// Payload attached to a persona-specific default slot (e.g.
// "summon_persona_1" for Invoker Kid's Forge Spirits) so the
// always-on visuals (asset_modifier entries that ship in the
// default item's `visuals` block) get applied at VPK build time
// even when the user has not selected a wearable for the slot.
//
// Authored by StyleProcessor::processPersonaDefaultSlotVisuals in
// the parser, serialized into dota_skins.json by
// OutputGenerator::buildPersonaSlotsJson, rehydrated by
// IGParser::ParseSlot into a UniversalSlot::data entry under the
// key "default_slot_visual_payload", and consumed by the persona
// default slot visual pre-pass in GUI::VPKWorkerThread.
//
// IMPORTANT: this struct is shared by IGParser and the VPK worker.
// The previous version had two identical function-local structs
// (one in IGParser::ParseSlot, one in VPKWorkerThread's pre-pass
// block). Function-local types have unique type_info, so the
// `typeid` check in `std::any_cast` never matched and the pre-pass
// silently extracted nothing — the user's persona default visuals
// were generated into dota_skins.json but never applied to
// items_game.txt. Defining the struct here in a shared header
// gives both translation units the same type_info.
struct DefaultSlotVisualPayload {
    int slot_id = -1;
    std::string slot_name;
    std::string display_name;
    std::vector<Modification> modifications;
    std::vector<Replacement> replacements;
    std::vector<HeroScalePayload> hero_scales;
};

// Path to a slot inside the tree: indices at each depth level.
using SlotPath = std::vector<int>;

// The root tree is just a list of top-level slots (heroes).
using SlotTree = std::vector<UniversalSlot>;
