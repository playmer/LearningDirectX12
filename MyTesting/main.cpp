#include "glm/glm.hpp"

#include "DX12Renderer/DX12/DX12Renderer.hpp"

struct VertexPosColor
{
  glm::vec3 Position;
  glm::vec3 Color;
};

int main()
{
  Dx12Renderer renderer;

  std::vector<VertexPosColor> verts = {
    { glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(0.0f, 0.0f, 0.0f) }, // 0
    { glm::vec3(-1.0f,  1.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f) }, // 1
    { glm::vec3(1.0f,  1.0f, -1.0f),  glm::vec3(1.0f, 1.0f, 0.0f) }, // 2
    { glm::vec3(1.0f, -1.0f, -1.0f),  glm::vec3(1.0f, 0.0f, 0.0f) }, // 3
    { glm::vec3(-1.0f, -1.0f,  1.0f), glm::vec3(0.0f, 0.0f, 1.0f) }, // 4
    { glm::vec3(-1.0f,  1.0f,  1.0f), glm::vec3(0.0f, 1.0f, 1.0f) }, // 5
    { glm::vec3(1.0f,  1.0f,  1.0f),  glm::vec3(1.0f, 1.0f, 1.0f) }, // 6
    { glm::vec3(1.0f, -1.0f,  1.0f),  glm::vec3(1.0f, 0.0f, 1.0f) }  // 7
  };
  
  auto allocator = renderer.MakeAllocator(AllocatorTypes::Mesh, 1024 * 1024);
  auto buffer = allocator->CreateBuffer<VertexPosColor>(
    verts.size() * sizeof(VertexPosColor), 
    GPUAllocation::BufferUsage::VertexBuffer, 
    GPUAllocation::MemoryProperty::DeviceLocal);

  buffer.Update(verts);

  renderer.UpdateWindow();

  return 0;
}