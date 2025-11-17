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
    float4 textureInfo; // .x: 1.0f = use texture, 0.0f = use vertex color
    float3 eyePosW; // 摄像机世界坐标
};

// 阴影通道使用
// 匹配 C++ 中的 ShadowConstants 结构体
cbuffer ShadowConstantBuffer : register(b0)
{
    float4x4 shadowLightWvpMatrix;
};


// 光照结构体 (b1)
//匹配 C++ 中的 Light
struct DirectionalLight
{
    float3 Color;
    float Intensity;
    float3 Direction;
    float Pad;
};

struct PointLight
{
    float3 Color;
    float Intensity;
    float3 Position;
    float Radius;
};

struct SpotLight
{
    float3 Color;
    float Intensity;
    float3 Position;
    float Radius;
    float3 Direction;
    float InnerCutOff; // cos(angle)
    float OuterCutOff; // cos(angle)
    float3 Pad;
};

// 主通道使用)
// 匹配 C++ 中的 LightingConstants 结构体
cbuffer LightingConstantBuffer : register(b1)
{
    DirectionalLight DirLight;
    PointLight pointLight;
    SpotLight spotLight;
    float3 AmbientLight;
};

//资源 (Textures & Samplers)
Texture2D g_AlbedoMap : register(t0);
Texture2D g_MetallicMap : register(t1);
Texture2D g_RoughnessMap : register(t2);
Texture2D g_AoMap : register(t3);
Texture2D g_NormalMap : register(t4);
Texture2D g_ShadowMap : register(t5);

Texture2D g_BrdfLutTexture : register(t7); // <-- 添加 IBL
TextureCube g_IrradianceMap : register(t8); // <-- 添加 IBL

SamplerState g_SamplerMain : register(s0);
SamplerComparisonState g_SamplerCmp : register(s1);


// C++ 顶点输入 (同前)
struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
    float2 texcoord : TEXCOORD;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
};

struct PSInput
{
    float4 position : SV_Position; // 摄像机裁剪空间
    float4 color : COLOR;
    float2 texcoord : TEXCOORD0;
    float4 lightSpacePos : TEXCOORD1; // 光源裁剪空间 (用于阴影)
    float3 worldPos : TEXCOORD2; // 世界坐标
    float3 normalW : TEXCOORD3; // 世界法线
    float3 tangentW : TEXCOORD4;
    float3 bitangentW : TEXCOORD5;
};

// VS -> PS (阴影通道)
struct ShadowPSInput
{
    float4 position : SV_Position;
};


// -阴影通道着色器 (Shadow Pass)
ShadowPSInput ShadowVS(VSInput input)
{
    ShadowPSInput output;
    output.position = mul(shadowLightWvpMatrix, float4(input.position, 1.0f));
    return output;
}

void ShadowPS(ShadowPSInput input)
{
    // 硬件会自动将 input.position.z 写入深度缓冲区
}

// Main Pass
PSInput VSMain(VSInput input)
{
    PSInput output;
    
    // 转换到摄像机裁剪空间 (用于光栅化)
    output.position = mul(wvpMatrix, float4(input.position, 1.0f));
    
    // 转换到光源裁剪空间 (用于阴影采样)
    output.lightSpacePos = mul(lightWvpMatrix, float4(input.position, 1.0f));

    // 计算世界坐标和法线 
    output.worldPos = mul(worldMatrix, float4(input.position, 1.0f)).xyz;
    // 注意: 非均匀缩放需要 (float3x3)transpose(inverse(worldMatrix)) 
    output.normalW = normalize(mul((float3x3) worldMatrix, input.normal));
    output.tangentW = normalize(mul((float3x3) worldMatrix, input.tangent));
    //计算副切线 (Bitangent)
    output.bitangentW = normalize(cross(output.normalW, output.tangentW));
    //传递其他数据
    output.color = input.color;
    output.texcoord = input.texcoord;

    return output;
}

static const float PI = 3.14159265359f;

float3 GetNormalFromMap(float3 N, float3 T, float3 B, float2 uv)
{
    // 1. 从法线贴图采样
    // (法线贴图存储的是切线空间法线)
    // .rgb 因为我们不需要 alpha
    float3 normalT = g_NormalMap.Sample(g_SamplerMain, uv).rgb;
    
    // 2. 将法线从 [0, 1] 范围重映射到 [-1, 1] 范围
    normalT = (normalT * 2.0) - 1.0;

    // 3. 构建 TBN 矩阵 (从切线空间 -> 世界空间)
    // (T, B, N 必须是正交的)
    float3x3 TBN = float3x3(
        normalize(T),
        normalize(B),
        normalize(N)
    );

    // 4. 将法线从切线空间转换到世界空间
    float3 normalW = normalize(mul(normalT, TBN));
    
    return normalW;
}

// D:法线分布 (Normal Distribution Function) - GGX/Trowbridge-Reitz
// H: 半程向量, N: 法线, roughness: 粗糙度
float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = saturate(dot(N, H));
    float NdotH2 = NdotH * NdotH;
    
    float num = a2;
    float den = (NdotH2 * (a2 - 1.0) + 1.0);
    den = PI * den * den;
    
    return num / den;
}

// G: 几何遮蔽 (Geometry Function) - Schlick-GGX
// N: 法线, V: 视图向量, k: (roughness+1)^2 / 8 (直接光) 或 roughness^2 / 2 (IBL)
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    
    float num = NdotV;
    float den = NdotV * (1.0 - k) + k;
    
    return num / den;
}

// G (Smith's method): G = G_SchlickGGX(N, V, k) * G_SchlickGGX(N, L, k)
float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

// F: 菲涅尔方程 (Fresnel Equation) - Schlick aapproximation
// F0: 0.04 (非金属) 到 1.0 (金属) 的基础反射率
float3 fresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

// PBR 光照计算
// L: 光照方向, V: 视图方向, N: 法线, F0: 基础反射率, albedo: 漫反射颜色
float3 CalculatePBR(float3 L, float3 V, float3 N, float3 F0, float3 albedo, float roughness, float metallic, float3 Radiance, float NdotL)
{
    float3 H = normalize(V + L); // 半程向量
    
    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    float3 F = fresnelSchlick(saturate(dot(H, V)), F0);
    
    // 镜面项 (Specular)
    float3 kS = F;
    float3 numerator = NDF * G * F;
    float den = 4.0 * saturate(dot(N, V)) * NdotL + 0.0001; // 0.0001 防止除零
    float3 specular = numerator / den;
    
    // 漫反射项 (Diffuse)
    float3 kD = float3(1.0, 1.0, 1.0) - kS;
    kD *= (1.0 - metallic); // 金属没有漫反射
    
    float3 diffuse = kD * albedo / PI;
    
    // 最终 BRDF
    // (diffuse + specular) * Radiance * NdotL
    return (diffuse + specular) * Radiance * NdotL;
}

// 光源计算函数
// 计算阴影因子
float CalculateShadowFactor(float4 lightSpacePos)
{
    float3 lightNdc = lightSpacePos.xyz / lightSpacePos.w;
    float2 shadowTexCoord;
    shadowTexCoord.x = 0.5f * lightNdc.x + 0.5f;
    shadowTexCoord.y = -0.5f * lightNdc.y + 0.5f;
    float fragmentDepth = lightNdc.z;
    
    float shadowFactor = g_ShadowMap.SampleCmp(g_SamplerCmp, shadowTexCoord, fragmentDepth);
    
    // 边缘处理
    if (saturate(shadowTexCoord.x) != shadowTexCoord.x || saturate(shadowTexCoord.y) != shadowTexCoord.y)
    {
        shadowFactor = 1.0;
    }
    
    return shadowFactor;
}

// 计算方向光
float3 ProcessDirectionalLight(DirectionalLight light, float3 N, float3 V, float3 worldPos, float4 lightSpacePos, float3 F0, float3 albedo, float roughness, float metallic)
{
    float3 L = normalize(-light.Direction); // 方向光 L 是 (to-light)
    float NdotL = saturate(dot(N, L));
    if (NdotL <= 0.0f)
        return float3(0, 0, 0);
        
    float3 Radiance = light.Color * light.Intensity;
    
    // 只有方向光计算阴影
    float shadowFactor = CalculateShadowFactor(lightSpacePos);
    
    return CalculatePBR(L, V, N, F0, albedo, roughness, metallic, Radiance, NdotL) * shadowFactor;
}

// 计算点光源
float3 ProcessPointLight(PointLight light, float3 N, float3 V, float3 worldPos, float3 F0, float3 albedo, float roughness, float metallic)
{
    float3 L = light.Position - worldPos; // (to-light)
    float distance = length(L);
    
    // 如果超出半径，则无贡献
    if (distance > light.Radius)
        return float3(0, 0, 0);
        
    L = normalize(L);
    
    float NdotL = saturate(dot(N, L));
    if (NdotL <= 0.0f)
        return float3(0, 0, 0);
        
    // 物理衰减 (r^2)
    float attenuation = 1.0 / (distance * distance + 0.0001);
    float3 Radiance = light.Color * light.Intensity * attenuation;
    
    return CalculatePBR(L, V, N, F0, albedo, roughness, metallic, Radiance, NdotL);
}

// 计算聚光灯 
float3 ProcessSpotLight(SpotLight light, float3 N, float3 V, float3 worldPos, float3 F0, float3 albedo, float roughness, float metallic)
{
    float3 L = light.Position - worldPos; // (to-light)
    float distance = length(L);
    
    // 半径检查
    if (distance > light.Radius)
        return float3(0, 0, 0);
        
    L = normalize(L);
    
    float NdotL = saturate(dot(N, L));
    if (NdotL <= 0.0f)
        return float3(0, 0, 0);
        
    // 衰减
    float attenuation = 1.0 / (distance * distance + 0.0001);
    
    // 聚光灯锥体因子
    float theta = dot(L, normalize(-light.Direction)); // (L 和 light.Direction 方向相反)
    float spotFactor = smoothstep(light.OuterCutOff, light.InnerCutOff, theta);
    
    // 组合
    float3 Radiance = light.Color * light.Intensity * attenuation * spotFactor;
    
    return CalculatePBR(L, V, N, F0, albedo, roughness, metallic, Radiance, NdotL);
}

// 主通道 - 像素着色器PBR
float4 PSMain(PSInput input) : SV_Target
{
    // 1. 从贴图采样材质属性
    float4 albedoMap = g_AlbedoMap.Sample(g_SamplerMain, input.texcoord);
    float3 albedo = albedoMap.rgb;
    float alpha = albedoMap.a; //获取Alpha!
    if (textureInfo.x == 0.0f)
    {
        albedo = input.color.rgb;
        alpha = input.color.a;
    }

    float metallic = g_MetallicMap.Sample(g_SamplerMain, input.texcoord).r;
    float roughness = g_RoughnessMap.Sample(g_SamplerMain, input.texcoord).r;
    float ao = g_AoMap.Sample(g_SamplerMain, input.texcoord).r;
    
    // 2. 获取几何属性
    float3 N; // 法线
    if (textureInfo.x == 1.0f) // 只有立方体使用法线贴图
    {
        N = GetNormalFromMap(input.normalW, input.tangentW, input.bitangentW, input.texcoord);
    }
    else
    {
        N = normalize(input.normalW); // 平面使用顶点法线
        
        // 为平面和玻璃球设置硬编码材质
        if (textureInfo.x == 0.0f) // 平面
        {
            metallic = 0.0f;
            roughness = 0.8f;
            ao = 1.0f;
            alpha = 1.0f; // 平面不透明
        }
        else // 假设 1.0f 是立方体, 0.0f 是平面, 其他值(比如 2.0f) 是玻璃球
        {
            albedo = float3(1.0, 1.0, 1.0); // 玻璃是白色的
            metallic = 0.0f; // 玻璃是电介质
            roughness = 0.1f; // 非常光滑的玻璃
            ao = 1.0f; // 假设没有 AO
            alpha = 0.3f; // 玻璃是半透明的
        }
    }
    
    float3 V = normalize(eyePosW - input.worldPos); // 视图方向
    
    // 3. 计算 F0 (基础反射率)
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    
    float3 kS = fresnelSchlick(saturate(dot(N, V)), F0); // 镜面反射部分
    float3 kD = (1.0 - kS) * (1.0 - metallic); // 漫反射部分 (金属没有漫反射)
    float3 irradiance = g_IrradianceMap.Sample(g_SamplerMain, N).rgb;
    float3 diffuse = irradiance * albedo; // * kD; (kD 理论上应该在这里，但通常 albedo * kD 已经分离了)
    
    
    // 4. 初始化出射光线 (Lo)
    float3 Lo = (kD * diffuse) * ao;
    
    // 5. 累加所有光源
    // (这些函数现在将使用我们从法线贴图计算出的 N)
    Lo += ProcessDirectionalLight(DirLight, N, V, input.worldPos, input.lightSpacePos, F0, albedo, roughness, metallic);
    Lo += ProcessPointLight(pointLight, N, V, input.worldPos, F0, albedo, roughness, metallic);
    Lo += ProcessSpotLight(spotLight, N, V, input.worldPos, F0, albedo, roughness, metallic);
    
    // 6. 后处理 (同前)
    float3 finalColor = Lo / (Lo + float3(1.0, 1.0, 1.0));
    finalColor = pow(finalColor, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));
    
    return float4(finalColor, alpha);
}




// 天空盒着色器非常简单, 只需要一个天空盒立方体贴图
// 它在自己的根签名中, 所以可以安全地使用 t0
TextureCube g_SkyboxTexture : register(t0);
SamplerState g_SamplerMainSky : register(s0);

struct VSInputSky
{
    float3 position : POSITION;
};

struct PSInputSky
{
    float4 position : SV_Position; // 裁剪空间
    float3 texcoord : TEXCOORD0; // 纹理坐标 (将是方向向量)
};

PSInputSky SkyboxVS(VSInputSky input)
{
    PSInputSky output;

    // 计算裁剪空间位置 
    output.position = mul(wvpMatrix, float4(input.position, 1.0f));

    // 我们将 Z 坐标强制设置为 W 坐标。
    // 在透视除法 (z/w) 之后, Z 值将恒为 1.0f。
    // 这确保了天空盒总是在深度缓冲区的最远处1.0f
    // 任何其他物体都会画在它前面。
    output.position.z = output.position.w;

    // 将顶点位置作为采样立方体贴图的 3D 纹理坐标
    // 应该使用 input.position, 因为它代表了从(0,0,0)出发的方向
    output.texcoord = input.position;

    return output;
}

float4 SkyboxPS(PSInputSky input) : SV_Target
{
    // 使用来自VS的 3D 坐标采样立方体贴图
    float4 skyColor = g_SkyboxTexture.Sample(g_SamplerMainSky, normalize(input.texcoord));
    
    // 我们稍后会在这里做 Gamma 校正, 但现在直接返回
    return skyColor;
}


// (VS) BRDF LUT - 全屏三角形
// PS的输入
struct PSInputBrdfLut
{
    float4 position : SV_Position;
    float2 texcoord : TEXCOORD0;
};

// 这是一个在VS中生成全屏三角形的技巧，不需要IB/VB
// 它会生成一个覆盖整个NDC空间的大三角形
PSInputBrdfLut BrdfLutVS(uint vid : SV_VertexID)
{
    PSInputBrdfLut output;
    
    // 将 0, 1, 2 映射到 texcoord (0, 0), (0, 2), (2, 0)
    output.texcoord = float2((vid << 1) & 2, (vid & 2));
    // 将 texcoord 映射到 position (-1, -1), (-1, 3), (3, -1)
    output.position = float4(output.texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    
    return output;
}

// (PS) BRDF LUT - 积分计算
// 基于 Epic Games 的 "Real Shading in Unreal Engine 4"
// 我们的目标是预计算镜面 BRDF 的两个部分：
// R 通道: scale = 积分( (1 - (1 - VdotH)^5) * G_Vis )
// G 通道: bias  = 积分( (1 - VdotH)^5 * G_Vis )
// (注意: 最终 scale = A, bias = B。这里代码计算的是 A 和 B)

// static const float PI = 3.14159265359f; (在前面已经定义)

// Hammersley 序列 (用于低差异序列)
float2 Hammersley(uint i, uint N)
{
    // Van Der Corput 序列 (base 2)
    uint bits = (i << 16u) | (i >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    float rdi = float(bits) * 2.3283064365386963e-10; // 1.0 / 0x100000000
    return float2(float(i) / float(N), rdi);
}

// GGX 重要性采样 (根据粗糙度生成 H)
float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    float a = roughness * roughness;
    
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    
    // 从球面坐标到笛卡尔坐标
    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;
    
    // 从切线空间转换到世界空间 (N=0,0,1)
    float3 up = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 T = normalize(cross(up, N));
    float3 B = cross(N, T);
    
    float3x3 TBN = float3x3(T, B, N);
    return normalize(mul(H, TBN));
}

// G (Schlick-GGX) - (用于IBL)
float GeometrySchlickGGX_IBL(float NdotV, float roughness)
{
    // IBL 使用 k = roughness^2 / 2
    float r = roughness;
    float k = (r * r) / 2.0;
    float num = NdotV;
    float den = NdotV * (1.0 - k) + k;
    return num / den;
}

// G (Smith) - (用于IBL)
float GeometrySmith_IBL(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));
    float ggx2 = GeometrySchlickGGX_IBL(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX_IBL(NdotL, roughness);
    return ggx1 * ggx2;
}

// 积分函数
float2 IntegrateBRDF(float NdotV, float roughness)
{
    float3 V;
    V.x = sqrt(1.0 - NdotV * NdotV); // sin
    V.y = 0.0;
    V.z = NdotV; // cos
    
    float3 N = float3(0.0, 0.0, 1.0);
    
    float A = 0.0; // 对应 R 通道
    float B = 0.0; // 对应 G 通道
    
    const uint SAMPLE_COUNT = 1024u;
    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, N, roughness);
        float3 L = normalize(2.0 * dot(V, H) * H - V);
        
        float NdotL = saturate(L.z);
        float NdotH = saturate(H.z);
        float VdotH = saturate(dot(V, H));
        
        if (NdotL > 0.0)
        {
            // 几何项
            float G = GeometrySmith_IBL(N, V, L, roughness);
            // (G * VdotH) / (NdotH * NdotV)
            float G_Vis = (G * VdotH) / (NdotH * NdotV + 0.0001f);
            // 菲涅尔 (Schlick)
            float Fc = pow(1.0 - VdotH, 5.0);
            
            // A = 积分( (1 - Fc) * G_Vis )
            A += (1.0 - Fc) * G_Vis;
            // B = 积分( Fc * G_Vis )
            B += Fc * G_Vis;
        }
    }
    
    return float2(A, B) / float(SAMPLE_COUNT);
}

float4 BrdfLutPS(PSInputBrdfLut input) : SV_Target
{
    // input.texcoord 在 [0, 1] (VS中已映射好)
    // 映射到 NdotV 和 roughness
    float NdotV = input.texcoord.x;
    float roughness = input.texcoord.y;
    
    // 计算积分
    float2 result = IntegrateBRDF(NdotV, roughness);
    
    // 将结果存储到 R16G16_FLOAT 纹理中
    return float4(result.x, result.y, 0.0, 1.0);
}

//IBL - Irradiance Map 卷积
float4 IrradiancePS(PSInputSky input) : SV_Target
{
    // N 是我们采样的方向 (来自 VS 的 texcoord)
    float3 N = normalize(input.texcoord);
    
    // 我们需要TBN来转换采样
    // (同 BrdfLutPS 中的转换)
    float3 up = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 T = normalize(cross(up, N));
    float3 B = cross(N, T);
    float3x3 TBN = float3x3(T, B, N);

    float3 irradiance = float3(0.0, 0.0, 0.0);
    
    // 蒙特卡洛积分
    // (我们使用低差异序列 Hammersley, 但也可以用随机)
    
    const uint SAMPLE_COUNT = 1024u; // (可以降低以提高速度, e.g. 256)
    float sampleDelta = 0.025f; // (用于切线空间采样的参数, 可调整)

    float3 sampleVec = float3(0, 0, 0);
    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        // 获取低差异采样点
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        
        // 生成余弦加权的半球采样
        float phi = 2.0 * PI * Xi.x;
        float cosTheta = sqrt(1.0 - Xi.y);
        float sinTheta = sqrt(Xi.y); // sqrt(1.0 - cosTheta*cosTheta)
        
        // 从球面坐标到笛卡尔坐标 (切线空间)
        float3 H;
        H.x = cos(phi) * sinTheta;
        H.y = sin(phi) * sinTheta;
        H.z = cosTheta;
        
        // 从切线空间转换到世界空间
        float3 L = normalize(mul(H, TBN)); // L
        
        // 用 L 采样天空盒，并累加 (N.L 因子已包含在重要性采样中)
        irradiance += g_SkyboxTexture.Sample(g_SamplerMainSky, L).rgb;
    }
    
    // 平均采样结果
    irradiance = irradiance / float(SAMPLE_COUNT);
    
    // (在 LDR 中, 我们不需要 * PI, 但在 HDR 中需要)
    // irradiance = PI * irradiance / float(SAMPLE_COUNT); 
    
    return float4(irradiance, 1.0);
}