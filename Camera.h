// Camera.h
#pragma once
#include "PCH.h"

// 跟踪按键状态
struct InputState
{
    bool forward = false;
    bool back = false;
    bool left = false;
    bool right = false;
    bool up = false;
    bool down = false;
    int mouseDeltaX = 0;
    int mouseDeltaY = 0;
};

class Camera
{
public:
    Camera();

    // 设置投影矩阵
    void SetProjection(float fovY, float aspect, float zNear, float zFar);
    // 设置摄像机位置
    void SetPosition(float x, float y, float z);

    // 获取矩阵
    XMMATRIX GetViewMatrix() const;
    XMMATRIX GetProjMatrix() const;
    XMFLOAT3 GetPosition() { return m_Position; }
    XMFLOAT3 GetLookAt() { return m_Look; }


    // 核心更新函数
    void Update(float deltaTime, const InputState& input);

private:
    void UpdateViewMatrix(); // 内部辅助函数

    // 摄像机“坐标系”
    XMFLOAT3 m_Position = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 m_Right = { 1.0f, 0.0f, 0.0f };
    XMFLOAT3 m_Up = { 0.0f, 1.0f, 0.0f };
    XMFLOAT3 m_Look = { 0.0f, 0.0f, 1.0f };

    // 摄像机朝向（欧拉角）
    float m_Yaw = 0.0f;   // 绕 Y 轴旋转
    float m_Pitch = 0.0f; // 绕 X 轴旋转

    // 速度
    float m_MoveSpeed = 5.0f;
    float m_LookSpeed = 3.0f;

    // 最终矩阵
    XMFLOAT4X4 m_ViewMatrix;
    XMFLOAT4X4 m_ProjMatrix;
};