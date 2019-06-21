# Pretending this is an abstract renderer.
add_library(Renderer STATIC)

add_subdirectory(DX12)

target_sources(Renderer 
PRIVATE
    Renderer.cpp
    Renderer.hpp
)

target_compile_features(Renderer PUBLIC cxx_std_17)

target_link_libraries(Renderer 
PUBLIC
    DirectXTex
    glm
    assimp
    d3d12.lib
    dxgi.lib
    dxguid.lib
)

target_include_directories(Renderer PRIVATE ../)
