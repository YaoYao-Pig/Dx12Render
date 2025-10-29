// Camera.cpp
#include "PCH.h"
#include "Camera.h"
#include <algorithm> // 用于 std::max/min

Camera::Camera()
{
    // 默认设置
    SetPosition(0.0f, 1.0f, -5.0f);
    m_Yaw = 0.0f; // 朝向 Z 轴正方向
    m_Pitch = 0.0f;
    UpdateViewMatrix();

    // 默认投影
    SetProjection(XMConvertToRadians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
}

void Camera::SetProjection(float fovY, float aspect, float zNear, float zFar)
{
    XMMATRIX P = XMMatrixPerspectiveFovLH(fovY, aspect, zNear, zFar);
    XMStoreFloat4x4(&m_ProjMatrix, P);
}

void Camera::SetPosition(float x, float y, float z)
{
    m_Position = XMFLOAT3(x, y, z);
    UpdateViewMatrix();
}

XMMATRIX Camera::GetViewMatrix() const { return XMLoadFloat4x4(&m_ViewMatrix); }
XMMATRIX Camera::GetProjMatrix() const { return XMLoadFloat4x4(&m_ProjMatrix); }

void Camera::Update(float deltaTime, const InputState& input)
{
    // --- 1. 更新鼠标朝向 (欧拉角) ---
    m_Yaw += input.mouseDeltaX * m_LookSpeed * deltaTime;
    m_Pitch += input.mouseDeltaY * m_LookSpeed * deltaTime;

    // 限制 Pitch (上下看) 的角度，防止“万向节死锁”
    m_Pitch = (std::max)( -XM_PIDIV2 + 0.01f,(std::min)((XM_PIDIV2 - 0.01f), m_Pitch));

    // --- 2. 重新计算摄像机坐标系 ---
    // 根据 Pitch 和 Yaw 旋转，计算出新的 Look(Z), Right(X) 向量
    XMMATRIX rotation = XMMatrixRotationRollPitchYaw(m_Pitch, m_Yaw, 0.0f);
    XMVECTOR look = XMVector3TransformNormal(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), rotation);
    XMVECTOR right = XMVector3TransformNormal(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), rotation);
    // Up(Y) 向量是 Look 和 Right 的叉积
    XMVECTOR up = XMVector3Cross(look, right);

    // --- 3. 更新键盘移动 ---
    XMVECTOR pos = XMLoadFloat3(&m_Position);
    if (input.forward)  pos += look * m_MoveSpeed * deltaTime;
    if (input.back)     pos -= look * m_MoveSpeed * deltaTime;
    if (input.right)    pos += right * m_MoveSpeed * deltaTime;
    if (input.left)     pos -= right * m_MoveSpeed * deltaTime;
    if (input.up)       pos += XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f) * m_MoveSpeed * deltaTime;
    if (input.down)     pos -= XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f) * m_MoveSpeed * deltaTime;

    // --- 4. 存储新向量并更新视图矩阵 ---
    XMStoreFloat3(&m_Position, pos);
    XMStoreFloat3(&m_Look, look);
    XMStoreFloat3(&m_Right, right);
    XMStoreFloat3(&m_Up, up);

    UpdateViewMatrix();
}

void Camera::UpdateViewMatrix()
{
    XMVECTOR pos = XMLoadFloat3(&m_Position);
    XMVECTOR look = XMLoadFloat3(&m_Look);
    XMVECTOR up = XMLoadFloat3(&m_Up);

    // 使用 LookTo (朝向) 而不是 LookAt (朝向某个点)
    XMMATRIX V = XMMatrixLookToLH(pos, look, up);
    XMStoreFloat4x4(&m_ViewMatrix, V);
}