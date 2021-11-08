set(TARGETNAME ramp_up_unittest)

set(SRC main.cpp ramp_up_unittest.cpp)

find_package(Boost COMPONENTS unit_test_framework system REQUIRED)

add_executable(${TARGETNAME} ${SRC})


target_compile_definitions(${TARGETNAME} PRIVATE
    UNIT_TESTING
)

target_link_libraries(${TARGETNAME} PUBLIC
    Boost::unit_test_framework
    thread_pool
)

add_test(${TARGETNAME} ${TARGETNAME})