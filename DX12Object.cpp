#include "PCH.h"
#include "DX12Object.h"

void DX12Object::Init(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList> commandList)
{
    m_Device = device;
    m_CommandList = commandList;
}

DX12Object& DX12Object::RestCommandList(ComPtr<ID3D12CommandAllocator> commandAllocator)
{
    m_CommandList->Reset(commandAllocator.Get(), nullptr);
    return *this;
}

DX12Object& DX12Object::SetRootSignature(ComPtr<ID3D12RootSignature> rs)
{
    m_CommandList->SetGraphicsRootSignature(rs.Get());
    return *this;
}

DX12Object& DX12Object::SetPSO(ComPtr<ID3D12PipelineState> pso)
{
    m_CommandList->SetPipelineState(pso.Get());
    return *this;
}

DX12Object& DX12Object::SetViewAndSSetViewports(const D3D12_VIEWPORT& vp, const D3D12_RECT& sr)
{
    m_CommandList->RSSetViewports(1, &vp);
    m_CommandList->RSSetScissorRects(1, &sr);
    return *this;
}

DX12Object& DX12Object::SetIA(D3D_PRIMITIVE_TOPOLOGY primitive, D3D12_VERTEX_BUFFER_VIEW& vertexBuffer, D3D12_INDEX_BUFFER_VIEW& indexBuffer)
{
    m_CommandList->IASetPrimitiveTopology(primitive);
    //D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    //{
    //    { "POSITION", 0, ..., 0, 0,  ... }, 
    //    { "COLOR",    0, ..., 0, 12, ... },
    //    { "TEXCOORD", 0, ..., 0, 28, ... },
    //    { "NORMAL",   0, ..., 0, 36, ... }
    //};
    m_CommandList->IASetVertexBuffers(0, // 这个0和上面注释当中，第一行第二个零是一样的，指的是GPU IA物理的slot，这里的意思是，吧vertexBuffer里指向的数据，塞到0号slot，
        // 然后D3D12_INPUT_ELEMENT_DESC的意思是，从0号slot的第x个字节，取出的东西是某个语义名 (Semantic)
        1,
        &vertexBuffer);
    m_CommandList->IASetIndexBuffer(&indexBuffer);
    return *this;
}

//第二个参数rootParameterslot，对应是根签名参数列表当中的对应的索引
DX12Object& DX12Object::SetRootSignatureDescriptorTable(std::vector<ID3D12DescriptorHeap*> ppHeaps,std::vector<int> rootParameterslot)
{
    m_CommandList->SetDescriptorHeaps(ppHeaps.size(), ppHeaps.data());
    for (int i = 0; i < ppHeaps.size(); ++i) {
        m_CommandList->SetGraphicsRootDescriptorTable(rootParameterslot[i], ppHeaps[i]->GetGPUDescriptorHandleForHeapStart());
    }
    return *this;
    
}

DX12Object& DX12Object::SetResourceBarrier(ComPtr<ID3D12Resource> targetResource, D3D12_RESOURCE_STATES srcState, D3D12_RESOURCE_STATES destState)
{
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(targetResource.Get(), srcState, destState);
    m_CommandList->ResourceBarrier(1, &barrier);
    return *this;
}
