// D3D12App.h
#pragma once

#include "PCH.h"
#include "Camera.h" // ** 新增: 包含 Camera.h **

class D3D12App
{
public:
    D3D12App(UINT width, UINT height, const wchar_t* title);
    virtual ~D3D12App();
    void Run();
    virtual LRESULT HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
    void InitWindow();
    void InitD3D12();
    void Cleanup();
    void WaitForGpu();
    void PopulateCommandList();

    virtual void OnLoadAssets();
    // ** 修改: OnUpdate 现在需要 deltaTime **
    virtual void OnUpdate(float deltaTime);
    virtual void OnRender();

    // --- 成员变量 ---
    HWND m_hWnd;
    UINT m_Width;
    UINT m_Height;
    std::wstring m_Title;

    // ... (D3D12 核心, 堆, 深度缓冲 - 同前) ...
    ComPtr<ID3D12Device> m_Device;
    ComPtr<ID3D12CommandQueue> m_CommandQueue;
    ComPtr<IDXGISwapChain3> m_SwapChain;
    ComPtr<ID3D12Resource> m_RenderTargets[2];
    ComPtr<ID3D12CommandAllocator> m_CommandAllocator;
    ComPtr<ID3D12GraphicsCommandList> m_CommandList;
    ComPtr<ID3D12DescriptorHeap> m_RtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_DsvHeap;
    ComPtr<ID3D12DescriptorHeap> m_SrvHeap;
    UINT m_RtvDescriptorSize;
    ComPtr<ID3D12Resource> m_DepthStencilBuffer;

    // ... (管线, 几何体, 纹理 - 同前) ...
    D3D12_VIEWPORT m_Viewport;
    D3D12_RECT m_ScissorRect;
    ComPtr<ID3D12RootSignature> m_RootSignature;
    ComPtr<ID3D12PipelineState> m_PipelineState;
    ComPtr<ID3D12Resource> m_VertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView;
    ComPtr<ID3D12Resource> m_IndexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_IndexBufferView;
    ComPtr<ID3D12Resource> m_Texture;

    // ... (常量缓冲区 - 同前) ...
    ComPtr<ID3D12Resource> m_ConstantBuffer;
    UINT8* m_pConstantBufferDataBegin;
    struct SceneConstants { XMFLOAT4X4 wvpMatrix; XMFLOAT4X4 worldMatrix; float padding[32]; };
    SceneConstants m_Constants;

    // ... (同步 - 同前) ...
    ComPtr<ID3D12Fence> m_Fence;
    UINT64 m_FenceValue;
    HANDLE m_FenceEvent;
    UINT m_FrameIndex;

    float m_TotalTime; // (现在只用于立方体旋转)

    // ** 新增: 摄像机和输入状态 **
    Camera m_Camera;
    InputState m_InputState;
    POINT m_LastMousePos;

    // ... (顶点结构体 - 同前) ...
    struct Vertex { float position[3]; float color[4]; float texCoord[2]; float normal[3]; };

private:
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    inline void ThrowIfFailed(HRESULT hr)
    {
        if (FAILED(hr))
        {
            _com_error err(hr);
            OutputDebugString(err.ErrorMessage());
            throw std::runtime_error("D3D12 Error");
        }
    }
};