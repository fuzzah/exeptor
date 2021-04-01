add_executable(exeptor-tests main.cpp test_bdd.cpp)

target_link_libraries(exeptor-tests PRIVATE yaml-cpp dl)
