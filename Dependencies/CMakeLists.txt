add_subdirectory(fmt)

##########################################################################
# Turn off/configure various parts of glm.
option(GLM_TEST_ENABLE "Build unit tests" OFF)
option(GLM_INSTALL_ENABLE "Allow installing of the library" OFF)
option(BUILD_SHARED_LIBS "Build shared library" OFF)
option(BUILD_STATIC_LIBS "Build static library" OFF)

add_subdirectory(glm)

target_compile_definitions(glm 
  INTERFACE 
    GLM_FORCE_CXX14 
    GLM_FORCE_SIZE_T_LENGTH 
    GLM_FORCE_DEPTH_ZERO_TO_ONE 
    GLM_ENABLE_EXPERIMENTAL 
    GLM_FORCE_SILENT_WARNINGS
)
