PROJECT(MAIN)

ADD_EXECUTABLE(httpServer main.cpp)
TARGET_LINK_LIBRARIES(httpServer task)
TARGET_LINK_LIBRARIES(httpServer fdwrapper)
TARGET_LINK_LIBRARIES(httpServer threadpool)
TARGET_LINK_LIBRARIES(httpServer pthread)
