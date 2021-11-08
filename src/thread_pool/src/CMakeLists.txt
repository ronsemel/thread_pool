set(TARGETNAME thread_pool) 

set(SRC thread_pool.cpp) 

find_package(Boost)

add_library(${TARGETNAME} STATIC ${SRC}) 

target_include_directories(${TARGETNAME} PUBLIC  
../include
${Boost_INCLUDE_DIRS}
)

target_link_libraries(${TARGETNAME} PRIVATE
Boost::headers
)