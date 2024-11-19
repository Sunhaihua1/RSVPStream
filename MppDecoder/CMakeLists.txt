file(GLOB_RECURSE srcs CONFIGURE_DEPENDS src/*.cpp include/*.h)
add_library(mppdecoder STATIC ${srcs})
target_include_directories(mppdecoder PUBLIC include)
target_link_libraries(mppdecoder PUBLIC ${FFMPEG_LIBS})
