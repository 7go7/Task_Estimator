cmake_minimum_required(VERSION 3.14)
project(TaskBackend)

set(CMAKE_CXX_STANDARD 17)

# Знаходимо пакети
find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)

# Підключаємо XGBoost (або вкажи шлях вручну, якщо не знайде)
find_library(XGBOOST_LIB xgboost)

add_executable(TaskBackend main.cpp)

# Лінкування
target_link_libraries(TaskBackend PRIVATE ${PQXX_LIBRARIES} ${XGBOOST_LIB} curl pthread)
target_include_directories(TaskBackend PRIVATE ${PQXX_INCLUDE_DIRS} /usr/local/include)