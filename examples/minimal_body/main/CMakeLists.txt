idf_component_register(SRCS "main.cpp"
                     REQUIRES neondoll
                     )

set_target_properties(${COMPONENT_LIB} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS YES)
