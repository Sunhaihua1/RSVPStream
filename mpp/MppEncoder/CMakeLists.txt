file(GLOB_RECURSE srcs CONFIGURE_DEPENDS src/*.cpp include/*.h)
add_library(mppencoder STATIC ${srcs})
target_include_directories(mppencoder PUBLIC include)
target_link_libraries(mppencoder PUBLIC ${dynamic_libs})
