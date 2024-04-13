#pragma once

#include "./Helpers/d3dUtil.h"
#include "./Helpers/MathHelper.h"
#include "./Helpers/UploadBuffer.h"

// pair to the cbuffer cbObject in the shader source(BasicShader.hlsl)
struct ObjectConstants
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
};

struct CommonConstants
{
	DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvView = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
	DirectX::XMFLOAT3 CameraPosW = { 0.0f, 0.0f, 0.0f };
	float CommonPad0 = 0.0f;
	DirectX::XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
	DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
	float NearZ = 0.0f;
	float FarZ = 0.0f;
	float TotalTime = 0.0f;
	float DeltaTime = 0.0f;

	DirectX::XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };

	Light Lights[MaxLights];			// #define MaxLights 16 in d3dUtil.h
};

struct Vertex
{
	Vertex() = default;
	Vertex(float x, float y, float z, float nx, float ny, float nz, float u, float v)
		: Position(x, y, z), Normal(nx, ny, nz), TexC(u, v)
	{

	}
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexC;
};

struct FrameBuffer
{
public:
	FrameBuffer(ID3D12Device* device, UINT commonCount, UINT objectCount, UINT materialCount);
	FrameBuffer(const FrameBuffer& rhs) = delete;
	FrameBuffer& operator=(const FrameBuffer & rhs) = delete;
	~FrameBuffer();

public:
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	std::unique_ptr<UploadBuffer<CommonConstants>> CommonCB = nullptr;
	std::unique_ptr<UploadBuffer<MaterialConstants>> MaterialCB = nullptr;		// MaterialConstants is defined in d3dUtil.h
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;

	UINT64 Fence = 0;
};




