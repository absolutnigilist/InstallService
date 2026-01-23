# Используемая минимальная версия CMake и объявление проекта
cmake_minimum_required(VERSION 3.16)
project(InstallService LANGUAGES CXX)

# Добавление исполняемого файла и его исходных файлов
add_executable (InstallService 
"adminChecker.cpp"
"createService.cpp"
"logging.cpp"
"main.cpp"
"pathUtils.cpp"
)
# Установка стандарта C++
if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET InstallService PROPERTY CXX_STANDARD 20)
endif()


