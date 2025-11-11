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
    
    // 3. 计算 F0 (基础反射率) (同前)
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    
    // 4. 初始化出射光线 (Lo) (同前)
    float3 Lo = AmbientLight * albedo * ao;
    
    // 5. 累加所有光源 (同前)
    // (这些函数现在将使用我们从法线贴图计算出的 N)
    Lo += ProcessDirectionalLight(DirLight, N, V, input.worldPos, input.lightSpacePos, F0, albedo, roughness, metallic);
    Lo += ProcessPointLight(pointLight, N, V, input.worldPos, F0, albedo, roughness, metallic);
    Lo += ProcessSpotLight(spotLight, N, V, input.worldPos, F0, albedo, roughness, metallic);
    
    // 6. 后处理 (同前)
    float3 finalColor = Lo / (Lo + float3(1.0, 1.0, 1.0));
    finalColor = pow(finalColor, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));
    
    return float4(finalColor, alpha);
}