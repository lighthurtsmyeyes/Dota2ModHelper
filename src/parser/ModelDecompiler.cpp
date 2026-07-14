#include "ModelDecompiler.h"
#include "Logger.h"
#include "Utils.h"
#include "SkinDataManager.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <thread>
#include <fstream>
#include "../VPKManager.h"
#include "../SteamManager.h"
#include "../VRF.h"
#include "../SecurityHardening.h"
namespace {
__declspec(noinline) void SH_AD_ModelDecompiler() noexcept {
    if (SH_DebugPort()) g_integritySeed ^= 0xDDEEFF00;
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
}
}


namespace fs = std::filesystem;

namespace skin_parser {

    static std::atomic<uint64_t> g_modelTempCounter{ 0 };

    std::string StubModelDecompiler::decompileModel(const std::string& model_path) {
    SH_AD_ModelDecompiler();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        auto& logger = Logger::instance();
        logger.log(OBF_CSTR("> Decompiling ") + model_path);

        fs::path data_path = OBF_CSTR("decompiled_models/") + model_path + OBF_CSTR(".data");
        if (fs::exists(data_path)) {
            logger.log(OBF_CSTR("> Using existing data from ") + model_path + OBF_CSTR(".data"));
            return Utils::readFile(data_path.string());
        }

        fs::path fp(model_path);
        fp.replace_extension(OBF_CSTR(".vmdl_c"));

        auto getResult = VPKManager::GetInstance().GetFileFromVPK(SteamManager::GetInstance().vpkPath, fp.string());

        if (getResult.IsErr()) {
            return OBF_CSTR("");
        }

        // Use a per-call unique temp file so parallel model decompiles never
        // overwrite each other's input/output.
        uint64_t uniqueId = g_modelTempCounter.fetch_add(1);
        fs::path tempDir = fs::path(OBF_CSTR("temp")) / (OBF_CSTR("model_decompile_") + std::to_string(uniqueId));
        std::error_code mkdirEc;
        fs::create_directories(tempDir, mkdirEc);
        std::string tempModelPath = (tempDir / OBF_CSTR("model.vmdl_c")).string();
        std::string tempOutputPath = (tempDir / OBF_CSTR("model.data")).string();

        FileData _temp = getResult.Value();
        VPKManager::GetInstance().SaveFileFromData(_temp, tempModelPath);

        std::string temp;
        VRF::GetInstance().TerminateLingeringDecompilerProcesses();
        VRF::GetInstance().DecompileBlock(tempModelPath, OBF_CSTR("DATA"), temp, OBF_CSTR("-o \"") + tempOutputPath + OBF_CSTR("\""));
        if (fs::exists(tempOutputPath)) {
            try {
                temp = Utils::readFile(tempOutputPath);
            }
            catch (const std::exception& e) {
                logger.log(OBF_CSTR("WARNING: Failed to read temp block output: ") + std::string(e.what()));
            }
        }
        Utils::quickSaveToFile(temp, data_path.string());

        {
            std::error_code rmEc;
            for (int i = 0; i < 10; ++i) {
                fs::remove_all(tempDir, rmEc);
                if (!rmEc) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }

        return temp;
    }

    bool decompileAndParseModel(const std::string& model_path) {
    SH_AD_ModelDecompiler();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        auto& logger = Logger::instance();
        auto& data_manager = SkinDataManager::instance();
        auto& decompiled_models = data_manager.decompiledModels();

        if (decompiled_models.find(model_path) != decompiled_models.end() &&
            decompiled_models[model_path].is_loaded) {
            return true;
        }

        if (!data_manager.hasModelDecompiler()) {
            logger.log(OBF_CSTR("ERROR: Model decompiler not initialized for: ") + model_path);
            return false;
        }

        auto* decompiler = data_manager.getModelDecompiler();
        if (!decompiler) {
            logger.log(OBF_CSTR("ERROR: Model decompiler is null for: ") + model_path);
            return false;
        }

        try {
            logger.log(OBF_CSTR("Decompiling model: ") + model_path);

            std::string data_content = decompiler->decompileModel(model_path);

            if (data_content.empty()) {
                logger.log(OBF_CSTR("WARNING: Empty decompilation result for: ") + model_path);
                return false;
            }

            DecompiledModel& model = decompiled_models[model_path];
            model.model_path = model_path;
            model.data_content = data_content;

            parseMaterialGroupsFromDecompiledData(model);

            model.is_loaded = true;
            logger.log(OBF_CSTR("Successfully decompiled and parsed model: ") + model_path +
                OBF_CSTR(" (") + std::to_string(model.material_groups.size()) + OBF_CSTR(" material groups)"));

            return true;
        }
        catch (const std::exception& e) {
            logger.log(OBF_CSTR("ERROR decompiling model ") + model_path + OBF_CSTR(": ") + std::string(e.what()));
            return false;
        }
    }

    void parseMaterialGroupsFromDecompiledData(DecompiledModel& model) {
    SH_AD_ModelDecompiler();
    volatile int _sh_decoy = static_cast<int>(__rdtsc() & 0xF);
    volatile int _sh_junk = 0;
    for (int _sh_i = 0; _sh_i < _sh_decoy + 1; ++_sh_i) _sh_junk ^= _sh_i * 0x66661;
    (void)_sh_junk;
        auto& logger = Logger::instance();

        try {
            const std::string& data_content = model.data_content;
            size_t pos = 0;

            while ((pos = data_content.find(OBF_CSTR("m_materialGroups"), pos)) != std::string::npos) {
                pos = data_content.find(OBF_CSTR("["), pos);
                if (pos == std::string::npos) break;

                int bracket_count = 1;
                size_t group_start = pos;
                size_t group_end = group_start + 1;

                while (group_end < data_content.length() && bracket_count > 0) {
                    if (data_content[group_end] == '[') bracket_count++;
                    else if (data_content[group_end] == ']') bracket_count--;
                    group_end++;
                }

                if (bracket_count != 0) break;

                std::string groups_str = data_content.substr(group_start, group_end - group_start);
                size_t group_pos = 0;

                while ((group_pos = groups_str.find(OBF_CSTR("m_name"), group_pos)) != std::string::npos) {
                    MaterialGroup material_group;

                    group_pos = groups_str.find(OBF_CSTR("\""), group_pos);
                    if (group_pos == std::string::npos) break;

                    size_t name_start = group_pos + 1;
                    size_t name_end = groups_str.find(OBF_CSTR("\""), name_start);
                    if (name_end == std::string::npos) break;

                    material_group.name = groups_str.substr(name_start, name_end - name_start);

                    size_t materials_pos = groups_str.find(OBF_CSTR("m_materials"), name_end);
                    if (materials_pos == std::string::npos) break;

                    materials_pos = groups_str.find(OBF_CSTR("["), materials_pos);
                    if (materials_pos == std::string::npos) break;

                    int materials_bracket_count = 1;
                    size_t materials_start = materials_pos;
                    size_t materials_end = materials_start + 1;

                    while (materials_end < groups_str.length() && materials_bracket_count > 0) {
                        if (groups_str[materials_end] == '[') materials_bracket_count++;
                        else if (groups_str[materials_end] == ']') materials_bracket_count--;
                        materials_end++;
                    }

                    if (materials_bracket_count != 0) break;

                    std::string materials_array = groups_str.substr(materials_start, materials_end - materials_start);
                    size_t resource_pos = 0;

                    while ((resource_pos = materials_array.find(OBF_CSTR("resource:\""), resource_pos)) != std::string::npos) {
                        size_t path_start = resource_pos + 10;
                        size_t path_end = materials_array.find(OBF_CSTR("\""), path_start);
                        if (path_end == std::string::npos) break;

                        std::string material_path = materials_array.substr(path_start, path_end - path_start);
                        material_group.materials.push_back(material_path);
                        resource_pos = path_end + 1;
                    }

                    model.material_groups.push_back(material_group);
                    logger.log(OBF_CSTR("Found material group '") + material_group.name +
                        OBF_CSTR("' with ") + std::to_string(material_group.materials.size()) +
                        OBF_CSTR(" materials for model: ") + model.model_path);

                    group_pos = materials_end;
                }

                pos = group_end;
            }
        }
        catch (const std::exception& e) {
            logger.log(OBF_CSTR("ERROR parsing material groups for ") + model.model_path + OBF_CSTR(": ") + std::string(e.what()));
        }
    }

} // namespace skin_parser
