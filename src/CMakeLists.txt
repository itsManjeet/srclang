file(GLOB_RECURSE SourceFiles "*.cpp" "*.h")
add_executable(${PROJECT_NAME} ${SourceFiles})
target_link_libraries(${PROJECT_NAME} PUBLIC PkgConfig::LIBFFI)