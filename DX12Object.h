#pragma once
#include "PCH.h"
struct ResourceBarrierInfo {
	ComPtr<ID3D12Resource> resource;
	D3D12_RESOURCE_STATES srcState;
	D3D12_RESOURCE_STATES destState;
};

class DX12Object {
public:
	ComPtr<ID3D12Device> m_Device;
	ComPtr<ID3D12GraphicsCommandList> m_CommandList;

	void Init(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList> commandList);
	DX12Object& RestCommandList(ComPtr<ID3D12CommandAllocator> commandAllocator);

	DX12Object& SetRootSignature(ComPtr<ID3D12RootSignature> rs);
	DX12Object& SetPSO(ComPtr<ID3D12PipelineState> pso);

	DX12Object& SetViewAndSSetViewports(const D3D12_VIEWPORT& vp, const D3D12_RECT& sr);

	DX12Object& SetIA(D3D_PRIMITIVE_TOPOLOGY primitive, D3D12_VERTEX_BUFFER_VIEW& vertexBuffer, D3D12_INDEX_BUFFER_VIEW& indexBuffer);

	DX12Object& SetRootSignatureDescriptorTable(std::vector<ID3D12DescriptorHeap*> ppHeaps, std::vector<int> rootParameterslot);


	DX12Object& SetResourceBarrier(ComPtr<ID3D12Resource> targetResource, D3D12_RESOURCE_STATES srcState, D3D12_RESOURCE_STATES destState);

	DX12Object& SetResourceBarrierList(std::vector<ResourceBarrierInfo> resList);

};