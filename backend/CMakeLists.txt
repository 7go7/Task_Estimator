cmake_minimum_required(VERSION 3.16)
project(task_estimator_backend)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)

include_directories(${PQXX_INCLUDE_DIRS} /usr/local/include /app/include /app)
link_directories(${PQXX_LIBRARY_DIRS} /usr/local/lib)

add_executable(task_estimator_backend
    src/main.cpp
)

target_link_libraries(task_estimator_backend
    ${PQXX_LIBRARIES}
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    xgboost
)
