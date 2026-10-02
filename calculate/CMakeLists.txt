set(_calculate_sources
    ${CMAKE_CURRENT_LIST_DIR}/dr16_calculate/chassis_control_dr16/chassis_control.c
    ${CMAKE_CURRENT_LIST_DIR}/dr16_calculate/gimbal_control_dr16/gimbal_control.c

)
target_sources(${PROJECT_NAME} PRIVATE ${_calculate_sources})
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/dr16_calculate/chassis_control_dr16
    ${CMAKE_CURRENT_LIST_DIR}/dr16_calculate/gimbal_control_dr16

)
