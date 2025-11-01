#pragma once
#include "PCH.h"
class PSOContainer {
public:
	PSOContainer() {

	}

	void Init(ComPtr<ID3D12Device> device) {
		_device = device;
	}

	void BuildPSO(RenderQueue renderTag, D3D12_GRAPHICS_PIPELINE_STATE_DESC* pipDes);

	ID3D12PipelineState* GetPSO(RenderQueue renderTag) {
		if (_allPipelineStateDic.find(renderTag) != _allPipelineStateDic.end()) {
			return _allPipelineStateDic[renderTag].Get();
		}
		return nullptr;
	}
private:
	std::unordered_map< RenderQueue, ComPtr<ID3D12PipelineState> > _allPipelineStateDic;
	ComPtr<ID3D12Device> _device;
};