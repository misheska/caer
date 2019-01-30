#ifndef SRC_ASIO_H_
#define SRC_ASIO_H_

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/version.hpp>
#include <deque>
#include <functional>
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
	bool sslConnection;

public:
	TCPTLSSocket(asioTCP::socket s, bool sslEnabled, asioSSL::context *sslContext) :
		localEndpoint(s.local_endpoint()),
		remoteEndpoint(s.remote_endpoint()),
#if defined(BOOST_VERSION) && (BOOST_VERSION / 100000) == 1 && (BOOST_VERSION / 100 % 1000) >= 66
		socket(std::move(s), *sslContext),
#else
		socket(s.get_io_service(), *sslContext),
#endif
		socketClosed(false),
		sslConnection(sslEnabled) {
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
		if (sslConnection) {
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

		if (sslConnection) {
			asio::async_write(socket, buf2, wrHandler);
		}
		else {
			asio::async_write(baseSocket(), buf2, wrHandler);
		}
	}

	/**
	 * Read handler needs following signature:
	 * void (const boost::system::error_code &, size_t)
	 */
	template<typename ReadHandler> void read(const asio::mutable_buffer &buf, ReadHandler &&rdHandler) {
		const asio::mutable_buffers_1 buf2(buf);

		if (sslConnection) {
			asio::async_read(socket, buf2, rdHandler);
		}
		else {
			asio::async_read(baseSocket(), buf2, rdHandler);
		}
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
	TCPTLSWriteOrderedSocket(asioTCP::socket s, bool sslEnabled, asioSSL::context *sslContext) :
		TCPTLSSocket(std::move(s), sslEnabled, sslContext) {
	}

	/**
	 * Write handler needs following signature:
	 * void (const boost::system::error_code &, size_t)
	 */
	template<typename WriteHandler> void write(const asio::const_buffer &buf, WriteHandler &&wrHandler) {
		bool writeInProgress = writesOutstanding();

		writeQueue.emplace_back(buf, wrHandler);

		if (!writeInProgress) {
			orderedWrite();
		}
	}

	bool writesOutstanding() {
		return (!writeQueue.empty());
	}

private:
	std::deque<std::pair<asio::const_buffer, std::function<void(const boost::system::error_code &, size_t)>>>
		writeQueue;

	void orderedWrite() {
		TCPTLSSocket::write(writeQueue.front().first, [this](const boost::system::error_code &error, size_t length) {
			writeQueue.front().second(error, length);

			if (!error) {
				writeQueue.pop_front();

				if (writesOutstanding()) {
					orderedWrite();
				}
			}
		});
	}
};

#endif /* SRC_ASIO_H_ */
