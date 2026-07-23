cmake_minimum_required(VERSION 3.20)
project(IsaacEmoteWheelFix LANGUAGES CXX)

include(CTest)

if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "Build this project with 32-bit MSVC (-A Win32)")
endif()

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(emote_input_hook SHARED src/hook.cpp)
target_compile_features(emote_input_hook PRIVATE cxx_std_17)
target_compile_definitions(emote_input_hook PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(emote_input_hook PRIVATE /W4 /utf-8 /GR- /GS /guard:cf-)
target_link_libraries(emote_input_hook PRIVATE bcrypt shell32)

add_library(userenv_proxy SHARED src/proxy.cpp userenv.def)
target_compile_features(userenv_proxy PRIVATE cxx_std_17)
target_compile_definitions(userenv_proxy PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(userenv_proxy PRIVATE /W4 /utf-8 /GR- /GS)
target_link_libraries(userenv_proxy PRIVATE shell32)
set_target_properties(userenv_proxy PROPERTIES OUTPUT_NAME "userenv")

add_executable(IsaacEmoteFixManager WIN32 src/manager.cpp)
target_compile_features(IsaacEmoteFixManager PRIVATE cxx_std_17)
target_compile_definitions(IsaacEmoteFixManager PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(IsaacEmoteFixManager PRIVATE /W4 /utf-8 /GR- /GS)
target_link_libraries(IsaacEmoteFixManager PRIVATE bcrypt shell32 comctl32 advapi32)

add_executable(proxy_smoke_test tests/proxy_smoke_test.cpp)
target_compile_features(proxy_smoke_test PRIVATE cxx_std_17)
target_compile_definitions(proxy_smoke_test PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(proxy_smoke_test PRIVATE /W4 /utf-8 /GR- /GS)
add_test(NAME proxy_forwarding COMMAND proxy_smoke_test "$<TARGET_FILE:userenv_proxy>")

set(PACKAGE_DIR "${CMAKE_BINARY_DIR}/package/IsaacEmoteWheelFix")
add_custom_target(package
  COMMAND ${CMAKE_COMMAND} -E make_directory "${PACKAGE_DIR}/payload"
  COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:IsaacEmoteFixManager>" "${PACKAGE_DIR}/IsaacEmoteFixManager.exe"
  COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:userenv_proxy>" "${PACKAGE_DIR}/payload/userenv.proxy.dll"
  COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:emote_input_hook>" "${PACKAGE_DIR}/payload/emote_input_hook.dll"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/README.md" "${PACKAGE_DIR}/README.md"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/TESTING.md" "${PACKAGE_DIR}/TESTING.md"
  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/LICENSE" "${PACKAGE_DIR}/LICENSE"
  COMMAND ${CMAKE_COMMAND} -DPACKAGE_DIR="${PACKAGE_DIR}" -P "${CMAKE_SOURCE_DIR}/cmake/WriteHashes.cmake"
  DEPENDS IsaacEmoteFixManager userenv_proxy emote_input_hook
)
