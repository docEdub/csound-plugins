option(BUILD_WEBSOCKET_OPCODE "Build the websocket opcode" ON)
FIND_PATH (WEBSOCKETS_H libwebsockets.h /usr/local/include /usr/include)

FIND_LIBRARY(websockets_library NAMES websockets)

check_deps(BUILD_WEBSOCKET_OPCODE websockets_library WEBSOCKETS_H)

if(BUILD_WEBSOCKET_OPCODE)
    # This gets breakpoints working for C++.
    #set(CMAKE_CXX_FLAGS -g)

    # NB: Ending the plugin name with a number makes lldb unable to set breakpoints. I don't know why.
    make_plugin(websocket2IO websocket_all.c ${websockets_library})
        message(STATUS "WEBSOCKETS LIB: ${websockets_library}")
        target_include_directories(websocket2IO PRIVATE ${CSOUND_INCLUDE_DIRS})
        target_include_directories(websocket2IO PRIVATE ${WEBSOCKETS_H})
        if(APPLE)
            target_include_directories(websocket2IO PRIVATE "/opt/homebrew/Cellar/openssl@3/3.1.0/include")
        endif()
endif()
