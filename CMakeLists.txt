cmake_minimum_required(VERSION 3.22)
project(Airport_Simu VERSION 0.1 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Debug")
endif ()

option(BUILD_TESTING "Enable unit tests" ON)

find_package(Threads REQUIRED)

# ---- Sanitizers -------------------------------------------------------------------
# ASan and TSan are mutually exclusive; UBSan may ride along with either.
set(SIM_SANITIZER "none" CACHE STRING "none | address | thread | undefined | address,undefined")
set_property(CACHE SIM_SANITIZER PROPERTY STRINGS none address thread undefined address,undefined)

if (SIM_SANITIZER STREQUAL "none")
	message(STATUS "Sanitizers: disabled")
else ()
	message(STATUS "Sanitizers: ${SIM_SANITIZER}")
	add_compile_options(-g -fno-omit-frame-pointer -fsanitize=${SIM_SANITIZER})
	add_link_options(-fsanitize=${SIM_SANITIZER})
endif ()

# ---- Valgrind ---------------------------------------------------------------------
# Runs every ctest target under valgrind instead of directly. Cannot be combined
# with a sanitizer build (valgrind refuses to run instrumented binaries).
option(SIM_VALGRIND "Run ctest targets under valgrind" OFF)
set(SIM_VALGRIND_TOOL "memcheck" CACHE STRING "memcheck | helgrind | drd")

if (SIM_VALGRIND)
	if (NOT SIM_SANITIZER STREQUAL "none")
		message(FATAL_ERROR "SIM_VALGRIND=ON cannot be combined with SIM_SANITIZER=${SIM_SANITIZER}")
	endif ()
	find_program(VALGRIND_EXECUTABLE valgrind REQUIRED)
	message(STATUS "Valgrind: ${SIM_VALGRIND_TOOL}")
endif ()

set(SIM_TEST_TIMEOUT 60 CACHE STRING "Per-test timeout in seconds")
# Extra environment for every test, e.g. "LD_PRELOAD=" to neutralise a
# session-wide LD_PRELOAD that would otherwise load before the sanitizer runtime.
set(SIM_TEST_ENV "" CACHE STRING "Extra environment for test invocations")
# Optional command prefix for every test, e.g. "setarch;-R" for TSan on kernels
# with 32 bits of mmap randomness.
set(SIM_TEST_WRAPPER "" CACHE STRING "Command prefix for test invocations")

# ---- Warnings ---------------------------------------------------------------------
add_library(sim_warnings INTERFACE)
target_compile_options(sim_warnings INTERFACE
	-Wall -Wextra -Wpedantic
	-Wshadow -Wconversion -Wsign-conversion
	-Wcast-align -Wcast-qual -Wwrite-strings
	-Wundef -Wpointer-arith
	-Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations
	-Wold-style-definition -Wredundant-decls -Wnested-externs
	-Wswitch-enum -Wswitch-default
	-Wformat=2 -Wformat-security
	-Wnull-dereference -Wvla
	-Wdouble-promotion -Wfloat-equal
	-Wstrict-overflow=2 -Waggregate-return
	-Werror=implicit-function-declaration
)

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
	target_compile_options(sim_warnings INTERFACE
		-Wduplicated-cond -Wduplicated-branches -Wlogical-op
		-Wjump-misses-init -Wstack-usage=1024
	)
endif ()

# ---- Targets ----------------------------------------------------------------------
if (BUILD_TESTING)
	enable_testing()
	add_subdirectory(tests)
else ()
	# Application target goes here once src/ grows past the queue module, e.g.
	#   add_executable(${PROJECT_NAME} src/main.c src/queue.c)
	#   target_link_libraries(${PROJECT_NAME} PRIVATE sim_warnings Threads::Threads)
	message(STATUS "BUILD_TESTING=OFF: no application target defined yet")
endif ()
