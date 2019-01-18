cmake_minimum_required(VERSION 2.8)

find_package(LibXml2 REQUIRED)
find_package(ZLIB REQUIRED)
find_package(CURL REQUIRED)

include_directories(include)

file(GLOB_RECURSE libdash_source *.cpp)
add_library(dash SHARED ${libdash_source})
target_link_libraries(dash ${CURL_LIBRARIES} ${ZLIB_LIBRARIES} ${LIBXML2_LIBRARIES})

install(TARGETS dash DESTINATION ${CMAKE_INSTALL_PREFIX}/lib PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_READ WORLD_EXECUTE)
file(GLOB_RECURSE headers include/*.h)
install(FILES ${headers} DESTINATION ${CMAKE_INSTALL_PREFIX}/include/libdash)
