add_library(component STATIC)
add_library(robot::component ALIAS component)

target_sources(component PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/comp_utils.c
)

target_include_directories(component PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_features(component PUBLIC c_std_11)
