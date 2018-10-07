add_executable(server
	main.cpp
	config.cpp
	sockets.cpp
	http.cpp
)

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
#set_property(TARGET server PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
target_compile_features(server PUBLIC cxx_std_17)
