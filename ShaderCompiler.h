#pragma once
#include "PCH.h"
class ShaderCompiler {
public:
    // ** 修改: 参数变为 const char* **
    void Init(std::vector<const char*> entryPoints);

    // ** 修改: 参数变为 const char* **
    void AddShaderEntryPoints(std::vector<const char*> entryPoints);

    void CompileShader();

    // ** 修改: 参数变为 const char* **
    ComPtr<ID3DBlob> GetShader(const char* name);

    // (旧的 vertexShader / pixelShader 成员应该已经被移除了)
private:
    // ** 修改: 键类型变为 std::string **
    std::unordered_map< std::string, ComPtr<ID3DBlob> > m_Shaders;

    // ** 修改: 成员类型变为 const char* **
    std::vector<const char*> m_EntryPoints;
};