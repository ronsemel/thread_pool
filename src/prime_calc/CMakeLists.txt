
set(TARGETNAME prime_calc)

add_executable(${TARGETNAME} prime_calc.cpp)

target_link_libraries(${TARGETNAME} PUBLIC thread_pool)
