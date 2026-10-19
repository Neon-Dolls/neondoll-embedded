idf_component_register(SRCS "main.cpp" REQUIRES neondoll json driver vfs lwip mbedtls)
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)
