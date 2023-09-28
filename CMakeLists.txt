cmake_minimum_required(VERSION 3.9)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

find_package(Boost 1.78.0 REQUIRED)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_library(MyLib STATIC 
    src/model.h
    src/model.cpp
    src/loot_generator.h
    src/loot_generator.cpp
	src/util/tagged.h 
	src/util/tagged_uuid.h 
	src/util/tagged_uuid.cpp 
)

add_library(collision_detection_lib STATIC
	src/collision_detector.h
	src/collision_detector.cpp
	src/geom.h
)

target_link_libraries(collision_detection_lib PUBLIC CONAN_PKG::boost Threads::Threads)

add_executable(game_server
	src/main.cpp
	src/http_server.cpp
	src/http_server.h 
	src/sdk.h
	src/boost_json.cpp
	src/json_loader.h
	src/json_loader.cpp
	src/log_response.h 
	src/request_handler.cpp
	src/request_handler.h
	src/player.cpp
	src/player.h
	src/application.h
	src/db_connection.h
	src/serialization.h
	src/serialization.cpp 
)

target_include_directories(game_server PRIVATE CONAN_PKG::boost)
target_link_libraries(game_server PRIVATE Threads::Threads CONAN_PKG::boost MyLib  collision_detection_lib CONAN_PKG::libpqxx) 

include(CTest)
include(${CONAN_BUILD_DIRS_CATCH2}/Catch.cmake)

 add_executable(game_server_tests
    tests/model_tests.cpp
    tests/loot_generator_tests.cpp
    tests/collision-detector-tests.cpp
)

target_include_directories(game_server_tests PRIVATE CONAN_PKG::boost CONAN_PKG::catch2)
target_link_libraries(game_server_tests PRIVATE 
    CONAN_PKG::catch2 
    CONAN_PKG::boost 
    Threads::Threads 
    MyLib
    collision_detection_lib)

catch_discover_tests(game_server_tests)
