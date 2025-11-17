// D3D12App.h
#pragma once

#include "PCH.h"
#include "PSOContainer.h"
#include "ShaderCompiler.h"
#include "Camera.h"
#include "Geometry.h"
#include "ResourceManager.h"
#include "DX12Object.h"
#include "Light.h"
#include "ShadowMap.h"
#include "stb_image.h"
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
    virtual void CreateDescriptorHeaps();
    virtual void CreateRootSignature();
    virtual void CreatePSO();
    virtual void CreateResources();

    void LoadCubeMapTexture(std::vector<std::string> filenames, ComPtr<ID3D12Resource>& textureResource, D3D12_CPU_DESCRIPTOR_HANDLE srvHandle);

    void RunCommand();
    void Cleanup();
    void WaitForGpu();
    virtual void RenderMainPass();
    virtual void RenderShadowMap();
    void GenerateMipMap(void* pData, int textureWidth, int textureHeight, int mipMapLevels, std::vector<stbi_uc*>& mipDataBuffers, int texturePixelSize);
    void LoadTextureFromFile(const char* filename,
        ComPtr<ID3D12Resource>& textureResource,
        D3D12_CPU_DESCRIPTOR_HANDLE srvHandle);
    virtual void OnLoadAssets();
    virtual void OnUpdate(float deltaTime);
    virtual void OnRender();

    // --- 成员变量 ---
    HWND m_hWnd;
    UINT m_Width;
    UINT m_Height;
    std::wstring m_Title;

    ComPtr<ID3D12Device> m_Device;
    ComPtr<ID3D12CommandQueue> m_CommandQueue;
    ComPtr<IDXGISwapChain3> m_SwapChain;
    ComPtr<ID3D12Resource> m_RenderTargets[2];
    ComPtr<ID3D12CommandAllocator> m_CommandAllocator;
    ComPtr<ID3D12GraphicsCommandList> m_CommandList;
    ComPtr<ID3D12DescriptorHeap> m_RtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_DsvHeap;
    ComPtr<ID3D12DescriptorHeap> m_SrvHeap;
    ComPtr<ID3D12DescriptorHeap> m_ConstHeap;
    UINT m_RtvDescriptorSize;
    ComPtr<ID3D12Resource> m_DepthStencilBuffer;

    D3D12_VIEWPORT m_Viewport;
    D3D12_RECT m_ScissorRect;
    ComPtr<ID3D12RootSignature> m_RootSignature;
    ComPtr<ID3D12RootSignature> m_ShadowRootSignature; //阴影通道的根签名


    ComPtr<ID3D12Resource> m_VertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView;
    ComPtr<ID3D12Resource> m_IndexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_IndexBufferView;
    XMFLOAT4X4 m_CubeWorldMatrix;
    
    ComPtr<ID3D12Resource> m_PlaneVertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_PlaneVertexBufferView;
    ComPtr<ID3D12Resource> m_PlaneIndexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_PlaneIndexBufferView;
    XMFLOAT4X4 m_PlaneWorldMatrix;

    ComPtr<ID3D12Resource> m_SphereVertexBuffer;
    ComPtr<ID3D12Resource> m_SphereIndexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_SphereVertexBufferView;
    D3D12_INDEX_BUFFER_VIEW m_SphereIndexBufferView;
    XMFLOAT4X4 m_SphereWorldMatrix; // 球体的世界矩阵



    ComPtr<ID3D12Resource> m_ConstantBuffer;
    UINT8* m_pConstantBufferDataBegin;
    struct SceneConstants 
    { 
        XMFLOAT4X4 wvpMatrix; 
        XMFLOAT4X4 worldMatrix; 
        XMFLOAT4X4 lightWvpMatrix;
        XMFLOAT4 textureInfo;
        XMFLOAT4 eyePos; //摄像机位置 (用于 PBR)
    };
    SceneConstants m_Constants;

    //  阴影通道的常量缓冲区
    // 这个缓冲区只包含光源的 WVP，用于渲染到阴影贴图
    ComPtr<ID3D12Resource> m_ShadowConstantBuffer;
    UINT8* m_pShadowConstantBufferDataBegin;
    struct ShadowConstants
    {
        XMFLOAT4X4 lightWvpMatrix; // 光源 WVP
        float padding[48];         // 填充到 256 字节
    };
    ShadowConstants m_ShadowConstants;

    ComPtr<ID3D12Fence> m_Fence;
    UINT64 m_FenceValue;
    HANDLE m_FenceEvent;
    UINT m_FrameIndex;

    float m_TotalTime;
    Camera m_Camera;
    InputState m_InputState;
    POINT m_LastMousePos;
    

    Geometry geometry;
    ShaderCompiler shaderCompiler;
    PSOContainer psoContainer;
    ResourceManager resourceManager;

    DX12Object dx12Object;

    //灯光
    ComPtr<ID3D12Resource> m_LightingConstantBuffer;
    UINT8* m_pLightingConstantBufferDataBegin;
    LightingConstants m_LightingConstants;

    std::unique_ptr<ShadowMap> m_ShadowMap;
    XMFLOAT4X4 m_LightViewMatrix;
    XMFLOAT4X4 m_LightProjMatrix;



    XMFLOAT3 m_BaseDirLightDirection;

    const UINT c_alignedSceneConstantBufferSize = (sizeof(D3D12App::SceneConstants) + 255) & ~255;
    const UINT c_alignedShadowConstantBufferSize = (sizeof(D3D12App::ShadowConstants) + 255) & ~255;

    //PBR
    ComPtr<ID3D12Resource> m_AlbedoTexture;    // t0
    ComPtr<ID3D12Resource> m_MetallicTexture;  // t1
    ComPtr<ID3D12Resource> m_RoughnessTexture; // t2
    ComPtr<ID3D12Resource> m_AoTexture;        // t3
    ComPtr<ID3D12Resource> m_NormalTexture;    // t4

    // --- 天空盒资源 ---
    ComPtr<ID3D12Resource> m_SkyboxTexture;
    ComPtr<ID3D12RootSignature> m_SkyboxRootSignature;


    ComPtr<ID3D12Resource> m_BrdfLutTexture;
    ComPtr<ID3D12Resource> m_IrradianceMapTexture;
    ComPtr<ID3D12DescriptorHeap> m_IblRtvHeap; // 用于 IBL 生成的 RTV 堆
    ComPtr<ID3D12RootSignature> m_BrdfLutRootSignature;

 private:
     void RenderOpaque();
     void RenderTransparent();
     void RenderSkyBox();

private:
    void RenderBrdfLut();
    void RenderIrradianceMap();

private:
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    std::string m_pbr_text_tail = ".jpg";
};