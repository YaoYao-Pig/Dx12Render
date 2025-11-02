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

![ShadowMap](assets/ShadowMap.gif)

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

