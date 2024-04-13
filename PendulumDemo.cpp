// 
// Demonstration of simple pendulum, whose newtonian equation of motion being updated with Euler method.
// code written by H.H. Yoo
//

#include "PendulumDemo.h"
#include "framework.h"
#include "Resource.h"

#include "./Helpers/d3dApp.h"
#include "./Helpers/MathHelper.h"
#include "./Helpers/UploadBuffer.h"
#include "./Helpers/GeometryGenerator.h"
#include "FrameBuffer.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace std;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

const int gNumFrameBuffers = 3;			// the size of the circular array to store resources per frame

const float gravConst = 9.8;			// gravitational acceleration constant (g = 9.8m/s^2 for earh)

class RenderItem
{
public:
	RenderItem() = default;

public:
	XMFLOAT4X4 World = MathHelper::Identity4x4();
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	int numFrameBufferFill = gNumFrameBuffers;		// for static objects

	UINT ObjCBIndex = -1;							// object Constant Buffer Index
	Material* Mat = nullptr;						// Material characteristics assigned to this render item.
	MeshGeometry* Geo = nullptr;					// Geometry data of this render item.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// parameters of ID3D12GraphicsCommandList::DrawIndexedInstanced method
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;
};

enum class RenderLayer : int
{
	Opaque = 0,
	Mirrors,
	Reflected,
	Transparent,
	Shadow,
	Count	// number of elements of this enumeration type.
};

struct SimplePendulum
{
	float initX;				// initial x-coordinates of a pendulum wire's center
	float initY;				// initial y-coordinates of a pendulum wire's center
	float initZ;				// initial z-coordinates of a pendulum wire's center

	float omega;				// pendulum's angular speed
	float theta;				// pendulum's angular position
	float wLength;				// pendulum's length(wire length)
};

class PendulumMotion : public D3DApp
{
public:
	PendulumMotion(HINSTANCE hInstance);
	PendulumMotion(const PendulumMotion& rhs) = delete;
	PendulumMotion& operator=(const PendulumMotion& rhs) = delete;
	~PendulumMotion();

	virtual bool Initialize() override;

private:
	virtual void OnResize() override;
	virtual void Update(const GameTimer& gt) override;
	virtual void Draw(const GameTimer& gt) override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

	void UpdateCamera(const GameTimer& gt);						// update the camera position complying mouse input.
	void UpdateObjectCBs(const GameTimer& gt);					// update object constant buffer.
	void UpdateMaterialCBs(const GameTimer& gt);				// update material constant buffer.
	void UpdateCommonCB(const GameTimer& gt);					// update constant buffer storing common parameters.
	void UpdateReflectedCommonCB(const GameTimer& gt);			// update reflected common constant buffer.
	//void UpdateReflectedAndShadowed(const GameTimer& gt);		// update object's copycat: reflected object showing up in the mirror and shadow image appearing on the floor.
	void UpdateReflectedAndShadowed();

	void EulerUpdate(float dt);									// update the pendulum's newtonian equation of motion.				

	// ----- preparatory methods -----
	void PrepareTextures();										// prepare various textures used in drawing a scene.
	void SetRootSignature();									// set root signature to notify the shader what resources are going to be used.
	void SetDescriptorHeaps();									// set shader resource descriptor heap (for textures)
	void SetShadersAndInputLayout();							// compile shader hlsl file and set up inputLayout(vertex, index structure).
	void SetBackgroundGeometry();								// set up the geometry of background(floor, wall, and mirror).
	void SetPendulumGeometry();									// set up the pendulum geometry composed of a ceiling, a wire, and a ball attached at the end of the wire)
	void SetPSOs();												// set up the pipeline state objects for drawing opaque objects, transparent objects, reflected objects, etc.
	void SetFrameBuffers();										// set frame buffers which carry several rendering resources.
	void SetMaterials();										// set material properties each to-be-rendered object carries.
	void SetRenderingItems();									// set up rendering items to be supplied to ID3D12GraphicsCommandList::DrawIndexedInstanced method.
	void WriteCaption();										// write captions regarding pendulum info.
	void DrawRenderingItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);		// it really draw a object.

	array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();				// get static samplers used in sampling texture data

	// --- member variables ---
private:
	vector<unique_ptr<FrameBuffer>> mFrameBuffers;	// for accesing frame buffer in a circular array format.
	FrameBuffer* mCurrentFrameBuffer = nullptr;
	int mCurrentFrameBufferIndex = 0;

	UINT mCbvSrvDescriptorSize = 0;					// for caching CB view, SR view descriptor size

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	unordered_map<string, unique_ptr<MeshGeometry>> mGeometries;		// categorize mesh geometries by name.
	unordered_map<string, unique_ptr<Material>> mMaterials;				// material characteristics categorized by name
	unordered_map<string, unique_ptr<Texture>> mTextures;				// textues categorized by name
	unordered_map<string, ComPtr<ID3DBlob>> mShaders;					// to store compiled shader in ComPtr with the type ID3DBlob
	unordered_map<string, ComPtr<ID3D12PipelineState>> mPSOs;			// to store pipeline state object in ComPtr(ID3D12PipelineState)

	vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;						// vertex, index buffer format supplied to the Input Assembler.

	// cache rendering items of a pendulum-related objects
	RenderItem* mCeilingRenderItem[3];			// index 0: object itself, index 1: its reflected object, index 2: its shadow object
	RenderItem* mWireRenderItem[3];				// index 0: object itself, index 1: its reflected object, index 2: its shadow object
	RenderItem* mBallRenderItem[3];				// index 0: object itself, index 1: its reflected object, index 2: its shadow object

	// List of all the rendering items
	vector<unique_ptr<RenderItem>> mAllRitems;

	// Rendering items categorized by PSO
	vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	CommonConstants mCommonCB;
	CommonConstants mReflectedCommonCB;

	SimplePendulum mSimplePend;					// pendulum object

	XMFLOAT3 mCameraPos = { 0.0f, 0.0f, 0.0f };					// camera position 
	XMFLOAT4X4 mView = MathHelper::Identity4x4();				// view matrix, inialized as identity matrix.
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();				// projection matrix, initialized as identity matrix.

	// camera position parameters in spherical coordinates(r, theta, phi).
	float mTheta = 1.5f * XM_PI;
	float mPhi = 0.4f * XM_PI;	
	float mRadius = 20.0f;

	POINT mLastMousePos;			// for tracking mouse pointer on the screen
};

// windows app's main function
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
	// enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		PendulumMotion thisApp(hInstance);
		if (!thisApp.Initialize())
		{
			return 0;
		}
		return thisApp.Run();
	}
	catch (DxException& err)
	{
		MessageBox(nullptr, err.ToString().c_str(), L"HR Failed...", MB_OK);
		return 0;
	}
}

PendulumMotion::PendulumMotion(HINSTANCE hInstance) : D3DApp(hInstance)
{
	mSimplePend.initX = 0.0f;
	mSimplePend.initY = 5.0f;
	mSimplePend.initZ = -5.0f;
	mSimplePend.omega = 0.0f;
	//mSimplePend.theta = MathHelper::Pi / 3.0f;	
	mSimplePend.theta = 0.0f;
	mSimplePend.wLength = 3.0f;
}

PendulumMotion::~PendulumMotion()
{
	if (md3dDevice != nullptr)
	{
		FlushCommandQueue();
	}
}

void PendulumMotion::EulerUpdate(float dt)
{
	if (D3DApp::mStatusChange == true)
	{
		mSimplePend.theta = D3DApp::inheritValue1 * MathHelper::Pi / 180.0f;
		mSimplePend.omega = 0.0f;
		D3DApp::mStatusChange = false;
	}

	// sample the newtonian differential equation into difference equation and update it every given delta time. (Euler method)
	this->mSimplePend.omega = this->mSimplePend.omega -
		(dt / 1.0f) * (gravConst / this->mSimplePend.wLength) * sinf(this->mSimplePend.theta);		// update angular speed.

	this->mSimplePend.theta = this->mSimplePend.theta +
		(dt / 1.0f) * this->mSimplePend.omega;														// update angle.
}

void PendulumMotion::WriteCaption()
{
	wostringstream outStr;
	outStr.precision(5);
	outStr << L"Pendulum demo: pendulum angle : " << mSimplePend.theta << L" in radians.";

	D3DApp::mMainWndCaption = outStr.str();
}

bool PendulumMotion::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	ThrowIfFailed(mCommandList->Reset(D3DApp::mDirectCmdListAlloc.Get(), nullptr));
	
	// query descriptor block size
	mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// preparatory actions: prepare render items, root signature and set pipeline state object
	PrepareTextures();
	SetRootSignature();
	SetDescriptorHeaps();
	SetShadersAndInputLayout();
	SetBackgroundGeometry();
	SetPendulumGeometry();
	SetMaterials();
	SetRenderingItems();
	SetFrameBuffers();
	SetPSOs();

	// execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// wait until the initialization is done.
	FlushCommandQueue();

	return true;		// initialization is complete.
}

void PendulumMotion::OnResize()
{
	D3DApp::OnResize();;

	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);
}

void PendulumMotion::Update(const GameTimer& gt)
{
	UpdateCamera(gt);
	WriteCaption();

	// cycle through the frame buffer in a circular array format.
	mCurrentFrameBufferIndex = (mCurrentFrameBufferIndex + 1) % gNumFrameBuffers;
	mCurrentFrameBuffer = mFrameBuffers[mCurrentFrameBufferIndex].get();

	if (mCurrentFrameBuffer->Fence != 0 && mFence->GetCompletedValue() < mCurrentFrameBuffer->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFrameBuffer->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateCommonCB(gt);
	UpdateReflectedCommonCB(gt);
}

void PendulumMotion::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrentFrameBuffer->CmdListAlloc;

	ThrowIfFailed(cmdListAlloc->Reset());

	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// clear the back buffer and depth buffer.
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr); // stencil buffer initial value : 0

	// Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	
	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	UINT commonCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(CommonConstants));

	// draw opaque items, floor, wall, and pendulum
	auto commonCB = mCurrentFrameBuffer->CommonCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(2, commonCB->GetGPUVirtualAddress());
	DrawRenderingItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

	// mark the visible mirror pixels on the stencil buffer with the value 1.
	mCommandList->OMSetStencilRef(1);
	mCommandList->SetPipelineState(mPSOs["markStencilMirror"].Get());
	DrawRenderingItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Mirrors]);

	// draw reflected image on the mirror (only for pixels where the stencil buffer is 1).
	// to draw reflected image, light reflected common constant buffer is required.
	mCommandList->SetGraphicsRootConstantBufferView(2, commonCB->GetGPUVirtualAddress() + commonCBByteSize);
	mCommandList->SetPipelineState(mPSOs["drawStencilReflections"].Get());
	DrawRenderingItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Reflected]);

	// restore the original common constants and stencil ref.
	mCommandList->SetGraphicsRootConstantBufferView(2, commonCB->GetGPUVirtualAddress());
	mCommandList->OMSetStencilRef(0);

	// draw mirror with transparent PSO so that object in the mirror can be seen through.
	mCommandList->SetPipelineState(mPSOs["transparent"].Get());
	DrawRenderingItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Transparent]);

	// draw shadow of the pendulum.
	mCommandList->SetPipelineState(mPSOs["shadow"].Get());
	DrawRenderingItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Shadow]);

	// indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// put the command list to the queue for excution.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// swap the back and front buffers.
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// advance the fence value to mark commands up to this fence point.
	mCurrentFrameBuffer->Fence = ++mCurrentFence;

	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void PendulumMotion::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void PendulumMotion::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void PendulumMotion::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		mTheta += dx;
		mPhi += dy;


		// restrict the view angle
		// restrict the angle mPhi.
		mPhi = MathHelper::Clamp(mPhi, MathHelper::Pi / 6.0f, MathHelper::Pi / 2.0f - 0.1f);
		//mTheta = MathHelper::Clamp(mTheta, -MathHelper::Pi * 5.0f / 6.0f, -MathHelper::Pi / 6.0f);
		mTheta = MathHelper::Clamp(mTheta, MathHelper::Pi * 7.0f / 6.0f, MathHelper::Pi * 11.0f / 6.0f);
	}
	else if ((btnState & MK_RBUTTON) != 0)
	{
		float dx = 0.2f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.2f * static_cast<float>(y - mLastMousePos.y);

		mRadius += dx - dy;

		// Restrict the radius.
		mRadius = MathHelper::Clamp(mRadius, 15.0f, 50.0f);
	}
	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void PendulumMotion::UpdateCamera(const GameTimer& gt)
{
	mCameraPos.x = mRadius * sinf(mPhi) * cosf(mTheta);
	mCameraPos.z = mRadius * sinf(mPhi) * sinf(mTheta);
	mCameraPos.y = mRadius * cosf(mPhi);

	// set the view matrix.
	XMVECTOR position = XMVectorSet(mCameraPos.x, mCameraPos.y, mCameraPos.z, 1.0f);
	//XMVECTOR target = XMVectorZero();
	XMVECTOR target = XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f);
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);		// indication of the direction up or down 

	XMMATRIX view = XMMatrixLookAtLH(position, target, up);
	XMStoreFloat4x4(&mView, view);
}

void PendulumMotion::UpdateObjectCBs(const GameTimer& gt)
{
	EulerUpdate(gt.DeltaTime());				// advance the equation of motion by the amount of delta(t) : gt.DeltaTime()
	UpdateReflectedAndShadowed();				// update the reflected and shadowed objects accordingly.
	
	auto currentObjectCB = mCurrentFrameBuffer->ObjectCB.get();
	for (auto& elem : mAllRitems)
	{
		if (elem->numFrameBufferFill > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&elem->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&elem->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			currentObjectCB->CopyData(elem->ObjCBIndex, objConstants);
		}
	}
}

void PendulumMotion::UpdateReflectedAndShadowed()	// update world matrices of a pendulum, its reflected object, and its shadow object
{
	// update the pendulum wire's world matrix.
	// wire itself
	XMMATRIX wireWorld = XMMatrixRotationZ(mSimplePend.theta) *
		XMMatrixTranslation(0.0f + (mSimplePend.wLength / 2.0f) * sinf(mSimplePend.theta),
			6.0f - (mSimplePend.wLength / 2.0f) * cosf(mSimplePend.theta), -5.0f);
	XMStoreFloat4x4(&mWireRenderItem[0]->World, wireWorld);

	// wire's reflection
	XMVECTOR mirrorPlane = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);			// xy-plane, normal vector to the mirror
	XMMATRIX R = XMMatrixReflect(mirrorPlane);
	XMStoreFloat4x4(&mWireRenderItem[1]->World, wireWorld * R);

	// wire's shadow
	XMVECTOR shadowPlane = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);			// xz-plane, normal vector to the floor
	XMVECTOR toMainLight = -XMLoadFloat3(&mCommonCB.Lights[0].Direction);
	XMMATRIX S = XMMatrixShadow(shadowPlane, toMainLight);
	XMMATRIX shadowOffSetY = XMMatrixTranslation(0.0f, 0.001f, 0.0f);
	XMStoreFloat4x4(&mWireRenderItem[2]->World, wireWorld * S * shadowOffSetY);

	// update the pendulum ball's world matrix
	// ball itself
	XMMATRIX ballWorld = XMMatrixRotationZ(mSimplePend.theta) *
		XMMatrixTranslation(0.0f + (mSimplePend.wLength + 0.1f) * sinf(mSimplePend.theta),			// ball's radius : 0.1f
			6.0f - (mSimplePend.wLength + 0.1f) * cosf(mSimplePend.theta), -5.0f);

	XMStoreFloat4x4(&mBallRenderItem[0]->World, ballWorld);
	
	// ball's reflection
	XMStoreFloat4x4(&mBallRenderItem[1]->World, ballWorld * R);

	// ball's shadow
	XMStoreFloat4x4(&mBallRenderItem[2]->World, ballWorld * S * shadowOffSetY);

	for (size_t i = 0; i < 3; ++i)
	{
		mWireRenderItem[i]->numFrameBufferFill = gNumFrameBuffers;
		mBallRenderItem[i]->numFrameBufferFill = gNumFrameBuffers;
	}
}


void PendulumMotion::UpdateMaterialCBs(const GameTimer& gt)
{
	auto currentMaterialCB = mCurrentFrameBuffer->MaterialCB.get();

	for (auto& elem : mMaterials)
	{
		Material* mat = elem.second.get();
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

			currentMaterialCB->CopyData(mat->MatCBIndex, matConstants);

			mat->NumFramesDirty--;
		}
	}
}

void PendulumMotion::UpdateCommonCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mCommonCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mCommonCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mCommonCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mCommonCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mCommonCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mCommonCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mCommonCB.CameraPosW = mCameraPos;
	mCommonCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mCommonCB.InvRenderTargetSize = XMFLOAT2(1.0f / (float)mClientWidth, 1.0f / (float)mClientHeight);
	mCommonCB.NearZ = 1.0f;
	mCommonCB.FarZ = 1000.0f;
	mCommonCB.TotalTime = gt.TotalTime();
	mCommonCB.DeltaTime = gt.DeltaTime();
	mCommonCB.AmbientLight = { 0.25f, 0.25f, 0.25f, 1.0f };
	mCommonCB.Lights[0].Direction = { 0.57735f, -0.70735f, 0.57735f };
	mCommonCB.Lights[0].Strength = { 0.8f, 0.8f, 0.8f };
	mCommonCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mCommonCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	mCommonCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mCommonCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

	auto currentCommonCB = mCurrentFrameBuffer->CommonCB.get();
	currentCommonCB->CopyData(0, mCommonCB);
}

void PendulumMotion::UpdateReflectedCommonCB(const GameTimer& gt)
{
	mReflectedCommonCB = mCommonCB;

	XMVECTOR mirrorPlane = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);			// xy-plane: a plane over which mirror is placed.
	XMMATRIX R = XMMatrixReflect(mirrorPlane);

	// reflect the light
	for (int i = 0; i < 3; ++i)
	{
		XMVECTOR lightDir = XMLoadFloat3(&mCommonCB.Lights[i].Direction);
		XMVECTOR reflectedLightDir = XMVector3TransformNormal(lightDir, R);
		XMStoreFloat3(&mReflectedCommonCB.Lights[i].Direction, reflectedLightDir);
	}

	// reflected common const. buffer stored right next to common const.
	auto currentCommonCB = mCurrentFrameBuffer->CommonCB.get();
	currentCommonCB->CopyData(1, mReflectedCommonCB);
}


void PendulumMotion::DrawRenderingItems(ID3D12GraphicsCommandList* cmdList, const vector<RenderItem*>& ritems)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = mCurrentFrameBuffer->ObjectCB->Resource();
	auto matCB = mCurrentFrameBuffer->MaterialCB->Resource();

	// draw each render items in the container storing pointer type of RenderItem.
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		RenderItem* ri = ritems[i];

		// feed IA(Input Assembler) with relevant data...
		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		tex.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * matCBByteSize;

		cmdList->SetGraphicsRootDescriptorTable(0, tex);
		cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
		cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

// ---------- preparatory methods ----------

void PendulumMotion::PrepareTextures()
{
	auto bricksTex = make_unique<Texture>();
	bricksTex->Name = "bricksTex";
	bricksTex->Filename = L"Textures/bricks3.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(), mCommandList.Get(),
		bricksTex->Filename.c_str(), bricksTex->Resource, bricksTex->UploadHeap));

	auto floorTex = make_unique<Texture>();
	floorTex->Name = "floorTex";
	floorTex->Filename = L"Textures/grass.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(), mCommandList.Get(),
		floorTex->Filename.c_str(), floorTex->Resource, floorTex->UploadHeap));

	auto mirrorTex = make_unique<Texture>();
	mirrorTex->Name = "mirrorTex";
	mirrorTex->Filename = L"Textures/ice.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(), mCommandList.Get(),
		mirrorTex->Filename.c_str(), mirrorTex->Resource, mirrorTex->UploadHeap));

	auto white1x1Tex = make_unique<Texture>();
	white1x1Tex->Name = "white1x1Tex";
	white1x1Tex->Filename = L"Textures/white1x1.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(), mCommandList.Get(),
		white1x1Tex->Filename.c_str(), white1x1Tex->Resource, white1x1Tex->UploadHeap));

	mTextures[bricksTex->Name] = move(bricksTex);
	mTextures[floorTex->Name] = move(floorTex);
	mTextures[mirrorTex->Name] = move(mirrorTex);
	mTextures[white1x1Tex->Name] = move(white1x1Tex);
}

void PendulumMotion::SetRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);	// textures, Texture2D gDiffuseMap : register(t0) in BasicShader.hlsl
	slotRootParameter[1].InitAsConstantBufferView(0);											// cbuffer cbObject : register(b0) in BasicShader.hlsl
	slotRootParameter[2].InitAsConstantBufferView(1);											// cbuffer cbCommon : register(b1) in BasicShader.hlsl
	slotRootParameter[3].InitAsConstantBufferView(2);											// cbuffer cbMaterial : register(b2) in BasicShader.hlsl

	// get a static sampler object.
	auto staticSamplers = GetStaticSamplers();

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter, (UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((const char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void PendulumMotion::SetDescriptorHeaps()
{
	// Create the shader resource view.
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 4;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	// fill up the heap with texture resource descriptors.
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	auto bricksTex = mTextures["bricksTex"]->Resource;
	auto floorTex = mTextures["floorTex"]->Resource;
	auto mirrorTex = mTextures["mirrorTex"]->Resource;
	auto white1x1Tex = mTextures["white1x1Tex"]->Resource;

	// 1st descriptor
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = bricksTex->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = -1;
	md3dDevice->CreateShaderResourceView(bricksTex.Get(), &srvDesc, hDescriptor);

	// 2nd descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);			// add offset to the resource view handle
	srvDesc.Format = floorTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(floorTex.Get(), &srvDesc, hDescriptor);

	// 3rd descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	srvDesc.Format = mirrorTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(mirrorTex.Get(), &srvDesc, hDescriptor);

	// 4th descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	srvDesc.Format = white1x1Tex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(white1x1Tex.Get(), &srvDesc, hDescriptor);
}

void PendulumMotion::SetShadersAndInputLayout()
{
	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\BasicShader.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\BasicShader.hlsl", nullptr, "PS", "ps_5_0");

	mInputLayout =				
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
}

void PendulumMotion::SetBackgroundGeometry()
{
	// create vertices, indices, normal vectors, and texture coordinates of a geometrical object: floor, wall and mirror attached to it.
	
	array<Vertex, 20> vertices =
	{
		// Floor: Observe we tile texture coordinates.
		Vertex(-3.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 0.0f, 4.0f), // 0 
		Vertex(-3.5f, 0.0f,   0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f),
		Vertex(7.5f, 0.0f,   0.0f, 0.0f, 1.0f, 0.0f, 4.0f, 0.0f),
		Vertex(7.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 4.0f, 4.0f),

		// Wall: Observe we tile texture coordinates, and that we
		// leave a gap in the middle for the mirror.
		Vertex(-3.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f), // 4
		Vertex(-3.5f, 5.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex(-2.5f, 5.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 0.0f),
		Vertex(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 2.0f),

		Vertex(2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f), // 8 
		Vertex(2.5f, 5.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex(7.5f, 5.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 0.0f),
		Vertex(7.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 2.0f),

		Vertex(-3.5f, 5.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f), // 12
		Vertex(-3.5f, 5.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex(7.5f, 5.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 0.0f),
		Vertex(7.5f, 5.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 1.0f),

		// Mirror
		Vertex(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f), // 16
		Vertex(-2.5f, 5.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex(2.5f, 5.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f),
		Vertex(2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f)
	};

	array<std::int16_t, 30> indices =
	{
		// Floor
		0, 1, 2,
		0, 2, 3,

		// Walls
		4, 5, 6,
		4, 6, 7,

		8, 9, 10,
		8, 10, 11,

		12, 13, 14,
		12, 14, 15,

		// Mirror
		16, 17, 18,
		16, 18, 19
	};

	SubmeshGeometry floorSubmesh;
	floorSubmesh.IndexCount = 6;
	floorSubmesh.StartIndexLocation = 0;
	floorSubmesh.BaseVertexLocation = 0;

	SubmeshGeometry wallSubmesh;
	wallSubmesh.IndexCount = 18;
	wallSubmesh.StartIndexLocation = 6;
	wallSubmesh.BaseVertexLocation = 0;

	SubmeshGeometry mirrorSubmesh;
	mirrorSubmesh.IndexCount = 6;
	mirrorSubmesh.StartIndexLocation = 24;
	mirrorSubmesh.BaseVertexLocation = 0;

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(uint16_t);

	auto geo = make_unique<MeshGeometry>();
	geo->Name = "backgroundGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
		vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
		indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["floor"] = floorSubmesh;
	geo->DrawArgs["wall"] = wallSubmesh;
	geo->DrawArgs["mirror"] = mirrorSubmesh;

	mGeometries[geo->Name] = move(geo);
}

void PendulumMotion::SetPendulumGeometry()			// opaque pendulum and ceiling
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData ceiling = geoGen.CreateBox(2.0f, 0.2f, 2.0f, 3);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.05f, 0.05f, 3.0f, 10, 10);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.2f, 10, 10);

	// Concatenating all individual geometries into one vertex/index buffer.
	// so, define the regions in the buffer each submesh covers.

	// cache the vertex offsets to each object in the concatenated vertex buffer.
	UINT ceilingVertexStart = 0;
	UINT cylinderVertexStart = (UINT)ceiling.Vertices.size();
	UINT sphereVertexStart = cylinderVertexStart + (UINT)cylinder.Vertices.size();

	// cache the starting index for each object in the concatenated index buffer.
	UINT ceilingIndexStart = 0;
	UINT cylinderIndexStart = (UINT)ceiling.Indices32.size();
	UINT sphereIndexStart = cylinderIndexStart + (UINT)cylinder.Indices32.size();

	SubmeshGeometry ceilingSubmesh;
	ceilingSubmesh.IndexCount = (UINT)ceiling.Indices32.size();
	ceilingSubmesh.StartIndexLocation = ceilingIndexStart;
	ceilingSubmesh.BaseVertexLocation = ceilingVertexStart;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.StartIndexLocation = cylinderIndexStart;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexStart;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexStart;
	sphereSubmesh.BaseVertexLocation = sphereVertexStart;

	auto totalVertexCount = ceiling.Vertices.size() + cylinder.Vertices.size() + sphere.Vertices.size();

	vector<Vertex> vertices(totalVertexCount);

	size_t k = 0;
	for (size_t i = 0; i < ceiling.Vertices.size(); ++i, ++k)
	{
		vertices[k].Position = ceiling.Vertices[i].Position;
		vertices[k].Normal = ceiling.Vertices[i].Normal;
		vertices[k].TexC = ceiling.Vertices[i].TexC;
	}

	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].Position = cylinder.Vertices[i].Position;
		vertices[k].Normal = cylinder.Vertices[i].Normal;
		vertices[k].TexC = cylinder.Vertices[i].TexC;
	}

	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Position = sphere.Vertices[i].Position;
		vertices[k].Normal = sphere.Vertices[i].Normal;
		vertices[k].TexC = sphere.Vertices[i].TexC;
	}

	vector<uint16_t> indices;
	indices.insert(indices.end(), ceiling.GetIndices16().begin(), ceiling.GetIndices16().end());
	indices.insert(indices.end(), cylinder.GetIndices16().begin(), cylinder.GetIndices16().end());
	indices.insert(indices.end(), sphere.GetIndices16().begin(), sphere.GetIndices16().end());

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(uint16_t);

	auto geo = make_unique<MeshGeometry>();
	geo->Name = "pendulumGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), vertices.data(),
		vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), indices.data(),
		ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["ceiling"] = ceilingSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;

	mGeometries[geo->Name] = move(geo);
}

void PendulumMotion::SetPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	// pipeline state object for opaque objects
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

	// pipeline state object for transparent objects
	D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = opaquePsoDesc;

	D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
	transparencyBlendDesc.BlendEnable = true;
	transparencyBlendDesc.LogicOpEnable = false;
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	transparentPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&mPSOs["transparent"])));

	// pipeline state object for marking mirror template on the stencil buffer.
	CD3DX12_BLEND_DESC mirrorBlendState(D3D12_DEFAULT);
	mirrorBlendState.RenderTarget[0].RenderTargetWriteMask = 0;

	D3D12_DEPTH_STENCIL_DESC mirrorDSS;
	mirrorDSS.DepthEnable = true;
	mirrorDSS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;			// no depth info. in mirror stencil
	mirrorDSS.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	mirrorDSS.StencilEnable = true;
	mirrorDSS.StencilReadMask = 0xff;
	mirrorDSS.StencilWriteMask = 0xff;

	mirrorDSS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	mirrorDSS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	mirrorDSS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;	// if stencil test passes stencil entry is always replaced with StencilRef(=1)
	mirrorDSS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;		

	// backfacing polygonal structure is out of concern. so, just fill up with normal settings.
	mirrorDSS.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	mirrorDSS.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	mirrorDSS.BackFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
	mirrorDSS.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC mirrorMarkingPsoDesc = opaquePsoDesc;			// mark a mirror on a stencil buffer
	mirrorMarkingPsoDesc.BlendState = mirrorBlendState;
	mirrorMarkingPsoDesc.DepthStencilState = mirrorDSS;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&mirrorMarkingPsoDesc, IID_PPV_ARGS(&mPSOs["markStencilMirror"])));

	// pipeline state object for stencil reflections. (for drawing objects being appeared in the mirror)
	D3D12_DEPTH_STENCIL_DESC reflectionsDSS;
	reflectionsDSS.DepthEnable = true;
	reflectionsDSS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	reflectionsDSS.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	reflectionsDSS.StencilEnable = true;
	reflectionsDSS.StencilReadMask = 0xff;
	reflectionsDSS.StencilWriteMask = 0xff;

	reflectionsDSS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	reflectionsDSS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	reflectionsDSS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	reflectionsDSS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	// backfacing polygonal structure is out of concern. so, just fill up with normal settings.
	reflectionsDSS.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	reflectionsDSS.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	reflectionsDSS.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	reflectionsDSS.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC drawReflectionsPsoDesc = opaquePsoDesc;
	drawReflectionsPsoDesc.DepthStencilState = reflectionsDSS;
	drawReflectionsPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;		
	drawReflectionsPsoDesc.RasterizerState.FrontCounterClockwise = true;		// for mirror symmetry
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&drawReflectionsPsoDesc, IID_PPV_ARGS(&mPSOs["drawStencilReflections"])));

	// pipeline state object for shadow objects.
	D3D12_DEPTH_STENCIL_DESC shadowDSS;
	shadowDSS.DepthEnable = true;
	shadowDSS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	shadowDSS.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	shadowDSS.StencilEnable = true;
	shadowDSS.StencilReadMask = 0xff;
	shadowDSS.StencilWriteMask = 0xff;

	shadowDSS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDSS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDSS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
	shadowDSS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
	
	shadowDSS.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDSS.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	shadowDSS.BackFace.StencilPassOp = D3D12_STENCIL_OP_INCR;
	shadowDSS.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPsoDesc = transparentPsoDesc;	
	shadowPsoDesc.DepthStencilState = shadowDSS;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&shadowPsoDesc, IID_PPV_ARGS(&mPSOs["shadow"])));
}

void PendulumMotion::SetFrameBuffers()
{
	for (int i = 0; i < gNumFrameBuffers; ++i)
	{
		mFrameBuffers.push_back(make_unique<FrameBuffer>(md3dDevice.Get(),
			2, (UINT)mAllRitems.size(), (UINT)mMaterials.size()));
	}
}

void PendulumMotion::SetMaterials()
{
	auto bricks = make_unique<Material>();
	bricks->Name = "bricks";
	bricks->MatCBIndex = 0;
	bricks->DiffuseSrvHeapIndex = 0;
	bricks->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	bricks->Roughness = 0.25f;

	auto grassfloor = make_unique<Material>();
	grassfloor->Name = "grassfloor";
	grassfloor->MatCBIndex = 1;
	grassfloor->DiffuseSrvHeapIndex = 1;
	grassfloor->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	grassfloor->FresnelR0 = XMFLOAT3(0.07f, 0.07f, 0.07f);
	grassfloor->Roughness = 0.3f;

	auto glassmirror = make_unique<Material>();
	glassmirror->Name = "grassmirror";
	glassmirror->MatCBIndex = 2;
	glassmirror->DiffuseSrvHeapIndex = 2;
	glassmirror->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.3f);
	glassmirror->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	glassmirror->Roughness = 0.5f;

	auto whitesurface = make_unique<Material>();
	whitesurface->Name = "whitesurface";
	whitesurface->MatCBIndex = 3;
	whitesurface->DiffuseSrvHeapIndex = 3;
	whitesurface->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	whitesurface->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	whitesurface->Roughness = 0.3f;

	auto shadow = make_unique<Material>();
	shadow->Name = "shadow";
	shadow->MatCBIndex = 4;
	shadow->DiffuseSrvHeapIndex = 3;
	shadow->DiffuseAlbedo = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.5f);
	shadow->FresnelR0 = XMFLOAT3(0.001f, 0.001f, 0.001f);
	shadow->Roughness = 0.0f;

	mMaterials["bricks"] = move(bricks);
	mMaterials["grassfloor"] = move(grassfloor);
	mMaterials["glassmirror"] = move(glassmirror);
	mMaterials["whitesurface"] = move(whitesurface);
	mMaterials["shadow"] = move(shadow);
}

void PendulumMotion::SetRenderingItems()
{
	auto floorRitem = make_unique<RenderItem>();
	floorRitem->World = MathHelper::Identity4x4();
	floorRitem->TexTransform = MathHelper::Identity4x4();
	floorRitem->ObjCBIndex = 0;
	floorRitem->Mat = mMaterials["grassfloor"].get();
	floorRitem->Geo = mGeometries["backgroundGeo"].get();
	floorRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	floorRitem->IndexCount = floorRitem->Geo->DrawArgs["floor"].IndexCount;
	floorRitem->StartIndexLocation = floorRitem->Geo->DrawArgs["floor"].StartIndexLocation;
	floorRitem->BaseVertexLocation = floorRitem->Geo->DrawArgs["floor"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(floorRitem.get());

	auto wallRitem = make_unique<RenderItem>();
	wallRitem->World = MathHelper::Identity4x4();
	wallRitem->TexTransform = MathHelper::Identity4x4();
	wallRitem->ObjCBIndex = 1;
	wallRitem->Mat = mMaterials["bricks"].get();
	wallRitem->Geo = mGeometries["backgroundGeo"].get();
	wallRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wallRitem->IndexCount = wallRitem->Geo->DrawArgs["wall"].IndexCount;
	wallRitem->StartIndexLocation = wallRitem->Geo->DrawArgs["wall"].StartIndexLocation;
	wallRitem->BaseVertexLocation = wallRitem->Geo->DrawArgs["wall"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(wallRitem.get());

	auto mirrorRitem = make_unique<RenderItem>();
	mirrorRitem->World = MathHelper::Identity4x4();
	mirrorRitem->TexTransform = MathHelper::Identity4x4();
	mirrorRitem->ObjCBIndex = 2;
	mirrorRitem->Mat = mMaterials["glassmirror"].get();
	mirrorRitem->Geo = mGeometries["backgroundGeo"].get();
	mirrorRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	mirrorRitem->IndexCount = mirrorRitem->Geo->DrawArgs["mirror"].IndexCount;
	mirrorRitem->StartIndexLocation = mirrorRitem->Geo->DrawArgs["mirror"].StartIndexLocation;
	mirrorRitem->BaseVertexLocation = mirrorRitem->Geo->DrawArgs["mirror"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Mirrors].push_back(mirrorRitem.get());				// for rendering on the stencil buffer
	mRitemLayer[(int)RenderLayer::Transparent].push_back(mirrorRitem.get());			// for rendering on the back buffer(real blended object appeared on the scene)

	// pendulum rendering items

	// 1-1. set up a ceiling item below which a pendulum hangs.
	auto ceilingRitem = make_unique<RenderItem>();
	XMStoreFloat4x4(&ceilingRitem->World, XMMatrixTranslation(0.0f, 6.0f, -5.0f));
	ceilingRitem->TexTransform = MathHelper::Identity4x4();
	ceilingRitem->ObjCBIndex = 3;
	ceilingRitem->Mat = mMaterials["grassfloor"].get();
	ceilingRitem->Geo = mGeometries["pendulumGeo"].get();
	ceilingRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	ceilingRitem->IndexCount = ceilingRitem->Geo->DrawArgs["ceiling"].IndexCount;
	ceilingRitem->StartIndexLocation = ceilingRitem->Geo->DrawArgs["ceiling"].StartIndexLocation;
	ceilingRitem->BaseVertexLocation = ceilingRitem->Geo->DrawArgs["ceiling"].BaseVertexLocation;
	mCeilingRenderItem[0] = ceilingRitem.get();
	mRitemLayer[(int)RenderLayer::Opaque].push_back(ceilingRitem.get());

	// 1-2. reflected ceiling item		// it will need a different world matrix so keep it separately in the buffer.
	auto reflectedCeilingRitem = make_unique<RenderItem>();
	*reflectedCeilingRitem = *ceilingRitem;
	reflectedCeilingRitem->ObjCBIndex = 4;
	mCeilingRenderItem[1] = reflectedCeilingRitem.get();
	mRitemLayer[(int)RenderLayer::Reflected].push_back(reflectedCeilingRitem.get());

	// 1-3. shadow ceiling item
	auto shadowedCeilingRitem = make_unique<RenderItem>();
	*shadowedCeilingRitem = *ceilingRitem;
	shadowedCeilingRitem->ObjCBIndex = 5;
	shadowedCeilingRitem->Mat = mMaterials["shadow"].get();
	mCeilingRenderItem[2] = shadowedCeilingRitem.get();
	mRitemLayer[(int)RenderLayer::Shadow].push_back(shadowedCeilingRitem.get());

	// 2-1. set up  a pendulum part: a wire 
	auto wireRitem = make_unique<RenderItem>();
	wireRitem->World = MathHelper::Identity4x4();
	wireRitem->TexTransform = MathHelper::Identity4x4();
	wireRitem->ObjCBIndex = 6;
	wireRitem->Mat = mMaterials["whitesurface"].get();
	wireRitem->Geo = mGeometries["pendulumGeo"].get();
	wireRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wireRitem->IndexCount = wireRitem->Geo->DrawArgs["cylinder"].IndexCount;
	wireRitem->StartIndexLocation = wireRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
	wireRitem->BaseVertexLocation = wireRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;
	mWireRenderItem[0] = wireRitem.get();
	mRitemLayer[(int)RenderLayer::Opaque].push_back(wireRitem.get());

	// 2-2. reflected wire item		// it will need a different world matrix so keep it separately in the buffer.
	auto reflectedWireRitem = make_unique<RenderItem>();
	*reflectedWireRitem = *wireRitem;
	reflectedWireRitem->ObjCBIndex = 7;
	mWireRenderItem[1] = reflectedWireRitem.get();
	mRitemLayer[(int)RenderLayer::Reflected].push_back(reflectedWireRitem.get());

	// 2-3. shadow wire item
	auto shadowedWireRitem = make_unique<RenderItem>();
	*shadowedWireRitem = *wireRitem;
	shadowedWireRitem->ObjCBIndex = 8;
	shadowedWireRitem->Mat = mMaterials["shadow"].get();
	mWireRenderItem[2] = shadowedWireRitem.get();
	mRitemLayer[(int)RenderLayer::Shadow].push_back(shadowedWireRitem.get());

	// 3-1. set up a pendulum part: a ball attached at the end of the wire
	auto ballRitem = make_unique<RenderItem>();
	ballRitem->World = MathHelper::Identity4x4();
	ballRitem->TexTransform = MathHelper::Identity4x4();
	ballRitem->ObjCBIndex = 9;
	ballRitem->Mat = mMaterials["whitesurface"].get();
	ballRitem->Geo = mGeometries["pendulumGeo"].get();
	ballRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	ballRitem->IndexCount = ballRitem->Geo->DrawArgs["sphere"].IndexCount;
	ballRitem->StartIndexLocation = ballRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	ballRitem->BaseVertexLocation = ballRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
	mBallRenderItem[0] = ballRitem.get();
	mRitemLayer[(int)RenderLayer::Opaque].push_back(ballRitem.get());

	// 3-2. reflected ball item
	auto reflectedBallRitem = make_unique<RenderItem>();
	*reflectedBallRitem = *ballRitem;
	reflectedBallRitem->ObjCBIndex = 10;
	mBallRenderItem[1] = reflectedBallRitem.get();
	mRitemLayer[(int)RenderLayer::Reflected].push_back(reflectedBallRitem.get());

	// 3-3. shadow ball item
	auto shadowedBallRitem = make_unique<RenderItem>();
	*shadowedBallRitem = *ballRitem;
	shadowedBallRitem->ObjCBIndex = 11;
	shadowedBallRitem->Mat = mMaterials["shadow"].get();
	mBallRenderItem[2] = shadowedBallRitem.get();
	mRitemLayer[(int)RenderLayer::Shadow].push_back(shadowedBallRitem.get());

	mAllRitems.push_back(move(floorRitem));
	mAllRitems.push_back(move(wallRitem));
	mAllRitems.push_back(move(mirrorRitem));

	mAllRitems.push_back(move(ceilingRitem));
	mAllRitems.push_back(move(reflectedCeilingRitem));
	mAllRitems.push_back(move(shadowedCeilingRitem));

	mAllRitems.push_back(move(wireRitem));
	mAllRitems.push_back(move(reflectedWireRitem));
	mAllRitems.push_back(move(shadowedWireRitem));

	mAllRitems.push_back(move(ballRitem));
	mAllRitems.push_back(move(reflectedBallRitem));
	mAllRitems.push_back(move(shadowedBallRitem));
}

array<const CD3DX12_STATIC_SAMPLER_DESC, 6> PendulumMotion::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}



















