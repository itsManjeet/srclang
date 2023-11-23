function(add_module TARGET_NAME)
    add_library(${TARGET_NAME} SHARED ${ARGN})
    target_link_libraries(${TARGET_NAME} ${PROJECT_NAME}_LIB)
    set_target_properties(${PROJECT_NAME}_LIB PROPERTIES
        PREFIX ""
        OUTPUT_NAME "${PROJECT_NAME}"
        SUFFIX ".so")
endfunction(add_module)

add_module (io io.cxx)