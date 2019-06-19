#include <algorithm>
#include <chrono>

#include "DX12Renderer/DX12/DX12Renderer.hpp"



Dx12Renderer* gRenderer = nullptr;



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

ComPtr<ID3D12CommandQueue> CreateCommandQueue(ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
{
  ComPtr<ID3D12CommandQueue> d3d12CommandQueue;

  D3D12_COMMAND_QUEUE_DESC desc = {};
  desc.Type = type;
  desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
  desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  desc.NodeMask = 0;

  ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&d3d12CommandQueue)));

  return d3d12CommandQueue;
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

  mQueue = CreateCommandQueue(mDevice, D3D12_COMMAND_LIST_TYPE_DIRECT);

  mSwapChain = CreateSwapChain(mWindowHandle, factory, mQueue, g_ClientWidth, g_ClientHeight, g_NumFrames);

  mCurrentBackBufferIndex = mSwapChain->GetCurrentBackBufferIndex();

  mRTVDescriptorHeap = CreateDescriptorHeap(mDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, g_NumFrames);
  mRTVDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  UpdateRenderTargetViews(this, mDevice, mSwapChain, mRTVDescriptorHeap);

  for (int i = 0; i < g_NumFrames; ++i)
  {
    mCommandAllocators[i] = CreateCommandAllocator(mDevice, D3D12_COMMAND_LIST_TYPE_DIRECT);
  }

  mCommandList = CreateCommandList(
    mDevice, 
    mCommandAllocators[mCurrentBackBufferIndex], 
    D3D12_COMMAND_LIST_TYPE_DIRECT);

  g_Fence = CreateFence(mDevice);
  g_FenceEvent = CreateEventHandle();

  ::ShowWindow(mWindowHandle, SW_SHOW);
}

Dx12Renderer::~Dx12Renderer()
{    
  // Make sure the command queue has finished all commands before closing.
  Flush(mQueue, g_Fence, g_FenceValue, g_FenceEvent);

  ::CloseHandle(g_FenceEvent);
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

GPUAllocator* Dx12Renderer::MakeAllocator(std::string const& aAllocatorType, size_t aBlockSize)
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
  auto commandAllocator = mCommandAllocators[mCurrentBackBufferIndex];
  auto backBuffer = mBackBuffers[mCurrentBackBufferIndex];

  commandAllocator->Reset();
  mCommandList->Reset(commandAllocator.Get(), nullptr);

  // Clear the render target.
  {
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
      backBuffer.Get(),
      D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

    mCommandList->ResourceBarrier(1, &barrier);
    FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(
      mRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
      mCurrentBackBufferIndex, 
      mRTVDescriptorSize);

    mCommandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
  }

  // Present
  {
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
      backBuffer.Get(),
      D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    mCommandList->ResourceBarrier(1, &barrier);
    ThrowIfFailed(mCommandList->Close());

    ID3D12CommandList* const commandLists[] = {
        mCommandList.Get()
    };

    mQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
    UINT syncInterval = g_VSync ? 1 : 0;
    UINT presentFlags = g_TearingSupported && !g_VSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
    ThrowIfFailed(mSwapChain->Present(syncInterval, presentFlags));

    g_FrameFenceValues[mCurrentBackBufferIndex] = Signal(mQueue, g_Fence, g_FenceValue);
    mCurrentBackBufferIndex = mSwapChain->GetCurrentBackBufferIndex();

    WaitForFenceValue(g_Fence, g_FrameFenceValues[mCurrentBackBufferIndex], g_FenceEvent);
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
    Flush(mQueue, g_Fence, g_FenceValue, g_FenceEvent);
    for (int i = 0; i < g_NumFrames; ++i)
    {
      // Any references to the back buffers must be released
      // before the swap chain can be resized.
      mBackBuffers[i].Reset();
      g_FrameFenceValues[i] = g_FrameFenceValues[mCurrentBackBufferIndex];
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