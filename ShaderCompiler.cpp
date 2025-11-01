#include "PCH.h"
#include "ShaderCompiler.h"

ShaderCompiler::ShaderCompiler()
{
    vertexShaderCode = R"(
        cbuffer SceneConstants : register(b0) { float4x4 g_WVP; float4x4 g_World; };
        cbuffer lightConstants : register(b1) { float3 lightPos; float3 lightDir; };
        struct VS_INPUT { float3 pos : POSITION; float4 color : COLOR; float2 tex : TEXCOORD; float3 normal : NORMAL; };
        struct VS_OUTPUT { float4 pos : SV_POSITION; float4 color : COLOR; float2 tex : TEXCOORD; float3 normal_world : NORMAL; };
        VS_OUTPUT VSMain(VS_INPUT input)
        {
            VS_OUTPUT output;
            output.pos = mul(g_WVP, float4(input.pos, 1.0f));
            output.normal_world = normalize(mul((float3x3)g_World, input.normal));
            output.color = input.color;
            output.tex = input.tex;
            return output;
        }
    )";
    pixelShaderCode = R"(
        Texture2D g_Texture : register(t0);
        SamplerState g_Sampler : register(s0);
        struct PS_INPUT { float4 pos : SV_POSITION; float4 color : COLOR; float2 tex : TEXCOORD; float3 normal_world : NORMAL; };
        float4 PSMain(PS_INPUT input) : SV_TARGET 
        {
            float3 lightDir = normalize(float3(0.5f, -0.8f, -1.0f));
            float3 lightColor = float3(1.0f, 1.0f, 0.9f);
            float ambient = 0.1f;
            float diffuse = saturate(dot(input.normal_world, -lightDir));
            float4 texColor = g_Texture.Sample(g_Sampler, input.tex);
            float3 finalColor = texColor.rgb * input.color.rgb;
            finalColor = finalColor * (ambient + (diffuse * lightColor));
            return float4(finalColor, texColor.a);
        }
    )";
    vertexShaderCodeLen = vertexShaderCode.size();
    pixelShaderCodeLen = pixelShaderCode.size();
}

void ShaderCompiler::CompileShader()
{
    D3DCompile(GetVertexShaderCode(), vertexShaderCodeLen, nullptr, nullptr, nullptr,
        entryPoint[0].c_str(), "vs_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0,
        &vertexShader, &vertexShaderError);

    D3DCompile(GetPixelShaderCode(), pixelShaderCodeLen, nullptr, nullptr, nullptr,
        entryPoint[1].c_str(), "ps_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0,
        &pixelShader, &pixelShaderError);
}

const char* ShaderCompiler::GetVertexShaderCode()
{
    return vertexShaderCode.c_str();
}

const char* ShaderCompiler::GetPixelShaderCode()
{
    return pixelShaderCode.c_str();
}

