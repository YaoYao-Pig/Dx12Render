// D3D12App.cpp

#include "PCH.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "D3D12App.h"
#include "ShaderCompiler.h"

// --- 静态 WndProc 路由 (同前) ---
LRESULT CALLBACK D3D12App::WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    D3D12App* pApp = nullptr;
    if (msg == WM_CREATE) { CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam); pApp = reinterpret_cast<D3D12App*>(pCreate->lpCreateParams); SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pApp); }
    else { pApp = reinterpret_cast<D3D12App*>(GetWindowLongPtr(hWnd, GWLP_USERDATA)); }
    if (pApp) { return pApp->HandleMessage(hWnd, msg, wParam, lParam); }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// --- 构造函数 (修改) ---
D3D12App::D3D12App(UINT width, UINT height, const wchar_t* title) :
    m_Width(width), m_Height(height), m_Title(title), m_hWnd(nullptr),
    m_RtvDescriptorSize(0), m_pConstantBufferDataBegin(nullptr),
    m_FenceValue(0), m_FenceEvent(nullptr), m_FrameIndex(0), m_TotalTime(0.0f),
    m_Camera(), // 默认构造 m_Camera
    m_InputState(), // 零初始化 m_InputState
    m_LastMousePos() // 零初始化 m_LastMousePos
{
    m_Viewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
    m_ScissorRect = { 0, 0, (LONG)width, (LONG)height };

    // ** 新增: 设置摄像机 (取代 OnUpdate 中的硬编码) **
    m_Camera.SetProjection(XMConvertToRadians(45.0f), (float)m_Width / (float)m_Height, 0.1f, 100.0f);
    m_Camera.SetPosition(0.0f, 1.0f, -5.0f);
}
D3D12App::~D3D12App() { Cleanup(); }

// --- 消息处理 (大改) ---
LRESULT D3D12App::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

        // --- 输入 ---
    case WM_LBUTTONDOWN: // 鼠标左键按下: 捕获光标
        SetCapture(m_hWnd);
        ShowCursor(FALSE);
        GetCursorPos(&m_LastMousePos);
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) // ESC: 释放光标
        {
            ReleaseCapture();
            ShowCursor(TRUE);
        }
        if (wParam == 'W') m_InputState.forward = true;
        if (wParam == 'S') m_InputState.back = true;
        if (wParam == 'A') m_InputState.left = true;
        if (wParam == 'D') m_InputState.right = true;
        if (wParam == VK_SPACE) m_InputState.up = true;
        if (wParam == 'C') m_InputState.down = true;
        return 0;

    case WM_KEYUP:
        if (wParam == 'W') m_InputState.forward = false;
        if (wParam == 'S') m_InputState.back = false;
        if (wParam == 'A') m_InputState.left = false;
        if (wParam == 'D') m_InputState.right = false;
        if (wParam == VK_SPACE) m_InputState.up = false;
        if (wParam == 'C') m_InputState.down = false;
        return 0;

    case WM_MOUSEMOVE:
        if (wParam & MK_LBUTTON) // 仅当鼠标被捕获时 (左键按下)
        {
            POINT currentMousePos;
            GetCursorPos(&currentMousePos);

            m_InputState.mouseDeltaX = currentMousePos.x - m_LastMousePos.x;
            m_InputState.mouseDeltaY = currentMousePos.y - m_LastMousePos.y;

            // 保持光标在屏幕中心 (或一个固定点) 来实现无限旋转
            SetCursorPos(m_LastMousePos.x, m_LastMousePos.y);
        }
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// --- 运行 (主循环) (修改) ---
void D3D12App::Run()
{
    InitWindow(); 
    InitD3D12(); 
    OnLoadAssets();
    ShowWindow(m_hWnd, SW_SHOWDEFAULT); UpdateWindow(m_hWnd);
    LARGE_INTEGER lastTime; LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&lastTime);
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }
        else
        {
            LARGE_INTEGER currentTime; QueryPerformanceCounter(&currentTime);
            float deltaTime = (float)(currentTime.QuadPart - lastTime.QuadPart) / (float)frequency.QuadPart;
            lastTime = currentTime; m_TotalTime += deltaTime;

            // ** 修改: 传递 deltaTime **
            OnUpdate(deltaTime);
            OnRender();
        }
    }
}

// --- 初始化窗口 (同前) ---
void D3D12App::InitWindow()
{
    // ... (同前, 100% 未修改) ...
    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX); wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = D3D12App::WindowProc; wc.hInstance = hInstance;
    wc.lpszClassName = L"DX12DemoWindowClass"; wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); RegisterClassEx(&wc);
    RECT windowRect = { 0, 0, (LONG)m_Width, (LONG)m_Height };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
    m_hWnd = CreateWindowEx(
        0, wc.lpszClassName, m_Title.c_str(), WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
        NULL, NULL, hInstance, this
    );
    if (m_hWnd == nullptr) throw std::runtime_error("Window Creation Failed!");
}

// --- D3D12 初始化 (同前) ---
void D3D12App::InitD3D12()
{
#if defined(_DEBUG)
    ComPtr<ID3D12Debug> pDebugController;
    if (SUCCEEDED(D3D12GetDebugInterface(__uuidof(ID3D12Debug), (void**)&pDebugController)))
    {
        pDebugController->EnableDebugLayer();
    }
#endif
    ComPtr<IDXGIFactory4> factory;
    UINT dxgiFactoryFlags = 0;
#if defined(_DEBUG)
    dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, __uuidof(IDXGIFactory4), (void**)&factory));
    ThrowIfFailed(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)&m_Device));
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(m_Device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), (void**)&m_CommandQueue));
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = 2; swapChainDesc.Width = m_Width; swapChainDesc.Height = m_Height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;
    ComPtr<IDXGISwapChain1> swapChain1;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(m_CommandQueue.Get(), m_hWnd, &swapChainDesc, nullptr, nullptr, &swapChain1));
    ThrowIfFailed(swapChain1.As(&m_SwapChain));
    m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();
    
    CreateDescriptorHeaps();
    
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_RtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT n = 0; n < 2; n++)
    {
        ThrowIfFailed(m_SwapChain->GetBuffer(n, __uuidof(ID3D12Resource), (void**)&m_RenderTargets[n]));
        m_Device->CreateRenderTargetView(m_RenderTargets[n].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, m_RtvDescriptorSize);
    }
    D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
    depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
    depthOptimizedClearValue.DepthStencil.Stencil = 0;
    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_D32_FLOAT, m_Width, m_Height, 1, 0, 1, 0,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
    );
    ThrowIfFailed(m_Device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthOptimizedClearValue,
        __uuidof(ID3D12Resource), (void**)&m_DepthStencilBuffer
    ));
    m_DepthStencilBuffer->SetName(L"Depth Stencil Buffer");
    m_Device->CreateDepthStencilView(m_DepthStencilBuffer.Get(), nullptr, m_DsvHeap->GetCPUDescriptorHandleForHeapStart());
    ThrowIfFailed(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&m_CommandAllocator));
    ThrowIfFailed(m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CommandAllocator.Get(), nullptr, __uuidof(ID3D12GraphicsCommandList), (void**)&m_CommandList));
    ThrowIfFailed(m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void**)&m_Fence));
    m_FenceValue = 1;
    m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_FenceEvent == nullptr) ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));

    // 其他
    shaderCompiler.Init({ "VSMain","PSMain" });
    psoContainer.Init(m_Device);
    geometry.LoadGeometry();
    resourceManager.Init(m_Device, m_CommandList);
    dx12Object.Init(m_Device, m_CommandList);
}

void D3D12App::CreateDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 2;
    ThrowIfFailed(m_Device->CreateDescriptorHeap(&rtvHeapDesc, __uuidof(ID3D12DescriptorHeap), (void**)&m_RtvHeap));
    m_RtvDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.NumDescriptors = 1;
    ThrowIfFailed(m_Device->CreateDescriptorHeap(&dsvHeapDesc, __uuidof(ID3D12DescriptorHeap), (void**)&m_DsvHeap));
     
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NumDescriptors = 1;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(m_Device->CreateDescriptorHeap(&srvHeapDesc, __uuidof(ID3D12DescriptorHeap), (void**)&m_SrvHeap));
}

void D3D12App::CreateRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE ranges[1];
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); //一个srv类型的view，存在t0
    CD3DX12_ROOT_PARAMETER parameters[2];
    parameters[0].InitAsConstantBufferView(0);//一个const
    parameters[1].InitAsDescriptorTable(1, &ranges[0]);


    //创建sampler
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

    sampler.MinLOD = 0;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;//s0
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    CD3DX12_ROOT_SIGNATURE_DESC signatureDes = {};
    signatureDes.Init(_countof(parameters), parameters,
        1, &sampler,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    ComPtr<ID3DBlob> signature, error;
    ThrowIfFailed(D3D12SerializeRootSignature(&signatureDes, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));

    ThrowIfFailed(m_Device->CreateRootSignature(0,
        signature->GetBufferPointer(),
        signature->GetBufferSize(),
        __uuidof(ID3D12RootSignature),
        (void**)&m_RootSignature));
}

void D3D12App::CreatePSO()
{
    //定义输入定点语义
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = m_RootSignature.Get();
    psoDesc.VS.pShaderBytecode = shaderCompiler.vertexShader->GetBufferPointer();
    psoDesc.VS.BytecodeLength = shaderCompiler.vertexShader->GetBufferSize();
    psoDesc.PS.pShaderBytecode = shaderCompiler.pixelShader->GetBufferPointer();
    psoDesc.PS.BytecodeLength = shaderCompiler.pixelShader->GetBufferSize();

    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.FrontCounterClockwise = FALSE;
    rasterizerDesc.DepthClipEnable = TRUE;

    psoDesc.RasterizerState = rasterizerDesc;

    D3D12_BLEND_DESC blendDesc = {};
    blendDesc.AlphaToCoverageEnable = FALSE; blendDesc.IndependentBlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendEnable = FALSE;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    psoDesc.BlendState = blendDesc;
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count = 1;

    psoContainer.BuildPSO(RenderQueue::Common, &psoDesc);
}

void D3D12App::CreateResources()
{
    resourceManager.UploadResource(geometry.vertexs.data(), geometry.vertexSize, m_VertexBuffer);
    resourceManager.UploadResource(geometry.indices.data(), geometry.indiceSize, m_IndexBuffer);
    int textureWidth, textureHeight, textureChannels;
    stbi_uc* pTextureData = stbi_load("texture.png", &textureWidth, &textureHeight, &textureChannels, STBI_rgb_alpha);
    if (pTextureData == nullptr) { throw std::runtime_error("Failed to load texture file!"); }
    UINT texturePixelSize = 4;
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = textureWidth; textureDesc.Height = textureHeight;
    textureDesc.DepthOrArraySize = 1; textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1; textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(m_Device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr, __uuidof(ID3D12Resource), (void**)&m_Texture
    ));
    m_Texture->SetName(L"Texture");
    UINT64 textureUploadBufferSize;
    m_Device->GetCopyableFootprints(&textureDesc, 0, 1, 0, nullptr, nullptr, nullptr, &textureUploadBufferSize);

    ComPtr<ID3D12Resource> pTextureUploadBuffer;
    heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(textureUploadBufferSize);
    ThrowIfFailed(m_Device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, __uuidof(ID3D12Resource), (void**)&pTextureUploadBuffer
    ));
    pTextureUploadBuffer->SetName(L"Texture Upload Heap");
    resourceManager.AddResource2TmpBuffer(pTextureUploadBuffer);

    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = pTextureData;
    textureData.RowPitch = textureWidth * texturePixelSize;
    textureData.SlicePitch = textureData.RowPitch * textureHeight;
    UpdateSubresources(m_CommandList.Get(), m_Texture.Get(), pTextureUploadBuffer.Get(), 0, 0, 1, &textureData);
    stbi_image_free(pTextureData);

    CD3DX12_RESOURCE_BARRIER barriers[3] = {
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_VertexBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST, //之前：它是一个“复制目标”
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER // 之后：它是一个“顶点/常量缓冲”
        ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_IndexBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_INDEX_BUFFER
        ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_Texture.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        )
    };
    m_CommandList->ResourceBarrier(_countof(barriers), barriers);

    m_VertexBufferView.BufferLocation = m_VertexBuffer->GetGPUVirtualAddress();
    m_VertexBufferView.StrideInBytes = sizeof(Vertex);
    m_VertexBufferView.SizeInBytes = geometry.vertexSize;
    m_IndexBufferView.BufferLocation = m_IndexBuffer->GetGPUVirtualAddress();
    m_IndexBufferView.Format = DXGI_FORMAT_R16_UINT;
    m_IndexBufferView.SizeInBytes = geometry.indiceSize;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    m_Device->CreateShaderResourceView(m_Texture.Get(), &srvDesc, m_SrvHeap->GetCPUDescriptorHandleForHeapStart());

    const UINT constantBufferSize = (sizeof(SceneConstants) + 255) & ~255;
    heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize);
    ThrowIfFailed(m_Device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, __uuidof(ID3D12Resource), (void**)&m_ConstantBuffer
    ));
    m_ConstantBuffer->SetName(L"Constant Buffer");
    ThrowIfFailed(m_ConstantBuffer->Map(0, nullptr, (void**)&m_pConstantBufferDataBegin));

    ThrowIfFailed(m_CommandList->Close());
    ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
    m_CommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

}

// --- 加载资源 (同前) ---
void D3D12App::OnLoadAssets()
{
    CreateRootSignature();
    shaderCompiler.CompileShader();
    CreatePSO();
    CreateResources();
    RunCommand();
    WaitForGpu();
    resourceManager.ClearTmpBuffers();
}

void D3D12App::OnUpdate(float deltaTime)
{
    // 1. 更新摄像机
    m_Camera.Update(deltaTime, m_InputState);

    // 2. 清空每帧的鼠标增量
    m_InputState.mouseDeltaX = 0;
    m_InputState.mouseDeltaY = 0;

    // 3. 获取新矩阵
    XMMATRIX viewMatrix = m_Camera.GetViewMatrix();
    XMMATRIX projMatrix = m_Camera.GetProjMatrix();
    XMMATRIX worldMatrix = XMMatrixRotationY(m_TotalTime * 1.0f); // 立方体仍然在旋转

    XMMATRIX wvpMatrix = worldMatrix * viewMatrix * projMatrix;

    // 4. 更新常量缓冲区
    XMStoreFloat4x4(&m_Constants.wvpMatrix, wvpMatrix);
    XMStoreFloat4x4(&m_Constants.worldMatrix, worldMatrix);

    memcpy(m_pConstantBufferDataBegin, &m_Constants, sizeof(SceneConstants));
}

void D3D12App::OnRender()
{
    PopulateCommandList();
    ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
    m_CommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    ThrowIfFailed(m_SwapChain->Present(1, 0));
    WaitForGpu();
}


// --- 填充命令列表 ---
void D3D12App::PopulateCommandList()
{
    dx12Object.RestCommandList(m_CommandAllocator)
        .SetRootSignature(m_RootSignature)
        .SetPSO(psoContainer.GetPSO(RenderQueue::Common))
        .SetViewAndSSetViewports(m_Viewport, m_ScissorRect)
        .SetResourceBarrier(m_RenderTargets[m_FrameIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_RtvHeap->GetCPUDescriptorHandleForHeapStart());
    rtvHandle.Offset(m_FrameIndex, m_RtvDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_DsvHeap->GetCPUDescriptorHandleForHeapStart());
    m_CommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
    const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    m_CommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_CommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);


    dx12Object.SetIA(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST, m_VertexBufferView, m_IndexBufferView);


    m_CommandList->SetGraphicsRootConstantBufferView(0, m_ConstantBuffer->GetGPUVirtualAddress());
    dx12Object.SetRootSignatureDescriptorTable({ m_SrvHeap.Get() }, { 1 });

    m_CommandList->DrawIndexedInstanced(36, 1, 0, 0, 0);
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_RenderTargets[m_FrameIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT
    );
    m_CommandList->ResourceBarrier(1, &barrier);
    ThrowIfFailed(m_CommandList->Close());
}






// --- 同步 (同前) ---
void D3D12App::WaitForGpu()
{
    // ... (同前, 100% 未修改) ...
    const UINT64 fenceToSignal = m_FenceValue;
    ThrowIfFailed(m_CommandQueue->Signal(m_Fence.Get(), fenceToSignal));
    m_FenceValue++;
    if (m_Fence->GetCompletedValue() < fenceToSignal)
    {
        ThrowIfFailed(m_Fence->SetEventOnCompletion(fenceToSignal, m_FenceEvent));
        WaitForSingleObject(m_FenceEvent, INFINITE);
    }
    m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();
}

void D3D12App::RunCommand()
{
    m_CommandList->Close();
    ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
    m_CommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
}

// --- 清理 (同前) ---
void D3D12App::Cleanup()
{
    // ... (同前, 100% 未修改) ...
    WaitForGpu(); WaitForGpu();
    if (m_pConstantBufferDataBegin)
    {
        m_ConstantBuffer->Unmap(0, nullptr);
        m_pConstantBufferDataBegin = nullptr;
    }
    if (m_FenceEvent)
    {
        CloseHandle(m_FenceEvent);
        m_FenceEvent = nullptr;
    }
}