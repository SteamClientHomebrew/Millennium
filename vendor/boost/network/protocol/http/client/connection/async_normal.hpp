#ifndef BOOST_NETWORK_PROTOCOL_HTTP_IMPL_HTTP_ASYNC_CONNECTION_HPP_20100601
#define BOOST_NETWORK_PROTOCOL_HTTP_IMPL_HTTP_ASYNC_CONNECTION_HPP_20100601

// Copyright 2010 (C) Dean Michael Berris
// Copyright 2010 (C) Sinefunc, Inc.
// Copyright 2011 Dean Michael Berris (dberris@google.com).
// Copyright 2011 Google,Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <iterator>
#include <cstdint>
#include <boost/algorithm/string/trim.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/assert.hpp>
#include <boost/logic/tribool.hpp>
#include <boost/network/constants.hpp>
#include <boost/network/detail/debug.hpp>
#include <boost/network/protocol/http/algorithms/linearize.hpp>
#include <boost/network/protocol/http/client/connection/async_protocol_handler.hpp>
#include <boost/network/protocol/http/message/wrappers/host.hpp>
#include <boost/network/protocol/http/message/wrappers/uri.hpp>
#include <boost/network/protocol/http/parser/incremental.hpp>
#include <boost/network/protocol/http/traits/delegate_factory.hpp>
#include <boost/network/traits/istream.hpp>
#include <boost/network/traits/ostream_iterator.hpp>
#include <boost/network/version.hpp>
#include <boost/range/algorithm/transform.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/throw_exception.hpp>
#include <boost/network/compat.hpp>

namespace boost {
namespace network {
namespace http {
namespace impl {

template <class buffer_type>
struct chunk_encoding_parser {
  typedef typename buffer_type::const_iterator const_iterator;
  typedef boost::iterator_range<const_iterator> char_const_range;

  chunk_encoding_parser() : state(state_t::header), chunk_size(0) {}

  enum class state_t { header, header_end, data, data_end };

  state_t state;
  size_t chunk_size;
  buffer_type buffer;

  template<typename T>
  void update_chunk_size(boost::iterator_range<T> const &range) {
    if (range.empty()) return;
    std::stringstream ss;
    ss << std::hex << range;
    size_t size;
    ss >> size;
    // New digits are appended as LSBs
    chunk_size = (chunk_size << (range.size() * 4)) | size;
  }

  template<typename T>
  char_const_range operator()(boost::iterator_range<T> const &range) {
    auto iter = boost::begin(range);
    auto begin = iter;
    auto pos = boost::begin(buffer);

    while (iter != boost::end(range)) switch (state) {
        case state_t::header:
          iter = std::find(iter, boost::end(range), '\r');
          update_chunk_size(boost::make_iterator_range(begin, iter));
          if (iter != boost::end(range)) {
            state = state_t::header_end;
            ++iter;
          }
          break;

        case state_t::header_end:
          BOOST_ASSERT(*iter == '\n');
          ++iter;
          state = state_t::data;
          break;

        case state_t::data:
          if (chunk_size == 0) {
            BOOST_ASSERT(*iter == '\r');
            ++iter;
            state = state_t::data_end;
          } else {
            auto len = std::min(chunk_size,
                                (size_t)std::distance(iter, boost::end(range)));
            begin = iter;
            iter = std::next(iter, len);
            pos = std::copy(begin, iter, pos);
            chunk_size -= len;
          }
          break;

        case state_t::data_end:
          BOOST_ASSERT(*iter == '\n');
          ++iter;
          begin = iter;
          state = state_t::header;
          break;

        default:
          BOOST_ASSERT(false && "Bug, report this to the developers!");
      }
    return boost::make_iterator_range(boost::begin(buffer), pos);
  }
};

template <class Tag, unsigned version_major, unsigned version_minor>
struct async_connection_base;

namespace placeholders = boost::asio::placeholders;

template <class Tag, unsigned version_major, unsigned version_minor>
struct http_async_connection
    : async_connection_base<Tag, version_major, version_minor>,
      protected http_async_protocol_handler<Tag, version_major, version_minor>,
      std::enable_shared_from_this<
          http_async_connection<Tag, version_major, version_minor> > {
  http_async_connection(http_async_connection const&) = delete;

  typedef async_connection_base<Tag, version_major, version_minor> base;
  typedef http_async_protocol_handler<Tag, version_major, version_minor>
      protocol_base;
  typedef typename base::resolver_type resolver_type;
  typedef typename base::resolver_base::resolver_iterator resolver_iterator;
  typedef typename base::resolver_base::resolver_iterator_pair
      resolver_iterator_pair;
  typedef typename base::response response;
  typedef typename base::string_type string_type;
  typedef typename base::request request;
  typedef typename base::resolver_base::resolve_function resolve_function;
  typedef typename base::char_const_range char_const_range;
  typedef
      typename base::body_callback_function_type body_callback_function_type;
  typedef
      typename base::body_generator_function_type body_generator_function_type;
  typedef http_async_connection<Tag, version_major, version_minor> this_type;
  typedef typename delegate_factory<Tag>::type delegate_factory_type;
  typedef typename delegate_factory_type::connection_delegate_ptr
      connection_delegate_ptr;
  typedef chunk_encoding_parser<typename protocol_base::buffer_type> chunk_encoding_parser_type;

  http_async_connection(resolver_type& resolver, resolve_function resolve,
                        bool follow_redirect, int timeout,
                        bool remove_chunk_markers,
                        connection_delegate_ptr delegate)
      : timeout_(timeout),
        remove_chunk_markers_(remove_chunk_markers),
        timer_(CPP_NETLIB_ASIO_GET_IO_SERVICE(resolver)),
        is_timedout_(false),
        follow_redirect_(follow_redirect),
        resolver_(resolver),
        resolve_(std::move(resolve)),
        request_strand_(CPP_NETLIB_ASIO_GET_IO_SERVICE(resolver)),
        delegate_(std::move(delegate)) {}

  // This is the main entry point for the connection/request pipeline.
  // We're
  // overriding async_connection_base<...>::start(...) here which is
  // called
  // by the client.
  virtual response start(request const& request, string_type const& method,
                         bool get_body, body_callback_function_type callback,
                         body_generator_function_type generator) {
    response response_;
    this->init_response(response_, get_body);
    linearize(request, method, version_major, version_minor,
              std::ostreambuf_iterator<typename char_<Tag>::type>(
                  &command_streambuf));
    this->method = method;
    std::uint16_t port_ = port(request);
    string_type host_ = host(request);
    std::uint16_t source_port = request.source_port();

    auto sni_hostname = request.sni_hostname();

    auto self = this->shared_from_this();
    resolve_(resolver_, host_, port_,
             request_strand_.wrap(
                                  [=] (boost::system::error_code const &ec,
                                       resolver_iterator_pair endpoint_range) {
                                    self->handle_resolved(host_, port_, source_port, sni_hostname, get_body,
                                                          callback, generator, ec, endpoint_range);
                                  }));
    if (timeout_ > 0) {
      timer_.expires_from_now(std::chrono::seconds(timeout_));
      timer_.async_wait(request_strand_.wrap([=] (boost::system::error_code const &ec) {
            self->handle_timeout(ec);
          }));
    }
    return response_;
  }

 private:
  void set_errors(boost::system::error_code const& ec, body_callback_function_type callback) {
    boost::system::system_error error(ec);
    this->version_promise.set_exception(error);
    this->status_promise.set_exception(error);
    this->status_message_promise.set_exception(error);
    this->headers_promise.set_exception(error);
    this->source_promise.set_exception(error);
    this->destination_promise.set_exception(error);
    this->body_promise.set_exception(error);
    if ( callback )
      callback( char_const_range(), ec );
    this->timer_.cancel();
  }

  void handle_timeout(boost::system::error_code const& ec) {
    if (!ec) delegate_->disconnect();
    is_timedout_ = true;
  }

  void handle_resolved(string_type host, std::uint16_t port,
                       std::uint16_t source_port, optional<string_type> sni_hostname, bool get_body,
                       body_callback_function_type callback,
                       body_generator_function_type generator,
                       boost::system::error_code const& ec,
                       resolver_iterator_pair endpoint_range) {
    if (!ec && !boost::empty(endpoint_range)) {
      // Here we deal with the case that there was an error encountered and
      // that there's still more endpoints to try connecting to.
      resolver_iterator iter = boost::begin(endpoint_range);
      boost::asio::ip::tcp::endpoint endpoint(iter->endpoint().address(), port);
      auto self = this->shared_from_this();
      delegate_->connect(
          endpoint, host, source_port, sni_hostname,
          request_strand_.wrap([=] (boost::system::error_code const &ec) {
              auto iter_copy = iter;
              self->handle_connected(host, port, source_port, sni_hostname, get_body, callback,
                                     generator, std::make_pair(++iter_copy, resolver_iterator()), ec);
            }));
    } else {
      set_errors((ec ? ec : boost::asio::error::host_not_found), callback);
    }
  }

  void handle_connected(string_type host, std::uint16_t port,
                        std::uint16_t source_port, optional<string_type> sni_hostname, bool get_body,
                        body_callback_function_type callback,
                        body_generator_function_type generator,
                        resolver_iterator_pair endpoint_range,
                        boost::system::error_code const& ec) {
    if (is_timedout_) {
      set_errors(boost::asio::error::timed_out, callback);
    } else if (!ec) {
      BOOST_ASSERT(delegate_.get() != 0);
      auto self = this->shared_from_this();
      delegate_->write(
          command_streambuf,
          request_strand_.wrap([=] (boost::system::error_code const &ec,
                                    std::size_t bytes_transferred) {
                                 self->handle_sent_request(get_body, callback, generator,
                                                           ec, bytes_transferred);
                               }));
    } else {
      if (!boost::empty(endpoint_range)) {
        resolver_iterator iter = boost::begin(endpoint_range);
        boost::asio::ip::tcp::endpoint endpoint(iter->endpoint().address(), port);
        auto self = this->shared_from_this();
        delegate_->connect(
            endpoint, host, source_port, sni_hostname,
            request_strand_.wrap([=] (boost::system::error_code const &ec) {
                auto iter_copy = iter;
                self->handle_connected(host, port, source_port, sni_hostname, get_body, callback,
                                       generator, std::make_pair(++iter_copy, resolver_iterator()),
                                       ec);
              }));
      } else {
        set_errors((ec ? ec : boost::asio::error::host_not_found), callback);
      }
    }
  }

  enum state_t { version, status, status_message, headers, body };

  void handle_sent_request(bool get_body, body_callback_function_type callback,
                           body_generator_function_type generator,
                           boost::system::error_code const& ec,
                           std::size_t /*bytes_transferred*/) {  // TODO(unassigned): use-case?
    if (!is_timedout_ && !ec) {
      if (generator) {
        // Here we write some more data that the generator provides, before we
        // wait for data from the server.
        string_type chunk;
        if (generator(chunk)) {
          // At this point this means we have more data to write, so we write
          // it out.
          std::copy(chunk.begin(), chunk.end(),
                    std::ostreambuf_iterator<typename char_<Tag>::type>(
                        &command_streambuf));
          auto self = this->shared_from_this();
          delegate_->write(
              command_streambuf,
              request_strand_.wrap([=] (boost::system::error_code const &ec,
                                        std::size_t bytes_transferred) {
                                     self->handle_sent_request(get_body, callback, generator,
                                                               ec, bytes_transferred);
                                   }));
          return;
        }
      }

      auto self = this->shared_from_this();
      delegate_->read_some(
          boost::asio::mutable_buffers_1(this->part.data(),
                                         this->part.size()),
          request_strand_.wrap([=] (boost::system::error_code const &ec,
                                    std::size_t bytes_transferred) {
                                 self->handle_received_data(version, get_body, callback,
                                                            ec, bytes_transferred);
                               }));
    } else {
      set_errors((is_timedout_ ? boost::asio::error::timed_out : ec), callback);
    }
  }

  void handle_received_data(state_t state, bool get_body,
                            body_callback_function_type callback,
                            boost::system::error_code const& ec,
                            std::size_t bytes_transferred) {
    static const long short_read_error = 335544539;
    bool is_ssl_short_read_error =
#ifdef BOOST_NETWORK_ENABLE_HTTPS
        ec.category() == boost::asio::error::ssl_category &&
        ec.value() == short_read_error;
#else
        false && short_read_error;
#endif
    if (!is_timedout_ &&
        (!ec || ec == boost::asio::error::eof || is_ssl_short_read_error)) {
      logic::tribool parsed_ok;
      size_t remainder;
      auto self = this->shared_from_this();
      switch (state) {
        case version:
          if (ec == boost::asio::error::eof) return;
          parsed_ok = this->parse_version(
              delegate_,
              request_strand_.wrap([=] (boost::system::error_code const &ec,
                                        std::size_t bytes_transferred) {
                                     self->handle_received_data(version, get_body, callback,
                                                                ec, bytes_transferred);
                                   }),
              bytes_transferred);
          if (!parsed_ok || indeterminate(parsed_ok)) {
            return;
          }
          // fall-through
        case status:
          if (ec == boost::asio::error::eof) return;
          parsed_ok = this->parse_status(
              delegate_,
              request_strand_.wrap([=] (boost::system::error_code const &ec,
                                        std::size_t bytes_transferred) {
                                     self->handle_received_data(status, get_body, callback,
                                                                ec, bytes_transferred);
                                   }),
              bytes_transferred);
          if (!parsed_ok || indeterminate(parsed_ok)) {
            return;
          }
          // fall-through
        case status_message:
          if (ec == boost::asio::error::eof) return;
          parsed_ok = this->parse_status_message(
              delegate_, request_strand_.wrap([=] (boost::system::error_code const &,
                                                   std::size_t bytes_transferred) {
                                                self->handle_received_data(status_message, get_body, callback,
                                                                           ec, bytes_transferred);
                                              }),
              bytes_transferred);
          if (!parsed_ok || indeterminate(parsed_ok)) {
            return;
          }
          // fall-through
        case headers:
          if (ec == boost::asio::error::eof) return;
          // In the following, remainder is the number of bytes that remain in
          // the buffer. We need this in the body processing to make sure that
          // the data remaining in the buffer is dealt with before another call
          // to get more data for the body is scheduled.
          std::tie(parsed_ok, remainder) = this->parse_headers(
              delegate_,
              request_strand_.wrap([=] (boost::system::error_code const &ec,
                                        std::size_t bytes_transferred) {
                                     self->handle_received_data(headers, get_body, callback,
                                                                ec, bytes_transferred);
                                   }),
              bytes_transferred);

          if (!parsed_ok || indeterminate(parsed_ok)) {
            return;
          }

          if (!get_body) {
            // We short-circuit here because the user does not want to get the
            // body (in the case of a HEAD request).
            this->body_promise.set_value("");
            if ( callback )
              callback( char_const_range(), boost::asio::error::eof );
            this->destination_promise.set_value("");
            this->source_promise.set_value("");
            // this->part.assign('\0');
            boost::copy("\0", std::begin(this->part));
            this->response_parser_.reset();
            return;
          }

          if (callback) {
            // Here we deal with the spill-over data from the headers
            // processing. This means the headers data has already been parsed
            // appropriately and we're looking to treat everything that remains
            // in the buffer.
            typename protocol_base::buffer_type::const_iterator begin =
                this->part_begin;
            typename protocol_base::buffer_type::const_iterator end = begin;
            std::advance(end, remainder);

            // We're setting the body promise here to an empty string because
            // this can be used as a signaling mechanism for the user to
            // determine that the body is now ready for processing, even though
            // the callback is already provided.
            this->body_promise.set_value("");

            // The invocation of the callback is synchronous to allow us to
            // wait before scheduling another read.
            if (this->is_chunk_encoding && remove_chunk_markers_) {
              callback(parse_chunk_encoding(make_iterator_range(begin, end)), ec);
            } else {
              callback(make_iterator_range(begin, end), ec);
            }
            auto self = this->shared_from_this();
            delegate_->read_some(
                boost::asio::mutable_buffers_1(this->part.data(),
                                               this->part.size()),
                request_strand_.wrap([=] (boost::system::error_code const &ec,
                                          std::size_t bytes_transferred) {
                                       self->handle_received_data(body, get_body, callback,
                                                                  ec, bytes_transferred);
                                     }));
          } else {
            // Here we handle the body data ourself and append to an
            // ever-growing string buffer.
            auto self = this->shared_from_this();
            this->parse_body(
                delegate_,
                request_strand_.wrap([=] (boost::system::error_code const &ec,
                                          std::size_t bytes_transferred) {
                                       self->handle_received_data(body, get_body, callback,
                                                                  ec, bytes_transferred);
                                     }),
                remainder);
          }
          return;
        case body:
          if (ec == boost::asio::error::eof || is_ssl_short_read_error) {
            // Here we're handling the case when the connection has been closed
            // from the server side, or at least that the end of file has been
            // reached while reading the socket. This signals the end of the
            // body processing chain.
            if (callback) {
              typename protocol_base::buffer_type::const_iterator
                  begin = this->part.begin(),
                  end = begin;
              std::advance(end, bytes_transferred);

              // We call the callback function synchronously passing the error
              // condition (in this case, end of file) so that it can handle it
              // appropriately.
              if (this->is_chunk_encoding && remove_chunk_markers_) {
                callback(parse_chunk_encoding(make_iterator_range(begin, end)), ec);
              } else {
                callback(make_iterator_range(begin, end), ec);
              }
            } else {
              string_type body_string;
              if (this->is_chunk_encoding && remove_chunk_markers_) {
                const auto parse_buffer_size = parse_chunk_encoding.buffer.size();
                for (size_t i = 0; i < this->partial_parsed.size(); i += parse_buffer_size) {
                  auto range = parse_chunk_encoding(boost::make_iterator_range(
                      this->partial_parsed.cbegin() + i,
                      this->partial_parsed.cbegin() +
                          std::min(i + parse_buffer_size, this->partial_parsed.size())));
                  body_string.append(boost::begin(range), boost::end(range));
                }
                this->partial_parsed.clear();
                auto range = parse_chunk_encoding(boost::make_iterator_range(
                    this->part.begin(),
                    this->part.begin() + bytes_transferred));
                body_string.append(boost::begin(range), boost::end(range));
                this->body_promise.set_value(body_string);
              } else {
                std::swap(body_string, this->partial_parsed);
                body_string.append(this->part.begin(),
                                   this->part.begin() + bytes_transferred);
                this->body_promise.set_value(body_string);
              }
            }
            // TODO(dberris): set the destination value somewhere!
            this->destination_promise.set_value("");
            this->source_promise.set_value("");
            // this->part.assign('\0');
            boost::copy("\0", std::begin(this->part));
            this->response_parser_.reset();
            this->timer_.cancel();
          } else {
            // This means the connection has not been closed yet and we want to
            // get more data.
            if (callback) {
              // Here we have a body_handler callback. Let's invoke the
              // callback from here and make sure we're getting more data right
              // after.
              typename protocol_base::buffer_type::const_iterator begin =
                  this->part.begin();
              typename protocol_base::buffer_type::const_iterator end = begin;
              std::advance(end, bytes_transferred);
              if (this->is_chunk_encoding && remove_chunk_markers_) {
                callback(parse_chunk_encoding(make_iterator_range(begin, end)), ec);
              } else {
                callback(make_iterator_range(begin, end), ec);
              }
              auto self = this->shared_from_this();
              delegate_->read_some(
                  boost::asio::mutable_buffers_1(this->part.data(),
                                                 this->part.size()),
                  request_strand_.wrap([=] (boost::system::error_code const &ec,
                                            std::size_t bytes_transferred) {
                                         self->handle_received_data(body, get_body, callback,
                                                                    ec, bytes_transferred);
                                       }));
            } else {
              // Here we don't have a body callback. Let's make sure that we
              // deal with the remainder from the headers part in case we do
              // have data that's still in the buffer.
              this->parse_body(
                  delegate_,
                  request_strand_.wrap([=] (boost::system::error_code const &ec,
                                            std::size_t bytes_transferred) {
                                         self->handle_received_data(body, get_body, callback,
                                                                    ec, bytes_transferred);
                                       }),
                  bytes_transferred);
            }
          }
          return;
        default:
          BOOST_ASSERT(false && "Bug, report this to the developers!");
      }
    } else {
      boost::system::error_code report_code = is_timedout_ ? boost::asio::error::timed_out : ec;
      boost::system::system_error error(report_code);
      this->source_promise.set_exception(error);
      this->destination_promise.set_exception(error);
      switch (state) {
        case version:
          this->version_promise.set_exception(error);
          // fall-through
        case status:
          this->status_promise.set_exception(error);
          // fall-through
        case status_message:
          this->status_message_promise.set_exception(error);
          // fall-through
        case headers:
          this->headers_promise.set_exception(error);
          // fall-through
        case body:
          if (!callback) {
            // N.B. if callback is non-null, then body_promise has already been
            // set to value "" to indicate body is handled by streaming handler
            // so no exception should be set
            this->body_promise.set_exception(error);
          }
          else
            callback( char_const_range(), report_code );
          break;
          // fall-through
        default:
          BOOST_ASSERT(false && "Bug, report this to the developers!");
      }
    }
  }

  int timeout_;
  bool remove_chunk_markers_;
  boost::asio::steady_timer timer_;
  bool is_timedout_;
  bool follow_redirect_;
  resolver_type& resolver_;
  resolve_function resolve_;
  boost::asio::io_service::strand request_strand_;
  connection_delegate_ptr delegate_;
  boost::asio::streambuf command_streambuf;
  string_type method;
  chunk_encoding_parser_type parse_chunk_encoding;
};

}  // namespace impl
}  // namespace http
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_IMPL_HTTP_ASYNC_CONNECTION_HPP_20100601
