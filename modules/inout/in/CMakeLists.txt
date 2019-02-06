# FILE
ADD_LIBRARY(input_file SHARED input_common.c file.c)

SET_TARGET_PROPERTIES(input_file
	PROPERTIES
	PREFIX "dv_"
)

TARGET_LINK_LIBRARIES(input_file ${DV_LIBS})

INSTALL(TARGETS input_file DESTINATION ${DV_MODULES_DIR})

# NET_TCP_CLIENT
ADD_LIBRARY(input_net_tcp_client SHARED input_common.c net_tcp.c)

SET_TARGET_PROPERTIES(input_net_tcp_client
	PROPERTIES
	PREFIX "dv_"
)

TARGET_LINK_LIBRARIES(input_net_tcp_client ${DV_LIBS})

INSTALL(TARGETS input_net_tcp_client DESTINATION ${DV_MODULES_DIR})

# NET_SOCKET_CLIENT
ADD_LIBRARY(input_net_socket_client SHARED input_common.c unix_socket.c)

SET_TARGET_PROPERTIES(input_net_socket_client
	PROPERTIES
	PREFIX "dv_"
)

TARGET_LINK_LIBRARIES(input_net_socket_client ${DV_LIBS})

INSTALL(TARGETS input_net_socket_client DESTINATION ${DV_MODULES_DIR})
