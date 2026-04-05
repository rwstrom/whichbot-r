cmake_minimum_required(VERSION 3.20)
project(Pathematics CXX)
set(CMAKE_CXX_STANDSRD 23)

add_library(pathematics)

if(MSVC)
    target_compile_options(pathematics PRIVATE /W4 )
else()
    target_link_options(pathematics PRIVATE -m32 )
    target_compile_options(pathematics PUBLIC -Wall -Wextra -Wshadow -m32 -fPIC )
endif()

target_include_directories(pathematics PRIVATE src/cpp)
#engine
target_include_directories(pathematics INTERFACE src/cpp/engine)
target_sources(pathematics PRIVATE
               src/cpp/engine/NodeIdBag.cpp
               src/cpp/engine/PathData.cpp
               src/cpp/engine/PathManager.cpp
                )
#model
target_include_directories(pathematics INTERFACE src/cpp/model)
target_sources(pathematics PRIVATE 
                src/cpp/model/Digraph.cpp
                src/cpp/model/Edge.cpp
                src/cpp/model/Node.cpp
                src/cpp/model/TerrainData.cpp
                )
 #test      
#add_executable(test src/cpp/test/PathematicsTest.cpp)
#target_include_directories(test PRIVATE src/cpp)
#target_link_libraries(test pathematics)