cmake_minimum_required(VERSION 3.16)
project(InstallService LANGUAGES CXX)

# -----------------------------
# 1) Исходники приложения
# -----------------------------
# Инициализация переменной для хранения списка исходных файлов
set(APP_SOURCES)

# Все .cpp внутри папки InstallService (и её подпапок, если они есть)
file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/InstallService/*.cpp"
)

# Создание исполняемого файла
add_executable(InstallService ${APP_SOURCES})
  
# Установка стандарта C++23
set_target_properties(InstallService PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# -----------------------------
# 2) Include-пути
# -----------------------------
target_include_directories(InstallService PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# -----------------------------
# 3) glog 
# -----------------------------
set(GLOG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/glog")

# Добавить include-папку glog
target_include_directories(InstallService PRIVATE
    "${GLOG_ROOT}/include"
)
target_compile_definitions(InstallService PRIVATE
  GLOG_NO_ABBREVIATED_SEVERITIES
  GLOG_CUSTOM_PREFIX_SUPPORT
)

# линковка с библиотеками
target_link_libraries(InstallService PRIVATE
  "${GLOG_ROOT}/lib/glog.lib"
  shell32
)

# копирование glog.dll в папку с бинарниками CMake
add_custom_command(TARGET InstallService POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${GLOG_ROOT}/bin/glog.dll"
        $<TARGET_FILE_DIR:InstallService>
)
# Включение горячей перезагрузки для компиляторов MSVC, если поддерживается.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()
