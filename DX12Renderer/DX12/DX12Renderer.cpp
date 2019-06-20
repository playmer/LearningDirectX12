#include <algorithm>
#include <chrono>

#include "DX12Renderer/DX12/DX12Renderer.hpp"



Dx12Renderer* gRenderer = nullptr;



struct Dx12GPUAllocatorData
{
  Dx12GPUAllocatorData(Dx12Renderer* aRenderer)
    : mRenderer{ aRenderer }
  {

  }

  Dx12Renderer* mRenderer;
};



Dx12UBOUpdates::Dx12UBOReference::Dx12UBOReference(Microsoft::WRL::ComPtr<ID3D12Resource> const& aBuffer,
  size_t aBufferOffset,
  size_t aSize)
  : mBuffer(aBuffer)
  , mBufferOffset{ aBufferOffset }
  , mSize{ aSize }
{

}

void Dx12UBOUpdates::Add(Microsoft::WRL::ComPtr<ID3D12Resource> const& aBuffer,
  uint8_t const* aData,
  size_t aSize,
  size_t aOffset)
{
  std::lock_guard<std::mutex> lock(mAddingMutex);
  mReferences.emplace_back(aBuffer, aOffset, aSize);
  mData.insert(mData.end(), aData, aData + aSize);
}

void Dx12UBOUpdates::Update(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>& aCommandBuffer)
{
  std::lock_guard<std::mutex> lock(mAddingMutex);

  auto size = mData.size();

  if (0 == size)
  {
    return;
  }
  
  if ((nullptr == mMappingBuffer) || size < mMappingBufferSize)
  {
    ThrowIfFailed(mRenderer->mDevice->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
      D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(size),
      D3D12_RESOURCE_STATE_COPY_SOURCE,
      nullptr,
      IID_PPV_ARGS(&mMappingBuffer)));

    mMappingBufferSize = size;
  }

  // No reading is required.
  CD3DX12_RANGE readRange(0, 0);

  void* pData;
  ThrowIfFailed(mMappingBuffer->Map(0, &readRange, &pData));
  std::memcpy(pData, mData.data(), size);

  CD3DX12_RANGE writeRange(0, size);
  mMappingBuffer->Unmap(0, &writeRange);

  size_t dataOffset = 0;
  
  for (auto const& reference : mReferences)
  {
    //vk::BufferCopy copyOperation{ dataOffset, reference.mBufferOffset, reference.mSize };
    //
    //aCommandBuffer->getResourceTracker()->track(reference.mBuffer);
    //commandBuffer.copyBuffer(*mMappingBuffer, *reference.mBuffer, copyOperation);

    D3D12_RESOURCE_DESC Desc = reference.mBuffer->GetDesc();
    ID3D12Device* pDevice = mRenderer->mDevice.Get();

    pDevice->GetCopyableFootprints(&Desc, FirstSubresource, NumSubresources, IntermediateOffset, pLayouts, pNumRows, pRowSizesInBytes, &RequiredSize);
    pDevice->Release();

    UINT64 Result = UpdateSubresources(&aCommandBuffer, reference.mBuffer, mMappingBuffer, FirstSubresource, NumSubresources, RequiredSize, pLayouts, pNumRows, pRowSizesInBytes, pSrcData);
    
    dataOffset += reference.mSize;
  }
  
  mData.clear();
  mReferences.clear();
}


//////////////////////////////////////////////////////////////////////////////////////////////
// DX12 Helpers:
ComPtr<IDXGIFactory4> CreateFactory()
{
  ComPtr<IDXGIFactory4> dxgiFactory4;
  UINT createFactoryFlags = 0;
#if defined(_DEBUG)
  createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

  ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)));

  return dxgiFactory4;
}

ComPtr<IDXGIAdapter4> GetAdapter(ComPtr<IDXGIFactory4> aFactory)
{
  ComPtr<IDXGIAdapter1> dxgiAdapter1;
  ComPtr<IDXGIAdapter4> dxgiAdapter4;

  SIZE_T maxDedicatedVideoMemory = 0;
  for (UINT i = 0; aFactory->EnumAdapters1(i, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++i)
  {
    DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
    dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

    // Check to see if the adapter can create a D3D12 device without actually 
    // creating it. The adapter with the largest dedicated video memory
    // is favored.
    if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
      SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(),
        D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) &&
      dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
    {
      maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
      ThrowIfFailed(dxgiAdapter1.As(&dxgiAdapter4));
    }
  }

  return dxgiAdapter4;
}

ComPtr<ID3D12Device2> CreateDevice(ComPtr<IDXGIAdapter4> adapter)
{
  ComPtr<ID3D12Device2> d3d12Device2;
  ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12Device2)));

  // Enable debug messages in debug mode.
#if defined(_DEBUG)
  ComPtr<ID3D12InfoQueue> pInfoQueue;
  if (SUCCEEDED(d3d12Device2.As(&pInfoQueue)))
  {
    pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
    pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
    pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
    
    // Suppress whole categories of messages
    //D3D12_MESSAGE_CATEGORY Categories[] = {};

    // Suppress messages based on their severity level
    D3D12_MESSAGE_SEVERITY Severities[] =
    {
        D3D12_MESSAGE_SEVERITY_INFO
    };

    // Suppress individual messages by their ID
    D3D12_MESSAGE_ID DenyIds[] = {
        D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,   // I'm really not sure how to avoid this message.
        D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
        D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
    };

    D3D12_INFO_QUEUE_FILTER NewFilter = {};
    //NewFilter.DenyList.NumCategories = _countof(Categories);
    //NewFilter.DenyList.pCategoryList = Categories;
    NewFilter.DenyList.NumSeverities = _countof(Severities);
    NewFilter.DenyList.pSeverityList = Severities;
    NewFilter.DenyList.NumIDs = _countof(DenyIds);
    NewFilter.DenyList.pIDList = DenyIds;

    ThrowIfFailed(pInfoQueue->PushStorageFilter(&NewFilter));
  }
#endif

  return d3d12Device2;
}

bool CheckTearingSupport()
{
  BOOL allowTearing = FALSE;

  // Rather than create the DXGI 1.5 factory interface directly, we create the
  // DXGI 1.4 interface and query for the 1.5 interface. This is to enable the 
  // graphics debugging tools which will not support the 1.5 factory interface 
  // until a future update.
  ComPtr<IDXGIFactory4> factory4;
  if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4))))
  {
    ComPtr<IDXGIFactory5> factory5;
    if (SUCCEEDED(factory4.As(&factory5)))
    {
      if (FAILED(factory5->CheckFeatureSupport(
        DXGI_FEATURE_PRESENT_ALLOW_TEARING,
        &allowTearing, sizeof(allowTearing))))
      {
        allowTearing = FALSE;
      }
    }
  }

  return allowTearing == TRUE;
}

ComPtr<IDXGISwapChain4> CreateSwapChain(
  HWND hWnd,
  ComPtr<IDXGIFactory4> aFactory,
  ComPtr<ID3D12CommandQueue> commandQueue,
  uint32_t width, 
  uint32_t height, 
  uint32_t bufferCount)
{
  ComPtr<IDXGISwapChain4> dxgiSwapChain4;

  DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
  swapChainDesc.Width = width;
  swapChainDesc.Height = height;
  swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  swapChainDesc.Stereo = FALSE;
  swapChainDesc.SampleDesc = { 1, 0 };
  swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapChainDesc.BufferCount = bufferCount;
  swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
  swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
  // It is recommended to always allow tearing if tearing support is available.
  swapChainDesc.Flags = CheckTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

  ComPtr<IDXGISwapChain1> swapChain1;
  ThrowIfFailed(aFactory->CreateSwapChainForHwnd(
    commandQueue.Get(),
    hWnd,
    &swapChainDesc,
    nullptr,
    nullptr,
    &swapChain1));

  // Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
  // will be handled manually.
  ThrowIfFailed(aFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));

  ThrowIfFailed(swapChain1.As(&dxgiSwapChain4));

  return dxgiSwapChain4;
}


void EnableDebugLayer()
{
#if defined(_DEBUG)
  // Always enable the debug layer before doing anything DX12 related
  // so all possible errors generated while creating DX12 objects
  // are caught by the debug layer.
  ComPtr<ID3D12Debug> debugInterface;
  ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
  debugInterface->EnableDebugLayer();
#endif
}

ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
  ComPtr<ID3D12Device2> device,
  D3D12_DESCRIPTOR_HEAP_TYPE type,
  uint32_t numDescriptors)
{
  ComPtr<ID3D12DescriptorHeap> descriptorHeap;

  D3D12_DESCRIPTOR_HEAP_DESC desc = {};
  desc.NumDescriptors = numDescriptors;
  desc.Type = type;

  ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));

  return descriptorHeap;
}

void UpdateRenderTargetViews(
  Dx12Renderer* aRenderer,
  ComPtr<ID3D12Device2> device,
  ComPtr<IDXGISwapChain4> swapChain,
  ComPtr<ID3D12DescriptorHeap> descriptorHeap)
{
  auto rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart());

  for (int i = 0; i < g_NumFrames; ++i)
  {
    ComPtr<ID3D12Resource> backBuffer;
    ThrowIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)));

    device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);

    aRenderer->mBackBuffers[i] = backBuffer;

    rtvHandle.Offset(rtvDescriptorSize);
  }
}

ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(ComPtr<ID3D12Device2> device,
  D3D12_COMMAND_LIST_TYPE type)
{
  ComPtr<ID3D12CommandAllocator> commandAllocator;
  ThrowIfFailed(device->CreateCommandAllocator(type, IID_PPV_ARGS(&commandAllocator)));

  return commandAllocator;
}

ComPtr<ID3D12GraphicsCommandList> CreateCommandList(ComPtr<ID3D12Device2> device,
  ComPtr<ID3D12CommandAllocator> commandAllocator, D3D12_COMMAND_LIST_TYPE type)
{
  ComPtr<ID3D12GraphicsCommandList> commandList;
  ThrowIfFailed(device->CreateCommandList(0, type, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

  ThrowIfFailed(commandList->Close());

  return commandList;
}

ComPtr<ID3D12Fence> CreateFence(ComPtr<ID3D12Device2> device)
{
  ComPtr<ID3D12Fence> fence;

  ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));

  return fence;
}


uint64_t Signal(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence,
  uint64_t& fenceValue)
{
  uint64_t fenceValueForSignal = ++fenceValue;
  ThrowIfFailed(commandQueue->Signal(fence.Get(), fenceValueForSignal));

  return fenceValueForSignal;
}

void WaitForFenceValue(ComPtr<ID3D12Fence> fence, uint64_t fenceValue, HANDLE fenceEvent,
  std::chrono::milliseconds duration = std::chrono::milliseconds::max())
{
  if (fence->GetCompletedValue() < fenceValue)
  {
    ThrowIfFailed(fence->SetEventOnCompletion(fenceValue, fenceEvent));
    ::WaitForSingleObject(fenceEvent, static_cast<DWORD>(duration.count()));
  }
}

void Flush(
  ComPtr<ID3D12CommandQueue> commandQueue, 
  ComPtr<ID3D12Fence> fence,
  uint64_t& fenceValue, 
  HANDLE fenceEvent)
{
  uint64_t fenceValueForSignal = Signal(commandQueue, fence, fenceValue);
  WaitForFenceValue(fence, fenceValueForSignal, fenceEvent);
}

HANDLE CreateEventHandle()
{
  HANDLE fenceEvent;

  fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
  assert(fenceEvent && "Failed to create fence event.");

  return fenceEvent;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// Dx12Queue
Dx12Queue::Dx12Queue(Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
  : mCommandListType{type}
  , mDevice{ device }
{
  D3D12_COMMAND_QUEUE_DESC desc = {};
  desc.Type = type;
  desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
  desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  desc.NodeMask = 0;

  ThrowIfFailed(mDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&mQueue)));
  ThrowIfFailed(mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));

  mFenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
  assert(mFenceEvent && "Failed to create fence event handle.");
}

Dx12Queue::~Dx12Queue()
{

}

Microsoft::WRL::ComPtr<ID3D12CommandAllocator> Dx12Queue::CreateCommandAllocator()
{
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
  ThrowIfFailed(mDevice->CreateCommandAllocator(mCommandListType, IID_PPV_ARGS(&commandAllocator)));

  return commandAllocator;
}

Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> Dx12Queue::CreateCommandList(Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator)
{
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList;
  ThrowIfFailed(mDevice->CreateCommandList(0, mCommandListType, allocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));

  return commandList;
}

// Get an available command list from the command queue.
Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> Dx12Queue::GetCommandList()
{
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList;

  if (!mCommandAllocatorQueue.empty() && IsFenceComplete(mCommandAllocatorQueue.front().fenceValue))
  {
    commandAllocator = mCommandAllocatorQueue.front().commandAllocator;
    mCommandAllocatorQueue.pop();

    ThrowIfFailed(commandAllocator->Reset());
  }
  else
  {
    commandAllocator = CreateCommandAllocator();
  }

  if (!mCommandListQueue.empty())
  {
    commandList = mCommandListQueue.front();
    mCommandListQueue.pop();

    ThrowIfFailed(commandList->Reset(commandAllocator.Get(), nullptr));
  }
  else
  {
    commandList = CreateCommandList(commandAllocator);
  }

  // Associate the command allocator with the command list so that it can be
  // retrieved when the command list is executed.
  ThrowIfFailed(commandList->SetPrivateDataInterface(__uuidof(ID3D12CommandAllocator), commandAllocator.Get()));

  return commandList;
}

// Execute a command list.
// Returns the fence value to wait for for this command list.
uint64_t Dx12Queue::ExecuteCommandList(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList)
{
  commandList->Close();

  ID3D12CommandAllocator* commandAllocator;
  UINT dataSize = sizeof(commandAllocator);
  ThrowIfFailed(commandList->GetPrivateData(__uuidof(ID3D12CommandAllocator), &dataSize, &commandAllocator));
  
  ID3D12CommandList* const ppCommandLists[] = {
      commandList.Get()
  };
  
  mQueue->ExecuteCommandLists(1, ppCommandLists);
  uint64_t fenceValue = Signal();
  
  mCommandAllocatorQueue.emplace(CommandAllocatorEntry{ fenceValue, commandAllocator });
  mCommandListQueue.push(commandList);
  
  // The ownership of the command allocator has been transferred to the ComPtr
  // in the command allocator queue. It is safe to release the reference 
  // in this temporary COM pointer here.
  commandAllocator->Release();

  return fenceValue;
}

uint64_t Dx12Queue::Signal()
{
  uint64_t fenceValueForSignal = ++mFenceValue;
  ThrowIfFailed(mQueue->Signal(mFence.Get(), fenceValueForSignal));

  return fenceValueForSignal;
}

bool Dx12Queue::IsFenceComplete(uint64_t fenceValue)
{
  return mFence->GetCompletedValue() >= fenceValue;
}

void Dx12Queue::WaitForFenceValue(uint64_t fenceValue)
{
  if (!IsFenceComplete(fenceValue))
  {
    auto event = ::CreateEvent(NULL, FALSE, FALSE, NULL);
    assert(event && "Failed to create fence event handle.");

    // Is this function thread safe?
    mFence->SetEventOnCompletion(fenceValue, event);
    ::WaitForSingleObject(event, DWORD_MAX);

    ::CloseHandle(event);
  }
}

void Dx12Queue::Flush()
{
  uint64_t fenceValueForSignal = Signal();
  WaitForFenceValue(fenceValueForSignal);
}



//////////////////////////////////////////////////////////////////////////////////////////////
// Window Management
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  if (nullptr != gRenderer)
  {
    switch (message)
    {
    case WM_PAINT:
      gRenderer->Update();
      gRenderer->Render();
      break;
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    {
      bool alt = (::GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

      switch (wParam)
      {
      case 'V':
        gRenderer->g_VSync = !gRenderer->g_VSync;
        break;
      case VK_ESCAPE:
        ::PostQuitMessage(0);
        break;
      case VK_RETURN:
        if (alt)
        {
      case VK_F11:
        gRenderer->SetFullscreen(!gRenderer->g_Fullscreen);
        }
        break;
      }
    }
    break;
    // The default window procedure will play a system notification sound 
    // when pressing the Alt+Enter keyboard combination if this message is 
    // not handled.
    case WM_SYSCHAR:
      break;
    case WM_SIZE:
    {
      RECT clientRect = {};
      ::GetClientRect(gRenderer->mWindowHandle, &clientRect);

      int width = clientRect.right - clientRect.left;
      int height = clientRect.bottom - clientRect.top;

      gRenderer->Resize(width, height);
    }
    break;
    case WM_DESTROY:
      ::PostQuitMessage(0);
      break;
    default:
      return ::DefWindowProcW(hwnd, message, wParam, lParam);
    }
  }
  else
  {
    return ::DefWindowProcW(hwnd, message, wParam, lParam);
  }

  return 0;
}

void RegisterWindowClass(HINSTANCE hInst, const wchar_t* windowClassName)
{
  // Register a window class for creating our render window with.
  WNDCLASSEXW windowClass = {};

  windowClass.cbSize = sizeof(WNDCLASSEX);
  windowClass.style = CS_HREDRAW | CS_VREDRAW;
  windowClass.lpfnWndProc = &WndProc;
  windowClass.cbClsExtra = 0;
  windowClass.cbWndExtra = 0;
  windowClass.hInstance = hInst;
  windowClass.hIcon = ::LoadIcon(hInst, NULL);
  windowClass.hCursor = ::LoadCursor(NULL, IDC_ARROW);
  windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  windowClass.lpszMenuName = NULL;
  windowClass.lpszClassName = windowClassName;
  windowClass.hIconSm = ::LoadIcon(hInst, NULL);

  static ATOM atom = ::RegisterClassExW(&windowClass);
  assert(atom > 0);
}

HWND CreateWindow(const wchar_t* windowClassName, HINSTANCE hInst,
  const wchar_t* windowTitle, uint32_t width, uint32_t height)
{
  int screenWidth = ::GetSystemMetrics(SM_CXSCREEN);
  int screenHeight = ::GetSystemMetrics(SM_CYSCREEN);

  RECT windowRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
  ::AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

  int windowWidth = windowRect.right - windowRect.left;
  int windowHeight = windowRect.bottom - windowRect.top;

  // Center the window within the screen. Clamp to 0, 0 for the top-left corner.
  int windowX = std::max<int>(0, (screenWidth - windowWidth) / 2);
  int windowY = std::max<int>(0, (screenHeight - windowHeight) / 2);

  HWND hWnd = ::CreateWindowExW(
    NULL,
    windowClassName,
    windowTitle,
    WS_OVERLAPPEDWINDOW,
    windowX,
    windowY,
    windowWidth,
    windowHeight,
    NULL,
    NULL,
    hInst,
    nullptr
  );

  assert(hWnd && "Failed to create window");

  return hWnd;
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Renderer


Dx12Renderer::Dx12Renderer() 
{
  EnableDebugLayer();
  gRenderer = this;

  // Windows 10 Creators update adds Per Monitor V2 DPI awareness context.
  // Using this awareness context allows the client area of the window 
  // to achieve 100% scaling while still allowing non-client window content to 
  // be rendered in a DPI sensitive fashion.
  SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

  // Window class name. Used for registering / creating the window.
  const wchar_t* windowClassName = L"DX12WindowClass";

  g_TearingSupported = CheckTearingSupport();

  auto factory = CreateFactory();

  auto hInstance = GetModuleHandle(NULL);

  RegisterWindowClass(hInstance, windowClassName);
  mWindowHandle = CreateWindow(
    windowClassName, 
    hInstance, 
    L"Learning DirectX 12",
    g_ClientWidth, 
    g_ClientHeight);

  // Initialize the global window rect variable.
  ::GetWindowRect(mWindowHandle, &mWindowRect);

  ComPtr<IDXGIAdapter4> dxgiAdapter4 = GetAdapter(factory);

  mDevice = CreateDevice(dxgiAdapter4);

  mGraphicsQueue.emplace(mDevice, D3D12_COMMAND_LIST_TYPE_DIRECT);
  mComputeQueue.emplace(mDevice, D3D12_COMMAND_LIST_TYPE_COMPUTE);
  mTransferQueue.emplace(mDevice, D3D12_COMMAND_LIST_TYPE_COPY);

  mSwapChain = CreateSwapChain(mWindowHandle, factory, &(*mGraphicsQueue), g_ClientWidth, g_ClientHeight, g_NumFrames);

  mCurrentBackBufferIndex = mSwapChain->GetCurrentBackBufferIndex();

  mRTVDescriptorHeap = CreateDescriptorHeap(mDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, g_NumFrames);
  mRTVDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  UpdateRenderTargetViews(this, mDevice, mSwapChain, mRTVDescriptorHeap);

  for (int i = 0; i < g_NumFrames; ++i)
  {
    mCommandAllocators[i] = CreateCommandAllocator(mDevice, D3D12_COMMAND_LIST_TYPE_DIRECT);
  }

  //g_Fence = CreateFence(mDevice);
  //g_FenceEvent = CreateEventHandle();

  ::ShowWindow(mWindowHandle, SW_SHOW);
}

Dx12Renderer::~Dx12Renderer()
{    
  // Make sure the command queue has finished all commands before closing.
  mGraphicsQueue->Flush();
  mComputeQueue->Flush();
  mTransferQueue->Flush();
}

std::unique_ptr<InstantiatedModel> Dx12Renderer::CreateModel(std::string& aMeshFile)
{
  return std::make_unique<InstantiatedModel>();
}

void Dx12Renderer::DestroyModel(InstantiatedModel* aModel)
{

}

Texture* Dx12Renderer::CreateTexture(std::string& aFilename, TextureType aType)
{
  return nullptr;
}


void Dx12Renderer::Update()
{
  static uint64_t frameCounter = 0;
  static double elapsedSeconds = 0.0;
  static std::chrono::high_resolution_clock clock;
  static auto t0 = clock.now();

  frameCounter++;
  auto t1 = clock.now();
  auto deltaTime = t1 - t0;
  t0 = t1;
  elapsedSeconds += deltaTime.count() * 1e-9;
  if (elapsedSeconds > 1.0)
  {
    char buffer[500];
    auto fps = frameCounter / elapsedSeconds;
    sprintf_s(buffer, 500, "FPS: %f\n", fps);
    OutputDebugString(buffer);

    frameCounter = 0;
    elapsedSeconds = 0.0;
  }
}

void Dx12Renderer::Render()
{
  auto backBuffer = mBackBuffers[mCurrentBackBufferIndex];
  auto commandList = mGraphicsQueue->GetCommandList();
  

  // Clear the render target.
  {
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
      backBuffer.Get(),
      D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

    commandList->ResourceBarrier(1, &barrier);
    FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(
      mRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
      mCurrentBackBufferIndex, 
      mRTVDescriptorSize);

    commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
  }

  // Present
  {
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
      backBuffer.Get(),
      D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    commandList->ResourceBarrier(1, &barrier);

    mFrameFenceValues[mCurrentBackBufferIndex] = mGraphicsQueue->ExecuteCommandList(commandList);

    UINT syncInterval = g_VSync ? 1 : 0;
    UINT presentFlags = g_TearingSupported && !g_VSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
    ThrowIfFailed(mSwapChain->Present(syncInterval, presentFlags));

    auto fenceValue = mGraphicsQueue->Signal();
    mCurrentBackBufferIndex = mSwapChain->GetCurrentBackBufferIndex();

    mGraphicsQueue->WaitForFenceValue(mFrameFenceValues[mCurrentBackBufferIndex]);
  }
}

void Dx12Renderer::Resize(unsigned aWidth, unsigned aHeight)
{
  if (g_ClientWidth != aWidth || g_ClientHeight != aHeight)
  {
    // Don't allow 0 size swap chain back buffers.
    g_ClientWidth = std::max(1u, aWidth);
    g_ClientHeight = std::max(1u, aHeight);

    // Flush the GPU queue to make sure the swap chain's back buffers
    // are not being referenced by an in-flight command list.
    mGraphicsQueue->Flush();

    for (int i = 0; i < g_NumFrames; ++i)
    {
      // Any references to the back buffers must be released
      // before the swap chain can be resized.
      mBackBuffers[i].Reset();
    }

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    ThrowIfFailed(mSwapChain->GetDesc(&swapChainDesc));
    ThrowIfFailed(mSwapChain->ResizeBuffers(g_NumFrames, g_ClientWidth, g_ClientHeight,
      swapChainDesc.BufferDesc.Format, swapChainDesc.Flags));

    mCurrentBackBufferIndex = mSwapChain->GetCurrentBackBufferIndex();

    UpdateRenderTargetViews(this, mDevice, mSwapChain, mRTVDescriptorHeap);
  }
}

void Dx12Renderer::SetFullscreen(bool aFullscreen)
{
  if (g_Fullscreen != aFullscreen)
  {
    g_Fullscreen = aFullscreen;

    if (g_Fullscreen) // Switching to fullscreen.
    {
      // Store the current window dimensions so they can be restored 
      // when switching out of fullscreen state.
      ::GetWindowRect(mWindowHandle, &mWindowRect);

      // Set the window style to a borderless window so the client area fills
      // the entire screen.
      UINT windowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);

      ::SetWindowLongW(mWindowHandle, GWL_STYLE, windowStyle);

      // Query the name of the nearest display device for the window.
      // This is required to set the fullscreen dimensions of the window
      // when using a multi-monitor setup.
      HMONITOR hMonitor = ::MonitorFromWindow(mWindowHandle, MONITOR_DEFAULTTONEAREST);
      MONITORINFOEX monitorInfo = {};
      monitorInfo.cbSize = sizeof(MONITORINFOEX);
      ::GetMonitorInfo(hMonitor, &monitorInfo);

      ::SetWindowPos(mWindowHandle, HWND_TOP,
        monitorInfo.rcMonitor.left,
        monitorInfo.rcMonitor.top,
        monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
        monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
        SWP_FRAMECHANGED | SWP_NOACTIVATE);

      ::ShowWindow(mWindowHandle, SW_MAXIMIZE);
    }
    else
    {
      // Restore all the window decorators.
      ::SetWindowLong(mWindowHandle, GWL_STYLE, WS_OVERLAPPEDWINDOW);

      ::SetWindowPos(mWindowHandle, HWND_NOTOPMOST,
        mWindowRect.left,
        mWindowRect.top,
        mWindowRect.right - mWindowRect.left,
        mWindowRect.bottom - mWindowRect.top,
        SWP_FRAMECHANGED | SWP_NOACTIVATE);

      ::ShowWindow(mWindowHandle, SW_NORMAL);
    }
  }
}

bool Dx12Renderer::UpdateWindow()
{
  MSG message;

  while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE) && (message.message != WM_QUIT))
  {
    TranslateMessage(&message);
    DispatchMessage(&message);
  }

  if (WM_QUIT == message.message)
  {
    return false;
  }

  return true;
}

GPUAllocator* Dx12Renderer::MakeAllocator(std::string const& aAllocatorType, size_t aBlockSize)
{
  return nullptr;
}








template <typename tType>
uint64_t ToU64(tType aValue)
{
  return static_cast<u64>(aValue);
}


//vk::MemoryPropertyFlags ToVulkan(GPUAllocation::MemoryProperty aValue)
//{
//  vk::MemoryPropertyFlags toReturn{};
//
//  auto value = ToU64(aValue);
//
//  if (0 != (value & ToU64(GPUAllocation::MemoryProperty::DeviceLocal)))
//  {
//    toReturn = toReturn | vk::MemoryPropertyFlagBits::eDeviceLocal;
//  }
//  if (0 != (value & ToU64(GPUAllocation::MemoryProperty::HostVisible)))
//  {
//    toReturn = toReturn | vk::MemoryPropertyFlagBits::eHostVisible;
//  }
//  if (0 != (value & ToU64(GPUAllocation::MemoryProperty::HostCoherent)))
//  {
//    toReturn = toReturn | vk::MemoryPropertyFlagBits::eHostCoherent;
//  }
//  if (0 != (value & ToU64(GPUAllocation::MemoryProperty::HostCached)))
//  {
//    toReturn = toReturn | vk::MemoryPropertyFlagBits::eHostCached;
//  }
//  if (0 != (value & ToU64(GPUAllocation::MemoryProperty::LazilyAllocated)))
//  {
//    toReturn = toReturn | vk::MemoryPropertyFlagBits::eLazilyAllocated;
//  }
//  if (0 != (value & ToU64(GPUAllocation::MemoryProperty::Protected)))
//  {
//    toReturn = toReturn | vk::MemoryPropertyFlagBits::eProtected;
//  }
//
//  return toReturn;
//}


//enum D3D12_HEAP_FLAGS
//{
//  D3D12_HEAP_FLAG_NONE = 0,
//  D3D12_HEAP_FLAG_SHARED = 0x1,
//  D3D12_HEAP_FLAG_DENY_BUFFERS = 0x4,
//  D3D12_HEAP_FLAG_ALLOW_DISPLAY = 0x8,
//  D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER = 0x20,
//  D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES = 0x40,
//  D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES = 0x80,
//  D3D12_HEAP_FLAG_HARDWARE_PROTECTED = 0x100,
//  D3D12_HEAP_FLAG_ALLOW_WRITE_WATCH = 0x200,
//  D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS = 0x400,
//  D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES = 0,
//  D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS = 0xc0,
//  D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES = 0x44,
//  D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES = 0x84
//}



//enum D3D12_RESOURCE_STATES
//{
//  D3D12_RESOURCE_STATE_COMMON = 0,
//  D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER = 0x1,
//  D3D12_RESOURCE_STATE_INDEX_BUFFER = 0x2,
//  D3D12_RESOURCE_STATE_RENDER_TARGET = 0x4,
//  D3D12_RESOURCE_STATE_UNORDERED_ACCESS = 0x8,
//  D3D12_RESOURCE_STATE_DEPTH_WRITE = 0x10,
//  D3D12_RESOURCE_STATE_DEPTH_READ = 0x20,
//  D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE = 0x40,
//  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE = 0x80,
//  D3D12_RESOURCE_STATE_STREAM_OUT = 0x100,
//  D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT = 0x200,
//  D3D12_RESOURCE_STATE_COPY_DEST = 0x400,
//  D3D12_RESOURCE_STATE_COPY_SOURCE = 0x800,
//  D3D12_RESOURCE_STATE_RESOLVE_DEST = 0x1000,
//  D3D12_RESOURCE_STATE_RESOLVE_SOURCE = 0x2000,
//  D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE = 0x400000,
//  D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE = 0x1000000,
//  D3D12_RESOURCE_STATE_GENERIC_READ = (((((0x1 | 0x2) | 0x40) | 0x80) | 0x200) | 0x800),
//  D3D12_RESOURCE_STATE_PRESENT = 0,
//  D3D12_RESOURCE_STATE_PREDICATION = 0x200,
//  D3D12_RESOURCE_STATE_VIDEO_DECODE_READ = 0x10000,
//  D3D12_RESOURCE_STATE_VIDEO_DECODE_WRITE = 0x20000,
//  D3D12_RESOURCE_STATE_VIDEO_PROCESS_READ = 0x40000,
//  D3D12_RESOURCE_STATE_VIDEO_PROCESS_WRITE = 0x80000,
//  D3D12_RESOURCE_STATE_VIDEO_ENCODE_READ = 0x200000,
//  D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE = 0x800000
//}

D3D12_RESOURCE_STATES ToDx12(GPUAllocation::BufferUsage aValue)
{
  D3D12_RESOURCE_STATES toReturn{};

  auto value = ToU64(aValue);

  if (0 != (value & ToU64(GPUAllocation::BufferUsage::TransferSrc)))
  {
    toReturn = toReturn | D3D12_RESOURCE_STATE_COPY_SOURCE;
  }
  if (0 != (value & ToU64(GPUAllocation::BufferUsage::TransferDst)))
  {
    toReturn = toReturn | D3D12_RESOURCE_STATE_COPY_DEST;
  }
  if (0 != (value & ToU64(GPUAllocation::BufferUsage::UniformTexelBuffer)))
  {
    //toReturn = toReturn | eUniformTexelBuffer;
  }
  if (0 != (value & ToU64(GPUAllocation::BufferUsage::StorageTexelBuffer)))
  {
    //toReturn = toReturn | eStorageTexelBuffer;
  }
  if (0 != (value & ToU64(GPUAllocation::BufferUsage::UniformBuffer)))
  {
    toReturn = toReturn | D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
  }
  if (0 != (value & ToU64(GPUAllocation::BufferUsage::StorageBuffer)))
  {
    //toReturn = toReturn | eStorageBuffer;
  }
  if (0 != (value & ToU64(GPUAllocation::BufferUsage::IndexBuffer)))
  {
    toReturn = toReturn | D3D12_RESOURCE_STATE_INDEX_BUFFER;
  }
  if (0 != (value & ToU64(GPUAllocation::BufferUsage::VertexBuffer)))
  {
    toReturn = toReturn | D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
  }
  if (0 != (value & ToU64(GPUAllocation::BufferUsage::IndirectBuffer)))
  {
    toReturn = toReturn | D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
  }

  return toReturn;
}








std::unique_ptr<GPUBufferBase> Dx12GPUAllocator::CreateBufferInternal(size_t aSize,
  GPUAllocation::BufferUsage aUsage,
  GPUAllocation::MemoryProperty aProperties)
{
  auto self = mData.Get<Dx12GPUAllocatorData>();
  
  auto base = std::make_unique<Dx12UBO>(aSize);
  auto uboData = base->GetData().ConstructAndGet<Dx12UBOData>();
  
  auto usage = ToDx12(aUsage);

  //auto properties = ToVulkan(aProperties);
  //
  //uboData->mBuffer = self->mDevice->createBuffer(aSize,
  //  ToVulkan(aUsage),
  //  vk::SharingMode::eExclusive,
  //  nullptr,
  //  ToVulkan(aProperties),
  //  self->mAllocator);
  
  auto error = self->mRenderer->mDevice->CreateCommittedResource(
    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
    D3D12_HEAP_FLAG_NONE,
    &CD3DX12_RESOURCE_DESC::Buffer(aSize, D3D12_RESOURCE_FLAG_NONE),
    usage,
    nullptr,
    IID_PPV_ARGS(&base->GetBuffer())
  );

  ThrowIfFailed(error);

  //ThrowIfFailed(device->CreateCommittedResource(
  //  &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
  //  D3D12_HEAP_FLAG_NONE,
  //  &CD3DX12_RESOURCE_DESC::Buffer(aSize, flags),
  //  D3D12_RESOURCE_STATE_COPY_DEST,
  //  nullptr,
  //  IID_PPV_ARGS(pDestinationResource)));


  uboData->mRenderer = self->mRenderer;
  
  return static_unique_pointer_cast<GPUBufferBase>(std::move(base));
}