#include "PCH.h"
#include "PSOContainer.h"

void PSOContainer::BuildPSO(RenderQueue renderTag, D3D12_GRAPHICS_PIPELINE_STATE_DESC* pipDes)
{
	ComPtr<ID3D12PipelineState> pso;
	_device->CreateGraphicsPipelineState(pipDes, __uuidof(ID3D12PipelineState), (void**)&pso);

	_allPipelineStateDic[renderTag] = pso;
}
