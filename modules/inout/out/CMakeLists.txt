# FILE
#ADD_LIBRARY(output_file SHARED dv_output.hpp output_common.c file.c)
#
#SET_TARGET_PROPERTIES(output_file
#	PROPERTIES
#	PREFIX "dv_"
#)
#
#TARGET_LINK_LIBRARIES(output_file ${DV_LIBS})
#
#INSTALL(TARGETS output_file DESTINATION ${DV_MODULES_DIR})

# NET_TCP_SERVER
ADD_LIBRARY(output_net_tcp_server SHARED net_tcp_server.cpp)

SET_TARGET_PROPERTIES(output_net_tcp_server
	PROPERTIES
	PREFIX "dv_"
)

TARGET_LINK_LIBRARIES(output_net_tcp_server ${DV_LIBS})

INSTALL(TARGETS output_net_tcp_server DESTINATION ${DV_MODULES_DIR})

# NET_TCP_CLIENT
ADD_LIBRARY(output_net_tcp_client SHARED output_common.c net_tcp.c)

SET_TARGET_PROPERTIES(output_net_tcp_client
	PROPERTIES
	PREFIX "dv_"
)

TARGET_LINK_LIBRARIES(output_net_tcp_client ${DV_LIBS})

INSTALL(TARGETS output_net_tcp_client DESTINATION ${DV_MODULES_DIR})

# NET_UDP
ADD_LIBRARY(output_net_udp SHARED output_common.c net_udp.c)

SET_TARGET_PROPERTIES(output_net_udp
	PROPERTIES
	PREFIX "dv_"
)

TARGET_LINK_LIBRARIES(output_net_udp ${DV_LIBS})

INSTALL(TARGETS output_net_udp DESTINATION ${DV_MODULES_DIR})

# NET_SOCKET_SERVER
ADD_LIBRARY(output_net_socket_server SHARED output_common.c unix_socket_server.c)

SET_TARGET_PROPERTIES(output_net_socket_server
	PROPERTIES
	PREFIX "dv_"
)

TARGET_LINK_LIBRARIES(output_net_socket_server ${DV_LIBS})

INSTALL(TARGETS output_net_socket_server DESTINATION ${DV_MODULES_DIR})

# NET_SOCKET_CLIENT
ADD_LIBRARY(output_net_socket_client SHARED output_common.c unix_socket.c)

SET_TARGET_PROPERTIES(output_net_socket_client
	PROPERTIES
	PREFIX "dv_"
)

TARGET_LINK_LIBRARIES(output_net_socket_client ${DV_LIBS})

INSTALL(TARGETS output_net_socket_client DESTINATION ${DV_MODULES_DIR})
