#include <array>

#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"
#include "assimp/types.h"
#include "assimp/vector3.h"

#include "glm/gtc/type_ptr.hpp"

#include "fmt/format.h"

#include "DX12Renderer/Renderer.hpp"


namespace AllocatorTypes
{
  const std::string Mesh{ "Mesh" };
  const std::string Texture{ "Texture" };
  const std::string UniformBufferObject{ "UniformBufferObject" };
  const std::string BufferUpdates{ "BufferUpdates" };
}

Renderer::Renderer()
{

}

InstantiatedModel::InstantiatedModel(Renderer* aRenderer)
  : mRenderer{ aRenderer }
  , mMesh{ nullptr }
  , mModelUBO{ aRenderer->CreateUBO<UBOs::Model>() }
{
}

void InstantiatedModel::Create()
{
  UBOs::Material modelMaterial{};
  modelMaterial.mDiffuse = glm::vec4{ 1.0f, 1.0f, 1.0f, 1.0f };
  modelMaterial.mAmbient = glm::vec4{ 1.0f, 1.0f, 1.0f, 1.0f };
  modelMaterial.mSpecular = glm::vec4{ 0.0f, 0.0f, 0.0f, 1.0f };
  modelMaterial.mEmissive = glm::vec4{ 0.0f, 0.0f, 0.0f, 1.0f };
  modelMaterial.mShininess = 1.0f;

  mUBOModelData.mModelMatrix = glm::mat4(1.0f);
}



















SubmeshData::TextureType ToYTE(aiTextureType aType)
{
  switch (aType)
  {
  case aiTextureType_DIFFUSE: return SubmeshData::TextureType::Diffuse;
  case aiTextureType_SPECULAR: return SubmeshData::TextureType::Specular;
  case aiTextureType_AMBIENT: return SubmeshData::TextureType::Ambient;
  case aiTextureType_EMISSIVE: return SubmeshData::TextureType::Emissive;
  case aiTextureType_HEIGHT: return SubmeshData::TextureType::Height;
  case aiTextureType_NORMALS: return SubmeshData::TextureType::Normal;
  case aiTextureType_SHININESS: return SubmeshData::TextureType::Shininess;
  case aiTextureType_OPACITY: return SubmeshData::TextureType::Opacity;
  case aiTextureType_DISPLACEMENT: return SubmeshData::TextureType::Displacment;
  case aiTextureType_LIGHTMAP: return SubmeshData::TextureType::Lightmap;
  case aiTextureType_REFLECTION: return SubmeshData::TextureType::Reflection;
  }

  return SubmeshData::TextureType::Unknown;
}











char const* SubmeshData::ToShaderString(TextureType aType)
{
  switch (aType)
  {
  case SubmeshData::TextureType::Diffuse: return "DIFFUSE";
  case SubmeshData::TextureType::Specular: return "SPECULAR";
  case SubmeshData::TextureType::Ambient: return "AMBIENT";
  case SubmeshData::TextureType::Emissive: return "EMISSIVE";
  case SubmeshData::TextureType::Height: return "HEIGHT";
  case SubmeshData::TextureType::Normal: return "NORMAL";
  case SubmeshData::TextureType::Shininess: return "SHININESS";
  case SubmeshData::TextureType::Opacity: return "OPACITY";
  case SubmeshData::TextureType::Displacment: return "DISPLACEMENT";
  case SubmeshData::TextureType::Lightmap: return "LIGHTMAP";
  case SubmeshData::TextureType::Reflection: return "REFLECTION";
  }

  return "UNKNOWN";
}

static inline
glm::vec3 ToGlm(const aiVector3D* aVector)
{
  return { aVector->x, aVector->y ,aVector->z };
}

static inline
glm::vec3 ToGlm(const aiColor3D* aVector)
{
  return { aVector->r, aVector->g ,aVector->b };
}

static inline
glm::quat ToGlm(const aiQuaternion* aQuat)
{
  glm::quat quaternion;

  quaternion.x = aQuat->x;
  quaternion.y = aQuat->y;
  quaternion.z = aQuat->z;
  quaternion.w = aQuat->w;

  return quaternion;
}

static inline
glm::mat4 ToGlm(const aiMatrix4x4& aMatrix)
{
  return glm::transpose(glm::make_mat4(&aMatrix.a1));
}

static inline
aiMatrix4x4 ToAssimp(const glm::mat4& aMatrix)
{
  auto transposed = glm::transpose(aMatrix);
  return *(reinterpret_cast<aiMatrix4x4*>(glm::value_ptr(transposed)));
}

//////////////////////////////////////////////////////////////////////////////
// Submesh
//////////////////////////////////////////////////////////////////////////////
Submesh::Submesh(Renderer* aRenderer,
  Mesh* aYTEMesh,
  const aiScene* aScene,
  const aiMesh* aMesh,
  uint32_t aBoneStartingVertexOffset)
{
  mData.mMesh = aYTEMesh;

  aiColor3D pColor(0.f, 0.f, 0.f);
  aScene->mMaterials[aMesh->mMaterialIndex]->Get(AI_MATKEY_COLOR_DIFFUSE,
    pColor);

  const aiVector3D Zero3D(0.0f, 0.0f, 0.0f);

  auto material = aScene->mMaterials[aMesh->mMaterialIndex];

  aiString name;

  aiColor3D Diffuse;
  aiColor3D Ambient;
  aiColor3D Specular;
  aiColor3D Emissive;
  aiColor3D Transparent;
  aiColor3D Reflective;

  material->Get(AI_MATKEY_COLOR_DIFFUSE, Diffuse);
  material->Get(AI_MATKEY_COLOR_AMBIENT, Ambient);
  material->Get(AI_MATKEY_COLOR_SPECULAR, Specular);
  material->Get(AI_MATKEY_COLOR_EMISSIVE, Emissive);
  material->Get(AI_MATKEY_COLOR_TRANSPARENT, Transparent);
  material->Get(AI_MATKEY_COLOR_REFLECTIVE, Reflective);

  auto& uboMaterial = mData.mUBOMaterial;

  uboMaterial.mDiffuse = glm::vec4(ToGlm(&Diffuse), 1.0f);
  uboMaterial.mAmbient = glm::vec4(ToGlm(&Ambient), 1.0f);
  uboMaterial.mSpecular = glm::vec4(ToGlm(&Specular), 1.0f);
  uboMaterial.mEmissive = glm::vec4(ToGlm(&Emissive), 1.0f);
  uboMaterial.mTransparent = glm::vec4(ToGlm(&Transparent), 1.0f);
  uboMaterial.mReflective = glm::vec4(ToGlm(&Reflective), 1.0f);
  uboMaterial.mFlags = 0;

  material->Get(AI_MATKEY_NAME, name);
  material->Get(AI_MATKEY_OPACITY, uboMaterial.mOpacity);
  material->Get(AI_MATKEY_SHININESS, uboMaterial.mShininess);
  material->Get(AI_MATKEY_SHININESS_STRENGTH, uboMaterial.mShininessStrength);
  material->Get(AI_MATKEY_REFLECTIVITY, uboMaterial.mReflectivity);
  material->Get(AI_MATKEY_REFRACTI, uboMaterial.mReflectiveIndex);
  material->Get(AI_MATKEY_BUMPSCALING, uboMaterial.mBumpScaling);

  mData.mName = aMesh->mName.C_Str();
  mData.mMaterialName = name.C_Str();

  std::array textureTypesSupported{
    // The texture is combined with the result of the diffuse lighting equation.
    aiTextureType_DIFFUSE,

    // The texture is combined with the result of the specular lighting equation.
    aiTextureType_SPECULAR,

    //// The texture is combined with the result of the ambient lighting equation.
    //aiTextureType_AMBIENT,

    //// The texture is added to the result of the lighting calculation. It isn't 
    //// influenced by incoming light.
    //aiTextureType_EMISSIVE,

    //// Height Map: By convention, higher gray-scale values stand for higher elevations 
    //// from the base height.
    //aiTextureType_HEIGHT,

    // Normal Map: (Tangent Space) Again, there are several conventions for tangent-space 
    // normal maps. Assimp does (intentionally) not distinguish here.
    aiTextureType_NORMALS,

    //// The texture defines the glossiness of the material.
    //// The glossiness is in fact the exponent of the specular (phong) lighting 
    //// equation. Usually there is a conversion function defined to map the linear
    //// color values in the texture to a suitable exponent. Have fun.
    //aiTextureType_SHININESS,

    //// The texture defines per-pixel opacity. Usually 'white' means opaque and 
    //// 'black' means 'transparency'. Or quite the opposite. Have fun.
    //aiTextureType_OPACITY,

    //// Displacement texture: The exact purpose and format is application-dependent. 
    //// Higher color values stand for higher vertex displacements.
    //aiTextureType_DISPLACEMENT,

    //// Lightmap texture: (aka Ambient Occlusion) Both 'Lightmaps' and dedicated 
    //// 'ambient occlusion maps' are covered by this material property. The texture contains 
    //// a scaling value for the final color value of a pixel. Its intensity is not affected by 
    //// incoming light.
    //aiTextureType_LIGHTMAP,

    //// Reflection texture: Contains the color of a perfect mirror reflection. 
    //// Rarely used, almost never for real-time applications.
    //aiTextureType_REFLECTION
  };


  std::string defaultTexture{ "white.png" };

  for (auto type : textureTypesSupported)
  {
    aiString aiTextureName;
    material->GetTexture(type, 0, &aiTextureName);

    std::string textureName;

    if (0 != aiTextureName.length)
    {
      textureName = aiTextureName.C_Str();

      if (aiTextureType_NORMALS == type)
      {
        uboMaterial.mUsesNormalTexture = 1;
      }
    }
    else
    {
      textureName = defaultTexture;
    }

    mData.mTextureData.emplace_back(textureName, TextureViewType::e2D, ToYTE(type));
    
    aRenderer->CreateTexture(textureName);
  }

  mData.mShaderSetName = "Phong";

  // get the vertex data with bones (if provided)
  for (unsigned int j = 0; j < aMesh->mNumVertices; j++)
  {
    const aiVector3D* pPos = aMesh->mVertices + j;
    const aiVector3D* pNormal = aMesh->mNormals + j;
    const aiVector3D* pTexCoord = &Zero3D;
    const aiVector3D* pTangent = &Zero3D;
    const aiVector3D* pBiTangent = &Zero3D;

    if (aMesh->HasTextureCoords(0))
    {
      pTexCoord = aMesh->mTextureCoords[0] + j;
    }

    if (aMesh->HasTangentsAndBitangents())
    {
      pTangent = aMesh->mTangents + j;
      pBiTangent = aMesh->mBitangents + j;
    }

    auto position = ToGlm(pPos);

    // NOTE: We do this to invert the uvs to what the texture would expect.
    auto textureCoordinates = glm::vec3{ pTexCoord->x,
                                         1.0f - pTexCoord->y,
                                         pTexCoord->z };

    auto normal = ToGlm(pNormal);
    auto color = glm::vec4{ ToGlm(&pColor), 1.0f };
    auto tangent = ToGlm(pTangent);
    auto binormal = glm::cross(tangent, normal);
    auto bitangent = ToGlm(pBiTangent);

    glm::vec3 boneWeights;
    glm::vec2 boneWeights2;
    glm::ivec3 boneIDs;
    glm::ivec2 boneIDs2;

    // no bones, so default weights
    boneWeights = glm::vec3(0.0f, 0.0f, 0.0f);
    boneWeights2 = glm::vec2(0.0f, 0.0f);
    boneIDs = glm::ivec3(0, 0, 0);
    boneIDs2 = glm::ivec2(0, 0);

    mData.mVertexData.mPositionData.emplace_back(position);
    mData.mVertexData.mTextureCoordinatesData.emplace_back(textureCoordinates);
    mData.mVertexData.mNormalData.emplace_back(normal);
    mData.mVertexData.mColorData.emplace_back(color);
    mData.mVertexData.mTangentData.emplace_back(tangent);
    mData.mVertexData.mBinormalData.emplace_back(binormal);
    mData.mVertexData.mBitangentData.emplace_back(bitangent);

    mData.mInitialTextureCoordinates.emplace_back(textureCoordinates);
  }

  uint32_t indexBase = static_cast<uint32_t>(mData.mIndexData.size());

  for (unsigned int j = 0; j < aMesh->mNumFaces; j++)
  {
    const aiFace& Face = aMesh->mFaces[j];

    if (Face.mNumIndices != 3)
    {
      continue;
    }

    mData.mIndexData.push_back(indexBase + Face.mIndices[0]);
    mData.mIndexData.push_back(indexBase + Face.mIndices[1]);
    mData.mIndexData.push_back(indexBase + Face.mIndices[2]);
  }

  Initialize();

  UpdateGPUVertexData();
  mIndexBuffer.Update(mData.mIndexData);

  // Index buffer must be divisible by 3.
  assert((mData.mIndexData.size() % 3) == 0);
}

Submesh::Submesh(SubmeshData&& aRightData)
  : mData{ std::move(aRightData) }
{
  Initialize();

  UpdateGPUVertexData();
  mIndexBuffer.Update(mData.mIndexData);
}

Submesh::Submesh(Submesh&& aRight)
  : mVertexBufferData{ std::move(aRight.mVertexBufferData) }
  , mIndexBuffer{ std::move(aRight.mIndexBuffer) }
  , mData{ std::move(aRight.mData) }
{
}

Submesh& Submesh::operator=(Submesh&& aRight)
{
  mVertexBufferData = std::move(aRight.mVertexBufferData);
  mIndexBuffer = std::move(aRight.mIndexBuffer);
  mData = std::move(aRight.mData);

  return *this;
}

void Submesh::Initialize()
{
  CreateGPUBuffers();
}


void Submesh::UpdateGPUVertexData()
{
  mVertexBufferData.mPositionBuffer.Update(mData.mVertexData.mPositionData);
  mVertexBufferData.mTextureCoordinatesBuffer.Update(mData.mVertexData.mTextureCoordinatesData);
  mVertexBufferData.mNormalBuffer.Update(mData.mVertexData.mNormalData);
  mVertexBufferData.mColorBuffer.Update(mData.mVertexData.mColorData);
  mVertexBufferData.mTangentBuffer.Update(mData.mVertexData.mTangentData);
  mVertexBufferData.mBinormalBuffer.Update(mData.mVertexData.mBinormalData);
  mVertexBufferData.mBitangentBuffer.Update(mData.mVertexData.mBitangentData);
}


ShaderDescriptions const& Submesh::CreateShaderDescriptions()
{
  auto& descriptions = mData.mDescriptions;

  if (mData.mDescriptionOverride)
  {
    return descriptions;
  }

  auto addUBO = [&descriptions](char const* aName, DescriptorType aDescriptorType, ShaderStageFlags aStage, size_t aBufferSize, size_t aBufferOffset = 0)
  {
    descriptions.AddPreludeLine(fmt::format("#define UBO_{}_BINDING {}", aName, descriptions.GetBufferBinding()));
    descriptions.AddDescriptor(aDescriptorType, aStage, aBufferSize, aBufferOffset);
  };

  addUBO("VIEW", DescriptorType::UniformBuffer, ShaderStageFlags::Vertex, sizeof(UBOs::View));
  addUBO("MODEL_MATERIAL", DescriptorType::UniformBuffer, ShaderStageFlags::Fragment, sizeof(UBOs::Material));
  addUBO("SUBMESH_MATERIAL", DescriptorType::UniformBuffer, ShaderStageFlags::Fragment, sizeof(UBOs::Material));
  addUBO("LIGHTS", DescriptorType::UniformBuffer, ShaderStageFlags::Fragment, sizeof(UBOs::LightManager));
  addUBO("ILLUMINATION", DescriptorType::UniformBuffer, ShaderStageFlags::Fragment, sizeof(UBOs::Illumination));
  addUBO("MODEL", DescriptorType::UniformBuffer, ShaderStageFlags::Vertex, sizeof(UBOs::Model));


  // Descriptions for the textures we support based on which maps we found above:
  for (auto sampler : mData.mTextureData)
  {
    descriptions.AddPreludeLine(fmt::format("#define UBO_{}_BINDING {}", SubmeshData::ToShaderString(sampler.mSamplerType), descriptions.GetBufferBinding()));
    descriptions.AddDescriptor(DescriptorType::CombinedImageSampler, ShaderStageFlags::Fragment, ImageLayout::ShaderReadOnlyOptimal);
  }

  descriptions.AddBindingAndAttribute<glm::vec3>(VertexInputRate::Vertex, VertexFormat::R32G32B32Sfloat);    //glm::vec3 mPosition;
  descriptions.AddBindingAndAttribute<glm::vec3>(VertexInputRate::Vertex, VertexFormat::R32G32B32Sfloat);    //glm::vec3 mTextureCoordinates;
  descriptions.AddBindingAndAttribute<glm::vec3>(VertexInputRate::Vertex, VertexFormat::R32G32B32Sfloat);    //glm::vec3 mNormal;
  descriptions.AddBindingAndAttribute<glm::vec4>(VertexInputRate::Vertex, VertexFormat::R32G32B32A32Sfloat); //glm::vec4 mColor;
  descriptions.AddBindingAndAttribute<glm::vec3>(VertexInputRate::Vertex, VertexFormat::R32G32B32Sfloat);    //glm::vec3 mTangent;
  descriptions.AddBindingAndAttribute<glm::vec3>(VertexInputRate::Vertex, VertexFormat::R32G32B32Sfloat);    //glm::vec3 mBinormal;
  descriptions.AddBindingAndAttribute<glm::vec3>(VertexInputRate::Vertex, VertexFormat::R32G32B32Sfloat);    //glm::vec3 mBitangent;
  descriptions.AddBindingAndAttribute<glm::vec3>(VertexInputRate::Vertex, VertexFormat::R32G32B32Sfloat);    //glm::vec4 mBoneWeights;
  descriptions.AddBindingAndAttribute<glm::vec2>(VertexInputRate::Vertex, VertexFormat::R32G32Sfloat);       //glm::vec2 mBoneWeights2;
  descriptions.AddBindingAndAttribute<glm::ivec3>(VertexInputRate::Vertex, VertexFormat::R32G32B32Sint);     //glm::ivec4 mBoneIDs;
  descriptions.AddBindingAndAttribute<glm::ivec2>(VertexInputRate::Vertex, VertexFormat::R32G32Sint);        //glm::ivec4 mBoneIDs;

  return descriptions;
}

template <typename T>
GPUBuffer<T> CreateBuffer(GPUAllocator* aAllocator, size_t aSize)
{
  return aAllocator->CreateBuffer<T>(aSize,
    GPUAllocation::BufferUsage::TransferDst |
    GPUAllocation::BufferUsage::VertexBuffer,
    GPUAllocation::MemoryProperty::DeviceLocal);
}

void Submesh::CreateGPUBuffers()
{
  auto allocator = mData.mMesh->mRenderer->GetAllocator(AllocatorTypes::Mesh);

  // Create Vertex and Index buffers.
  mVertexBufferData.mPositionBuffer = CreateBuffer<glm::vec3>(allocator, mData.mVertexData.mPositionData.size());
  mVertexBufferData.mTextureCoordinatesBuffer = CreateBuffer<glm::vec3>(allocator, mData.mVertexData.mTextureCoordinatesData.size());
  mVertexBufferData.mNormalBuffer = CreateBuffer<glm::vec3>(allocator, mData.mVertexData.mNormalData.size());
  mVertexBufferData.mColorBuffer = CreateBuffer<glm::vec4>(allocator, mData.mVertexData.mColorData.size());
  mVertexBufferData.mTangentBuffer = CreateBuffer<glm::vec3>(allocator, mData.mVertexData.mTangentData.size());
  mVertexBufferData.mBinormalBuffer = CreateBuffer<glm::vec3>(allocator, mData.mVertexData.mBinormalData.size());
  mVertexBufferData.mBitangentBuffer = CreateBuffer<glm::vec3>(allocator, mData.mVertexData.mBitangentData.size());

  mIndexBuffer = allocator->CreateBuffer<uint32_t>(mData.mIndexData.size(),
    GPUAllocation::BufferUsage::TransferDst |
    GPUAllocation::BufferUsage::IndexBuffer,
    GPUAllocation::MemoryProperty::DeviceLocal);
}

void Submesh::UpdatePositionBuffer(std::vector<glm::vec3> const& aData)
{
  mData.mVertexData.mPositionData = aData;
  mVertexBufferData.mPositionBuffer.Update(mData.mVertexData.mPositionData);
}

void Submesh::UpdateTextureCoordinatesBuffer(std::vector<glm::vec3> const& aData)
{
  mData.mVertexData.mTextureCoordinatesData = aData;
  mVertexBufferData.mTextureCoordinatesBuffer.Update(mData.mVertexData.mTextureCoordinatesData);
}

void Submesh::UpdateNormalBuffer(std::vector<glm::vec3> const& aData)
{
  mData.mVertexData.mNormalData = aData;
  mVertexBufferData.mNormalBuffer.Update(mData.mVertexData.mNormalData);
}

void Submesh::UpdateColorBuffer(std::vector<glm::vec4> const& aData)
{
  mData.mVertexData.mColorData = aData;
  mVertexBufferData.mColorBuffer.Update(mData.mVertexData.mColorData);
}

void Submesh::UpdateTangentBuffer(std::vector<glm::vec3> const& aData)
{
  mData.mVertexData.mTangentData = aData;
  mVertexBufferData.mTangentBuffer.Update(mData.mVertexData.mTangentData);
}

void Submesh::UpdateBinormalBuffer(std::vector<glm::vec3> const& aData)
{
  mData.mVertexData.mBinormalData = aData;
  mVertexBufferData.mBinormalBuffer.Update(mData.mVertexData.mBinormalData);
}

void Submesh::UpdateBitangentBuffer(std::vector<glm::vec3> const& aData)
{
  mData.mVertexData.mBitangentData = aData;
  mVertexBufferData.mBitangentBuffer.Update(mData.mVertexData.mBitangentData);
}

//////////////////////////////////////////////////////////////////////////////
// Mesh
//////////////////////////////////////////////////////////////////////////////
Mesh::Mesh()
{

}

Mesh::Mesh(Renderer* aRenderer,
  const std::string& aFile)
  : mRenderer{ aRenderer }
  , mInstanced(false)
{
  Assimp::Importer Importer;

  //std::string filename = aFile; // TODO: don't actually make a copy lol
  //std::string meshFile;
  //
  //// check that the mesh file exists in the game assets folder
  //bool success = FileCheck(Path::GetGamePath(), "Models", filename);
  //
  //if (success)
  //{
  //  // if so, get the model path
  //  meshFile = Path::GetModelPath(Path::GetGamePath(), aFile);
  //}
  //else
  //{
  //  // otherwise, it's not in the game assets, so check the engine assets folder
  //  success = FileCheck(Path::GetEnginePath(), "Models", filename);
  //
  //  if (success)
  //  {
  //    // if it's in the engine assets, get the path
  //    meshFile = Path::GetModelPath(Path::GetEnginePath(), aFile);
  //  }
  //  else
  //  {
  //    // otherwise throw an error
  //    throw "Mesh does not exist.";
  //  }
  //}

  auto meshScene = Importer.ReadFile(aFile.c_str(),
    aiProcess_Triangulate |
    aiProcess_CalcTangentSpace |
    aiProcess_GenSmoothNormals |
    aiProcess_PreTransformVertices);

  if (meshScene)
  {
    mParts.clear();
    mParts.reserve(meshScene->mNumMeshes);

    //printf("Mesh FileName: %s\n", aFile.c_str());

    uint32_t numMeshes = meshScene->mNumMeshes;

    // Load Mesh
    uint32_t startingVertex = 0;
    for (uint32_t i = 0; i < numMeshes; i++)
    {
      mParts.emplace_back(aRenderer,
        this,
        meshScene,
        meshScene->mMeshes[i],
        startingVertex);
      startingVertex += meshScene->mMeshes[i]->mNumVertices;
    }

    mName = aFile;
  }
}

Mesh::Mesh(Renderer* aRenderer,
  std::string const& aFile,
  ContiguousRange<SubmeshData> aSubmeshes)
  : mRenderer{ aRenderer }
  , mInstanced(false)
{
  mName = aFile;

  mParts.reserve(aSubmeshes.size());

  for (auto& submeshData : aSubmeshes)
  {
    submeshData.mMesh = this;
    auto& submesh = mParts.emplace_back(std::move(submeshData));
  }
}

Mesh::~Mesh()
{

}

std::vector<Submesh>& Mesh::GetSubmeshes()
{
  return mParts;
}
