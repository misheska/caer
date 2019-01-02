#ifndef SRC_ASIO_H_
#define SRC_ASIO_H_

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <deque>
#include <functional>
#include <utility>

namespace asio    = boost::asio;
namespace asioSSL = boost::asio::ssl;
namespace asioIP  = boost::asio::ip;
using asioTCP     = boost::asio::ip::tcp;

class TCPTLSSocket {
private:
	asioSSL::stream<asioTCP::socket> socket;
	bool sslConnection;
	bool closed;

public:
	TCPTLSSocket(asioTCP::socket s, bool sslEnabled, asioSSL::context &sslContext) :
		socket(std::move(s), sslContext),
		sslConnection(sslEnabled),
		closed(false) {
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
		if (!closed) {
			next_layer().shutdown(asioTCP::socket::shutdown_both);
			next_layer().close();

			closed = true;
		}
	}

	/**
	 * Startup handler needs following signature:
	 * void (const boost::system::error_code &)
	 */
	template<typename StartupHandler> void start(StartupHandler &&stHandler) {
		if (sslConnection) {
			socket.async_handshake(asioSSL::stream_base::server, stHandler);
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

private:
	const asioTCP::socket &next_layer() const {
		return (socket.next_layer());
	}

	asioTCP::socket &next_layer() {
		return (socket.next_layer());
	}
};

class TCPTLSWriteOrderedSocket : public TCPTLSSocket {
public:
	TCPTLSWriteOrderedSocket(asioTCP::socket s, bool sslEnabled, asioSSL::context &sslContext) :
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
