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