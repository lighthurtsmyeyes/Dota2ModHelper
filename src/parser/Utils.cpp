#include "Utils.h"
#include "Logger.h"
#include <map>


namespace skin_parser {

    void Utils::quickSaveToFile(const std::string& content, const std::string& filename, bool append) {
        try {
            std::filesystem::path file_path(filename);
            std::filesystem::create_directories(file_path.parent_path());

            std::ios_base::openmode mode = std::ios::out;
            if (append) {
                mode |= std::ios::app;
            }

            std::ofstream file(filename, mode);
            if (!file.is_open()) {
                std::cerr << "ERROR: Cannot open file for quick save: " << filename << std::endl;
                return;
            }

            file << content;
            file.close();
        }
        catch (const std::exception& e) {
            std::cerr << "Error saving to " << filename << ": " << e.what() << std::endl;
        }
    }

    std::string Utils::readFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file: " + filename);
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    std::string Utils::normalizeHeroName(const std::string& name) {
        static const std::map<std::string, std::string> name_mappings = {
            {"windranger", "wind ranger"},
            {"centaur", "centaur warruner"},
            {"centaur warruner's", "centaur warruner"},
            {"enigma's", "enigma"},
            {"beastmaster's", "beastmaster"},
            {"bloodseeker", "blood seeker"},
            {"antimage", "anti-mage"},
            {"wind ranger", "windranger"},
            {"centaur warruner", "centaur"},
            {"blood seeker", "bloodseeker"},
            {"anti-mage", "antimage"},
            {"necrophos'", "necrophos"},
            {"necrophos's", "necrophos"}
        };

        std::string lower_name = toLower(name);
        auto it = name_mappings.find(lower_name);
        if (it != name_mappings.end()) {
            return it->second;
        }
        return name;
    }

    std::string Utils::normalizeSlotName(const std::string& slot_name) {
        std::string normalized = slot_name;

        size_t persona_pos = normalized.find("_persona_");
        if (persona_pos != std::string::npos) {
            normalized = normalized.substr(0, persona_pos);
        }

        std::vector<std::string> suffixes = { "_persona", "_alt", "_variant", "_style" };
        for (const std::string& suffix : suffixes) {
            size_t pos = normalized.find(suffix);
            if (pos != std::string::npos) {
                normalized = normalized.substr(0, pos);
                break;
            }
        }

        return normalized;
    }

    std::string Utils::extractSlotTypeFromName(const std::string& slot_name, const std::string& hero_name) {
        std::string normalized_slot_name = toLower(slot_name);
        std::string normalized_hero_name = toLower(hero_name);

        std::vector<std::string> patterns_to_remove = {
            normalized_hero_name + "'s ",
            normalized_hero_name + "' ",
            normalized_hero_name + " ",
            normalized_hero_name + "'s",
            normalized_hero_name + "'",
            normalized_hero_name
        };

        for (const std::string& pattern : patterns_to_remove) {
            size_t pos = normalized_slot_name.find(pattern);
            if (pos != std::string::npos) {
                normalized_slot_name.erase(pos, pattern.length());
                break;
            }
        }

        normalized_slot_name = std::regex_replace(normalized_slot_name, std::regex("^\\s+"), "");
        normalized_slot_name = std::regex_replace(normalized_slot_name, std::regex("\\s+$"), "");
        normalized_slot_name = std::regex_replace(normalized_slot_name, std::regex("'"), "");

        if (normalized_slot_name == "sickle") return "weapon";
        if (normalized_slot_name == "meathook") return "weapon";
        if (normalized_slot_name == "hook") return "weapon";
        if (normalized_slot_name == "reel") return "weapon";
        if (normalized_slot_name == "mantle") return "back";
        if (normalized_slot_name == "shroud") return "back";

        return normalized_slot_name;
    }

    std::string Utils::addCSuffix(const std::string& path) {
        if (path.empty()) return path;

        if (path.length() >= 2 && path.substr(path.length() - 2) == "_c") {
            return path;
        }

        std::vector<std::string> extensions = { ".vmat", ".vmdl", ".vpcf", ".vsnd", ".vtex" };
        for (const std::string& ext : extensions) {
            if (path.length() > ext.length() && path.substr(path.length() - ext.length()) == ext) {
                return path + "_c";
            }
        }

        return path;
    }

    bool Utils::isModelPath(const std::string& path) {
        if (path.empty()) return false;
        if (path.length() >= 5 && path.substr(path.length() - 5) == ".vmdl") return true;
        if (path.length() >= 7 && path.substr(path.length() - 7) == ".vmdl_c") return true;
        return false;
    }

    bool Utils::isIrrelevantSlot(const std::string& slot_name, const std::string& slot_text) {
        static const std::vector<std::string> irrelevant_slots = {
            "taunt", "hero_effigy", "pet", "courier", "ward", "ambient_effect", "ambient_effects",
            "hud_skin", "music", "weather", "terrain", "loading_screen",
            "emoticon", "announcer", "cursor_pack", "megakill", "ability_effects",
            "bundle", "bundle_", "persona", "emote"
        };

        std::string lower_name = toLower(slot_name);
        std::string lower_text = toLower(slot_text);

        for (const auto& irrelevant : irrelevant_slots) {
            if (lower_name.find(irrelevant) != std::string::npos) {
                return true;
            }
        }

        if (lower_text.find("#loadoutslot_pet") != std::string::npos ||
            lower_text.find("pet") != std::string::npos ||
            lower_text.find("taunt") != std::string::npos ||
            lower_text.find("effigy") != std::string::npos) {
            return true;
        }

        return false;
    }

    bool Utils::isPersonaSkin(const std::string& name, const std::string& item_slot,
        const std::string& model_player, const std::string& image_inventory) {
        std::string lower_name = toLower(name);
        std::string lower_item_slot = toLower(item_slot);
        std::string lower_model = toLower(model_player);
        std::string lower_image = toLower(image_inventory);

        std::vector<std::string> persona_indicators = {
            "_persona_", "persona_", "_persona", "persona"
        };

        for (const std::string& indicator : persona_indicators) {
            if (lower_item_slot.find(indicator) != std::string::npos ||
                lower_name.find(indicator) != std::string::npos ||
                lower_model.find(indicator) != std::string::npos ||
                lower_image.find(indicator) != std::string::npos) {
                return true;
            }
        }

        return false;
    }

    std::string Utils::toLower(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    }

} // namespace skin_parser