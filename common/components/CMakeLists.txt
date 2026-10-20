add_library(rtype-components INTERFACE)
add_library(rtype::components ALIAS rtype-components)
target_include_directories(rtype-components INTERFACE ${PROJECT_SOURCE_DIR})
target_compile_features(rtype-components INTERFACE cxx_std_20)
