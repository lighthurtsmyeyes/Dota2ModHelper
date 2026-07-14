#pragma once

#include <string>
#include <memory>
#include <vector>

namespace skin_parser {

    class ModelDecompiler {
    public:
        virtual ~ModelDecompiler() = default;
        virtual std::string decompileModel(const std::string& model_path) = 0;
    };

    class StubModelDecompiler : public ModelDecompiler {
    public:
        std::string decompileModel(const std::string& model_path) override;
    };

    // Вспомогательные функции для работы с декомпилированными моделями
    bool decompileAndParseModel(const std::string& model_path);
    void parseMaterialGroupsFromDecompiledData(struct DecompiledModel& model);

} // namespace skin_parser