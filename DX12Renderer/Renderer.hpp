#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <variant>
#include <unordered_map>
#include <map>
#include <typeindex>

#include "glm/glm.hpp"

class InstantiatedModel;
class Texture {};
class Mesh;


class aiScene;
class aiMesh;

class Renderer;



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


template<typename tTo, typename tFrom>
std::unique_ptr<tTo> static_unique_pointer_cast(std::unique_ptr<tFrom>&& aValue)
{
  //conversion: unique_ptr<FROM>->FROM*->TO*->unique_ptr<TO>
  return std::unique_ptr<tTo>{static_cast<tTo*>(aValue.release())};
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




namespace detail
{
  template <typename tReturn, typename Arg = tReturn>
  struct GetReturnType {};

  template <typename tReturn, typename tObject, typename ...tArguments>
  struct GetReturnType<tReturn(tObject::*)(tArguments...)>
  {
    using ReturnType = tReturn;
    using tObjectType = tObject;
  };

  template <typename tIteratorType>
  struct iterator
  {
    iterator(tIteratorType& aIterator)
      : mPair(aIterator, 0)
    {

    }

    iterator& operator++()
    {
      ++mPair.first;
      ++mPair.second;

      return *this;
    }

    auto& operator*()
    {
      return mPair;
    }

    template <typename tOtherIterator>
    bool operator!=(const iterator<tOtherIterator> aRight)
    {
      return mPair.first != aRight.mPair.first;
    }

  private:
    std::pair<tIteratorType, size_t> mPair;
  };

  template <typename tIteratorBegin, typename tIteratorEnd = tIteratorBegin>
  struct range
  {
  public:
    range(tIteratorBegin aBegin, tIteratorEnd aEnd)
      : mBegin(aBegin)
      , mEnd(aEnd)
    {

    }

    bool IsRange() { return mBegin != mEnd; }

    iterator<tIteratorBegin> begin() { return mBegin; }
    iterator<tIteratorEnd> end() { return mEnd; }

  private:
    iterator<tIteratorBegin> mBegin;
    iterator<tIteratorEnd> mEnd;
  };
}

template <typename tType>
auto enumerate(tType& aContainer)
{
  detail::range<decltype(aContainer.begin()), decltype(aContainer.end())> toReturn{ aContainer.begin(), aContainer.end() };

  return toReturn;
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
    mBuffer->Update(reinterpret_cast<uint8_t const*>(&aData), sizeof(tType), 0);
  }

  void Update(tType const* aData, size_t aSize)
  {
    mBuffer->Update(reinterpret_cast<uint8_t const*>(aData), sizeof(tType) * aSize, 0);
  }

  void Update(ContiguousRange<tType> aData)
  {
    mBuffer->Update(reinterpret_cast<uint8_t const*>(aData.begin()), sizeof(tType) * aData.size(), 0);
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


template <typename tType>
class GPUBufferRef
{
public:
  GPUBufferRef()
  {

  }

  template <typename tType>
  GPUBufferRef(GPUBuffer<tType>& aBuffer)
    : mBuffer{ &(aBuffer.GetBase()) }
  {

  }

  GPUBufferRef(GPUBufferBase& aBuffer)
    : mBuffer{ &aBuffer }
  {

  }

  GPUBufferRef(GPUBufferBase* aBuffer)
    : mBuffer{ aBuffer }
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

private:
  GPUBufferBase* mBuffer;
};

namespace UBOs
{
  const glm::mat4 mat4Identity( 1.0f, 0.0f, 0.0f, 0.0f,
                                0.0f, 1.0f, 0.0f, 0.0f,
                                0.0f, 0.0f, 1.0f, 0.0f,
                                0.0f, 0.0f, 0.0f, 1.0f );
  constexpr size_t LightCount = 16;
  constexpr size_t WaterInformationCount = 16;

  struct View
  {
    glm::mat4 mProjectionMatrix = mat4Identity;
    glm::mat4 mViewMatrix = mat4Identity;
    glm::vec4 mCameraPosition = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
  };

  struct Model
  {
    glm::mat4 mModelMatrix = mat4Identity;
    glm::vec4 mDiffuseColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f); // tempDiffuseColorComponent
  };

  struct Material
  {
    Material(glm::vec4 aDiffuse,
      glm::vec4 aAmbient,
      glm::vec4 aSpecular,
      glm::vec4 aEmissive,
      glm::vec4 aTransparent,
      glm::vec4 aReflective,
      float aOpacity,
      float aShininess,
      float aShininessStrength,
      float aReflectivity,
      float aReflectiveIndex,
      float aBumpScaling)
      : mDiffuse(aDiffuse)
      , mAmbient(aAmbient)
      , mSpecular(aSpecular)
      , mEmissive(aEmissive)
      , mTransparent(aTransparent)
      , mReflective(aReflective)
      , mOpacity(aOpacity)
      , mShininess(aShininess)
      , mShininessStrength(aShininessStrength)
      , mReflectivity(aReflectivity)
      , mReflectiveIndex(aReflectiveIndex)
      , mBumpScaling(aBumpScaling)
    {

    }

    Material()
    {

    }

    glm::vec4 mDiffuse = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    glm::vec4 mAmbient = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    glm::vec4 mSpecular = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    glm::vec4 mEmissive = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    glm::vec4 mTransparent = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    glm::vec4 mReflective = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    float mOpacity = 1.0f;
    float mShininess = 1.0f;
    float mShininessStrength = 1.0f;
    float mReflectivity = 1.0f;
    float mReflectiveIndex = 1.0f;
    float mBumpScaling = 1.0f;
    uint32_t mFlags = 0;
    int32_t mUsesNormalTexture = 0;
  };


  struct Light
  {
    glm::vec3 mPosition = glm::vec3(0.0f, 0.0f, 0.0f);
    float mActive = 0.0f; // bool
    glm::vec4 mDirection = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    glm::vec4 mAmbient = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    glm::vec4 mDiffuse = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 mSpecular = glm::vec3(0.0f, 0.0f, 0.0f);
    float mIntensity = 1.0f;
    glm::vec2 mSpotLightConeAngles = glm::vec2(0.0f, 0.0f); // (inner, outer)
    unsigned int mLightType = 0;
    float mSpotLightFalloff = 0.0f;
  };

  struct Illumination
  {
    glm::vec4 mCameraPosition = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    glm::vec4 mGlobalIllumination = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    glm::vec4 mFogColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    glm::vec4 mFogCoefficients = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    glm::vec2 mFogPlanes = glm::vec2(0.0f, 0.0f);
    float mTime;
    float mPadding;
  };

  struct LightManager
  {
    Light mLights[UBOs::LightCount];
    unsigned int mNumOfLights = 0;
    float mActive = 0.0f;  // bool
  };
}

struct Vertex
{
  glm::vec3 mPosition;
  glm::vec3 mTextureCoordinates;
  glm::vec3 mNormal;
  glm::vec4 mColor;
  glm::vec3 mTangent;
  glm::vec3 mBinormal;
  glm::vec3 mBitangent;

  Vertex(glm::vec3& aPosition,
    glm::vec3& aTextureCoordinates,
    glm::vec3& aNormal,
    glm::vec4& aColor,
    glm::vec3& aTangent,
    glm::vec3& aBinormal,
    glm::vec3& aBitangent)
    : mPosition(aPosition)
    , mTextureCoordinates(aTextureCoordinates)
    , mNormal(aNormal)
    , mColor(aColor)
    , mTangent(aTangent)
    , mBinormal(aBinormal)
    , mBitangent(aBitangent)
  {

  }

  Vertex()
  {

  }
};












enum class VertexFormat
{
  Undefined,
  R4G4UnormPack8,
  R4G4B4A4UnormPack16,
  B4G4R4A4UnormPack16,
  R5G6B5UnormPack16,
  B5G6R5UnormPack16,
  R5G5B5A1UnormPack16,
  B5G5R5A1UnormPack16,
  A1R5G5B5UnormPack16,
  R8Unorm,
  R8Snorm,
  R8Uscaled,
  R8Sscaled,
  R8Uint,
  R8Sint,
  R8Srgb,
  R8G8Unorm,
  R8G8Snorm,
  R8G8Uscaled,
  R8G8Sscaled,
  R8G8Uint,
  R8G8Sint,
  R8G8Srgb,
  R8G8B8Unorm,
  R8G8B8Snorm,
  R8G8B8Uscaled,
  R8G8B8Sscaled,
  R8G8B8Uint,
  R8G8B8Sint,
  R8G8B8Srgb,
  B8G8R8Unorm,
  B8G8R8Snorm,
  B8G8R8Uscaled,
  B8G8R8Sscaled,
  B8G8R8Uint,
  B8G8R8Sint,
  B8G8R8Srgb,
  R8G8B8A8Unorm,
  R8G8B8A8Snorm,
  R8G8B8A8Uscaled,
  R8G8B8A8Sscaled,
  R8G8B8A8Uint,
  R8G8B8A8Sint,
  R8G8B8A8Srgb,
  B8G8R8A8Unorm,
  B8G8R8A8Snorm,
  B8G8R8A8Uscaled,
  B8G8R8A8Sscaled,
  B8G8R8A8Uint,
  B8G8R8A8Sint,
  B8G8R8A8Srgb,
  A8B8G8R8UnormPack32,
  A8B8G8R8SnormPack32,
  A8B8G8R8UscaledPack32,
  A8B8G8R8SscaledPack32,
  A8B8G8R8UintPack32,
  A8B8G8R8SintPack32,
  A8B8G8R8SrgbPack32,
  A2R10G10B10UnormPack32,
  A2R10G10B10SnormPack32,
  A2R10G10B10UscaledPack32,
  A2R10G10B10SscaledPack32,
  A2R10G10B10UintPack32,
  A2R10G10B10SintPack32,
  A2B10G10R10UnormPack32,
  A2B10G10R10SnormPack32,
  A2B10G10R10UscaledPack32,
  A2B10G10R10SscaledPack32,
  A2B10G10R10UintPack32,
  A2B10G10R10SintPack32,
  R16Unorm,
  R16Snorm,
  R16Uscaled,
  R16Sscaled,
  R16Uint,
  R16Sint,
  R16Sfloat,
  R16G16Unorm,
  R16G16Snorm,
  R16G16Uscaled,
  R16G16Sscaled,
  R16G16Uint,
  R16G16Sint,
  R16G16Sfloat,
  R16G16B16Unorm,
  R16G16B16Snorm,
  R16G16B16Uscaled,
  R16G16B16Sscaled,
  R16G16B16Uint,
  R16G16B16Sint,
  R16G16B16Sfloat,
  R16G16B16A16Unorm,
  R16G16B16A16Snorm,
  R16G16B16A16Uscaled,
  R16G16B16A16Sscaled,
  R16G16B16A16Uint,
  R16G16B16A16Sint,
  R16G16B16A16Sfloat,
  R32Uint,
  R32Sint,
  R32Sfloat,
  R32G32Uint,
  R32G32Sint,
  R32G32Sfloat,
  R32G32B32Uint,
  R32G32B32Sint,
  R32G32B32Sfloat,
  R32G32B32A32Uint,
  R32G32B32A32Sint,
  R32G32B32A32Sfloat,
  R64Uint,
  R64Sint,
  R64Sfloat,
  R64G64Uint,
  R64G64Sint,
  R64G64Sfloat,
  R64G64B64Uint,
  R64G64B64Sint,
  R64G64B64Sfloat,
  R64G64B64A64Uint,
  R64G64B64A64Sint,
  R64G64B64A64Sfloat,
  B10G11R11UfloatPack32,
  E5B9G9R9UfloatPack32,
  D16Unorm,
  X8D24UnormPack32,
  D32Sfloat,
  S8Uint,
  D16UnormS8Uint,
  D24UnormS8Uint,
  D32SfloatS8Uint,
  Bc1RgbUnormBlock,
  Bc1RgbSrgbBlock,
  Bc1RgbaUnormBlock,
  Bc1RgbaSrgbBlock,
  Bc2UnormBlock,
  Bc2SrgbBlock,
  Bc3UnormBlock,
  Bc3SrgbBlock,
  Bc4UnormBlock,
  Bc4SnormBlock,
  Bc5UnormBlock,
  Bc5SnormBlock,
  Bc6HUfloatBlock,
  Bc6HSfloatBlock,
  Bc7UnormBlock,
  Bc7SrgbBlock,
  Etc2R8G8B8UnormBlock,
  Etc2R8G8B8SrgbBlock,
  Etc2R8G8B8A1UnormBlock,
  Etc2R8G8B8A1SrgbBlock,
  Etc2R8G8B8A8UnormBlock,
  Etc2R8G8B8A8SrgbBlock,
  EacR11UnormBlock,
  EacR11SnormBlock,
  EacR11G11UnormBlock,
  EacR11G11SnormBlock,
  Astc4x4UnormBlock,
  Astc4x4SrgbBlock,
  Astc5x4UnormBlock,
  Astc5x4SrgbBlock,
  Astc5x5UnormBlock,
  Astc5x5SrgbBlock,
  Astc6x5UnormBlock,
  Astc6x5SrgbBlock,
  Astc6x6UnormBlock,
  Astc6x6SrgbBlock,
  Astc8x5UnormBlock,
  Astc8x5SrgbBlock,
  Astc8x6UnormBlock,
  Astc8x6SrgbBlock,
  Astc8x8UnormBlock,
  Astc8x8SrgbBlock,
  Astc10x5UnormBlock,
  Astc10x5SrgbBlock,
  Astc10x6UnormBlock,
  Astc10x6SrgbBlock,
  Astc10x8UnormBlock,
  Astc10x8SrgbBlock,
  Astc10x10UnormBlock,
  Astc10x10SrgbBlock,
  Astc12x10UnormBlock,
  Astc12x10SrgbBlock,
  Astc12x12UnormBlock,
  Astc12x12SrgbBlock,
  G8B8G8R8422Unorm,
  B8G8R8G8422Unorm,
  G8B8R83Plane420Unorm,
  G8B8R82Plane420Unorm,
  G8B8R83Plane422Unorm,
  G8B8R82Plane422Unorm,
  G8B8R83Plane444Unorm,
  R10X6UnormPack16,
  R10X6G10X6Unorm2Pack16,
  R10X6G10X6B10X6A10X6Unorm4Pack16,
  G10X6B10X6G10X6R10X6422Unorm4Pack16,
  B10X6G10X6R10X6G10X6422Unorm4Pack16,
  G10X6B10X6R10X63Plane420Unorm3Pack16,
  G10X6B10X6R10X62Plane420Unorm3Pack16,
  G10X6B10X6R10X63Plane422Unorm3Pack16,
  G10X6B10X6R10X62Plane422Unorm3Pack16,
  G10X6B10X6R10X63Plane444Unorm3Pack16,
  R12X4UnormPack16,
  R12X4G12X4Unorm2Pack16,
  R12X4G12X4B12X4A12X4Unorm4Pack16,
  G12X4B12X4G12X4R12X4422Unorm4Pack16,
  B12X4G12X4R12X4G12X4422Unorm4Pack16,
  G12X4B12X4R12X43Plane420Unorm3Pack16,
  G12X4B12X4R12X42Plane420Unorm3Pack16,
  G12X4B12X4R12X43Plane422Unorm3Pack16,
  G12X4B12X4R12X42Plane422Unorm3Pack16,
  G12X4B12X4R12X43Plane444Unorm3Pack16,
  G16B16G16R16422Unorm,
  B16G16R16G16422Unorm,
  G16B16R163Plane420Unorm,
  G16B16R162Plane420Unorm,
  G16B16R163Plane422Unorm,
  G16B16R162Plane422Unorm,
  G16B16R163Plane444Unorm,
  Pvrtc12BppUnormBlockIMG,
  Pvrtc14BppUnormBlockIMG,
  Pvrtc22BppUnormBlockIMG,
  Pvrtc24BppUnormBlockIMG,
  Pvrtc12BppSrgbBlockIMG,
  Pvrtc14BppSrgbBlockIMG,
  Pvrtc22BppSrgbBlockIMG,
  Pvrtc24BppSrgbBlockIMG,
  G8B8G8R8422UnormKHR,
  B8G8R8G8422UnormKHR,
  G8B8R83Plane420UnormKHR,
  G8B8R82Plane420UnormKHR,
  G8B8R83Plane422UnormKHR,
  G8B8R82Plane422UnormKHR,
  G8B8R83Plane444UnormKHR,
  R10X6UnormPack16KHR,
  R10X6G10X6Unorm2Pack16KHR,
  R10X6G10X6B10X6A10X6Unorm4Pack16KHR,
  G10X6B10X6G10X6R10X6422Unorm4Pack16KHR,
  B10X6G10X6R10X6G10X6422Unorm4Pack16KHR,
  G10X6B10X6R10X63Plane420Unorm3Pack16KHR,
  G10X6B10X6R10X62Plane420Unorm3Pack16KHR,
  G10X6B10X6R10X63Plane422Unorm3Pack16KHR,
  G10X6B10X6R10X62Plane422Unorm3Pack16KHR,
  G10X6B10X6R10X63Plane444Unorm3Pack16KHR,
  R12X4UnormPack16KHR,
  R12X4G12X4Unorm2Pack16KHR,
  R12X4G12X4B12X4A12X4Unorm4Pack16KHR,
  G12X4B12X4G12X4R12X4422Unorm4Pack16KHR,
  B12X4G12X4R12X4G12X4422Unorm4Pack16KHR,
  G12X4B12X4R12X43Plane420Unorm3Pack16KHR,
  G12X4B12X4R12X42Plane420Unorm3Pack16KHR,
  G12X4B12X4R12X43Plane422Unorm3Pack16KHR,
  G12X4B12X4R12X42Plane422Unorm3Pack16KHR,
  G12X4B12X4R12X43Plane444Unorm3Pack16KHR,
  G16B16G16R16422UnormKHR,
  B16G16R16G16422UnormKHR,
  G16B16R163Plane420UnormKHR,
  G16B16R162Plane420UnormKHR,
  G16B16R163Plane422UnormKHR,
  G16B16R162Plane422UnormKHR,
  G16B16R163Plane444UnormKHR
};

enum class VertexInputRate
{
  Vertex,
  Instance
};

enum class DescriptorType
{
  Sampler,
  CombinedImageSampler,
  SampledImage,
  StorageImage,
  UniformTexelBuffer,
  StorageTexelBuffer,
  UniformBuffer,
  StorageBuffer,
  UniformBufferDynamic,
  StorageBufferDynamic,
  InputAttachment,
  InlineUniformBlockEXT,
  AccelerationStructureNV
};

enum class ShaderStageFlags
{
  Vertex = 0x00000001,
  TessellationControl = 0x00000002,
  TessellationEvaluation = 0x00000004,
  Geometry = 0x00000008,
  Fragment = 0x00000010,
  Compute = 0x00000020,
  AllGraphics = 0x0000001F,
  All = 0x7FFFFFFF,
  RaygenNV = 0x00000100,
  AnyHitNV = 0x00000200,
  ClosestHitNV = 0x00000400,
  MissNV = 0x00000800,
  IntersectionNV = 0x00001000,
  CallableNV = 0x00002000,
  TaskNV = 0x00000040,
  MeshNV = 0x00000080
};

inline bool operator&(ShaderStageFlags lhs, ShaderStageFlags rhs)
{
  using T = std::underlying_type_t <ShaderStageFlags>;
  return static_cast<T>(lhs) & static_cast<T>(rhs);
}

enum class ImageLayout
{
  Undefined,
  General,
  ColorAttachmentOptimal,
  DepthStencilAttachmentOptimal,
  DepthStencilReadOnlyOptimal,
  ShaderReadOnlyOptimal,
  TransferSrcOptimal,
  TransferDstOptimal,
  Preinitialized,
  DepthReadOnlyStencilAttachmentOptimal,
  DepthAttachmentStencilReadOnlyOptimal,
  PresentSrcKHR,
  SharedPresentKHR,
  ShadingRateOptimalNV,
  FragmentDensityMapOptimalEXT,
  DepthReadOnlyStencilAttachmentOptimalKHR,
  DepthAttachmentStencilReadOnlyOptimalKHR
};

struct VertexInputAttributeDescription
{
  uint32_t location;
  uint32_t binding;
  VertexFormat format;
  uint32_t offset;
};

struct VertexInputBindingDescription
{
  uint32_t binding;
  uint32_t stride;
  VertexInputRate inputRate;
};

struct DescriptorSetLayoutBinding
{
  DescriptorSetLayoutBinding(uint32_t aBinding,
    DescriptorType aDescriptorType,
    ShaderStageFlags aStageFlags,
    size_t aSize,
    size_t aOffset)
    : mBinding{ aBinding }
    , mDescriptorType{ aDescriptorType }
    , mStageFlags{ aStageFlags }
    , mType{ Type::Buffer }
  {
    mBufferOrImage.mBuffer = Buffer{ aSize, aOffset };
  }

  DescriptorSetLayoutBinding(uint32_t aBinding,
    DescriptorType aDescriptorType,
    ShaderStageFlags aStageFlags,
    ImageLayout aLayout)
    : mBinding{ aBinding }
    , mDescriptorType{ aDescriptorType }
    , mStageFlags{ aStageFlags }
    , mType{ Type::Image }
  {
    mBufferOrImage.mLayout = aLayout;
  }

  enum class Type
  {
    Image,
    Buffer
  };

  uint32_t mBinding;
  DescriptorType mDescriptorType;
  ShaderStageFlags mStageFlags;
  Type mType;

  struct Buffer
  {
    size_t mSize;
    size_t mOffset;
  };

  union BufferOrImage
  {
    Buffer mBuffer;
    ImageLayout mLayout;
  } mBufferOrImage;
};

class ShaderDescriptions
{
public:
  ShaderDescriptions(size_t aNumberOfBindings = 2, size_t aNumberOfAttributes = 8);

  //void Append(ShaderDescriptions aDescriptions);

  template <typename tType>
  void AddAttribute(VertexFormat aFormat)
  {
    DebugObjection(mBindings.size() == 0,
      "Haven't added a Vertex binding yet, "
      "so we can't add attribute inputs.");

    VertexInputAttributeDescription toAdd;
    toAdd.binding = mBinding - 1;
    toAdd.location = mLocation;
    toAdd.format = aFormat;
    toAdd.offset = mVertexOffset;

    mAttributes.emplace_back(toAdd);

    ++mLocation;
    mVertexOffset += sizeof(tType);
  }

  template <typename tType>
  void AddBinding(VertexInputRate aInputRate)
  {
    VertexInputBindingDescription toAdd;
    toAdd.binding = mBinding;
    toAdd.inputRate = aInputRate;
    toAdd.stride = sizeof(tType);

    mBindings.emplace_back(toAdd);

    ++mBinding;
    mVertexOffset = 0;
  }


  template <typename tType>
  void AddBindingAndAttribute(VertexInputRate aInputRate, VertexFormat aFormat)
  {
    AddBinding<tType>(aInputRate);
    AddAttribute<tType>(aFormat);
  }

  void AddDescriptor(DescriptorType aDescriptorType, ShaderStageFlags aStage, ImageLayout aLayout)
  {
    mDescriptorSetLayouts.emplace_back(mBufferBinding, aDescriptorType, aStage, aLayout);

    ++mBufferBinding;
  }

  void AddDescriptor(DescriptorType aDescriptorType, ShaderStageFlags aStage, size_t aBufferSize, size_t aBufferOffset = 0)
  {
    mDescriptorSetLayouts.emplace_back(mBufferBinding, aDescriptorType, aStage, aBufferSize, aBufferOffset);

    ++mBufferBinding;
  }

  uint32_t CountDescriptorsOfType(DescriptorType aType) const;

  /////////////////////////////////
  // Getter 
  /////////////////////////////////
  VertexInputBindingDescription* BindingData()
  {
    return mBindings.data();
  }

  size_t BindingSize()
  {
    return mBindings.size();
  }

  std::vector<VertexInputBindingDescription>& Bindings()
  {
    return mBindings;
  }

  std::vector<VertexInputBindingDescription> const& Bindings() const
  {
    return mBindings;
  }

  std::vector<VertexInputAttributeDescription>& Attributes()
  {
    return mAttributes;
  }

  std::vector<VertexInputAttributeDescription> const& Attributes() const
  {
    return mAttributes;
  }

  uint32_t GetBufferBinding()
  {
    return mBufferBinding;
  }

  void AddPreludeLine(std::string_view aLine)
  {
    mLines.emplace_back(aLine.begin(), aLine.end());
  }

  std::string GetLines()
  {
    //fmt::MemoryWriter w;
    //
    //for (auto& line : mLines)
    //{
    //  w.write(line);
    //  w.write("\n");
    //}
    //
    //return w.str();
  }


  std::vector<std::string> const& Lines() const
  {
    return mLines;
  }

  std::vector<DescriptorSetLayoutBinding>& DescriptorSetLayouts()
  {
    return mDescriptorSetLayouts;
  }

  std::vector<DescriptorSetLayoutBinding> const& DescriptorSetLayouts() const
  {
    return mDescriptorSetLayouts;
  }

private:
  // Vertex Input Data
  std::vector<VertexInputBindingDescription> mBindings;
  std::vector<VertexInputAttributeDescription> mAttributes;
  std::vector<std::string> mLines;
  uint32_t mBinding = 0;
  uint32_t mVertexOffset = 0;
  uint32_t mLocation = 0;
  uint32_t mConstant = 0;

  // Buffer Data
  std::vector<DescriptorSetLayoutBinding> mDescriptorSetLayouts;
  uint32_t mBufferBinding = 0;
};











































// create info struct that can be passed into the mesh ctor
struct CreateInfo
{
  glm::vec3 mScale;
  glm::vec2 mUVscale;
  glm::vec3 mCenter;
};


enum class TextureViewType
{
  e2D,
  eCube
};


// Dimension struct is used for bounding box of 3D mesh
struct Dimension
{
  glm::vec3 mMin = glm::vec3(std::numeric_limits<float>::max());
  glm::vec3 mMax = glm::vec3(-std::numeric_limits<float>::max());

  float GetRadius()
  {
    return std::max(glm::length(mMin), glm::length(mMax));
  }

  glm::vec3 GetCenter()
  {
    auto difference = mMax - mMin;

    return mMin + (difference * .5f);
  }
};

struct VertexData
{
  void AddVertex(Vertex const& aVertex)
  {
    mPositionData.emplace_back(aVertex.mPosition);
    mTextureCoordinatesData.emplace_back(aVertex.mTextureCoordinates);
    mNormalData.emplace_back(aVertex.mNormal);
    mColorData.emplace_back(aVertex.mColor);
    mTangentData.emplace_back(aVertex.mTangent);
    mBinormalData.emplace_back(aVertex.mBinormal);
    mBitangentData.emplace_back(aVertex.mBitangent);
  }

  void Clear()
  {
    mPositionData.clear();
    mTextureCoordinatesData.clear();
    mNormalData.clear();
    mColorData.clear();
    mTangentData.clear();
    mBinormalData.clear();
    mBitangentData.clear();
  }

  std::vector<glm::vec3> mPositionData;
  std::vector<glm::vec3> mTextureCoordinatesData;
  std::vector<glm::vec3> mNormalData;
  std::vector<glm::vec4> mColorData;
  std::vector<glm::vec3> mTangentData;
  std::vector<glm::vec3> mBinormalData;
  std::vector<glm::vec3> mBitangentData;
};

struct VertexBufferData
{
  GPUBuffer<glm::vec3> mPositionBuffer;
  GPUBuffer<glm::vec3> mTextureCoordinatesBuffer;
  GPUBuffer<glm::vec3> mNormalBuffer;
  GPUBuffer<glm::vec4> mColorBuffer;
  GPUBuffer<glm::vec3> mTangentBuffer;
  GPUBuffer<glm::vec3> mBinormalBuffer;
  GPUBuffer<glm::vec3> mBitangentBuffer;
};

struct SubmeshData
{
  enum class TextureType
  {
    Diffuse,
    Specular,
    Normal,
    Ambient,
    Emissive,
    Height,
    Shininess,
    Opacity,
    Displacment,
    Lightmap,
    Reflection,
    Unknown
  };

  static char const* ToShaderString(TextureType aType);

  struct TextureData
  {
    TextureData(std::string aName, TextureViewType aViewType, TextureType aSamplerType)
      : mName{ aName }
      , mViewType{ aViewType }
      , mSamplerType{ aSamplerType }
    {

    }

    std::string mName;
    TextureViewType mViewType = TextureViewType::e2D;
    TextureType mSamplerType;
  };

  ShaderDescriptions mDescriptions;
  VertexData mVertexData;
  std::vector<uint32_t> mIndexData;
  std::vector<TextureData> mTextureData;

  std::vector<glm::vec3> mInitialTextureCoordinates;

  UBOs::Material mUBOMaterial;

  std::string mName;
  std::string mMaterialName;
  std::string mShaderSetName;
  Mesh* mMesh;
  bool mDescriptionOverride = false;
  bool mCullBackFaces = true;
};

// Submesh class contains all the data of the actual submesh
class Submesh
{
public:
  Submesh() = default;

  Submesh(Renderer* aRenderer,
    Mesh* aYTEMesh,
    aiScene const* aScene,
    aiMesh const* aMesh,
    uint32_t aBoneStartingVertexOffset);

  Submesh(SubmeshData&& aSubmesh);
  Submesh(Submesh&& aSubmesh);
  Submesh& operator=(Submesh&& aSubmesh);

  // Call this if you've built up a default constructed Submesh. 
  // This will create the GPU buffers and calculate the dimensions.
  void Initialize();

  void UpdatePositionBuffer(std::vector<glm::vec3> const& aData);
  void UpdateTextureCoordinatesBuffer(std::vector<glm::vec3> const& aData);
  void UpdateNormalBuffer(std::vector<glm::vec3> const& aData);
  void UpdateColorBuffer(std::vector<glm::vec4> const& aData);
  void UpdateTangentBuffer(std::vector<glm::vec3> const& aData);
  void UpdateBinormalBuffer(std::vector<glm::vec3> const& aData);
  void UpdateBitangentBuffer(std::vector<glm::vec3> const& aData);
  void UpdateBoneWeightsBuffer(std::vector<glm::vec3> const& aData);
  void UpdateBoneWeights2Buffer(std::vector<glm::vec2> const& aData);
  void UpdateBoneIDsBuffer(std::vector<glm::ivec3> const& aData);
  void UpdateBoneIDs2Buffer(std::vector<glm::ivec2> const& aData);

  size_t GetTriangleCount() const
  {
    return mData.mIndexData.size() / 3;
  }

  glm::uvec3 GetTriangle(size_t aIndex) const
  {
    glm::uvec3 tri;
    tri.x = (glm::uint)mData.mIndexData[aIndex];
    tri.y = (glm::uint)mData.mIndexData[aIndex + 1];
    tri.z = (glm::uint)mData.mIndexData[aIndex + 2];
    return tri;
  }

  ShaderDescriptions const& CreateShaderDescriptions();

  VertexBufferData mVertexBufferData;
  GPUBuffer<uint32_t> mIndexBuffer;

  SubmeshData mData;

private:
  void UpdateGPUVertexData();
  void CreateGPUBuffers();

  Submesh& operator=(Submesh const& aSubmesh) = delete;
  Submesh(Submesh const& aSubmesh) = delete;
};

class Mesh
{
public:
  Mesh();

  Mesh(Renderer* aRenderer,
    const std::string& aFile);

  Mesh(Renderer* aRenderer,
    const std::string& aFile,
    ContiguousRange<SubmeshData> aSubmeshes);

  virtual ~Mesh();

  std::vector<Submesh>& GetSubmeshes();

  std::string mName;
  std::vector<Submesh> mParts;
  Renderer* mRenderer;
  bool mInstanced;

private:
  Mesh(Mesh const&) = delete;
  Mesh& operator=(Mesh const&) = delete;
};
















































enum class ShaderType
{
  Triangles,
  Lines,
  Curves,
  Wireframe,
  ShaderNoCull,
  AlphaBlendShader,
  AdditiveBlendShader,
};


class InstantiatedModel
{
public:
  using OwnedBuffer  = std::unique_ptr<GPUBufferBase>;
  using ObservedBuffer = GPUBufferBase *;
  using BufferRef = std::variant<OwnedBuffer, ObservedBuffer>;

  InstantiatedModel(Renderer* aRenderer);

  void Create();

  virtual ~InstantiatedModel()
  {

  }

  void SetDefaultAnimationOffset();

  void UpdateUBOModel(UBOs::Model const& aUBO)
  {
    mUBOModelData = aUBO;
    mModelUBO.Update(mUBOModelData);
  }

  /////////////////////////////////
  // Getters / Setters
  /////////////////////////////////
  UBOs::Model const& GetUBOModelData()
  {
    return mUBOModelData;
  }

  UBOs::Material GetUBOMaterialData()
  {
    return mUBOModelMaterialData;
  }

  Mesh* GetMesh()
  {
    return mMesh;
  }


  void SetVisibility(bool aVisibility)
  {
    mVisibility = aVisibility;
  }

  bool GetVisibility()
  {
    return mVisibility;
  }

  ShaderType mType = ShaderType::Triangles;
  float mLineWidth = 1.0f;

  GPUBuffer<UBOs::Model>& GetModelUBOBuffer()
  {
    return mModelUBO;
  }

  template <typename tType>
  GPUBufferRef<tType> GetBufferRef()
  {
    auto it = mTypeToIndex.find(TypeId<tType>());

    if (it != mTypeToIndex.end())
    {
      if (auto ownedBuffer = std::get_if<OwnedBuffer>(&mBuffers[it->second]))
      {
        return GPUBufferRef<tType>{ **ownedBuffer };
      }
      else if (auto observedBuffer = std::get_if<ObservedBuffer>(&mBuffers[it->second]))
      {
        return GPUBufferRef<tType>{ **observedBuffer };
      }
    }

    return { nullptr };
  }

  std::vector<std::variant<OwnedBuffer, ObservedBuffer>>& GetBuffers()
  {
    return mBuffers;
  }

  // Does not take ownership of the buffer
  template <typename tType>
  void AddBuffer(GPUBufferBase* aBufferRef)
  {
    mBuffers.emplace_back(aBufferRef);
    mTypeToIndex.emplace(TypeId<tType>(), mBuffers.size());
  }

  // Does not takes ownership of the buffer
  template <typename tType>
  void AddBuffer(GPUBuffer<tType>* aBufferRef)
  {
    AddBuffer<tType>(&aBufferRef->GetBase());
  }

  // Takes ownership of the buffer
  template <typename tType>
  void AddBuffer(GPUBuffer<tType>& aBufferRef)
  {
    mBuffers.emplace_back(aBufferRef.Steal());
    mTypeToIndex.emplace(TypeId<tType>(), mBuffers.size());
  }

protected:
  std::vector<BufferRef> mBuffers;
  std::multimap<std::type_index, size_t> mTypeToIndex;

  Renderer* mRenderer;
  Mesh* mMesh;
  GPUBuffer<UBOs::Model> mModelUBO;
  UBOs::Model mUBOModelData;
  bool mVisibility = true;
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

class InstantiatedLight;

class LightManager
{
public:
  LightManager();

  ~LightManager()
  {
    mBuffer.reset();
    mLights.clear();
  }

  void LoadToGPU();

  void AddLight(InstantiatedLight* aLight);
  void DestroyLight(InstantiatedLight* aLight);

  GPUBuffer<UBOs::LightManager>& GetUBOLightBuffer()
  {
    return mBuffer;
  }

private:
  std::vector<InstantiatedLight*> mLights;
  UBOs::LightManager mLightData;
  GPUBuffer<UBOs::LightManager> mBuffer;
  Renderer* mRenderer;
  unsigned char mLightUse[UBOs::LightCount];
  bool mUpdateRequired;
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