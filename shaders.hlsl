// shaders.hlsl

// -----------------------------------------------------------------
// 1. 常量缓冲区 (Constant Buffers)
// -----------------------------------------------------------------

// (主通道使用)
// 匹配 C++ 中的 SceneConstants 结构体
cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 wvpMatrix; // 主摄像机 WVP
    float4x4 worldMatrix; // 世界矩阵
    float4x4 lightWvpMatrix; // 光源 WVP (用于阴影)
    float4 textureInfo;
};

// (阴影通道使用)
// 匹配 C++ 中的 ShadowConstants 结构体
cbuffer ShadowConstantBuffer : register(b0) // (Slot 仍然是 b0，因为根签名不同)
{
    float4x4 shadowLightWvpMatrix;
};

// (光照结构体)
cbuffer LightConstantBuffer : register(b1)
{
    float3 lightPos;
    float pad1;
    float3 lightDir;
    float pad2;
};

// -----------------------------------------------------------------
// 2. 资源 (Textures & Samplers)
// -----------------------------------------------------------------

// 主纹理 (t0)
Texture2D g_Texture : register(t0);
// 阴影贴图 (t1)
Texture2D g_ShadowMap : register(t1);

// 主纹理采样器 (s0)
SamplerState g_SamplerLinear : register(s0);
// 阴影贴图比较采样器 (s1)
SamplerComparisonState g_SamplerCmp : register(s1);


// -----------------------------------------------------------------
// 3. 结构体 (Structs)
// -----------------------------------------------------------------

// C++ 顶点输入 (匹配你的 Input Layout)
struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
    float2 texcoord : TEXCOORD; // (在 CreatePSO 中添加)
    float3 normal : NORMAL; // (在 CreatePSO 中添加)
};

// VS -> PS (主通道)
struct PSInput
{
    float4 position : SV_Position; // 摄像机裁剪空间
    float4 color : COLOR;
    float2 texcoord : TEXCOORD0;
    float4 lightSpacePos : TEXCOORD1; // 光源裁剪空间
};

// VS -> PS (阴影通道)
struct ShadowPSInput
{
    // 阴影通道只需要位置
    float4 position : SV_Position; // 光源裁剪空间
};


// -----------------------------------------------------------------
// 4. 阴影通道着色器 (Shadow Pass)
// -----------------------------------------------------------------

// 阴影通道 - 顶点着色器
ShadowPSInput ShadowVS(VSInput input)
{
    ShadowPSInput output;
    
    // 只使用光源 WVP 矩阵转换顶点
    output.position = mul(shadowLightWvpMatrix, float4(input.position, 1.0f));
    
    return output;
}

// 阴影通道 - 像素着色器
// (我们不需要写任何颜色，只需要深度)
// (返回 void 告诉硬件我们不输出颜色)
void ShadowPS(ShadowPSInput input)
{
    // (硬件会自动将 input.position.z 写入深度缓冲区)
}


// -----------------------------------------------------------------
// 5. 主通道着色器 (Main Pass)
// -----------------------------------------------------------------

// 主通道 - 顶点着色器
PSInput VSMain(VSInput input)
{
    PSInput output;
    
    // 转换到摄像机裁剪空间 (用于光栅化)
    output.position = mul(wvpMatrix, float4(input.position, 1.0f));
    
    // 转换到光源裁剪空间 (用于阴影采样)
    output.lightSpacePos = mul(lightWvpMatrix, float4(input.position, 1.0f));

    //传递其他数据
    output.color = input.color;
    output.texcoord = input.texcoord;

    return output;
}


// 主通道 - 像素着色器
float4 PSMain(PSInput input) : SV_Target
{
    // 获取基础颜色
    float4 baseColor = input.color;
    if (textureInfo.x == 1.0f)
    {
        //采样贴图
        baseColor = g_Texture.Sample(g_SamplerLinear, input.texcoord);
    }
    else
    {
        // 使用顶点颜色
        baseColor = input.color;
    }

    // 计算阴影
    float shadowFactor = 1.0;

    float3 lightNdc = input.lightSpacePos.xyz / input.lightSpacePos.w;
    float2 shadowTexCoord;
    shadowTexCoord.x = 0.5f * lightNdc.x + 0.5f;
    shadowTexCoord.y = -0.5f * lightNdc.y + 0.5f;
    float fragmentDepth = lightNdc.z;
    
    shadowFactor = g_ShadowMap.SampleCmp(g_SamplerCmp, shadowTexCoord, fragmentDepth);

    if (saturate(shadowTexCoord.x) != shadowTexCoord.x || saturate(shadowTexCoord.y) != shadowTexCoord.y)
    {
        shadowFactor = 1.0;
    }


    // 组合最终颜色
    //    这可以确保即使物体处于阴影中 (shadowFactor = 0.0)，
    float3 ambientLight = float3(0.2f, 0.2f, 0.2f); // 20% 的基础亮度

    // - 被照亮时: (0.2 + 1.0) * baseColor -> 超过 1.0
    // - 在阴影时: (0.2 + 0.0) * baseColor -> 0.2 * baseColor (深灰色)
    float3 finalColor = baseColor.rgb * (ambientLight + shadowFactor);
    
    // 使用 saturate() 来防止颜色过曝
    finalColor = saturate(finalColor);

    return float4(finalColor, baseColor.a);
}