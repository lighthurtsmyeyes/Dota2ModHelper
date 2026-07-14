#include "Utils.h"
#include "Logger.h"
#include <map>
#include "../SecurityHardening.h"
namespace {
__declspec(noinline) void SH_AD_Utils() noexcept {
    if (SH_PebBeingDebugged()) g_integritySeed ^= 0xAABBCCDD;
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
}
}


namespace skin_parser {

    void Utils::quickSaveToFile(const std::string& content, const std::string& filename, bool append) {
    SH_AD_Utils();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        try {
            std::filesystem::path file_path(filename);
            std::filesystem::create_directories(file_path.parent_path());

            std::ios_base::openmode mode = std::ios::out;
            if (append) {
                mode |= std::ios::app;
            }

            std::ofstream file(filename, mode);
            if (!file.is_open()) {
                std::cerr << OBF_CSTR("ERROR: Cannot open file for quick save: ") << filename << std::endl;
                return;
            }

            file << content;
            file.close();
        }
        catch (const std::exception& e) {
            std::cerr << OBF_CSTR("Error saving to ") << filename << OBF_CSTR(": ") << e.what() << std::endl;
        }
    }

    std::string Utils::readFile(const std::string& filename) {
    SH_AD_Utils();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error(OBF_CSTR("Cannot open file: ") + filename);
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    std::string Utils::normalizeHeroName(const std::string& name) {
    SH_AD_Utils();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        static const std::map<std::string, std::string> name_mappings = {
            {OBF_CSTR("windranger"), OBF_CSTR("wind ranger")},
            {OBF_CSTR("centaur"), OBF_CSTR("centaur warruner")},
            {OBF_CSTR("centaur warruner's"), OBF_CSTR("centaur warruner")},
            {OBF_CSTR("enigma's"), OBF_CSTR("enigma")},
            {OBF_CSTR("beastmaster's"), OBF_CSTR("beastmaster")},
            {OBF_CSTR("bloodseeker"), OBF_CSTR("blood seeker")},
            {OBF_CSTR("antimage"), OBF_CSTR("anti-mage")},
            {OBF_CSTR("wind ranger"), OBF_CSTR("windranger")},
            {OBF_CSTR("centaur warruner"), OBF_CSTR("centaur")},
            {OBF_CSTR("blood seeker"), OBF_CSTR("bloodseeker")},
            {OBF_CSTR("anti-mage"), OBF_CSTR("antimage")},
            {OBF_CSTR("necrophos'"), OBF_CSTR("necrophos")},
            {OBF_CSTR("necrophos's"), OBF_CSTR("necrophos")}
        };

        std::string lower_name = toLower(name);
        auto it = name_mappings.find(lower_name);
        if (it != name_mappings.end()) {
            return it->second;
        }
        return name;
    }

    std::string Utils::normalizeSlotName(const std::string& slot_name) {
    SH_AD_Utils();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        std::string normalized = slot_name;

        size_t persona_pos = normalized.find(OBF_CSTR("_persona_"));
        if (persona_pos != std::string::npos) {
            normalized = normalized.substr(0, persona_pos);
        }

        std::vector<std::string> suffixes = { OBF_CSTR("_persona"), OBF_CSTR("_alt"), OBF_CSTR("_variant"), OBF_CSTR("_style") };
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
    SH_AD_Utils();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        std::string normalized_slot_name = toLower(slot_name);
        std::string normalized_hero_name = toLower(hero_name);

        std::vector<std::string> patterns_to_remove = {
            normalized_hero_name + OBF_CSTR("'s "),
            normalized_hero_name + OBF_CSTR("' "),
            normalized_hero_name + OBF_CSTR(" "),
            normalized_hero_name + OBF_CSTR("'s"),
            normalized_hero_name + OBF_CSTR("'"),
            normalized_hero_name
        };

        for (const std::string& pattern : patterns_to_remove) {
            size_t pos = normalized_slot_name.find(pattern);
            if (pos != std::string::npos) {
                normalized_slot_name.erase(pos, pattern.length());
                break;
            }
        }

        normalized_slot_name = std::regex_replace(normalized_slot_name, std::regex(OBF_CSTR("^\\s+")), OBF_CSTR(""));
        normalized_slot_name = std::regex_replace(normalized_slot_name, std::regex(OBF_CSTR("\\s+$")), OBF_CSTR(""));
        normalized_slot_name = std::regex_replace(normalized_slot_name, std::regex(OBF_CSTR("'")), OBF_CSTR(""));

        if (normalized_slot_name == OBF_CSTR("sickle")) return OBF_CSTR("weapon");
        if (normalized_slot_name == OBF_CSTR("meathook")) return OBF_CSTR("weapon");
        if (normalized_slot_name == OBF_CSTR("hook")) return OBF_CSTR("weapon");
        if (normalized_slot_name == OBF_CSTR("reel")) return OBF_CSTR("weapon");
        if (normalized_slot_name == OBF_CSTR("mantle")) return OBF_CSTR("back");
        if (normalized_slot_name == OBF_CSTR("shroud")) return OBF_CSTR("back");

        return normalized_slot_name;
    }

    std::string Utils::addCSuffix(const std::string& path) {
    SH_AD_Utils();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        if (path.empty()) return path;

        if (path.length() >= 2 && path.substr(path.length() - 2) == OBF_CSTR("_c")) {
            return path;
        }

        std::vector<std::string> extensions = { OBF_CSTR(".vmat"), OBF_CSTR(".vmdl"), OBF_CSTR(".vpcf"), OBF_CSTR(".vsnd"), OBF_CSTR(".vtex") };
        for (const std::string& ext : extensions) {
            if (path.length() > ext.length() && path.substr(path.length() - ext.length()) == ext) {
                return path + OBF_CSTR("_c");
            }
        }

        return path;
    }

    bool Utils::isModelPath(const std::string& path) {
    SH_AD_Utils();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        if (path.empty()) return false;
        if (path.length() >= 5 && path.substr(path.length() - 5) == OBF_CSTR(".vmdl")) return true;
        if (path.length() >= 7 && path.substr(path.length() - 7) == OBF_CSTR(".vmdl_c")) return true;
        return false;
    }

    bool Utils::isIrrelevantSlot(const std::string& slot_name, const std::string& slot_text) {
    SH_AD_Utils();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        static const std::vector<std::string> irrelevant_slots = {
            OBF_CSTR("taunt"), OBF_CSTR("hero_effigy"), OBF_CSTR("pet"), OBF_CSTR("courier"), OBF_CSTR("ward"), OBF_CSTR("ambient_effect"), OBF_CSTR("ambient_effects"),
            OBF_CSTR("hud_skin"), OBF_CSTR("music"), OBF_CSTR("weather"), OBF_CSTR("terrain"), OBF_CSTR("loading_screen"),
            OBF_CSTR("emoticon"), OBF_CSTR("announcer"), OBF_CSTR("cursor_pack"), OBF_CSTR("megakill"), OBF_CSTR("ability_effects"),
            OBF_CSTR("bundle"), OBF_CSTR("bundle_"), OBF_CSTR("persona"), OBF_CSTR("emote")
        };

        std::string lower_name = toLower(slot_name);
        std::string lower_text = toLower(slot_text);

        for (const auto& irrelevant : irrelevant_slots) {
            if (lower_name.find(irrelevant) != std::string::npos) {
                return true;
            }
        }

        if (lower_text.find(OBF_CSTR("#loadoutslot_pet")) != std::string::npos ||
            lower_text.find(OBF_CSTR("pet")) != std::string::npos ||
            lower_text.find(OBF_CSTR("taunt")) != std::string::npos ||
            lower_text.find(OBF_CSTR("effigy")) != std::string::npos) {
            return true;
        }

        return false;
    }

    bool Utils::isPersonaSkin(const std::string& name, const std::string& item_slot,
        const std::string& model_player, const std::string& image_inventory) {
    SH_AD_Utils();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        std::string lower_name = toLower(name);
        std::string lower_item_slot = toLower(item_slot);
        std::string lower_model = toLower(model_player);
        std::string lower_image = toLower(image_inventory);

        std::vector<std::string> persona_indicators = {
            OBF_CSTR("_persona_"), OBF_CSTR("persona_"), OBF_CSTR("_persona"), OBF_CSTR("persona")
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
    SH_AD_Utils();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    }

} // namespace skin_parser