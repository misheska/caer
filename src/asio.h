#ifndef SRC_ASIO_H_
#define SRC_ASIO_H_

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

namespace asio    = boost::asio;
namespace asioSSL = boost::asio::ssl;
namespace asioIP  = boost::asio::ip;
using asioTCP     = boost::asio::ip::tcp;

class TCPSSLSocket {
private:
	asioSSL::stream<asioTCP::socket> socket;
	bool sslConnection;
	bool sslInitialized;

public:
	TCPSSLSocket(asioTCP::socket s, bool sslEnabled, asioSSL::context &sslContext) :
		socket(std::move(s), sslContext),
		sslConnection(sslEnabled),
		sslInitialized(false) {
	}

	~TCPSSLSocket() {
		// Shutdown SSL cleanly, if enabled and initialized successfully.
		if (sslConnection && sslInitialized) {
			socket.shutdown();
		}

		// Close underlying TCP socket cleanly.
		// TCP shutdown() should be called for portability.
		next_layer().shutdown(asioTCP::socket::shutdown_both);
		next_layer().close();
	}

	const asioTCP::socket &next_layer() const {
		return (socket.next_layer());
	}

	asioTCP::socket &next_layer() {
		return (socket.next_layer());
	}

	/**
	 * Startup handler needs following signature:
	 * void (const boost::system::error_code &)
	 */
	template<typename StartupHandler> void start(StartupHandler &&stHandler) {
		if (sslConnection) {
			socket.async_handshake(
				asioSSL::stream_base::server, [this, stHandler](const boost::system::error_code &error) {
					if (!error) {
						sslInitialized = true;
					}

					stHandler(error);
				});
		}
		else {
			stHandler(boost::system::error_code());
		}
	}

	/**
	 * Write handler needs following signature:
	 * void (const boost::system::error_code &, size_t)
	 */
	template<typename WriteHandler> void write(const asio::const_buffer &buf, WriteHandler &&wrHandler) {
		if (sslConnection) {
			asio::async_write(socket, buf, wrHandler);
		}
		else {
			asio::async_write(next_layer(), buf, wrHandler);
		}
	}

	/**
	 * Read handler needs following signature:
	 * void (const boost::system::error_code &, size_t)
	 */
	template<typename ReadHandler> void read(const asio::mutable_buffer &buf, ReadHandler &&rdHandler) {
		if (sslConnection) {
			asio::async_read(socket, buf, rdHandler);
		}
		else {
			asio::async_read(next_layer(), buf, rdHandler);
		}
	}

	asioTCP::endpoint local_endpoint() const {
		return (next_layer().local_endpoint());
	}

	asioIP::address local_address() const {
		return (local_endpoint().address());
	}

	uint16_t local_port() const {
		return (local_endpoint().port());
	}

	asioTCP::endpoint remote_endpoint() const {
		return (next_layer().remote_endpoint());
	}

	asioIP::address remote_address() const {
		return (remote_endpoint().address());
	}

	uint16_t remote_port() const {
		return (remote_endpoint().port());
	}
};

#endif /* SRC_ASIO_H_ */
