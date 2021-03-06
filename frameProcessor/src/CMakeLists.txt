
set(CMAKE_INCLUDE_CURRENT_DIR on)
ADD_DEFINITIONS(-DBOOST_TEST_DYN_LINK)

include_directories(${FRAMEPROCESSOR_DIR}/include ${HDF5_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS} ${LOG4CXX_INCLUDE_DIRS}/.. ${ZEROMQ_INCLUDE_DIRS})
add_definitions(${HDF5_DEFINITIONS})

file(GLOB LIB_SOURCES DataBlock.cpp
                      DataBlockPool.cpp
                      FrameProcessorPlugin.cpp
                      Frame.cpp
                      MetaMessage.cpp
                      MetaMessagePublisher.cpp
                      IFrameCallback.cpp )

# Add library for common plugin code
add_library(${LIB_PROCESSOR} SHARED ${LIB_SOURCES})
target_link_libraries(${LIB_PROCESSOR} ${Boost_LIBRARIES} ${LOG4CXX_LIBRARIES} ${ZEROMQ_LIBRARIES} ${COMMON_LIBRARY})
install(TARGETS ${LIB_PROCESSOR} DESTINATION lib)

file(GLOB APP_SOURCES FrameProcessorController.cpp
                      SharedMemoryController.cpp
                      SharedMemoryParser.cpp
                      SocketHandler.cpp )

add_executable(frameProcessor ${APP_SOURCES} FrameProcessorApp.cpp)

message(STATUS "HDF5 include files:  " ${HDF5_INCLUDE_DIRS})
message(STATUS "HDF5 libs:           " ${HDF5_LIBRARIES} ${HDF5HL_LIBRARIES})
message(STATUS "HDF5 defs:           " ${HDF5_DEFINITIONS})

target_link_libraries(frameProcessor ${LIB_PROCESSOR} ${Boost_LIBRARIES} ${LOG4CXX_LIBRARIES} ${ZEROMQ_LIBRARIES} ${HDF5_LIBRARIES} ${HDF5HL_LIBRARIES} ${COMMON_LIBRARY})
if ( ${CMAKE_SYSTEM_NAME} MATCHES Linux )
    find_library(PTHREAD_LIBRARY
             NAMES pthread)
    target_link_libraries(frameProcessor ${PTHREAD_LIBRARY} )
endif()

install(TARGETS frameProcessor RUNTIME DESTINATION bin)

# Add library for dummy plugin
add_library(DummyPlugin SHARED DummyPlugin.cpp)
target_link_libraries(DummyPlugin ${LIB_PROCESSOR} ${Boost_LIBRARIES} ${LOG4CXX_LIBRARIES} ${ZEROMQ_LIBRARIES} ${HDF5_LIBRARIES} ${HDF5HL_LIBRARIES} ${COMMON_LIBRARY})
install(TARGETS DummyPlugin DESTINATION lib)

# Add library for HDF5 writer plugin
add_library(Hdf5Plugin SHARED FileWriterPlugin.cpp FileWriterPluginLib.cpp HDF5File.cpp Acquisition.cpp)
target_link_libraries(Hdf5Plugin ${LIB_PROCESSOR} ${Boost_LIBRARIES} ${LOG4CXX_LIBRARIES} ${ZEROMQ_LIBRARIES} ${HDF5_LIBRARIES} ${HDF5HL_LIBRARIES} ${COMMON_LIBRARY})
install(TARGETS Hdf5Plugin DESTINATION lib)

# Add library for UIDAdjustment plugin
add_library(UIDAdjustmentPlugin SHARED UIDAdjustmentPlugin.cpp UIDAdjustmentPluginLib.cpp)
target_link_libraries(UIDAdjustmentPlugin ${LIB_PROCESSOR} ${Boost_LIBRARIES} ${LOG4CXX_LIBRARIES} ${ZEROMQ_LIBRARIES} ${HDF5_LIBRARIES} ${HDF5HL_LIBRARIES} ${COMMON_LIBRARY})
install(TARGETS UIDAdjustmentPlugin DESTINATION lib)

# Add library for OffsetAdjustment plugin
add_library( OffsetAdjustmentPlugin SHARED  OffsetAdjustmentPlugin.cpp  OffsetAdjustmentPluginLib.cpp)
target_link_libraries(OffsetAdjustmentPlugin ${LIB_PROCESSOR} ${Boost_LIBRARIES} ${LOG4CXX_LIBRARIES} ${ZEROMQ_LIBRARIES} ${HDF5_LIBRARIES} ${HDF5HL_LIBRARIES} ${COMMON_LIBRARY})
install(TARGETS OffsetAdjustmentPlugin DESTINATION lib)

# Add library for LiveView plugin
add_library(LiveViewPlugin SHARED LiveViewPlugin.cpp LiveViewPluginLib.cpp)
target_link_libraries(LiveViewPlugin ${LIB_PROCESSOR} ${Boost_LIBRARIES} ${LOG4CXX_LIBRARIES} ${ZEROMQ_LIBRARIES} ${COMMON_LIBRARY})
install(TARGETS LiveViewPlugin DESTINATION lib)

# Add library for Sum plugin
add_library(SumPlugin SHARED SumPlugin.cpp SumPluginLib.cpp)
target_link_libraries(SumPlugin ${LIB_PROCESSOR} ${Boost_LIBRARIES} ${LOG4CXX_LIBRARIES} ${ZEROMQ_LIBRARIES} ${COMMON_LIBRARY})
install(TARGETS SumPlugin DESTINATION lib)

# Add test and project source files to executable
if ( ${CMAKE_SYSTEM_NAME} MATCHES Linux )
  # librt required for timing functions
  find_library(REALTIME_LIBRARY
               NAMES rt)
  target_link_libraries( frameProcessor ${REALTIME_LIBRARY} )
endif()

