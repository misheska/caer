# NET_TCP_SERVER
ADD_LIBRARY(output_net_tcp_server SHARED net_tcp_server.cpp)

SET_TARGET_PROPERTIES(output_net_tcp_server
	PROPERTIES
	PREFIX "dv_"
)

TARGET_LINK_LIBRARIES(output_net_tcp_server
	PRIVATE
		dvsdk
		Boost::boost)

INSTALL(TARGETS output_net_tcp_server DESTINATION ${DV_MODULES_DIR})
