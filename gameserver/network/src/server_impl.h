/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Simon Sandström
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef NETWORK_SERVERIMPL_H_
#define NETWORK_SERVERIMPL_H_

#include <unordered_map>
#include <boost/asio.hpp>  //NOLINT

#include "server.h"
#include "acceptor.h"
#include "connection.h"

// Forward declarations
class IncomingPacket;
class OutgoingPacket;

class ServerImpl : public Server
{
 public:
  ServerImpl(boost::asio::io_service* io_service,
             unsigned short port,
             const Callbacks& callbacks);
  ~ServerImpl();

  bool start();
  void stop();

  void sendPacket(ConnectionId connectionId, OutgoingPacket&& packet);
  void closeConnection(ConnectionId connectionId, bool force);

 private:
  // Handler for Acceptor
  void onAccept(boost::asio::ip::tcp::socket socket);

  // Handler for Connection
  void onPacketReceived(ConnectionId connectionId, IncomingPacket* packet);
  void onDisconnected(ConnectionId connectionId);
  void onConnectionClosed(ConnectionId connectionId);

  // Backend for Acceptor
  // TODO(gurka): Common backend for acceptor and connection + move to loginserver / worldserver
  struct Backend
  {
    using Service = boost::asio::io_service;

    class Acceptor : public boost::asio::ip::tcp::acceptor
    {
     public:
      Acceptor(Service& io_service, int port)
        : boost::asio::ip::tcp::acceptor(io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
      {
      }
    };

    using Socket = boost::asio::ip::tcp::socket;
    using ErrorCode = boost::system::error_code;
    using Error = boost::asio::error::basic_errors;
    using shutdown_type = boost::asio::ip::tcp::socket::shutdown_type;

    static void async_write(Socket& socket,
                            const uint8_t* buffer,
                            std::size_t length,
                            const std::function<void(const Backend::ErrorCode&, std::size_t)>& handler)
    {
      boost::asio::async_write(socket, boost::asio::buffer(buffer, length), handler);
    }

    static void async_read(Socket& socket,
                           uint8_t* buffer,
                           std::size_t length,
                           const std::function<void(const Backend::ErrorCode&, std::size_t)>& handler)
    {
      boost::asio::async_read(socket, boost::asio::buffer(buffer, length), handler);
    }
  };

  Acceptor<Backend> acceptor_;
  Callbacks callbacks_;

  ConnectionId nextConnectionId_;
  std::unordered_map<ConnectionId, Connection<Backend>> connections_;  // TODO(gurka): vector/array?
};

#endif  // NETWORK_SERVERIMPL_H_
