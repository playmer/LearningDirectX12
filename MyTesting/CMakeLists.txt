add_executable(MyTesting)

target_sources(MyTesting PRIVATE main.cpp)

target_link_libraries(MyTesting PRIVATE Renderer)

target_compile_features(MyTesting PUBLIC cxx_std_17)

target_include_directories(MyTesting PRIVATE ../)