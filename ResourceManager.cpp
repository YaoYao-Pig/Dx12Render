#include "PCH.h"
#include "ResourceManager.h"
void ResourceManager::Init(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList> commandList)
{
	m_Device = device;
	m_CommandList = commandList;
}

ComPtr<ID3D12Resource> ResourceManager::UploadResource(void* pSource, UINT dataSize,ComPtr<ID3D12Resource>& targetResource)
{
	D3D12_HEAP_PROPERTIES uploadHeapPro = {};
	uploadHeapPro.Type = D3D12_HEAP_TYPE_UPLOAD;
	uploadHeapPro.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	uploadHeapPro.CreationNodeMask = 1;
	uploadHeapPro.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Alignment = 0;

	//设置一维缓冲区的大小
	resDesc.Height = 1; //y
	resDesc.Width = dataSize; //x
	resDesc.DepthOrArraySize = 1; //z

	resDesc.MipLevels = 1;
	resDesc.Format = DXGI_FORMAT_UNKNOWN;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;

	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; // 行主序
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ComPtr<ID3D12Resource> uploadBuffer;
	ThrowIfFailed(m_Device->CreateCommittedResource(
		&uploadHeapPro,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		__uuidof(ID3D12Resource),
		(void**)(&uploadBuffer)
	));

	D3D12_HEAP_PROPERTIES targetHeapPro = {};
	targetHeapPro.Type = D3D12_HEAP_TYPE_DEFAULT;
	targetHeapPro.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	targetHeapPro.CreationNodeMask = 1;
	targetHeapPro.VisibleNodeMask = 1;
	ThrowIfFailed(m_Device->CreateCommittedResource(
		&targetHeapPro,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		__uuidof(ID3D12Resource),
		(void**)&targetResource
	));

	void* pData = nullptr;
	uploadBuffer->Map(0, nullptr, &pData);
	memcpy(pData, pSource, dataSize);
	uploadBuffer->Unmap(0, nullptr);
	m_CommandList->CopyResource(targetResource.Get(), uploadBuffer.Get());
	tempUploadBuffers.push_back(uploadBuffer);
	return uploadBuffer;
}
