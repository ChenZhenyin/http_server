PROJECT(HTTP_SERVER)

INCLUDE_DIRECTORIES(include include/threadpool include/fdwrapper include/Task)

ADD_SUBDIRECTORY(include/threadpool bin)
ADD_SUBDIRECTORY(include/fdwrapper bin)
ADD_SUBDIRECTORY(include/Task bin)
ADD_SUBDIRECTORY(src bin)
