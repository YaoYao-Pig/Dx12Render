#pragma once
#include "PCH.h"
class ShaderCompiler {
public:
    void Init(std::vector<const char*> entryPoints);

    void AddShaderEntryPoints(std::vector<const char*> entryPoints);

    void CompileShader();
    void CompileNewShader(wchar_t* fileName, std::vector<const char*> entryPoints);

    ComPtr<ID3DBlob> GetShader(const char* name);
private:
    std::unordered_map< std::string, ComPtr<ID3DBlob> > m_Shaders;
    std::vector<const char*> m_EntryPoints;
    std::vector<wchar_t*> fileNameList;
};