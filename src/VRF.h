#pragma once

#include <iostream>
#include <string>
#include <curl/curl.h>
#include <zip.h>
#include <filesystem>
#include "SecurityHardening.h"

namespace fs = std::filesystem;

class VRF
{
public:
	static VRF& GetInstance();

    bool Setup();
    static bool IsSetupNeeded();
    void Decompile(std::string args);
    bool DecompileWithOutput(std::string args, std::string& outLog);
    void Decompile(std::string input, std::string output, std::string args = OBF_CSTR(""));
    bool DecompileWithOutput(std::string input, std::string output, std::string args, std::string& outLog);
    bool DecompileBlock(std::string input, std::string block_name, std::string& output, std::string args = OBF_CSTR(""));

	static void TerminateLingeringDecompilerProcesses();
    static void TerminateAllDecompilerProcesses();
    static std::string GenerateUniqueSuffix();

private:
	VRF() = default;
	~VRF() = default;

	VRF(const VRF&) = delete;
	VRF& operator=(const VRF&) = delete;
};


