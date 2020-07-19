/**************************************************************
	Project:		D3D12 Lighting App
	Created:		2020-07-13
	Modified:		2020-07-17
	Author:			Liam Blake
	Lines of Code:	625
**************************************************************/
#include <Windows.h>		// For Windows32
#include <d3d12.h>			// For Direct3D 12
#include <dxgi1_4.h>		// For DirectX Graphics Infrastructure Objects
#include "d3dx12.h"			// Extensions of D3D12
#include <d3dcompiler.h>	// Compiling Shaders
#include <DirectXMath.h>	// For World Transforms and Lighting
#include <DirectXColors.h>	// For Light Colors
#include <ctime>			
#include <array>
#include <string>
#include "DDSTextureLoader.h"	// Loading Textures (see example 08)

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#define ThrowIfFailed(hr) if (!SUCCEEDED(hr)) { DebugBreak(); } 
#define BUFFERCOUNT 3
#define cos_radians(x) cos(DirectX::XMConvertToRadians(x))
#define sin_radians(y) sin(DirectX::XMConvertToRadians(y))

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_DESTROY:
		PostQuitMessage(EXIT_SUCCESS);
		break;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int mCmdShow)
{
	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = WndProc;
	wc.lpszClassName = "hw3d";
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);

	RegisterClass(&wc);

	HWND hwnd = CreateWindow("hw3d", "D3D12 App - Lights! Camera! Action!", WS_CAPTION | WS_SYSMENU | WS_SIZEBOX | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
		100, 100, 800, 600, nullptr, nullptr, hInstance, nullptr);

	ShowWindow(hwnd, mCmdShow);

	// Init D3D
	IDXGISwapChain1* m_dxgiSwapChain;
	IDXGIFactory2* m_dxgiFactory;
	ID3D12Device* m_device;
	ID3D12CommandQueue* m_commandQueue;
	ID3D12CommandAllocator* m_commandAllocator;
	ID3D12GraphicsCommandList* m_commandList;
	ID3D12Fence* m_fence;
	HANDLE m_fenceEvent;
	UINT64 m_iCurrentFence = 0;

	// Render target
	ID3D12DescriptorHeap* m_rtvDescHeap;
	UINT m_iCurrentFrameIndex = 2;

	// Triangle
	ID3D12PipelineState* m_pipelineState;
	ID3D12RootSignature* m_rootSignature;
	ID3DBlob* m_rootSignatureBlob;
	ID3D12Resource* m_vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
	ID3D12Resource* m_indexBuffer;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

	// Constant Buffer
	ID3D12Resource* m_cbvResource[2];

	ThrowIfFailed(CreateDXGIFactory(IID_PPV_ARGS(&m_dxgiFactory)));

	ThrowIfFailed(D3D12CreateDevice(0, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&m_device)));

	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	ThrowIfFailed(m_device->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&m_commandQueue)));

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = BUFFERCOUNT;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

	ThrowIfFailed(m_dxgiFactory->CreateSwapChainForHwnd(
		m_commandQueue,
		hwnd,
		&swapChainDesc,
		0,
		0,
		&m_dxgiSwapChain
	));

	ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));

	ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator, 0, IID_PPV_ARGS(&m_commandList)));
	m_commandList->Close();

	ThrowIfFailed(m_device->CreateFence(m_iCurrentFence, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
	m_fenceEvent = CreateEvent(0, 0, 0, 0);

	D3D12_DESCRIPTOR_HEAP_DESC rtvDescHeap = {};
	rtvDescHeap.NumDescriptors = BUFFERCOUNT;
	rtvDescHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

	ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvDescHeap, IID_PPV_ARGS(&m_rtvDescHeap)));

	CD3DX12_CPU_DESCRIPTOR_HANDLE m_rtvHeapHandle(m_rtvDescHeap->GetCPUDescriptorHandleForHeapStart());

	for (UINT frame = 0; frame < BUFFERCOUNT; frame++)
	{
		ID3D12Resource* pBackBuffer;
		ThrowIfFailed(m_dxgiSwapChain->GetBuffer(frame, IID_PPV_ARGS(&pBackBuffer)));

		m_device->CreateRenderTargetView(
			pBackBuffer,
			0,
			m_rtvHeapHandle
		);
		m_rtvHeapHandle.Offset(1, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
	}
	struct Light
	{
		DirectX::XMFLOAT4 Position;
		DirectX::XMFLOAT4 Color;
	};

	struct ConstantBuffer
	{
		DirectX::XMMATRIX World;
		DirectX::XMMATRIX Model;

		Light light;

		DirectX::XMFLOAT4 Eye;
	};
	UINT s = sizeof(ConstantBuffer);

	OutputDebugString((std::to_string(s)).c_str());

	ConstantBuffer cBuffer;
	 
	// 4-Component vectors representing our view matrix info
	DirectX::XMFLOAT4 Eye = DirectX::XMFLOAT4(0.0f, 0.0f, -2.0f, 1.0f );
	DirectX::XMFLOAT4 Focus = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	DirectX::XMFLOAT4 Up = DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);

	// 3D Model matrix
	DirectX::XMMATRIX Model = DirectX::XMMatrixTranslation(0.0f, 0.0f, 0.0f);

	// 3D View matrix
	DirectX::XMMATRIX View = DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat4(&Eye), DirectX::XMLoadFloat4(&Focus), DirectX::XMLoadFloat4(&Up));

	// 3D Projection matrix
	DirectX::XMMATRIX Proj = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(45.0f), 4.0f / 3.0f, 0.1f, 300.0f);

	cBuffer.World = DirectX::XMMatrixTranspose(Model * View * Proj);
	//cBuffer.Color = DirectX::Colors::LightBlue;
	cBuffer.light.Position = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	cBuffer.light.Color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	
	// Fill out a decscriptor heap with our data and attach it to 
	// the root signature so we can use it in our shaders.
	const UINT cBufferSize = sizeof(cBuffer);

	for (UINT i = 0; i < 2; i++)
	{
		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(256U),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			0,
			IID_PPV_ARGS(&m_cbvResource[i])
		));
		void* data;
		m_cbvResource[i]->Map(0, 0, reinterpret_cast<void**>(&data));
		CopyMemory(data, &cBuffer, cBufferSize);
		m_cbvResource[i]->Unmap(0, nullptr);

	}
	Microsoft::WRL::ComPtr<ID3D12Resource> m_srvResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_srvResourceUpload;
	CD3DX12_STATIC_SAMPLER_DESC m_samplerState;
	ID3D12DescriptorHeap* m_srvHeap;

	m_commandList->Reset(m_commandAllocator, nullptr);

	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(
		m_device,
		m_commandList,
		L"checkboard.dds",
		m_srvResource,
		m_srvResourceUpload
	));
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = { };
	srvHeapDesc.NumDescriptors = 1;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)));


	D3D12_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
	ZeroMemory(&shaderResourceViewDesc, sizeof(D3D12_SHADER_RESOURCE_VIEW_DESC));

	shaderResourceViewDesc.Format = m_srvResource->GetDesc().Format;
	shaderResourceViewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	shaderResourceViewDesc.Texture2D.MipLevels = m_srvResource->GetDesc().MipLevels;
	shaderResourceViewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

	m_device->CreateShaderResourceView(m_srvResource.Get(), &shaderResourceViewDesc, m_srvHeap->GetCPUDescriptorHandleForHeapStart());

	m_samplerState.Init(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP
	);

	CD3DX12_ROOT_PARAMETER slotParameters[2];
	
	CD3DX12_DESCRIPTOR_RANGE srvRange;
	srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);


	slotParameters[0].InitAsConstantBufferView(0);
	slotParameters[1].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Init(_countof(slotParameters), slotParameters, 1, &m_samplerState, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	
	ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &m_rootSignatureBlob, 0));
	
	ThrowIfFailed(m_device->CreateRootSignature(0, m_rootSignatureBlob->GetBufferPointer(), 
		m_rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
	
	ID3DBlob* vs, *ps;
	ThrowIfFailed(D3DCompileFromFile(L"Shaders.hlsl", 0, 0, "VSMain", "vs_5_0", 0, 0, &vs, 0));
	ThrowIfFailed(D3DCompileFromFile(L"Shaders.hlsl", 0, 0, "PSMain", "ps_5_0", 0, 0, &ps, 0));
	
	D3D12_INPUT_ELEMENT_DESC inputLayoutDesc[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};

	// Depth Stencil Resources:
	ID3D12DescriptorHeap* m_dsvHeap;
	ID3D12Resource* m_depthStencilResource;

	D3D12_DESCRIPTOR_HEAP_DESC depthStencilHeap = {};
	depthStencilHeap.NumDescriptors = 1;
	depthStencilHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

	ThrowIfFailed(m_device->CreateDescriptorHeap(&depthStencilHeap, IID_PPV_ARGS(&m_dsvHeap)));

	{
		DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		// Create the depth/stencil buffer and view.
		D3D12_RESOURCE_DESC depthStencilDesc = {};
		depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthStencilDesc.Width = 800;
		depthStencilDesc.Height = 600;
		depthStencilDesc.DepthOrArraySize = 1;
		depthStencilDesc.MipLevels = 1;
		depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
		depthStencilDesc.SampleDesc.Count = 1;
		depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE optClear;
		optClear.Format = mDepthStencilFormat;
		optClear.DepthStencil.Depth = 1.0f;
		optClear.DepthStencil.Stencil = 0;

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&depthStencilDesc,
			D3D12_RESOURCE_STATE_COMMON,
			&optClear,
			IID_PPV_ARGS(&m_depthStencilResource)));

		// Create descriptor to mip level 0 of entire resource using the format of the resource.
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Format = mDepthStencilFormat;
		dsvDesc.Texture2D.MipSlice = 0;
		m_device->CreateDepthStencilView(m_depthStencilResource, &dsvDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
	}
	D3D12_RASTERIZER_DESC rasterizerDesc = {};
	rasterizerDesc.AntialiasedLineEnable = true;			// Anti-Aliasing turned on
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;			// Back face culling
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;	// Wire frame primitive instead of solid

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = { 0 };
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(vs);
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(ps);
	psoDesc.pRootSignature = m_rootSignature;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.StencilEnable = false;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.InputLayout = { inputLayoutDesc, _countof(inputLayoutDesc) };
	psoDesc.NumRenderTargets = 1;
	
	ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
	
	struct Vertex 
	{
		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT2 TexCoord;
		DirectX::XMFLOAT3 Normal;

		Vertex(float px, float py, float pz, float u, float v, float nx, float ny, float nz) 
			: Position(px, py, pz), TexCoord(u, v), Normal(nx, ny, nz) { }

		Vertex() { }

	};

	// Frank luna and hooman used the std::array so I'm following in their
	// footsteps :)
	std::array<Vertex, 24> vertices;
	// Fill in the front face vertex data.
	vertices[0] =  Vertex(-0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, -1.0f);
	vertices[1] =  Vertex(-0.5f, +0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f);
	vertices[2] =  Vertex(+0.5f, +0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f);
	vertices[3] =  Vertex(+0.5f, -0.5f, -0.5f, 1.0f, 1.0f, 0.0f, 0.0f, -1.0f);
														   
	vertices[4] =  Vertex(-0.5f, -0.5f, +0.5f, 1.0f, 1.0f, 0.0f, 0.0f, +1.0f);
	vertices[5] =  Vertex(+0.5f, -0.5f, +0.5f, 0.0f, 1.0f, 0.0f, 0.0f, +1.0f);
	vertices[6] =  Vertex(+0.5f, +0.5f, +0.5f, 0.0f, 0.0f, 0.0f, 0.0f, +1.0f);
	vertices[7] =  Vertex(-0.5f, +0.5f, +0.5f, 1.0f, 0.0f, 0.0f, 0.0f, +1.0f);
														  
	vertices[8] =  Vertex(-0.5f, +0.5f, -0.5f, 0.0f, 1.0f, 0.0f, +1.0f, 0.0f);
	vertices[9] =  Vertex(-0.5f, +0.5f, +0.5f, 0.0f, 0.0f, 0.0f, +1.0f, 0.0f);
	vertices[10] = Vertex(+0.5f, +0.5f, +0.5f, 1.0f, 0.0f, 0.0f, +1.0f, 0.0f);
	vertices[11] = Vertex(+0.5f, +0.5f, -0.5f, 1.0f, 1.0f, 0.0f, +1.0f, 0.0f);
														   
	vertices[12] = Vertex(-0.5f, -0.5f, -0.5f, 1.0f, 1.0f, 0.0f, -1.0f, 0.0f);
	vertices[13] = Vertex(+0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f);
	vertices[14] = Vertex(+0.5f, -0.5f, +0.5f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f);
	vertices[15] = Vertex(-0.5f, -0.5f, +0.5f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f);
														  
	vertices[16] = Vertex(-0.5f, -0.5f, +0.5f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f);
	vertices[17] = Vertex(-0.5f, +0.5f, +0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f);
	vertices[18] = Vertex(-0.5f, +0.5f, -0.5f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f);
	vertices[19] = Vertex(-0.5f, -0.5f, -0.5f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f);
														  
	vertices[20] = Vertex(+0.5f, -0.5f, -0.5f, 0.0f, 1.0f, +1.0f, 0.0f, 0.0f);
	vertices[21] = Vertex(+0.5f, +0.5f, -0.5f, 0.0f, 0.0f, +1.0f, 0.0f, 0.0f);
	vertices[22] = Vertex(+0.5f, +0.5f, +0.5f, 1.0f, 0.0f, +1.0f, 0.0f, 0.0f);
	vertices[23] = Vertex(+0.5f, -0.5f, +0.5f, 1.0f, 1.0f, +1.0f, 0.0f, 0.0f);

	std::array<std::uint16_t, 36> indices;
	// Fill in the front face index data
	indices[0] = 0; indices[1] = 1; indices[2] = 2;
	indices[3] = 0; indices[4] = 2; indices[5] = 3;

	// Fill in the back face index data
	indices[6] = 4; indices[7] = 5; indices[8] = 6;
	indices[9] = 4; indices[10] = 6; indices[11] = 7;

	// Fill in the top face index data
	indices[12] = 8; indices[13] = 9; indices[14] = 10;
	indices[15] = 8; indices[16] = 10; indices[17] = 11;

	// Fill in the bottom face index data
	indices[18] = 12; indices[19] = 13; indices[20] = 14;
	indices[21] = 12; indices[22] = 14; indices[23] = 15;

	// Fill in the left face index data
	indices[24] = 16; indices[25] = 17; indices[26] = 18;
	indices[27] = 16; indices[28] = 18; indices[29] = 19;

	// Fill in the right face index data
	indices[30] = 20; indices[31] = 21; indices[32] = 22;
	indices[33] = 20; indices[34] = 22; indices[35] = 23;

	const UINT vBufferSize = sizeof(Vertex) * std::size(vertices);
	const UINT iBufferSize = sizeof(std::uint16_t) * std::size(indices);

	
	ThrowIfFailed(m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(vBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		0,
		IID_PPV_ARGS(&m_vertexBuffer)));
	
	void* data2;
	CD3DX12_RANGE readRange(0, 0);
	m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&data2));
	CopyMemory(data2, &vertices, sizeof(vertices));
	m_vertexBuffer->Unmap(0, nullptr);
	
	m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
	m_vertexBufferView.SizeInBytes = vBufferSize;
	m_vertexBufferView.StrideInBytes = sizeof(Vertex);	

	ThrowIfFailed(m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(iBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_indexBuffer)
	));

	void* data3;
	CD3DX12_RANGE readRange2;
	m_indexBuffer->Map(0, &readRange2, reinterpret_cast<void**>(&data3));
	CopyMemory(data3, &indices, iBufferSize);

	m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
	m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
	m_indexBufferView.SizeInBytes = iBufferSize;

	D3D12_VIEWPORT viewPort = {};
	viewPort.TopLeftX = 0;
	viewPort.TopLeftY = 0;
	viewPort.Width = 800;
	viewPort.Height = 600;
	viewPort.MinDepth = 0.0f;
	viewPort.MaxDepth = 1.0f;

	CD3DX12_RECT scissorsRect = {};
	scissorsRect.left = 0;
	scissorsRect.top = 0;
	scissorsRect.right = 800;
	scissorsRect.bottom = 600;

	MSG msg = { 0 };
	bool quit = false;
	srand((unsigned)time(NULL));

	DirectX::XMVECTOR RandomColors[20] = 
	{
		DirectX::Colors::Blue,
		DirectX::Colors::DarkGreen,
		DirectX::Colors::Aqua,
		DirectX::Colors::Gold,
		DirectX::Colors::MediumPurple,

		DirectX::Colors::Lavender,
		DirectX::Colors::Lavender,
		DirectX::Colors::DarkTurquoise,
		DirectX::Colors::DarkTurquoise,
		DirectX::Colors::Cyan,
		
		DirectX::Colors::ForestGreen,
		DirectX::Colors::Wheat,
		DirectX::Colors::Plum,
		DirectX::Colors::Tomato,
		DirectX::Colors::Silver,

		DirectX::Colors::OrangeRed,
		DirectX::Colors::Violet,
		DirectX::Colors::RoyalBlue,
		DirectX::Colors::LimeGreen,
		DirectX::Colors::Bisque

	};
	int index[2] = { 0, 9 };
	int random = 0;
	
	float pitch = 30.0f;
	float yaw = -90.0f;

	while (!quit)
	{
		// Our descriptor heap already knows the location of our constant buffer.
		// So we don't have to re-create the constant buffer view or descriptor heap for it.
		// This is the benefit of using DirectX 12!
		
		//yaw += 1.0f;
		//pitch += 1.0f;
		//
		// Outer camera
		Eye.x = 5.0f * (cos_radians(yaw) * cos_radians(pitch));
		Eye.y = 5.0f * (sin_radians(pitch));			   
		Eye.z = 5.0f * (sin_radians(yaw) * cos_radians(pitch)); 

		cBuffer.Eye = Eye;

		if (m_iCurrentFence % 60 == 0)
		{
			index[0] = (index[0] + 1) % _countof(RandomColors);
			index[1] = (index[1] + 1) % _countof(RandomColors);
		}

		// Camera eye position, Camera eye focus position, Camera orientation
		View = DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat4(&Eye), DirectX::XMLoadFloat4(&Focus), DirectX::XMLoadFloat4(&Up));

		cBuffer.World = DirectX::XMMatrixTranspose(Model * View * Proj);

		// After one second, change the color of our box.
		// Work: 1 fence passed per frame * 60 fps = 60.
			
		while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			switch (msg.message)
			{
			case WM_QUIT:
				quit = true;
				break;
			case WM_KEYDOWN:
				if (msg.wParam == 27)
					quit = true;
				break;
			}
		}
		m_commandAllocator->Reset();
		m_commandList->Reset(m_commandAllocator, m_pipelineState);

		m_iCurrentFrameIndex = (m_iCurrentFrameIndex + 1) % BUFFERCOUNT;

		// Point our cpu handle to the memory address of the first desriptor
		m_rtvHeapHandle = m_rtvDescHeap->GetCPUDescriptorHandleForHeapStart();

		// Now off set the cpu to whatever frame index we are at
		// simple math: (render target view size * currentFrameIndex)
		m_rtvHeapHandle.Offset(1, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV) * m_iCurrentFrameIndex);
		
		m_commandList->OMSetRenderTargets(1, &m_rtvHeapHandle, false, &m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

		float clear_color[4] = { 0.0f, 0.0f, 0.2f, 1.0f };
		m_commandList->ClearRenderTargetView(m_rtvHeapHandle, clear_color, 0, 0);
		m_commandList->ClearDepthStencilView(m_dsvHeap->GetCPUDescriptorHandleForHeapStart(),
			D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		m_commandList->SetGraphicsRootSignature(m_rootSignature);
		m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
		m_commandList->IASetIndexBuffer(&m_indexBufferView);
		m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_commandList->RSSetScissorRects(1, &scissorsRect);
		m_commandList->RSSetViewports(1, &viewPort);

		m_commandList->SetDescriptorHeaps(1, &m_srvHeap);
		m_commandList->SetGraphicsRootDescriptorTable(1, m_srvHeap->GetGPUDescriptorHandleForHeapStart());
		
		{
			Model = DirectX::XMMatrixRotationY(0.0f) *
				DirectX::XMMatrixTranslation(2.0f*cos(DirectX::XMConvertToRadians((float)m_iCurrentFence+180.0f)), 0.0f, 2.0f*sin(DirectX::XMConvertToRadians((float)m_iCurrentFence + 180.0f)));

			cBuffer.Model = DirectX::XMMatrixTranspose(Model);
			cBuffer.World = DirectX::XMMatrixTranspose(Model * View * Proj);


			void* data4;
			m_cbvResource[0]->Map(0, 0, reinterpret_cast<void**>(&data4));
			CopyMemory(data4, &cBuffer, cBufferSize);
			m_cbvResource[0]->Unmap(0, nullptr);

			m_commandList->SetGraphicsRootConstantBufferView(0, m_cbvResource[0]->GetGPUVirtualAddress());
			m_commandList->DrawIndexedInstanced(std::size(indices), 1, 0, 0, 0);
		}
		{

			Model = DirectX::XMMatrixRotationY(0.0f) *
				DirectX::XMMatrixTranslation(2.0f*cos(DirectX::XMConvertToRadians((float)m_iCurrentFence)), 0.0f, 2.0f*sin(DirectX::XMConvertToRadians((float)m_iCurrentFence)));

			cBuffer.Model = DirectX::XMMatrixTranspose(Model);
			cBuffer.World = DirectX::XMMatrixTranspose(Model * View * Proj);

			DirectX::XMStoreFloat4(&cBuffer.light.Color, RandomColors[index[1]]);
			void* data4;
			m_cbvResource[1]->Map(0, 0, reinterpret_cast<void**>(&data4));
			CopyMemory(data4, &cBuffer, cBufferSize);
			m_cbvResource[1]->Unmap(0, nullptr);

			m_commandList->SetGraphicsRootConstantBufferView(0, m_cbvResource[1]->GetGPUVirtualAddress());
			m_commandList->DrawIndexedInstanced(std::size(indices), 1, 0, 0, 0);
		}
		m_commandList->Close();

		ID3D12CommandList* commandLists[] = { m_commandList };
		m_commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

		m_commandQueue->Signal(m_fence, m_iCurrentFence);
		if (m_fence->GetCompletedValue() < m_iCurrentFence)
		{
			ThrowIfFailed(m_fence->SetEventOnCompletion(m_iCurrentFence, m_fenceEvent));
			WaitForSingleObject(m_fenceEvent, INFINITE);
		}
		m_iCurrentFence++;

		ThrowIfFailed(m_dxgiSwapChain->Present(1, 0));
	}

	return 0;
}