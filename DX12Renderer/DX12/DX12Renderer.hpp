#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h> // For CommandLineToArgvW

// The min/max macros conflict with like-named member functions.
// Only use std::min and std::max defined in <algorithm>.
#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

// In order to define a function called CreateWindow, the Windows macro needs to
// be undefined.
#if defined(CreateWindow)
#undef CreateWindow
#endif

// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>
using namespace Microsoft::WRL;

// DirectX 12 specific headers.
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

// D3D12 extension library.
#include <d3dx12.h>

#include "DX12Renderer/Renderer.hpp"

// From DXSampleHelper.h 
// Source: https://github.com/Microsoft/DirectX-Graphics-Samples
inline void ThrowIfFailed(HRESULT hr)
{
  if (FAILED(hr))
  {
    throw std::exception();
  }
}

// The number of swap chain back buffers.
constexpr uint8_t g_NumFrames = 3;

class Dx12Renderer : public Renderer
{
public:
  Dx12Renderer();
  ~Dx12Renderer();
  std::unique_ptr<InstantiatedModel> CreateModel(std::string& aMeshFile) override;
  void DestroyModel(InstantiatedModel* aModel) override;

  Texture* CreateTexture(std::string& aFilename, TextureType aType) override;
  
  GPUAllocator* MakeAllocator(std::string const& aAllocatorType, size_t aBlockSize) override;

  void Update() override;
  void Render() override;
  void Resize(unsigned aWidth, unsigned aHeight) override;
  void SetFullscreen(bool aFullscreen) override;

  bool UpdateWindow() override;


public:
  // Window handle.
  HWND mWindowHandle;
  // Window rectangle (used to toggle fullscreen state).
  RECT mWindowRect;

  // DirectX 12 Objects
  ComPtr<ID3D12Device2> mDevice;
  ComPtr<ID3D12CommandQueue> mQueue;
  ComPtr<IDXGISwapChain4> mSwapChain;
  ComPtr<ID3D12Resource> mBackBuffers[g_NumFrames];
  ComPtr<ID3D12GraphicsCommandList> mCommandList;
  ComPtr<ID3D12CommandAllocator> mCommandAllocators[g_NumFrames];
  ComPtr<ID3D12DescriptorHeap> mRTVDescriptorHeap;
  UINT mRTVDescriptorSize;
  UINT mCurrentBackBufferIndex;
  
  // Synchronization objects
  ComPtr<ID3D12Fence> g_Fence;
  uint64_t g_FenceValue = 0;
  uint64_t g_FrameFenceValues[g_NumFrames] = {};
  HANDLE g_FenceEvent;


  bool g_UseWarp = false;

  uint32_t g_ClientWidth = 1280;
  uint32_t g_ClientHeight = 720;

  // By default, enable V-Sync.
  // Can be toggled with the V key.
  bool g_VSync = true;
  bool g_TearingSupported = false;
  // By default, use windowed mode.
  // Can be toggled with the Alt+Enter or F11
  bool g_Fullscreen = false;
};
