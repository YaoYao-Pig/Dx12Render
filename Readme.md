# ShadowMap

![ShadowMap](assets/ShadowMap.gif)

总结一下实现：

需要两个Pass实现：（实际上就是用两个PSO），这两个PSO的根签名也不同，所以也需要两个根签名

## ShadowPass：

1. 在这个pass中，RTV是空的，因为我们并不关心这时候的渲染颜色，这个Pass绘制的东西也不会被SwapChain提交。

2. 这个pass中，是要绘制DSV。

   > 这里就涉及到我对于DX12的View/Descriptor和Resource设计的一个新的理解了：
   >
   > 简单来说Resource相当于是一种结构化的GPU内存上的数据，View是一个存储有烘焙数据的指向这个GPU的句柄。因此很自然的，可以有多个View指向同一个Resource。
   >
   > 这就可以在这里起到作用，我们两轮pass当中，会对同一个Resource操作：
   >
   > 1. ShadowPass会通过DSV向ComPtr\<ID3D12Resource\> m_ShadowMap写入深度数据。
   > 2. MainPass当中我们则会借助SRV把ComPtr\<ID3D12Resource\> m_ShadowMap当作是一个贴图从里面采样

3. 这里需要用到光源的MVP矩阵。我是把光源作为一个在旋转的方向光源，并且光源的投影矩阵我把它当作是正交投影来处理的。这里第一次使用光源的MVP矩阵的原因是，我们需要假设把摄像机（或者说是渲染平面）移动到光源的位置，然后计算深度，写入DSV指向的Resouce

## MainPass：

1. 绘制的主Pass，这个pass除了正常计算各个模型的MVP矩阵，来计算模型位置和变换之外，还要考虑阴影的计算。阴影的计算需要首先对把m_ShadowMap作为一个SRV进行采样。
2. 主MainPss的Shader需要额外计算一个顶点在光源坐标系下的位置：阴影采样代码是一个三步转换：`裁剪空间 -> NDC空间 -> UV空间`
   1. 通过VSMain当中，把顶点转换到光源的裁剪空间之后，再把它做了透视除法，就转换到NDC空间了` float3 lightNdc = input.lightSpacePos.xyz / input.lightSpacePos.w;`
   2. 然后我们知道，贴图的UV空间是\[0,1\]\[0,1\]的，因此我们只需要做一次Y轴反转缩放+X轴缩放到0，1就可以映射到UV空间采样了
   3. 同时，除了x，y之外，lightNdc.z当中，存储的就是一个vertex，在光源坐标系下的深度值，这个值要和阴影贴图当中采样出的结果来比较：物理的含义是，在光源视角下，可能会有很多点，投影后的x，y是一样的，但是深度不同，只有最贴近光源（深度最小的）才能有光照
3. `shadowFactor = g_ShadowMap.SampleCmp(g_SamplerCmp, shadowTexCoord, fragmentDepth);`,g_SamplerCmp这个sampler在定义的时候，定义了它的比较函数

```c++
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
```

### 上面值得注意的是*比较过滤器*和*比较函数*

![image-20251102221135578](assets/image-20251102221135578.png)

这实际上是实现**柔和阴影边缘PCF**用的，也就是计算出来的shadowFactor是一个百分比，意思是通过四个临近点采样的均值来计算在阴影中的百分比

![image-20251102221255048](assets/image-20251102221255048.png)



```c++
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
```

我让AI总结了下上面的流程：

![image-20251102213639088](assets/image-20251102213639088.png)

# 多物体渲染

![多物体渲染](assets/ShadowMap.gif)

还是这张图

这里还需要关注的是，我实现了两种不同的贴图效果：地面用的是白色，但是box用的是在贴图上采样

有两种做法：

最直观的一种是，我们认为每一种贴图对应一个PSO，因此两个PSO+独立的ConstBuffer就可以完成这点

但是显然，这俩的Vertex定义都是一样的，所以RootSignature也是一样的，区别是：

1. 使用的贴图不同 
2. 使用的matrix不同

我才用的方法是，使用同一个PSO，分多次绘制，每次绘制都绑定一次IA，第一次绘制的时候绑定Box 的IA，第二次绑定Planet的IA。然后使用哪种贴图则是在ConstBuffer当中开了一个标记位，用这个标记位来标记当前是使用白色渲染还是使用贴图采样：

```c++
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

```

### CPUGPU顺序问题

但这里有个坑，就是如何管理这个ConstBuffer。

一开始我的做法是用一个Constbuffer，然后每次**录制绘制操作前**，给常量缓冲区值赋值。但这样是有严重问题的：因为，CPU和GPU并不同步。

我们使用CommandList的时候，只是在录制指令，这个指令并没有执行，并且，这个指令也是需要GPU来执行的。然而，赋值这个操作都是在CPU执行的，这俩根本就不同步，所以当第二次赋值的时候，实际上覆盖了第一次赋值。具体的流程如下：

> 第一次给ConstBuffer赋值 -> 第二次给Constbuffer赋值 -> Box的IA绑定，绘制等在GPU执行 -> Planet的IA绑定，绘制等在GPU执行

![image-20251102210045224](assets/image-20251102210045224.png)

这个流程根本就不同步。解决方法是，我们要开一个足够大的ConstBuffer，也就是Const的这个Resource是要能容纳多个模型的Matrix（或者说常量资源的），然后赋值的时候，要对每个资源的偏移赋值

![image-20251102223555796](assets/image-20251102223555796.png)

# 多光源



# PBR

先说一下对于PBR的理解

PBR的光照计算分为两部分：镜面反射+漫反射。而漫反射则是基于能量守恒从 1- F项的

镜面反射则是基于D，G和F这三个项的计算

用一个比喻

> 想象一下，你（摄像机 V）在看一个体育场，太阳（光源 L）在你身后。体育场里坐满了**100万个**拿着**小镜子**的人（微表面）。

###  D 项 (Distribution) - “有多少镜子对准了？”

- **它回答的问题：** 在这100万人中，有多少人的镜子**恰好**对准了能把太阳（L）反射给你（V）的那个完美角度（H向量）？
- **这取决于粗糙度(Roughness)：**
  - **光滑表面 (如镜子)**：`Roughness = 0`。体育场纪律严明，**所有100万人**都把镜子对准了**同一个方向**。`D`项的值在那个方向上是一个**极大**的数（这就是为什么`D`项可以大于1，它不是百分比，是*浓度*）。
  - **粗糙表面 (如毛玻璃)**：`Roughness = 1`。体育场里乱作一团，人们的镜子朝向各不相同。可能**只有1000人**恰好对准了你。`D`项的值很小，但分布很广。

**D项 是在计算“有多少微表面（人）在参与反射”。**

### G 项 (Geometry) - “有多少对准的镜子没被挡住？”

- **它回答的问题：** 在那些**已经对准了你**的人（由`D`项选出）中，有多少人**没有**被前一排的人（其他微表面）挡住？
- **这取决于你观察的角度：**
  - **垂直观察 (V ≈ N)**：你从体育场正上方往下看，几乎**没人**被挡住。`G`项 ≈ 1.0。
  - **侧面掠射 (V ≈ 90度)**：你从体育场侧面看台的边缘看过去，**绝大多数**对准你的人都被他们前排的人挡住了。`G`项 ≈ 0.0。

**G项 是在计算“参与反射的人中，有多少是可见的”。**

###  F 项 (Fresnel) - “镜子本身的反射率是多少？”

- **它回答的问题：** OK，现在我们有了一群**对准了**（D）并且**没被挡住**（G）的人。最后一步是，他们手里拿的小镜子**本身**（材质物理特性）能反射**百分之多少**的光？
- **这取决于观察角度和材质：**
  - 菲涅尔效应说，光线以不同角度照射在“空气-镜子”这个界面时，反射的比例是不同的。
  - 比如，他们拿的是非金属（塑料镜子）。你垂直看，它只反射 4%（`F0`）。你从侧面看，它反射 100%。

**F项 是在计算“单一微表面（镜子）的物理反射比例”**

`F` 是**材质的物理特性**。（镜子是金的还是塑料的？）

`D` 和 `G` 是**表面的几何特性**。（这群人坐得是整齐还是混乱？你从哪个角度看他们？）

## 总结

或者说，我认为DGF这三个项从逻辑上时依次进行的

我们现在知道，我从一个方向观察，如果我要接收到这个方向的光，那么就一定要是存在一个平面，它的平面方向（法线）一定要是正好能让出射光反射到我的眼睛里，那么这个平面法线就必须是入射和出射光的半程向量（h）

![image-20251104235502156](assets/image-20251104235502156.png)

当加入了微表面的原理之后，我们就能想到，当我从一个角度观察一个平面，那么就一定存在一束光（这个光并不是一条光线，而是很多光线的集合），照在这个平面上，那么就一定要有**正好平面法线等于h的微平面**

D项回应的就是，到底有多少这个数量或者比例的微平面

有了D项的基础，微平面还存在自遮挡现象，也就是有的微平面会遮挡其他平面。这个现象也和h有关系，所以G项回应的就是，从这个角度观察，有多少会自遮挡。

`D * G` 得到的是 **在几何上，有多少微平面（的面积）准备好了将光线 L 反射到 V**

接下来要解决的就是**在这个微表面材质，当入射光入射之后，有多少会被反射，有多少会进入物体内部变成漫反射（F0）**，这个问题和物体的材质（F0）以及观察角度都有关系，所以，这是一个**物理问题**、

> 菲涅尔效应: 任何材质的反射率，都会随着观察角度的变化而变化。
>
> 为啥观察角度会决定反射率呢，因为当我从一个角度观察的时候，就决定了只有特定角度的入射光才能传到我的眼睛里，从而这部分入射的角度不同会导致不同的反射率的反射
>
> ![image-20251105001113562](assets/image-20251105001113562.png)

![image-20251105000456751](assets/image-20251105000456751.png)

# 摩尔纹

![摩尔纹](assets/image-20251107164903419.png)

方法就是各向异性，对于DX12来说就是直接改Sampler

```c++
// D3D12App::CreateRootSignature 函数
void D3D12App::CreateRootSignature()
{
...
    //Sampler 0: 用于主纹理
    D3D12_STATIC_SAMPLER_DESC textureSampler = {};
    
    // **  升级为各向异性过滤 **
    textureSampler.Filter = D3D12_FILTER_ANISOTROPIC; // <-- 从 MIN_MAG_MIP_LINEAR 修改
    
    textureSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    textureSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    textureSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

    textureSampler.MinLOD = 0;
    textureSampler.MaxLOD = D3D12_FLOAT32_MAX;
    textureSampler.ShaderRegister = 0;//s0
    textureSampler.RegisterSpace = 0;
    textureSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // ** 新增: 设置各向异性过滤的级别 (16x 是高质量) **
    textureSampler.MaxAnisotropy = 16; 


    // Sampler 1: 用于阴影贴图 (比较采样器)
    D3D12_STATIC_SAMPLER_DESC shadowSampler = {};
...
```

然后我发现还是很严重。。

![image-20251107172349328](assets/image-20251107172349328.png)

## MIpMap

原来是缺少了MipMap。DX12要手动的创建MipMap

![image-20251107214729790](assets/image-20251107214729790.png)

其实这个问题可以用DX12的接口来做，但是我用的CPU的MipMap。实际上就是在CPU提前创建好所有的MipMap的子对象和他的图片，然后作为SubResource，创建Resource的时候直接创建好。

> DX12中，Resouce就是由SubResouce组成的，只是如果不指定的话，就是存在第0个位置
>
> ![image-20251107220529048](assets/image-20251107220529048.png)

这里要注意的是，第一MipMap的计算方法：我们最后的目的是要让最远的时候就是一个像素，每次都是切一半，所以他是一个log2N的规模缩小，知道变成长宽只有一个像素大小。因此：

> ```c++
> int maxDim = max(textureWidth, textureHeight);
> UINT mipLevels = 1 + (UINT)floor(log2(maxDim)); // 1 + log2(max)
> ```

核心降采样用的是stbir_resize_uint8_srgb这个接口，这个接口考虑了非线性的色彩空间，大概的计算逻辑是：`ToLinear(sRGB_A) + ToLinear(sRGB_B)` -> 多相滤波-> `ToSRGB(result)(这个转换约等于pow(pixel_linear, 1.0/2.2))`

> `stb_image_resize` (stbir) 是一个高质量的图像缩放库，它在缩放时（尤其是降采样）使用的是**高质量的多相滤波 (High-Quality Polyphase Filtering)**。
>
> - **什么是多相滤波？** 当你从 2048x2048 缩放到 1024x1024 时 (生成Mip 1)，Box降采样只会查看源图像上一个 2x2 的像素块。
> - `stbir` 使用的默认滤波器（例如 Mitchell-Netravali 或 Catmull-Rom）会查看一个**更大的采样核 (Kernel)**，比如 4x4 或 6x6 的源像素，并应用一个复杂的加权平均。
> - **为什么这样做？** 这样可以**极大地减少锯齿 (Aliasing)** 并保留更多的图像细节，使纹理在远处看起来更平滑、更清晰，而不会闪烁。
>
> 

```c++
void D3D12App::GenerateMipMap(void* pData, int textureWidth, 
    int textureHeight, int mipMapLevels, 
    std::vector<stbi_uc*>& mipDataBuffers,
    int texturePixelSize = 4) {
    stbi_uc* pTextureData = (stbi_uc *) (pData);
    mipDataBuffers.push_back(pTextureData); //mipMap0;
    int curWidth = textureWidth;
    int curHeight = textureHeight;
    for (int i = 1; i < mipMapLevels; ++i) {
        int mipWidth = textureWidth >> i;
        int mipHeight = textureHeight >> i;
        stbi_uc* pMipData = (stbi_uc*)malloc(mipWidth * mipHeight * texturePixelSize);
        if (pMipData == nullptr) { throw std::runtime_error("Failed to allocate memory for Mipmap"); }

        mipDataBuffers.push_back(pMipData);
        // 计算步长 (stride)
        // 0 表示使用默认的 (width * 4)
        int input_stride_bytes = 0;
        int output_stride_bytes = 0;
        stbir_resize_uint8_srgb(
            mipDataBuffers[i - 1], textureWidth >> i-1, textureHeight >> i-1, 0, // Source
            pMipData, mipWidth, mipHeight, 0,                      // Dest
            STBIR_RGBA
        );
    }
}
```



```c++
void D3D12App::LoadTextureFromFile(
    const char* filename, 
    ComPtr<ID3D12Resource>& textureResource, 
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle)
{
    int textureWidth, textureHeight, textureChannels;
    //第0级MipMap
    stbi_uc* pTextureData = stbi_load(filename, &textureWidth, &textureHeight, &textureChannels, STBI_rgb_alpha);
    if (pTextureData == nullptr) {
        std::string errorMsg = "Failed to load texture file: ";
        errorMsg += filename;
        throw std::runtime_error(errorMsg);
    }
    UINT texturePixelSize = 4; // STBI_rgb_alpha

    int maxDim = max(textureWidth, textureHeight);
    UINT mipLevels = 1 + (UINT)floor(log2(maxDim)); // 1 + log2(max)

    // 创建纹理资源 (Texture Resource)
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = textureWidth;
    textureDesc.Height = textureHeight;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = mipLevels;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // STBI_rgb_alpha 强制转为 4 通道
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(m_Device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr, IID_PPV_ARGS(&textureResource)
    ));

    std::vector<stbi_uc*> mipDataBuffers;
    GenerateMipMap(pTextureData, textureWidth, textureHeight, mipLevels, mipDataBuffers);

    //准备 Subresource 数据
    int currentWidth = textureWidth;
    int currentHeight = textureHeight;
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    for (UINT i = 0; i < mipLevels; ++i)
    {
        D3D12_SUBRESOURCE_DATA subresourceData = {};
        subresourceData.pData = mipDataBuffers[i];
        subresourceData.RowPitch = (UINT64)currentWidth * texturePixelSize;
        subresourceData.SlicePitch = subresourceData.RowPitch * currentHeight;

        subresources.push_back(subresourceData);

        currentWidth = max(1, currentWidth / 2);
        currentHeight = max(1, currentHeight / 2);
    }

    // 获取上传缓冲区大小 (Upload Buffer Size)
    UINT64 textureUploadBufferSize;
    m_Device->GetCopyableFootprints(&textureDesc, 0, mipLevels, 0, nullptr, nullptr, nullptr, &textureUploadBufferSize);

    // 创建上传缓冲区 (Upload Buffer)
    ComPtr<ID3D12Resource> pTextureUploadBuffer;
    heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(textureUploadBufferSize);
    ThrowIfFailed(m_Device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&pTextureUploadBuffer)
    ));
    resourceManager.AddResource2TmpBuffer(pTextureUploadBuffer); // 使用你的 ResourceManager 来管理临时缓冲区
    // 拷贝数据到纹理资源
    UpdateSubresources(
        m_CommandList.Get(), 
        textureResource.Get(), 
        pTextureUploadBuffer.Get(), 
        0, 0, mipLevels, 
        subresources.data());

    stbi_image_free(mipDataBuffers[0]);
    // (stbi_resize 的 Mip 1+)
    for (UINT i = 1; i < mipLevels; ++i)
    {
        free(mipDataBuffers[i]);
    }

    // 创建 SRV (Shader Resource View)
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = -1;
    m_Device->CreateShaderResourceView(textureResource.Get(), &srvDesc, srvHandle);
}
```

> UpdateSubresources在做什么：
>
> ![image-20251107221119695](assets/image-20251107221119695.png)
>
> ![image-20251107221129795](assets/image-20251107221129795.png)
>
> ![image-20251107221136235](assets/image-20251107221136235.png)





这里想说下Sampler。Sampler可以理解为一种通用的，采样逻辑封装。也就是一种，算子？

当我们想要采样贴图的时候，总会有各种采样方式，是MipMap，线性差值还是三线性等等，这些都通过Sampler来封装。

Sampler是绑定在根签名上的



# TODO

1. IBL
2. 透明管线
3. 多模型加载封装
4. 多光源阴影计算
5. 模板缓冲区实现镜子效果
6. LOD（基于顶点重建）
7. MipMap
