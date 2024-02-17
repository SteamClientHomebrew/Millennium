//            Copyright (c) Glyn Matthews 2012.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef __BOOST_NETWORK_URI_BUILDER_INC__
#define __BOOST_NETWORK_URI_BUILDER_INC__

#include <boost/asio/ip/address.hpp>
#include <boost/network/uri/uri.hpp>

namespace boost {
namespace network {
namespace uri {
class builder {

  typedef uri::string_type string_type;

 public:
  explicit builder(uri &uri_) : uri_(uri_) {}

  builder &set_scheme(const string_type &scheme) {
    uri_.uri_.append(scheme);
    if (opaque_schemes::exists(scheme)) {
      uri_.uri_.append(":");
    } else {
      uri_.uri_.append("://");
    }
    uri_.parse();
    return *this;
  }

  builder &scheme(const string_type &scheme) { return set_scheme(scheme); }

  builder &set_user_info(const string_type &user_info) {
    uri_.uri_.append(user_info);
    uri_.uri_.append("@");
    uri_.parse();
    return *this;
  }

  builder &user_info(const string_type &user_info) {
    return set_user_info(user_info);
  }

  builder &set_host(const string_type &host) {
    uri_.uri_.append(host);
    uri_.parse();
    return *this;
  }

  builder &host(const string_type &host) { return set_host(host); }

  builder &set_host(const boost::asio::ip::address &address) {
    uri_.uri_.append(address.to_string());
    uri_.parse();
    return *this;
  }

  builder &host(const boost::asio::ip::address &host) { return set_host(host); }

  builder &set_host(const boost::asio::ip::address_v4 &address) {
    uri_.uri_.append(address.to_string());
    uri_.parse();
    return *this;
  }

  builder &host(const boost::asio::ip::address_v4 &host) { return set_host(host); }

  builder &set_host(const boost::asio::ip::address_v6 &address) {
    uri_.uri_.append("[");
    uri_.uri_.append(address.to_string());
    uri_.uri_.append("]");
    uri_.parse();
    return *this;
  }

  builder &host(const boost::asio::ip::address_v6 &host) { return set_host(host); }

  builder &set_port(const string_type &port) {
    uri_.uri_.append(":");
    uri_.uri_.append(port);
    uri_.parse();
    return *this;
  }

  builder &port(const string_type &port) { return set_port(port); }

  builder &port(uint16_t port) {
    return set_port(std::to_string(port));
  }

  builder &set_path(const string_type &path) {
    uri_.uri_.append(path);
    uri_.parse();
    return *this;
  }

  builder &path(const string_type &path) { return set_path(path); }

  builder &encoded_path(const string_type &path) {
    string_type encoded_path;
    encode(path, std::back_inserter(encoded_path));
    return set_path(encoded_path);
  }

  builder &set_query(const string_type &query) {
    uri_.uri_.append("?");
    uri_.uri_.append(query);
    uri_.parse();
    return *this;
  }

  builder &set_query(const string_type &key, const string_type &value) {
    if (!uri_.query_range()) {
      uri_.uri_.append("?");
    } else {
      uri_.uri_.append("&");
    }
    uri_.uri_.append(key);
    uri_.uri_.append("=");
    uri_.uri_.append(value);
    uri_.parse();
    return *this;
  }

  builder &query(const string_type &query) { return set_query(query); }

  builder &query(const string_type &key, const string_type &value) {
    return set_query(key, value);
  }

  builder &set_fragment(const string_type &fragment) {
    uri_.uri_.append("#");
    uri_.uri_.append(fragment);
    uri_.parse();
    return *this;
  }

  builder &fragment(const string_type &fragment) {
    return set_fragment(fragment);
  }

 private:
  uri &uri_;
};
}  // namespace uri
}  // namespace network
}  // namespace boost

#endif  // __BOOST_NETWORK_URI_BUILDER_INC__
