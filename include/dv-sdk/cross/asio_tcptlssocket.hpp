#ifndef DV_SDK_ASIO_TCPTLSSOCKET_HPP_
#define DV_SDK_ASIO_TCPTLSSOCKET_HPP_

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/version.hpp>
#include <deque>
#include <utility>

namespace asio    = boost::asio;
namespace asioSSL = asio::ssl;
namespace asioIP  = asio::ip;
using asioTCP     = asioIP::tcp;

class TCPTLSSocket {
private:
	asioTCP::endpoint localEndpoint;
	asioTCP::endpoint remoteEndpoint;
	asioSSL::stream<asioTCP::socket> socket;
	bool socketClosed;
	bool secureConnection;

public:
	TCPTLSSocket(asioTCP::socket s, bool tlsEnabled, asioSSL::context *tlsContext) :
		localEndpoint(s.local_endpoint()),
		remoteEndpoint(s.remote_endpoint()),
#if defined(BOOST_VERSION) && (BOOST_VERSION / 100000) == 1 && (BOOST_VERSION / 100 % 1000) >= 66
		socket(std::move(s), *tlsContext),
#else
		socket(s.get_io_service(), *tlsContext),
#endif
		socketClosed(false),
		secureConnection(tlsEnabled) {
#if defined(BOOST_VERSION) && (BOOST_VERSION / 100000) == 1 && (BOOST_VERSION / 100 % 1000) < 66
		socket.next_layer() = std::move(s);
#endif
	}

	~TCPTLSSocket() {
		close();
	}

	void close() {
		// Close underlying TCP socket cleanly.
		// TCP shutdown() should be called for portability.
		// Note: no TLS shutdown, as the ASIO implementation does not
		// easily allow to just send a close_notify and close the socket.
		// It waits on reply from the other side, which we can't and don't
		// want to guarantee. There is a workaround, but it makes the
		// whole thing much more complex. Since shutdown only really
		// protects against a truncation attack, and it is not a problem
		// for our protocol, we can safely ignore it.
		if (!socketClosed) {
			boost::system::error_code ec;
			baseSocket().shutdown(asioTCP::socket::shutdown_both, ec);
			baseSocket().close(ec);

			socketClosed = true;
		}
	}

	/**
	 * Startup handler needs following signature:
	 * void (const boost::system::error_code &)
	 */
	template<typename StartupHandler>
	void start(StartupHandler &&stHandler, asioSSL::stream_base::handshake_type type) {
		if (secureConnection) {
			socket.async_handshake(type, stHandler);
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
		const asio::const_buffers_1 buf2(buf);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
		if (secureConnection) {
			asio::async_write(socket, buf2, wrHandler);
		}
		else {
			asio::async_write(baseSocket(), buf2, wrHandler);
		}
#pragma GCC diagnostic pop
	}

	/**
	 * Read handler needs following signature:
	 * void (const boost::system::error_code &, size_t)
	 */
	template<typename ReadHandler> void read(const asio::mutable_buffer &buf, ReadHandler &&rdHandler) {
		const asio::mutable_buffers_1 buf2(buf);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
		if (secureConnection) {
			asio::async_read(socket, buf2, rdHandler);
		}
		else {
			asio::async_read(baseSocket(), buf2, rdHandler);
		}
#pragma GCC diagnostic pop
	}

	asioTCP::endpoint local_endpoint() const {
		return (localEndpoint);
	}

	asioIP::address local_address() const {
		return (local_endpoint().address());
	}

	uint16_t local_port() const {
		return (local_endpoint().port());
	}

	asioTCP::endpoint remote_endpoint() const {
		return (remoteEndpoint);
	}

	asioIP::address remote_address() const {
		return (remote_endpoint().address());
	}

	uint16_t remote_port() const {
		return (remote_endpoint().port());
	}

private:
	asioTCP::socket &baseSocket() {
		return (socket.next_layer());
	}
};

class TCPTLSWriteOrderedSocket : public TCPTLSSocket {
public:
	TCPTLSWriteOrderedSocket(asioTCP::socket s, bool tlsEnabled, asioSSL::context *tlsContext) :
		TCPTLSSocket(std::move(s), tlsEnabled, tlsContext) {
	}

	/**
	 * Write handler needs following signature:
	 * void (const boost::system::error_code &, size_t)
	 */
	template<typename WriteHandler> void write(const asio::const_buffer &buf, WriteHandler &&wrHandler) {
		std::function<void(const boost::system::error_code &, size_t)> orderedHandler
			= [this, wrHandler](const boost::system::error_code &error, size_t length) {
				  // Execute bound handler.
				  wrHandler(error, length);

				  // Remove currently executing handler from queue (placeholder only).
				  writeQueue.pop_front();

				  // On error, clear pending writes and do nothing.
				  if (error) {
					  writeQueue.clear();
				  }
				  else {
					  // Start new writes.
					  if (!writeQueue.empty()) {
						  TCPTLSSocket::write(writeQueue.front().first, writeQueue.front().second);
					  }
				  }
			  };

		// Check current status.
		bool noWrites = writeQueue.empty();

		// Enqueue all writes.
		writeQueue.emplace_back(buf, orderedHandler);

		if (noWrites) {
			// Start first write.
			TCPTLSSocket::write(writeQueue.front().first, writeQueue.front().second);
		}
	}

private:
	std::deque<std::pair<asio::const_buffer, std::function<void(const boost::system::error_code &, size_t)>>>
		writeQueue; // No locking for writeQueue because all changes are posted to io_service thread.
};

#endif /* DV_SDK_ASIO_TCPTLSSOCKET_HPP_ */
