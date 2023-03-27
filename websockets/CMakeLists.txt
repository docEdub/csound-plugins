option(BUILD_WEBSOCKET_OPCODE "Build the websocket opcode" ON)
FIND_PATH (WEBSOCKETS_H libwebsockets.h /usr/local/include /usr/include)

FIND_LIBRARY(websockets_library NAMES websockets)

if(APPLE)
  # AMF: I hacked this to work with homebrew on macOS.
  # - I installed the `openssl` library with `brew install openssl`.
  # - I also installed the `crypol` library with `brew install cryptol`, but I don't think it's needed.
  # - The `websockets` library was already installed in homebrew. The `openssl` library may have brought it in.
  set(HOMEBREW_OPENSSL_PATH "/opt/homebrew/Cellar/openssl@3/3.1.0")
  FIND_LIBRARY (ssl_library NAMES ssl PATHS "${HOMEBREW_OPENSSL_PATH}/lib")
  FIND_LIBRARY (crypto_library NAMES crypto PATHS "${HOMEBREW_OPENSSL_PATH}/lib")
  check_deps(BUILD_WEBSOCKET_OPCODE websockets_library ssl_library
     crypto_library WEBSOCKETS_H)
else()
  check_deps(BUILD_WEBSOCKET_OPCODE websockets_library WEBSOCKETS_H)
endif()

if(BUILD_WEBSOCKET_OPCODE)
 	make_plugin(websocketIO WebSocketOpcode.c)
 	target_link_libraries(websocketIO ${websockets_library})
        message(STATUS "WEBSOCKETS LIB: ${websockets_library}")
        target_include_directories(websocketIO PRIVATE ${CSOUND_INCLUDE_DIRS})
        target_include_directories(websocketIO PRIVATE ${WEBSOCKETS_H})
       if(APPLE)
         target_include_directories(websocketIO PRIVATE "${HOMEBREW_OPENSSL_PATH}/include")
         target_link_libraries(websocketIO ${ssl_library} ${crypto_library}  z)
       endif()
endif()
