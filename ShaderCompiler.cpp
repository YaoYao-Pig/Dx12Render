// ShaderCompiler.cpp
#include "PCH.h"
#include "ShaderCompiler.h"

// ** (!!!) 确保你删除了这里所有的 ConvertWideToNarrow 函数 (!!!) **


// ** 修改: 参数变为 const char* **
void ShaderCompiler::Init(std::vector<const char*> entryPoints) {
    m_EntryPoints = entryPoints;
}

// ** 修改: 参数变为 const char* **
void ShaderCompiler::AddShaderEntryPoints(std::vector<const char*> entryPoints)
{
    m_EntryPoints.insert(m_EntryPoints.end(), entryPoints.begin(), entryPoints.end());
}

// ** 修改: 参数变为 const char* **
ComPtr<ID3DBlob> ShaderCompiler::GetShader(const char* name)
{
    // ** 修改: 这是一个简单、安全的 const char* -> std::string 转换 **
    std::string key(name);

    if (m_Shaders.find(key) != m_Shaders.end())
    {
        return m_Shaders[key];
    }
    return nullptr;
}

// ** 修改: CompileShader 函数 **
void ShaderCompiler::CompileShader() {
#if defined(_DEBUG)
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif

    // ** 修改: 遍历 const char* **
    for (const char* entryPoint : m_EntryPoints)
    {
        ComPtr<ID3DBlob> shaderBlob;
        ComPtr<ID3DBlob> errorBlob;

        // ** 修改: 全部使用 std::string **
        std::string entryPointStr(entryPoint);
        std::string target;

        // ** 修改: 使用窄字符串 "VS" 和 "PS" **
        if (entryPointStr.find("VS") != std::string::npos) {
            target = "vs_5_0";
        }
        else if (entryPointStr.find("PS") != std::string::npos) {
            target = "ps_5_0";
        }
        else {
            target = "ps_5_0"; // 默认
        }

        // ** 关键: **
        // pFileName *必须* 是 LPCWSTR (宽字符)
        // pEntrypointName 和 pTarget *必须* 是 LPCSTR (窄字符)
        // 我们现在不再需要任何转换！
        HRESULT hr = D3DCompileFromFile(
            L"shaders.hlsl",             // 1. (宽) 文件名, 我们保持L"..."
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entryPointStr.c_str(),       // 2. (窄) 入口点, 直接使用 .c_str()
            target.c_str(),              // 3. (窄) 目标, 直接使用 .c_str()
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

        // ** 修改: 使用 std::string 作为键 **
        m_Shaders[entryPointStr] = shaderBlob;
    }
}