# SafeSound CMake configuration

CMAKE_MINIMUM_REQUIRED(VERSION 3.10)
PROJECT(SafeSound C)

set(LIB_DIR ${CMAKE_SOURCE_DIR}/lib)

include_directories(
	${PROJECT_SOURCE_DIR}/inc
	${PROJECT_SOURCE_DIR}/src
)

# find all include and source files
file(GLOB all_SRCS
	"${PROJECT_SOURCE_DIR}/inc/*.h"
	"${PROJECT_SOURCE_DIR}/src/*.c"
)

# Import .o files as object libraries
add_library(classifier OBJECT IMPORTED GLOBAL)
set_property(TARGET classifier PROPERTY IMPORTED_OBJECTS ${LIB_DIR}/classifier.o)
add_library(featurizer OBJECT IMPORTED GLOBAL)
set_property(TARGET featurizer PROPERTY IMPORTED_OBJECTS ${LIB_DIR}/featurizer.o)

# Create executable
ADD_EXECUTABLE(${PROJECT_NAME} ${all_SRCS} $<TARGET_OBJECTS:featurizer> $<TARGET_OBJECTS:classifier>)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${AZURE_SPHERE_API_SET_DIR}/usr/include/azureiot)
TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PUBLIC AZURE_IOT_HUB_CONFIGURED)
TARGET_LINK_LIBRARIES(${PROJECT_NAME} m azureiot applibs pthread gcc_s c)

# Add MakeImage post-build command
INCLUDE("${AZURE_SPHERE_MAKE_IMAGE_FILE}")