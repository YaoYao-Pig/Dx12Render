#include "PCH.h"
#include "Geometry.h"
void CalculateTangents(std::vector<Vertex>& vertices, const std::vector<unsigned short>& indices)
{
    // 为每个顶点初始化切线为 (0,0,0)
    for (auto& v : vertices)
    {
        v.tangent = XMFLOAT3(0.0f, 0.0f, 0.0f);
    }

    // 遍历所有三角形
    for (size_t i = 0; i < indices.size(); i += 3)
    {
        // 获取三角形的三个顶点索引
        unsigned short i0 = indices[i + 0];
        unsigned short i1 = indices[i + 1];
        unsigned short i2 = indices[i + 2];

        // 获取顶点
        Vertex& v0 = vertices[i0];
        Vertex& v1 = vertices[i1];
        Vertex& v2 = vertices[i2];

        // 计算边 (Edge)
        XMVECTOR e1 = XMLoadFloat3(&v1.position) - XMLoadFloat3(&v0.position);
        XMVECTOR e2 = XMLoadFloat3(&v2.position) - XMLoadFloat3(&v0.position);

        // 计算UV增量 (Delta UV)
        float du1 = v1.texcoord.x - v0.texcoord.x;
        float dv1 = v1.texcoord.y - v0.texcoord.y;
        float du2 = v2.texcoord.x - v0.texcoord.x;
        float dv2 = v2.texcoord.y - v0.texcoord.y;

        // 计算 T (Tangent) 和 B (Bitangent)
        float r = 1.0f / (du1 * dv2 - dv1 * du2 + 0.0001f); // 0.0001f 避免除零

        XMVECTOR tangent = (e1 * dv2 - e2 * dv1) * r;

        // 我们不需要 bitangent，因为我们可以在着色器中用 N 和 T 叉乘得到
        // XMVECTOR bitangent = (e2 * du1 - e1 * du2) * r; 

        // 将这个三角形计算出的切线 累加 到它的三个顶点上
        XMFLOAT3 t;
        XMStoreFloat3(&t, tangent);

        v0.tangent.x += t.x; v0.tangent.y += t.y; v0.tangent.z += t.z;
        v1.tangent.x += t.x; v1.tangent.y += t.y; v1.tangent.z += t.z;
        v2.tangent.x += t.x; v2.tangent.y += t.y; v2.tangent.z += t.z;
    }

    // 正交化 (Gram-Schmidt) 和归一化
    for (auto& v : vertices)
    {
        XMVECTOR n = XMLoadFloat3(&v.normal);
        XMVECTOR t = XMLoadFloat3(&v.tangent);

        // n = normalize(n)
        n = XMVector3Normalize(n);

        // t = normalize(t - n * dot(n, t))
        // (Gram-Schmidt: 确保 T 和 N 垂直)
        t = XMVector3Normalize(t - n * XMVector3Dot(n, t));

        // (我们不需要 bitangent.w 来存储方向)
        // float tangentDir = (XMVectorGetX(XMVector3Dot(XMVector3Cross(n, t), b)) > 0.0f) ? 1.0f : -1.0f;

        XMStoreFloat3(&v.normal, n);
        XMStoreFloat3(&v.tangent, t);
    }
}


void Geometry::LoadCubeGeometry()
{
    Vertex cubeVertices[] =
    {
        { { -0.5f, -0.5f,  0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
        { { -0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
        { {  0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
        { {  0.5f, -0.5f,  0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
        { { -0.5f, -0.5f, -0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
        { {  0.5f, -0.5f, -0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
        { {  0.5f,  0.5f, -0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
        { { -0.5f,  0.5f, -0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
        { { -0.5f, -0.5f, -0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } },
        { { -0.5f,  0.5f, -0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
        { { -0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
        { { -0.5f, -0.5f,  0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } },
        { {  0.5f, -0.5f,  0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f } },
        { {  0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f } },
        { {  0.5f,  0.5f, -0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f } },
        { {  0.5f, -0.5f, -0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f }, { -1.0f, 0.0f, 0.0f } },
        { { -0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, -1.0f, 0.0f } },
        { { -0.5f,  0.5f, -0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, -1.0f, 0.0f } },
        { {  0.5f,  0.5f, -0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f } },
        { {  0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, -1.0f, 0.0f } },
        { { -0.5f, -0.5f, -0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } },
        { { -0.5f, -0.5f,  0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
        { {  0.5f, -0.5f,  0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
        { {  0.5f, -0.5f, -0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } }
    };

    vertexs.assign(cubeVertices, cubeVertices + 24);

    unsigned short cubeIndices[] =
    {
        0, 1, 2, 0, 3, 1, // 正
        4, 5, 6, 4, 6, 7, // 背
        8, 9, 10, 8, 11, 9, // 左
        12, 13, 14, 12, 14, 15, // 右
        16, 17, 18, 16, 18, 19, // 顶
        20, 21, 22, 20, 22, 23  // 底
    };
    indices.assign(cubeIndices, cubeIndices + 36);

    CalculateTangents(vertexs, indices);

    vertexSize = (UINT)vertexs.size() * sizeof(Vertex);
    indiceSize = (UINT)indices.size() * sizeof(unsigned short);


    
}

void Geometry::LoadPlaneGeometry()
{
    // 定义一个平面 (一个大的四边形)，法线朝上 (+Y)
    Vertex planeVerts[] =
    {
        { { -1.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
        { {-1.0f, 0.0f,  1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} },
        { {1.0f, 0.0f,  1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f} },
        { {1.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f} }
    };
    planeVertexs.assign(planeVerts, planeVerts + 4);

    unsigned short planeIdx[] = { 
        0, 1, 2, 
        0, 3, 1 };
    planeIndices.assign(planeIdx, planeIdx + 6);

    planeVertexSize = (UINT)planeVertexs.size() * sizeof(Vertex);
    planeIndiceSize = (UINT)planeIndices.size() * sizeof(unsigned short);
}

void Geometry::LoadSphereGeometry()
{
    sphereVertexs.clear();
    sphereIndices.clear();

    const float PI = 3.1415926535f;
    const float radius = 1.0f;
    const UINT stackCount = 20; // 纬度
    const UINT sliceCount = 20; // 经度

    // --- 创建顶点 ---
    // 从北极点开始
    Vertex topVertex;
    topVertex.position = XMFLOAT3(0.0f, radius, 0.0f);
    topVertex.normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
    topVertex.texcoord = XMFLOAT2(0.5f, 0.0f);
    topVertex.tangent = XMFLOAT3(1.0f, 0.0f, 0.0f); // 北极点的切线
    topVertex.color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    sphereVertexs.push_back(topVertex);

    // 中间的"环"
    float phiStep = PI / stackCount;
    float thetaStep = 2.0f * PI / sliceCount;

    for (UINT i = 1; i <= stackCount - 1; ++i)
    {
        float phi = i * phiStep; // 0 (北极) 到 PI (南极)

        for (UINT j = 0; j <= sliceCount; ++j)
        {
            float theta = j * thetaStep; // 0 到 2*PI

            Vertex v;

            // 球坐标 -> 笛卡尔坐标
            v.position.x = radius * sinf(phi) * cosf(theta);
            v.position.y = radius * cosf(phi);
            v.position.z = radius * sinf(phi) * sinf(theta);

            // 法线 (对于球体，法线就是归一化的位置)
            XMStoreFloat3(&v.normal, XMVector3Normalize(XMLoadFloat3(&v.position)));

            // 纹理坐标
            v.texcoord = XMFLOAT2(theta / (2.0f * PI), phi / PI);

            // 切线 (dPos / dTheta)
            v.tangent.x = -radius * sinf(phi) * sinf(theta);
            v.tangent.y = 0.0f;
            v.tangent.z = radius * sinf(phi) * cosf(theta);
            XMStoreFloat3(&v.tangent, XMVector3Normalize(XMLoadFloat3(&v.tangent)));

            v.color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

            sphereVertexs.push_back(v);
        }
    }

    // 南极点
    Vertex bottomVertex;
    bottomVertex.position = XMFLOAT3(0.0f, -radius, 0.0f);
    bottomVertex.normal = XMFLOAT3(0.0f, -1.0f, 0.0f);
    bottomVertex.texcoord = XMFLOAT2(0.5f, 1.0f);
    bottomVertex.tangent = XMFLOAT3(1.0f, 0.0f, 0.0f); // 南极点的切线
    bottomVertex.color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    sphereVertexs.push_back(bottomVertex);


    // --- 创建索引 ---
    // 1. 北极的"扇形"
    for (UINT i = 1; i <= sliceCount; ++i)
    {
        sphereIndices.push_back(0); // 北极点
        sphereIndices.push_back(i + 1);
        sphereIndices.push_back(i);
    }

    // 2. 中间的"四边形"
    // baseIndex 指向当前"环"的起始顶点
    // (第一个环是从索引 1 开始的)
    UINT baseIndex = 1;
    UINT ringVertexCount = sliceCount + 1;
    for (UINT i = 0; i < stackCount - 2; ++i)
    {
        for (UINT j = 0; j < sliceCount; ++j)
        {
            sphereIndices.push_back(baseIndex + i * ringVertexCount + j);
            sphereIndices.push_back(baseIndex + i * ringVertexCount + j + 1);
            sphereIndices.push_back(baseIndex + (i + 1) * ringVertexCount + j);

            sphereIndices.push_back(baseIndex + (i + 1) * ringVertexCount + j);
            sphereIndices.push_back(baseIndex + i * ringVertexCount + j + 1);
            sphereIndices.push_back(baseIndex + (i + 1) * ringVertexCount + j + 1);
        }
    }

    // 3. 南极的"扇形"
    // southPoleIndex 是最后一个顶点 (南极点)
    UINT southPoleIndex = (UINT)sphereVertexs.size() - 1;
    // (stackCount-1) * ringVertexCount + 1
    baseIndex = southPoleIndex - ringVertexCount;

    for (UINT i = 0; i < sliceCount; ++i)
    {
        sphereIndices.push_back(southPoleIndex);
        sphereIndices.push_back(baseIndex + i);
        sphereIndices.push_back(baseIndex + i + 1);
    }

    // 设置大小
    sphereVertexSize = (UINT)sphereVertexs.size() * sizeof(Vertex);
    sphereIndiceSize = (UINT)sphereIndices.size() * sizeof(uint16_t);
}
