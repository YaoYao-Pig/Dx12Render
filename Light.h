#pragma once

#include <DirectXMath.h>

struct Material
{
    DirectX::XMFLOAT3 Albedo = { 1.0f, 1.0f, 1.0f };
    float Metallic = 0.0f;
    float Roughness = 0.5f;
    float AO = 1.0f; // 环境光遮蔽 (暂未使用)
};

enum LightType
{
    Directional = 0,
    Point = 1,
    Spot = 2
};
// 方向光
struct DirectionalLight
{
    DirectX::XMFLOAT3 Color = { 1.0f, 1.0f, 1.0f };
    float Intensity = 1.0f;
    DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };
    float Pad; // 填充到 16 字节
};

// 点光源
struct PointLight
{
    DirectX::XMFLOAT3 Color = { 1.0f, 1.0f, 1.0f };
    float Intensity = 10.0f;
    DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };

    // PBR 衰减 (基于物理)
    // Intensity / (Attenuation.x + Attenuation.y * d + Attenuation.z * d^2)
    // 我们将使用更简单的 r^2 衰减 (见 HLSL)
    // 但为了结构完整性，我们保留半径
    float Radius = 5.0f;
};

// 聚光灯
struct SpotLight
{
    DirectX::XMFLOAT3 Color = { 1.0f, 1.0f, 1.0f };
    float Intensity = 20.0f;
    DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };
    float Radius = 10.0f; // 影响半径
    DirectX::XMFLOAT3 Direction = { 0.0f, 0.0f, 1.0f };

    // cos(InnerConeAngle)
    float InnerCutOff = 0.94f; // ~20 度
    // cos(OuterConeAngle)
    float OuterCutOff = 0.86f; // ~30 度
    float Pad[3]; // 填充
};

struct LightingConstants
{
    DirectionalLight DirLight;
    PointLight PointLight;
    SpotLight SpotLight;

    // 全局环境光
    DirectX::XMFLOAT3 AmbientLight = { 0.03f, 0.03f, 0.03f };
    float Pad; // 填充
};