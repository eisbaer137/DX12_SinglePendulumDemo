#include "FrameBuffer.h"

FrameBuffer::FrameBuffer(ID3D12Device* device, UINT commonCount, UINT objectCount, UINT materialCount)
{
	ThrowIfFailed(device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));
	
	CommonCB = std::make_unique<UploadBuffer<CommonConstants>>(device, commonCount, true);
	MaterialCB = std::make_unique<UploadBuffer<MaterialConstants>>(device, materialCount, true);
	ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
}

FrameBuffer::~FrameBuffer()
{

}