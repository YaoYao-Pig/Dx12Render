#include "PCH.h"
#include "ShaderCompiler.h"

void ShaderCompiler::Init(std::vector<const char*> entryPoints) {
    m_EntryPoints = entryPoints;
}

void ShaderCompiler::AddShaderEntryPoints(std::vector<const char*> entryPoints)
{
    m_EntryPoints.insert(m_EntryPoints.end(), entryPoints.begin(), entryPoints.end());
}

ComPtr<ID3DBlob> ShaderCompiler::GetShader(const char* name)
{
    std::string key(name);

    if (m_Shaders.find(key) != m_Shaders.end())
    {
        return m_Shaders[key];
    }
    return nullptr;
}

void ShaderCompiler::CompileShader() {
#if defined(_DEBUG)
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif

    for (const char* entryPoint : m_EntryPoints)
    {
        ComPtr<ID3DBlob> shaderBlob;
        ComPtr<ID3DBlob> errorBlob;

        std::string entryPointStr(entryPoint);
        std::string target;

        if (entryPointStr.find("VS") != std::string::npos) {
            target = "vs_5_0";
        }
        else if (entryPointStr.find("PS") != std::string::npos) {
            target = "ps_5_0";
        }
        else {
            target = "ps_5_0"; // д╛хо
        }
        HRESULT hr = D3DCompileFromFile(
            L"shaders.hlsl",
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entryPointStr.c_str(),       
            target.c_str(),              
            compileFlags,
            0,
            &shaderBlob,
            &errorBlob
        );

        if (FAILED(hr)) {
            if (errorBlob) {
                OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            }
            ThrowIfFailed(hr);
        }

        m_Shaders[entryPointStr] = shaderBlob;
    }
}

void ShaderCompiler::CompileNewShader(wchar_t* fileName, std::vector<const char*> entryPoints)
{
#if defined(_DEBUG)
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif
    for (const char* entryPoint : entryPoints)
    {
        ComPtr<ID3DBlob> shaderBlob;
        ComPtr<ID3DBlob> errorBlob;

        std::string entryPointStr(entryPoint);
        std::string target;

        if (entryPointStr.find("VS") != std::string::npos) {
            target = "vs_5_0";
        }
        else if (entryPointStr.find("PS") != std::string::npos) {
            target = "ps_5_0";
        }
        else {
            target = "ps_5_0"; // д╛хо
        }
        HRESULT hr = D3DCompileFromFile(
            fileName,
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entryPointStr.c_str(),
            target.c_str(),
            compileFlags,
            0,
            &shaderBlob,
            &errorBlob
        );

        if (FAILED(hr)) {
            if (errorBlob) {
                OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            }
            ThrowIfFailed(hr);
        }

        m_Shaders[entryPointStr] = shaderBlob;
    }
}
