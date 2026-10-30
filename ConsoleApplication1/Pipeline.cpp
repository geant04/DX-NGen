#include "Pipeline.h"

#include <iostream>

namespace Gfx {

void CreatePSO( const ComPtr<ID3D12Device> &device, DrawTask* drawTask )
{
	ComPtr<ID3D12RootSignature> &rootSignature = drawTask->rootSignature;
	ComPtr<ID3D12PipelineState> &outPSO = drawTask->pipelineState;
	const char* vsAssetName = drawTask->initResources.vertexShaderName;
	const char* psAssetName = drawTask->initResources.pixelShaderName;

	const uint32_t renderTargetsSize = drawTask->renderTargets.size();
	const bool isDepthOnly = ( renderTargetsSize == 0 ) && drawTask->depthStencilTarget;
	ComPtr<ID3DBlob> vertexShader;
	ComPtr<ID3DBlob> pixelShader;
	ComPtr<ID3DBlob> errorMessage;

	// For debugging, you might wantt o have the following flags:
	// D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION
	// This way, if we use graphics debugging tools, we get fun symbols and stuff.
	int32_t compileFlags = 0;

	auto vsPath = GetAssetFullPath(vsAssetName);
	auto psPath = GetAssetFullPath(psAssetName);

	HRESULT hr = D3DCompileFromFile( vsPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &errorMessage );
	if ( FAILED( hr ) )
	{
		if (errorMessage)
		{
			OutputDebugStringA(
				static_cast<const char*>(errorMessage->GetBufferPointer())
			);
			std::cout << "ERROR: " << (char*)errorMessage->GetBufferPointer() << std::endl;
		}
		return;
	}

	if ( !isDepthOnly )
	{
		hr = D3DCompileFromFile( psPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &errorMessage );
		if ( FAILED( hr ) )
		{
			if (errorMessage)
			{
				OutputDebugStringA(
					static_cast<const char*>(errorMessage->GetBufferPointer())
				);
				std::cout << "ERROR: " << (char*)errorMessage->GetBufferPointer() << std::endl;
			}
			return;
		}
	}

	D3D12_SHADER_BYTECODE vsByteCode = {};
	vsByteCode.pShaderBytecode = vertexShader->GetBufferPointer();
	vsByteCode.BytecodeLength = vertexShader->GetBufferSize();

	D3D12_SHADER_BYTECODE psByteCode = {};
	if ( !isDepthOnly )
	{
		psByteCode.pShaderBytecode = pixelShader->GetBufferPointer();
		psByteCode.BytecodeLength = pixelShader->GetBufferSize();
	}

	// Vertex shader specification data is hard-coded here, as specificed by the HelloTriangle example.
	// When we have more complex objects, we will refactor this accordingly.
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = 
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};

	const bool depthOrStencil = ( drawTask->depthStencilTarget != nullptr );
	const bool depthTest = depthOrStencil && drawTask->depthTest;
	const bool stencilTest = depthOrStencil && drawTask->stencilTest;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	psoDesc.pRootSignature = rootSignature.Get();
	psoDesc.VS = vsByteCode;
	psoDesc.PS = psByteCode;
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC( D3D12_DEFAULT );
	psoDesc.BlendState = CD3DX12_BLEND_DESC( D3D12_DEFAULT );

	if ( depthOrStencil )
	{
		psoDesc.DSVFormat = drawTask->depthStencilTarget->format;
	}
	if ( depthTest )
	{
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC( D3D12_DEFAULT );
		psoDesc.DepthStencilState.DepthEnable = TRUE;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	}
	
	psoDesc.DepthStencilState.StencilEnable = stencilTest;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = renderTargetsSize;

	for ( uint32_t i = 0; i < renderTargetsSize; i++ )
	{
		psoDesc.RTVFormats[i] = drawTask->renderTargets[i]->rtvTexture->format;
	}

	psoDesc.SampleDesc.Count = 1;

	hr = device->CreateGraphicsPipelineState( &psoDesc, IID_PPV_ARGS( &outPSO ) );

	if (outPSO == NULL) {
		// Problem detected, need to log this!
		return;
	}
}

void CreateComputePSO(
	ComPtr<ID3D12Device> &device, 
	ComPtr<ID3D12RootSignature> &rootSignature, 
	ComPtr<ID3D12PipelineState> &outPSO,
	const char* csAssetName,
	const char* entryPoint
)
{
	ComPtr<ID3DBlob> csShader;

	// For debugging, you might wantt o have the following flags:
	// D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION
	// This way, if we use graphics debugging tools, we get fun symbols and stuff.
	int32_t compileFlags = 0;

	auto csPath = GetAssetFullPath(csAssetName);

	ComPtr<ID3DBlob> errorMsg;
	HRESULT hr = D3DCompileFromFile(
		csPath.c_str(), 
		nullptr, 
		D3D_COMPILE_STANDARD_FILE_INCLUDE, 
		entryPoint, 
		"cs_5_0", 
		compileFlags, 
		0, 
		&csShader, 
		&errorMsg
	);

	if (FAILED(hr)) {
		if (errorMsg) {
			OutputDebugStringA((char*)errorMsg->GetBufferPointer());
			std::cout << "ERROR: " << (char*)errorMsg->GetBufferPointer() << std::endl;
		}
		return;
	}

	D3D12_SHADER_BYTECODE csByteCode = {};
	csByteCode.pShaderBytecode = csShader->GetBufferPointer();
	csByteCode.BytecodeLength = csShader->GetBufferSize();

	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = rootSignature.Get();
	psoDesc.CS = csByteCode;

	device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&outPSO));

	if (outPSO == NULL) {
		// Problem detected, need to log this!
		return;
	}
}

}