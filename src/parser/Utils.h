#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>
#include <algorithm>
#include <cctype>

namespace skin_parser {

    class Utils {
    public:
        // Работа с файлами
        static void quickSaveToFile(const std::string& content, const std::string& filename, bool append = false);
        static std::string readFile(const std::string& filename);

        // Работа со строками и путями
        static std::string normalizeHeroName(const std::string& name);
        static std::string normalizeSlotName(const std::string& slot_name);
        static std::string extractSlotTypeFromName(const std::string& slot_name, const std::string& hero_name);
        static std::string addCSuffix(const std::string& path);

        // Проверки
        static bool isModelPath(const std::string& path);
        static bool isIrrelevantSlot(const std::string& slot_name, const std::string& slot_text);
        static bool isPersonaSkin(const std::string& name, const std::string& item_slot,
            const std::string& model_player, const std::string& image_inventory);

        // Преобразования
        static std::string toLower(const std::string& str);

    private:
        Utils() = delete; // Статический класс
    };

} // namespace skin_parser