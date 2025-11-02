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
    m_RtvDescriptorSize(0),
    m_pConstantBufferDataBegin(nullptr), 
    m_LightConstantBuffer(nullptr),
    m_pShadowConstantBufferDataBegin(nullptr),
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
    //初始化DSVHeap
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
    //深度缓冲区
    m_Device->CreateDepthStencilView(m_DepthStencilBuffer.Get(), nullptr, m_DsvHeap->GetCPUDescriptorHandleForHeapStart());
    //ShadowMap使用的以光源为起始的深度缓冲
    CD3DX12_CPU_DESCRIPTOR_HANDLE shadowDsvHandler(m_DsvHeap->GetCPUDescriptorHandleForHeapStart());
    shadowDsvHandler.Offset(1, m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV)); //偏移1，现在就是拿到了DsvHeap中第二个view
    CD3DX12_CPU_DESCRIPTOR_HANDLE shadowCpuSrvHandler(m_SrvHeap->GetCPUDescriptorHandleForHeapStart());
    shadowCpuSrvHandler.Offset(1, m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    CD3DX12_GPU_DESCRIPTOR_HANDLE shadowGpuSrvHandler(m_SrvHeap->GetGPUDescriptorHandleForHeapStart());
    shadowGpuSrvHandler.Offset(1, m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    // 创建 ShadowMap 对象，分辨率 2048x2048 (你可以按需调整)
    m_ShadowMap = std::make_unique<ShadowMap>(m_Device.Get(), 2048, 2048);
    // 告知 ShadowMap 它在堆中的位置
    m_ShadowMap->BuildDescriptors(shadowCpuSrvHandler, shadowGpuSrvHandler, shadowDsvHandler);

    //  创建阴影通道的常量缓冲区 (m_ShadowConstantBuffer) 
    //Pass 1 (阴影通道) 专属的常量缓冲区，它只存放 ShadowVS 工作所必需的数据：光源WVP矩阵。
    const UINT shadowConstantBufferSize = c_alignedShadowConstantBufferSize * 64; //最多可以存储64个物体
    heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(shadowConstantBufferSize);
    ThrowIfFailed(m_Device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&m_ShadowConstantBuffer)
    ));
    m_ShadowConstantBuffer->SetName(L"Shadow Constant Buffer");
    ThrowIfFailed(m_ShadowConstantBuffer->Map(0, nullptr, (void**)&m_pShadowConstantBufferDataBegin));

    //创建 Light Constant Buffer (m_LightConstantBuffer)
    constexpr  UINT lightSize = (sizeof(Light) + 255) & ~255;
    resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(lightSize);
    heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    ThrowIfFailed(m_Device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        __uuidof(ID3D12Resource), (void**)&m_LightConstantBuffer));

    D3D12_CONSTANT_BUFFER_VIEW_DESC constBufferDesc;
    constBufferDesc.BufferLocation = m_LightConstantBuffer->GetGPUVirtualAddress();
    constBufferDesc.SizeInBytes = lightSize;
    D3D12_CPU_DESCRIPTOR_HANDLE constHandler = m_ConstHeap->GetCPUDescriptorHandleForHeapStart();
    m_Device->CreateConstantBufferView(&constBufferDesc, constHandler);
    m_LightConstantBuffer->SetName(L"Light Constant Buffer");
    m_LightConstantBuffer->Map(0, nullptr, (void**)&m_lightConstBufferDataBegin);


    ThrowIfFailed(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&m_CommandAllocator));
    ThrowIfFailed(m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CommandAllocator.Get(), nullptr, __uuidof(ID3D12GraphicsCommandList), (void**)&m_CommandList));
    ThrowIfFailed(m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void**)&m_Fence));
    
    m_FenceValue = 1;
    m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_FenceEvent == nullptr) ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));

    // 其他
    shaderCompiler.Init({ "VSMain", "PSMain" });
    shaderCompiler.AddShaderEntryPoints({ "ShadowVS", "ShadowPS" }); //注册阴影着色器入口点
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
    dsvHeapDesc.NumDescriptors = 2; //第一个用于深度缓冲，第二个用于shadowmap
    ThrowIfFailed(m_Device->CreateDescriptorHeap(&dsvHeapDesc, __uuidof(ID3D12DescriptorHeap), (void**)&m_DsvHeap));
     
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NumDescriptors = 2; //第1个用于主纹理,第2个用于阴影贴图
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(m_Device->CreateDescriptorHeap(&srvHeapDesc, __uuidof(ID3D12DescriptorHeap), (void**)&m_SrvHeap));


    D3D12_DESCRIPTOR_HEAP_DESC constHeapDesc = {};
    constHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    constHeapDesc.NumDescriptors = 1;
    constHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(m_Device->CreateDescriptorHeap(&constHeapDesc, __uuidof(ID3D12DescriptorHeap), (void**)&m_ConstHeap));
}

void D3D12App::CreateRootSignature()
{
    //----------- 创建主渲染通道的根签名 (m_RootSignature) -----------
    CD3DX12_DESCRIPTOR_RANGE ranges[2];
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0); //一个srv类型的view，存在t0
    ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);// 一个cbv，存在于b1;
    CD3DX12_ROOT_PARAMETER parameters[3];
    parameters[0].InitAsConstantBufferView(0);//一个const,存在于b0: SceneConstants
    parameters[1].InitAsDescriptorTable(1, &ranges[0]); // t0, t1
    parameters[2].InitAsDescriptorTable(1, &ranges[1]); // b1

    //Sampler 0: 用于主纹理
    D3D12_STATIC_SAMPLER_DESC textureSampler = {};
    textureSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    textureSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    textureSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    textureSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

    textureSampler.MinLOD = 0;
    textureSampler.MaxLOD = D3D12_FLOAT32_MAX;
    textureSampler.ShaderRegister = 0;//s0
    textureSampler.RegisterSpace = 0;
    textureSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Sampler 1: 用于阴影贴图 (比较采样器)
    D3D12_STATIC_SAMPLER_DESC shadowSampler = {};
    shadowSampler.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT; // 比较过滤器
    shadowSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER; // 超出范围的区域按 1.0 处理 (不在阴影中)
    shadowSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    shadowSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    shadowSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    shadowSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; //比较函数
    shadowSampler.ShaderRegister = 1; // s1
    shadowSampler.RegisterSpace = 0;
    shadowSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC samplers[] = { textureSampler, shadowSampler };

    CD3DX12_ROOT_SIGNATURE_DESC signatureDes = {};
    signatureDes.Init(_countof(parameters), parameters,
        _countof(samplers), samplers,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    ComPtr<ID3DBlob> signature, error;
    ThrowIfFailed(D3D12SerializeRootSignature(&signatureDes, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));

    ThrowIfFailed(m_Device->CreateRootSignature(0,
        signature->GetBufferPointer(),
        signature->GetBufferSize(),
        __uuidof(ID3D12RootSignature),
        (void**)&m_RootSignature));


    // ----------- 创建阴影通道的根签名 (m_ShadowRootSignature) -----------
    // Slot 0 (b0): ShadowConstants (只包含 Light WVP)
    CD3DX12_ROOT_PARAMETER shadowParameters[1];
    shadowParameters[0].InitAsConstantBufferView(0); // b0: ShadowConstants
    CD3DX12_ROOT_SIGNATURE_DESC shadowSignatureDes = {};
    shadowSignatureDes.Init(_countof(shadowParameters), shadowParameters,
        0, nullptr, // 阴影通道不需要 Sampler
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    ComPtr<ID3DBlob> shadowSignature, shadowError;
    ThrowIfFailed(D3D12SerializeRootSignature(&shadowSignatureDes, D3D_ROOT_SIGNATURE_VERSION_1, &shadowSignature, &shadowError));
    ThrowIfFailed(m_Device->CreateRootSignature(0,
        shadowSignature->GetBufferPointer(), shadowSignature->GetBufferSize(),
        IID_PPV_ARGS(&m_ShadowRootSignature)));
    m_ShadowRootSignature->SetName(L"Shadow Root Signature");
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
    // ----------- 创建主渲染通道的 PSO (RenderQueue::Common) -----------
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = m_RootSignature.Get();
    psoDesc.VS.pShaderBytecode = shaderCompiler.GetShader("VSMain")->GetBufferPointer();
    psoDesc.VS.BytecodeLength = shaderCompiler.GetShader("VSMain")->GetBufferSize();
    psoDesc.PS.pShaderBytecode = shaderCompiler.GetShader("PSMain")->GetBufferPointer();
    psoDesc.PS.BytecodeLength = shaderCompiler.GetShader("PSMain")->GetBufferSize();

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

    // -----------  创建阴影通道的 PSO (RenderQueue::Shadow) -----------
    D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPsoDesc = {};
    shadowPsoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    shadowPsoDesc.pRootSignature = m_ShadowRootSignature.Get(); // 使用 m_ShadowRootSignature

    // 使用新的 ShadowVS / ShadowPS 着色器 
    shadowPsoDesc.VS.pShaderBytecode = shaderCompiler.GetShader("ShadowVS")->GetBufferPointer();
    shadowPsoDesc.VS.BytecodeLength = shaderCompiler.GetShader("ShadowVS")->GetBufferSize();
    shadowPsoDesc.PS.pShaderBytecode = shaderCompiler.GetShader("ShadowPS")->GetBufferPointer();
    shadowPsoDesc.PS.BytecodeLength = shaderCompiler.GetShader("ShadowPS")->GetBufferSize();

    // 阴影PSO的光栅化状态
    D3D12_RASTERIZER_DESC shadowRasterizerDesc = {};
    shadowRasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    shadowRasterizerDesc.CullMode = D3D12_CULL_MODE_NONE; // 剔除背面
    shadowRasterizerDesc.FrontCounterClockwise = FALSE;
    shadowRasterizerDesc.DepthClipEnable = TRUE;
    // 为阴影添加深度偏移 - 避免shadow acne 
    shadowRasterizerDesc.DepthBias = 10000;
    shadowRasterizerDesc.DepthBiasClamp = 0.1f;
    shadowRasterizerDesc.SlopeScaledDepthBias = 2.f;

    shadowPsoDesc.RasterizerState = shadowRasterizerDesc;
    shadowPsoDesc.BlendState = blendDesc;

    // 阴影PSO的深度状态
    shadowPsoDesc.DepthStencilState.DepthEnable = TRUE;
    shadowPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    shadowPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

    shadowPsoDesc.SampleMask = UINT_MAX;
    shadowPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    //阴影通道不渲染到 RTV
    shadowPsoDesc.NumRenderTargets = 0;
    shadowPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;

    //阴影通道渲染到 DSV
    shadowPsoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT; //必须匹配 ShadowMap的DSV格式 
    shadowPsoDesc.SampleDesc.Count = 1;
    psoContainer.BuildPSO(RenderQueue::Shadow, &shadowPsoDesc);
}

void D3D12App::CreateResources()
{
    resourceManager.UploadResource(geometry.vertexs.data(), geometry.vertexSize, m_VertexBuffer);
    resourceManager.UploadResource(geometry.indices.data(), geometry.indiceSize, m_IndexBuffer);

    resourceManager.UploadResource(geometry.planeVertexs.data(), geometry.planeVertexSize, m_PlaneVertexBuffer);
    resourceManager.UploadResource(geometry.planeIndices.data(), geometry.planeIndiceSize, m_PlaneIndexBuffer);

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
    //阴影贴图的 SRV 是在 InitD3D12 中由 m_ShadowMap 自己创建的
    CD3DX12_RESOURCE_BARRIER barriers[5] = {
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
        ),
       CD3DX12_RESOURCE_BARRIER::Transition(
            m_PlaneVertexBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
        ),
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_PlaneIndexBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_INDEX_BUFFER
        )
    };
    m_CommandList->ResourceBarrier(_countof(barriers), barriers);

    m_VertexBufferView.BufferLocation = m_VertexBuffer->GetGPUVirtualAddress();
    m_VertexBufferView.StrideInBytes = sizeof(Vertex);
    m_VertexBufferView.SizeInBytes = geometry.vertexSize;
    m_IndexBufferView.BufferLocation = m_IndexBuffer->GetGPUVirtualAddress();
    m_IndexBufferView.Format = DXGI_FORMAT_R16_UINT;
    m_IndexBufferView.SizeInBytes = geometry.indiceSize;

    m_PlaneVertexBufferView.BufferLocation = m_PlaneVertexBuffer->GetGPUVirtualAddress();
    m_PlaneVertexBufferView.StrideInBytes = sizeof(Vertex);
    m_PlaneVertexBufferView.SizeInBytes = geometry.planeVertexSize;

    m_PlaneIndexBufferView.BufferLocation = m_PlaneIndexBuffer->GetGPUVirtualAddress();
    m_PlaneIndexBufferView.Format = DXGI_FORMAT_R16_UINT;
    m_PlaneIndexBufferView.SizeInBytes = geometry.planeIndiceSize;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    m_Device->CreateShaderResourceView(m_Texture.Get(), &srvDesc, m_SrvHeap->GetCPUDescriptorHandleForHeapStart());

    const UINT constantBufferSize = c_alignedSceneConstantBufferSize * 64;
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
    float pos[3] = { 0.5f, -0.8f, -1.0f };
    float dir[3] = { 1.0f, -1.0f, 0.9f };
    light.Init(pos, dir);
    memcpy(m_lightConstBufferDataBegin, &light, sizeof(Light));
    CreateRootSignature();
    shaderCompiler.CompileShader();
    CreatePSO();
    CreateResources();
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

    // 更新光源 为了让阴影动起来，光源绕 Y 轴旋转
    XMMATRIX lightRotation = XMMatrixRotationY(m_TotalTime * 0.2f);
    // (使用你 Light.h 中的默认方向或你期望的方向)
    XMVECTOR lightDir = XMVectorSet(light.dir[0], light.dir[1], light.dir[2], 0.0f);
    lightDir = XMVector3Normalize(lightDir);
    lightDir = XMVector3TransformNormal(lightDir, lightRotation);
    //计算光源的 View 和 Proj 矩阵
    // 使用正交投影来模拟定向光
    // 光源位置 (我们把它放在离原点较远的地方，沿着光照反方向)
    XMVECTOR lightPos = XMVectorScale(XMVectorNegate(lightDir), 20.0f); // 距离原点20个单位
    XMVECTOR targetPos = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f); // 看向原点
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX view = XMMatrixLookAtLH(lightPos, targetPos, up);
    // 创建一个覆盖场景的正交投影 (20x20的范围, 深度范围 1.0 到 50.0)
    XMMATRIX proj = XMMatrixOrthographicLH(20.0f, 20.0f, 1.0f, 50.0f);

    // 光源的 View-Projection 矩阵
    XMMATRIX lightWvp = view * proj;
    XMStoreFloat4x4(&m_LightViewMatrix, view);
    XMStoreFloat4x4(&m_LightProjMatrix, proj);

    //相机
    // 获取新矩阵
    XMMATRIX viewMatrix = m_Camera.GetViewMatrix();
    XMMATRIX projMatrix = m_Camera.GetProjMatrix();
    XMMATRIX worldMatrix = XMMatrixRotationY(m_TotalTime * 1.0f); // 立方体仍然在旋转
    XMMATRIX wvpMatrix = worldMatrix * viewMatrix * projMatrix;


    XMMATRIX cubeWorldMatrix = XMMatrixRotationY(m_TotalTime * 1.0f); // 立方体旋转
    XMStoreFloat4x4(&m_CubeWorldMatrix, cubeWorldMatrix);

    // 平面：放大10倍，并下移1个单位
    XMMATRIX planeWorldMatrix = XMMatrixScaling(10.0f, 10.0f, 10.0f) * XMMatrixTranslation(0.0f, -0.5f, 0.0f);
    XMStoreFloat4x4(&m_PlaneWorldMatrix, planeWorldMatrix);
}

void D3D12App::OnRender()
{
    // Pass 1: 渲染阴影贴图
    RenderShadowMap();
    // Pass 2: 渲染主场景 
    RenderMainPass();
    ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
    m_CommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    ThrowIfFailed(m_SwapChain->Present(1, 0));
    WaitForGpu();
}

// Pass 1: 渲染阴影贴图
void D3D12App::RenderShadowMap()
{
    // 重置命令列表分配器和命令列表
    ThrowIfFailed(m_CommandAllocator->Reset());
    ThrowIfFailed(m_CommandList->Reset(m_CommandAllocator.Get(), psoContainer.GetPSO(RenderQueue::Shadow)));
    m_CommandList->SetGraphicsRootSignature(m_ShadowRootSignature.Get());
    // 设置视口和裁剪矩形 (使用 ShadowMap 的)
    D3D12_VIEWPORT shadowViewport = m_ShadowMap->Viewport();
    D3D12_RECT shadowScissorRect = m_ShadowMap->ScissorRect();
    m_CommandList->RSSetViewports(1, &shadowViewport);
    m_CommandList->RSSetScissorRects(1, &shadowScissorRect);

    // 资源屏障: 转换阴影贴图
    // 从 GENERIC_READ (初始/上一帧的状态) 转换为 DEPTH_WRITE
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_ShadowMap->Resource(),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_RESOURCE_STATE_DEPTH_WRITE
    );
    m_CommandList->ResourceBarrier(1, &barrier);

    // 清除深度缓冲区 (阴影贴图)
    m_CommandList->ClearDepthStencilView(
        m_ShadowMap->Dsv(),
        D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr
    );

    // 设置渲染目标
    CD3DX12_CPU_DESCRIPTOR_HANDLE shadowDsvHandle = m_ShadowMap->Dsv();
    m_CommandList->OMSetRenderTargets(0, nullptr, FALSE, &shadowDsvHandle);
    
    // 绘制立方体 (Caster) 
        //  为立方体更新阴影CBV
    XMMATRIX cubeWorld = XMLoadFloat4x4(&m_CubeWorldMatrix);
    XMMATRIX lightView = XMLoadFloat4x4(&m_LightViewMatrix);
    XMMATRIX lightProj = XMLoadFloat4x4(&m_LightProjMatrix);
    XMMATRIX shadowWvp = cubeWorld * lightView * lightProj;
    XMStoreFloat4x4(&m_ShadowConstants.lightWvpMatrix, shadowWvp);
    memcpy(m_pShadowConstantBufferDataBegin, &m_ShadowConstants, sizeof(ShadowConstants));

    //绑定立方体的数据并绘制
    m_CommandList->SetGraphicsRootConstantBufferView(0, m_ShadowConstantBuffer->GetGPUVirtualAddress());
    m_CommandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_CommandList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
    m_CommandList->IASetIndexBuffer(&m_IndexBufferView);
    m_CommandList->DrawIndexedInstanced(36, 1, 0, 0, 0); // 立方体索引 (36)

    // 绘制平面 (Receiver, 但它也投射阴影)
    // 为平面更新阴影CBV
    XMMATRIX planeWorld = XMLoadFloat4x4(&m_PlaneWorldMatrix);
    shadowWvp = planeWorld * lightView * lightProj;
    XMStoreFloat4x4(&m_ShadowConstants.lightWvpMatrix, shadowWvp);
    UINT8* pPlaneShadowData = m_pShadowConstantBufferDataBegin + c_alignedShadowConstantBufferSize * 1; 
    memcpy(pPlaneShadowData, &m_ShadowConstants, sizeof(ShadowConstants));

    // 绑定平面的数据并绘制
    D3D12_GPU_VIRTUAL_ADDRESS planeShadowGpuVA = m_ShadowConstantBuffer->GetGPUVirtualAddress() + c_alignedShadowConstantBufferSize * 1;
    m_CommandList->SetGraphicsRootConstantBufferView(0, planeShadowGpuVA);
    m_CommandList->IASetVertexBuffers(0, 1, &m_PlaneVertexBufferView);
    m_CommandList->IASetIndexBuffer(&m_PlaneIndexBufferView);
    m_CommandList->DrawIndexedInstanced(6, 1, 0, 0, 0); // 平面索引 (6)


    //资源屏障: 转换阴影贴图
    //     从 DEPTH_WRITE 转换为 GENERIC_READ (以便主通道读取)
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_ShadowMap->Resource(),
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        D3D12_RESOURCE_STATE_GENERIC_READ
    );
    m_CommandList->ResourceBarrier(1, &barrier);
}

// --- 填充命令列表 ---
void D3D12App::RenderMainPass()
{
    // 设置主通道 PSO
    m_CommandList->SetPipelineState(psoContainer.GetPSO(RenderQueue::Common));

    // 设置主根签名
    m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());

    // 设置视口和裁剪矩形 (主摄像机)
    m_CommandList->RSSetViewports(1, &m_Viewport);
    m_CommandList->RSSetScissorRects(1, &m_ScissorRect);

    // 资源屏障 (Render Target)
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_RenderTargets[m_FrameIndex].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );
    m_CommandList->ResourceBarrier(1, &barrier);

    // 设置主渲染目标 (RTV 和 DSV) 
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_RtvHeap->GetCPUDescriptorHandleForHeapStart());
    rtvHandle.Offset(m_FrameIndex, m_RtvDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_DsvHeap->GetCPUDescriptorHandleForHeapStart()); // (主 DSV 在 Slot 0)
    m_CommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    // 清除 RTV 和 DSV
    const float clearColor[] = { 0.1f, 0.2f, 0.4f, 1.0f };
    m_CommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_CommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    //  绑定描述符堆 
    // SRV 堆现在包含 Texture 和 ShadowMap
    // Const 堆包含 Light)
    // Slot 1 现在指向包含 2 个描述符的表：t0=Texture, t1=ShadowMap
    ID3D12DescriptorHeap* srvHeap[] = { m_SrvHeap.Get() };
    m_CommandList->SetDescriptorHeaps(_countof(srvHeap), srvHeap);
    m_CommandList->SetGraphicsRootDescriptorTable(1, m_SrvHeap->GetGPUDescriptorHandleForHeapStart());

    // 2. 切换(Set)到 Const 堆，并绑定 Slot 2
    // (Slot 2 指向 b1=Light)
    ID3D12DescriptorHeap* constHeap[] = { m_ConstHeap.Get() };
    m_CommandList->SetDescriptorHeaps(_countof(constHeap), constHeap); // 这一步会替换掉 SrvHeap
    m_CommandList->SetGraphicsRootDescriptorTable(2, m_ConstHeap->GetGPUDescriptorHandleForHeapStart());


    // --- 绘制立方体 ---
    // 为立方体更新主CBV
    XMMATRIX cubeWorld = XMLoadFloat4x4(&m_CubeWorldMatrix);
    XMMATRIX view = m_Camera.GetViewMatrix();
    XMMATRIX proj = m_Camera.GetProjMatrix();
    XMMATRIX lightView = XMLoadFloat4x4(&m_LightViewMatrix);
    XMMATRIX lightProj = XMLoadFloat4x4(&m_LightProjMatrix);

    XMMATRIX wvp = cubeWorld * view * proj;
    XMMATRIX lightWvp = cubeWorld * lightView * lightProj;

    XMStoreFloat4x4(&m_Constants.wvpMatrix, wvp);
    XMStoreFloat4x4(&m_Constants.worldMatrix, cubeWorld);
    XMStoreFloat4x4(&m_Constants.lightWvpMatrix, lightWvp);
    m_Constants.textureInfo.x = 1.0f;
    memcpy(m_pConstantBufferDataBegin, &m_Constants, sizeof(SceneConstants));

    // 绑定立方体的数据并绘制
    m_CommandList->SetGraphicsRootConstantBufferView(0, m_ConstantBuffer->GetGPUVirtualAddress());
    m_CommandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_CommandList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
    m_CommandList->IASetIndexBuffer(&m_IndexBufferView);
    m_CommandList->DrawIndexedInstanced(36, 1, 0, 0, 0); // 立方体

    // --- 绘制平面 --- 
    // 为平面更新主CBV
    XMMATRIX planeWorld = XMLoadFloat4x4(&m_PlaneWorldMatrix);

    wvp = planeWorld * view * proj;
    lightWvp = planeWorld * lightView * lightProj;

    XMStoreFloat4x4(&m_Constants.wvpMatrix, wvp);
    XMStoreFloat4x4(&m_Constants.worldMatrix, planeWorld);
    XMStoreFloat4x4(&m_Constants.lightWvpMatrix, lightWvp);
    m_Constants.textureInfo.x = 0.0f;
    memcpy(m_pConstantBufferDataBegin + c_alignedSceneConstantBufferSize * 1, &m_Constants, sizeof(SceneConstants));

    // 绑定平面的数据并绘制
    m_CommandList->SetGraphicsRootConstantBufferView(0, m_ConstantBuffer->GetGPUVirtualAddress() + c_alignedSceneConstantBufferSize * 1);
    m_CommandList->IASetVertexBuffers(0, 1, &m_PlaneVertexBufferView);
    m_CommandList->IASetIndexBuffer(&m_PlaneIndexBufferView);
    m_CommandList->DrawIndexedInstanced(6, 1, 0, 0, 0); // 平面

    // 资源屏障 (Present)
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m_RenderTargets[m_FrameIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT
    );
    m_CommandList->ResourceBarrier(1, &barrier);

    //关闭命令列表
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

void D3D12App::Cleanup()
{
    WaitForGpu(); WaitForGpu();
    if (m_pConstantBufferDataBegin)
    {
        m_ConstantBuffer->Unmap(0, nullptr);
        m_pConstantBufferDataBegin = nullptr;
    }
    if (m_pShadowConstantBufferDataBegin)
    {
        m_ShadowConstantBuffer->Unmap(0, nullptr);
        m_pShadowConstantBufferDataBegin = nullptr;
    }
    if (m_lightConstBufferDataBegin)
    {
        m_LightConstantBuffer->Unmap(0, nullptr);
        m_lightConstBufferDataBegin = nullptr;
    }
    if (m_FenceEvent)
    {
        CloseHandle(m_FenceEvent);
        m_FenceEvent = nullptr;
    }
}