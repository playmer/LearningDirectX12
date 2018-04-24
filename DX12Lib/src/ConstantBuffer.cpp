#include <DX12LibPCH.h>

#include <ConstantBuffer.h>

#include <Application.h>

#include <d3dx12.h>

ConstantBuffer::ConstantBuffer(const std::wstring & name)
    : Buffer(name)
    , m_SizeInBytes(0)
{
    m_ConstantBufferView = Application::Get().AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

ConstantBuffer::~ConstantBuffer()
{}

void ConstantBuffer::CreateViews(size_t numElements, size_t elementSize)
{
    m_SizeInBytes = numElements * elementSize;

    D3D12_CONSTANT_BUFFER_VIEW_DESC d3d12ConstantBufferViewDesc;
    d3d12ConstantBufferViewDesc.BufferLocation = m_d3d12Resource->GetGPUVirtualAddress();
    d3d12ConstantBufferViewDesc.SizeInBytes = static_cast<UINT>(Math::AlignUp(m_SizeInBytes, 16));

    auto device = Application::Get().GetDevice();

    device->CreateConstantBufferView(&d3d12ConstantBufferViewDesc, m_ConstantBufferView);
}

D3D12_CPU_DESCRIPTOR_HANDLE ConstantBuffer::GetShaderResourceView() const
{
    throw std::exception("ConstantBuffer::GetShaderResourceView should not be called.");
}

D3D12_CPU_DESCRIPTOR_HANDLE ConstantBuffer::GetUnorderedAccessView(uint32_t subresource) const
{
    throw std::exception("ConstantBuffer::GetUnorderedAccessView should not be called.");
}
