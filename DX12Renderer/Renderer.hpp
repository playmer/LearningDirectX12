#include <cstdint>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>


class InstantiatedModel {};
class Texture {};
class Mesh {};


using byte = std::uint8_t;

// Enable if helpers
namespace EnableIf
{
  template<typename tType>
  using IsNotVoid = std::enable_if_t<!std::is_void_v<tType>>;

  template<typename tType>
  using IsVoid = std::enable_if_t<std::is_void_v<tType>>;

  template<typename tType>
  using IsMoveConstructible = std::enable_if_t<std::is_move_constructible_v<tType>>;

  template<typename tType>
  using IsNotMoveConstructible = std::enable_if_t<!std::is_move_constructible_v<tType>>;

  template<typename tType>
  using IsCopyConstructible = std::enable_if_t<std::is_copy_constructible_v<tType>>;

  template<typename tType>
  using IsNotCopyConstructible = std::enable_if_t<!std::is_copy_constructible_v<tType>>;

  template<typename tType>
  using IsDefaultConstructible = std::enable_if_t<std::is_default_constructible_v<tType>>;

  template<typename tType>
  using IsNotDefaultConstructible = std::enable_if_t<!std::is_default_constructible_v<tType>>;
}


template <typename tType>
inline void GenericDestructByte(byte* aMemory)
{
  (reinterpret_cast<tType*>(aMemory))->~tType();
}

template <typename tType>
inline void GenericDestruct(void* aMemory)
{
  (static_cast<tType*>(aMemory))->~tType();
}

template <typename tType, typename = void>
struct GenericDefaultConstructStruct
{

};

template <typename tType>
struct GenericDefaultConstructStruct<tType, EnableIf::IsNotDefaultConstructible<tType>>
{
  static inline void DefaultConstruct(void*)
  {
    runtime_assert(false, "Trying to default construct something without a default constructor.");
  }
};

template <typename tType>
struct GenericDefaultConstructStruct<tType, EnableIf::IsDefaultConstructible<tType>>
{
  static inline void DefaultConstruct(void* aMemory)
  {
    new (aMemory) tType();
  }
};


template <typename T, typename Enable = void>
struct GenericCopyConstructStruct
{

};

template <typename tType>
struct GenericCopyConstructStruct<tType, EnableIf::IsNotCopyConstructible<tType>>
{
  static inline void CopyConstruct(const void*, void*)
  {
    runtime_assert(false, "Trying to copy construct something without a copy constructor.");
  }
};

template <typename tType>
struct GenericCopyConstructStruct<tType, EnableIf::IsCopyConstructible<tType>>
{
  static inline void CopyConstruct(void const* aObject, void* aMemory)
  {
    new (aMemory) tType(*static_cast<tType const*>(aObject));
  }
};

template <typename tType, typename Enable = void>
struct GenericMoveConstructStruct
{

};

template <typename tType>
struct GenericMoveConstructStruct<tType, EnableIf::IsNotMoveConstructible<tType>>
{
  static inline void MoveConstruct(void*, void*)
  {
    runtime_assert(false, "Trying to move construct something without a move constructor.");
  }
};

template <typename tType>
struct GenericMoveConstructStruct<tType, EnableIf::IsMoveConstructible<tType>>
{
  static inline void MoveConstruct(void* aObject, void* aMemory)
  {
    new (aMemory) tType(std::move(*static_cast<tType*>(aObject)));
  }
};

template <typename tType>
inline void GenericDefaultConstruct(void* aMemory)
{
  GenericDefaultConstructStruct<tType>::DefaultConstruct(aMemory);
}

template <typename tType>
inline void GenericCopyConstruct(const void* aObject, void* aMemory)
{
  GenericCopyConstructStruct<tType>::CopyConstruct(aObject, aMemory);
}

template <typename tType>
inline void GenericMoveConstruct(void* aObject, void* aMemory)
{
  GenericMoveConstructStruct<tType>::MoveConstruct(aObject, aMemory);
}


template <>
inline void GenericDestruct<void>(void*)
{
}

template <>
inline void GenericDefaultConstruct<void>(void*)
{
}

template <>
inline void GenericCopyConstruct<void>(const void*, void*)
{
}

// Helper to call the constructor of a type.
template<typename tType = void>
inline void GenericDoNothing(tType*)
{
}


template <typename tType>
class ContiguousRange
{
public:
  ContiguousRange(std::vector<tType>& aContainer)
    : mBegin(aContainer.data())
    , mEnd(aContainer.data() + aContainer.size())
  {
  }

  ContiguousRange(tType& aValue)
    : mBegin(&aValue)
    , mEnd(&aValue + 1)
  {
  }

  ContiguousRange(tType* aBegin, tType* aEnd)
    : mBegin(aBegin)
    , mEnd(aEnd)
  {
  }

  bool IsRange() { return mBegin != mEnd; }

  tType const* begin() const { return mBegin; }
  tType const* end() const { return mEnd; }
  tType* begin() { return mBegin; }
  tType* end() { return mEnd; }

  typename size_t size() const { return mEnd - mBegin; }
protected:
  tType* mBegin;
  tType* mEnd;
};

template <typename tType>
ContiguousRange<tType> MakeContiguousRange(std::vector<tType> aContainer)
{
  return ContiguousRange<tType>(&*aContainer.begin(), &*aContainer.end());
}

//template<typename tType, size_t tElementCount>
//ContiguousRange<tType> MakeContiguousRange(std::array<tType, tElementCount> aContainer)
//{
//  return ContiguousRange<tType>(&*aContainer.begin(), &*aContainer.end());
//}

template <typename tType>
ContiguousRange<tType> MakeContiguousRange(tType& aValue)
{
  return ContiguousRange<tType>(&aValue, &aValue + 1);
}

template <int SizeInBytes>
class PrivateImplementationLocal
{
public:
  using Destructor = decltype(GenericDoNothing<void>)*;

  PrivateImplementationLocal() : mDestructor(GenericDoNothing) {}

  ~PrivateImplementationLocal()
  {
    // Destruct our data if it's already been constructed.
    Release();
  }

  void Release()
  {
    mDestructor(mMemory);
    mDestructor = GenericDoNothing;
  }

  template <typename tType, typename... tArguments>
  tType* ConstructAndGet(tArguments&& ...aArguments)
  {
    static_assert(sizeof(tType) < SizeInBytes,
      "Constructed Type must be smaller than our size.");

    // Destruct any undestructed object.
    mDestructor(mMemory);

    // Capture the destructor of the new type.
    mDestructor = GenericDestruct<tType>;

    // Create a T in our local memory by forwarding any provided arguments.
    new (mMemory) tType(std::forward<tArguments&&>(aArguments)...);

    return Get<tType>();
  }

  template <typename tType>
  tType* Get()
  {
    if (&GenericDestruct<tType> != mDestructor)
    {
      return nullptr;
    }

    return std::launder<tType>(reinterpret_cast<tType*>(mMemory));
  }

private:

  byte mMemory[SizeInBytes];
  Destructor mDestructor;
};

enum class TextureLayout
{
  RGBA,
  Bc1_Rgba_Srgb,
  Bc3_Srgb,
  Bc3_Unorm,
  Bc7_Unorm_Opaque,
  InvalidLayout
};

enum class TextureType
{
  e1D,
  e2D,
  e3D,
  eCube,
  e1DArray,
  e2DArray,
  eCubeArray
};


class GPUBufferBase
{
public:
  GPUBufferBase(size_t aSize)
    : mArraySize{ aSize }
  {

  }

  virtual ~GPUBufferBase()
  {

  }

  PrivateImplementationLocal<32> & GetData()
  {
    return mData;
  }

  virtual void Update(uint8_t const* aPointer, size_t aBytes, size_t aOffset) = 0;

protected:
  PrivateImplementationLocal<32> mData;
  size_t mArraySize;
};

template <typename tType>
class GPUBuffer
{
public:
  GPUBuffer()
  {

  }

  GPUBuffer(std::unique_ptr<GPUBufferBase> aBuffer)
    : mBuffer{ std::move(aBuffer) }
  {

  }

  GPUBufferBase& GetBase()
  {
    return *mBuffer;
  }

  void Update(tType const& aData)
  {
    mBuffer->Update(reinterpret_cast<u8 const*>(&aData), sizeof(tType), 0);
  }

  void Update(tType const* aData, size_t aSize)
  {
    mBuffer->Update(reinterpret_cast<u8 const*>(aData), sizeof(tType) * aSize, 0);
  }

  void Update(ContiguousRange<tType> aData)
  {
    mBuffer->Update(reinterpret_cast<u8 const*>(aData.begin()), sizeof(tType) * aData.size(), 0);
  }

  operator bool()
  {
    return mBuffer != nullptr;
  }

  void reset()
  {
    mBuffer.reset();
  }

  std::unique_ptr<GPUBufferBase> Steal()
  {
    return std::move(mBuffer);
  }

private:
  std::unique_ptr<GPUBufferBase> mBuffer;
};

namespace GPUAllocation
{
  enum class MemoryProperty
  {
    DeviceLocal = 0x00000001,
    HostVisible = 0x00000002,
    HostCoherent = 0x00000004,
    HostCached = 0x00000008,
    LazilyAllocated = 0x00000010,
    Protected = 0x00000020,
  };

  enum class BufferUsage
  {
    TransferSrc = 0x00000001,
    TransferDst = 0x00000002,
    UniformTexelBuffer = 0x00000004,
    StorageTexelBuffer = 0x00000008,
    UniformBuffer = 0x00000010,
    StorageBuffer = 0x00000020,
    IndexBuffer = 0x00000040,
    VertexBuffer = 0x00000080,
    IndirectBuffer = 0x00000100,
  };


  // https://softwareengineering.stackexchange.com/a/204566
  inline MemoryProperty operator | (MemoryProperty lhs, MemoryProperty rhs)
  {
    using T = std::underlying_type_t <MemoryProperty>;
    return static_cast<MemoryProperty>(static_cast<T>(lhs) | static_cast<T>(rhs));
  }

  inline MemoryProperty& operator |= (MemoryProperty& lhs, MemoryProperty rhs)
  {
    lhs = lhs | rhs;
    return lhs;
  }

  inline BufferUsage operator | (BufferUsage lhs, BufferUsage rhs)
  {
    using T = std::underlying_type_t <BufferUsage>;
    return static_cast<BufferUsage>(static_cast<T>(lhs) | static_cast<T>(rhs));
  }

  inline GPUAllocation::BufferUsage& operator |= (BufferUsage& lhs, BufferUsage rhs)
  {
    lhs = lhs | rhs;
    return lhs;
  }
}

namespace AllocatorTypes
{
  extern const std::string Mesh;
  extern const std::string Texture;
  extern const std::string UniformBufferObject;
  extern const std::string BufferUpdates;
}

struct GPUAllocator
{
  GPUAllocator(size_t aBlockSize)
    : mBlockSize{ aBlockSize }
  {

  }

  // Creates a ubo of the given type, aSize allows you to make an array of them.
  // Size must be at least 1
  template <typename tType>
  GPUBuffer<tType> CreateBuffer(size_t aSize,
    GPUAllocation::BufferUsage aUse,
    GPUAllocation::MemoryProperty aProperties)
  {
    size_t sizeOfObject = sizeof(tType) * aSize;

    auto buffer = CreateBufferInternal(sizeOfObject, aUse, aProperties);

    return GPUBuffer<tType>(std::move(buffer));
  }

  PrivateImplementationLocal<64> & GetData()
  {
    return mData;
  }

private:
  virtual std::unique_ptr<GPUBufferBase> CreateBufferInternal(size_t aSize,
    GPUAllocation::BufferUsage aUse,
    GPUAllocation::MemoryProperty aProperties) = 0;

protected:
  PrivateImplementationLocal<64> mData;
  size_t mBlockSize;
};


class Renderer
{
public:
  Renderer();
  virtual ~Renderer() {};
  virtual std::unique_ptr<InstantiatedModel> CreateModel(std::string& aMeshFile) = 0;
  virtual void DestroyModel(InstantiatedModel* aModel) = 0;

  virtual Texture* CreateTexture(std::string& aFilename, TextureType aType) = 0;

  virtual void Update() = 0;
  virtual void Render() = 0;

  virtual void Resize(unsigned aWidth, unsigned aHeight) = 0;
  virtual void SetFullscreen(bool aFullscreen) = 0;

  virtual bool UpdateWindow() = 0;

  template <typename tType>
  GPUBuffer<tType> CreateUBO(
    size_t aSize = 1, 
    GPUAllocation::MemoryProperty aProperty = GPUAllocation::MemoryProperty::DeviceLocal)
  {
    auto allocator = GetAllocator(AllocatorTypes::UniformBufferObject);

    return allocator->CreateBuffer<tType>(aSize,
                                          GPUAllocation::BufferUsage::TransferDst |
                                          GPUAllocation::BufferUsage::UniformBuffer,
                                          aProperty);
  }

  GPUAllocator* GetAllocator(std::string const& aAllocatorType)
  {
    if (auto it = mAllocators.find(aAllocatorType); it != mAllocators.end())
    {
      return it->second.get();
    }

    return nullptr;
  }

  virtual GPUAllocator* MakeAllocator(std::string const& aAllocatorType, size_t aBlockSize) = 0;

protected:
  std::unordered_map<std::string, std::unique_ptr<Mesh>> mBaseMeshes;
  std::unordered_map<std::string, std::unique_ptr<Texture>> mBaseTextures;
  std::unordered_map<std::string, std::unique_ptr<GPUAllocator>> mAllocators;
};