set(_calculate_sources
    ${CMAKE_CURRENT_LIST_DIR}/chassis_control_dr16/chassis_control.c
    
)
target_sources(${PROJECT_NAME} PRIVATE ${_calculate_sources})
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/chassis_control_dr16

)
