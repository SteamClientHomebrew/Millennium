find_program(WINDRES windres)
if(WINDRES)
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/version.o
        COMMAND ${WINDRES} -i ${MILLENNIUM_BASE}/scripts/version.rc -o ${CMAKE_BINARY_DIR}/version.o
        DEPENDS ${MILLENNIUM_BASE}/scripts/version.rc
    )
    add_custom_target(resource DEPENDS ${CMAKE_BINARY_DIR}/version.o)
    add_dependencies(Millennium resource)
    target_link_libraries(Millennium ${CMAKE_BINARY_DIR}/version.o)
endif()
