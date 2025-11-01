#pragma once
#include "PCH.h"
class ResourceManager {
public:
	void Init(ComPtr<ID3D12Device> device, ComPtr<ID3D12GraphicsCommandList> commandList);
	ComPtr<ID3D12Resource> UploadResource(void* pSource, UINT dataSize, ComPtr<ID3D12Resource>& targetResource);

	void AddResource2TmpBuffer(ComPtr<ID3D12Resource> res) {
		tempUploadBuffers.push_back(res);
	}
	void ClearTmpBuffers() {
		tempUploadBuffers = {};
	}
private:
	ComPtr<ID3D12Device> m_Device;
	ComPtr<ID3D12GraphicsCommandList> m_CommandList;
	std::vector<ComPtr<ID3D12Resource>> tempUploadBuffers;
};