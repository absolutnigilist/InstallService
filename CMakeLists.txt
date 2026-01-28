cmake_minimum_required(VERSION 3.16)
project(InstallService LANGUAGES CXX)

# Создаем исполняемый файл
add_executable(InstallService
  src/main.cpp
  src/Cli.cpp
  src/Installer.cpp
  src/Paths.cpp
  src/Platform.cpp
  src/Process.cpp
 )
# Требуемые стандарты C++
target_compile_features(InstallService PRIVATE cxx_std_20)

# Включаемые директории
target_include_directories(InstallService PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# glog
set(GLOG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/glog")

target_include_directories(InstallService PRIVATE
  "${GLOG_ROOT}/include"
)

target_compile_definitions(InstallService PRIVATE
  GLOG_NO_ABBREVIATED_SEVERITIES
  GLOG_CUSTOM_PREFIX_SUPPORT
)

target_link_libraries(InstallService PRIVATE
  shell32
  "${GLOG_ROOT}/lib/glog.lib"
)

add_custom_command(TARGET InstallService POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${GLOG_ROOT}/bin/glog.dll"
          "$<TARGET_FILE_DIR:InstallService>"
)
# Платформо-зависимые исходники
if (WIN32)
  target_sources(InstallService PRIVATE
    src/platform/PlatformImpl.hpp
    src/platform/windows/PlatformWin.cpp
    src/platform/windows/BackendWinSc.cpp
    src/platform/ProcessImpl.hpp
    src/platform/windows/ProcessWin.cpp
    src/platform/windows/RemoveDirWin.cpp
  )
elseif (UNIX AND NOT APPLE)
  target_sources(InstallService PRIVATE
    src/platform/PlatformImpl.hpp
    src/platform/linux/PlatformLinux.cpp
    src/platform/linux/BackendLinuxSystemd.cpp
    src/platform/ProcessImpl.hpp
    src/platform/linux/ProcessLinux.cpp
    src/platform/linux/RemoveDirLinux.cpp
  )
else()
  message(FATAL_ERROR "Unsupported platform")
endif()
