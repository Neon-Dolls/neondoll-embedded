idf_component_register(SRCS "main.cpp" "../../bind_probe.cpp" REQUIRES neondoll json driver vfs lwip mbedtls)
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)
