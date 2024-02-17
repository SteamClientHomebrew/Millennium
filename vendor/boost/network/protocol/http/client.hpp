#ifndef BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_20091215
#define BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_20091215

//          Copyright Dean Michael Berris 2007-2010.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <boost/config.hpp>
#include <boost/network/protocol/http/message.hpp>
#include <boost/network/protocol/http/request.hpp>
#include <boost/network/protocol/http/response.hpp>
#include <boost/network/traits/ostringstream.hpp>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <istream>
#include <map>
#include <ostream>
#include <stdexcept>
#include <string>

#include <boost/network/protocol/http/client/facade.hpp>
#include <boost/network/protocol/http/client/options.hpp>

namespace boost {
namespace network {
namespace http {

/**
 * Basic Client API.
 *
 * This template defines an HTTP client object that is non-copyable and is
 * implemented depending on the `Tag` parameter. It also hard-codes the version
 * of HTTP to support.
 *
 * @tparam Tag The Tag type used to determine certain traits of the
 *   implementation. We use the Tag as an input to other template metafunctions
 *   to determine things like the string type, whether to use TCP or UDP for the
 *   resolver, etc.
 * @tparam version_major The HTTP major version.
 * @tparam version_minor The HTTP minor version.
 *
 */
template <class Tag, unsigned version_major, unsigned version_minor>
class basic_client : public basic_client_facade<Tag, version_major, version_minor> {
  typedef basic_client_facade<Tag, version_major, version_minor>
      base_facade_type;

 public:
  typedef Tag tag_type;
  typedef client_options<Tag> options;

  /**
   * This constructor takes a single options argument of type
   * client_options. See boost/network/protocol/http/client/options.hpp for
   * more details.
   */
  explicit basic_client(options const& options) : base_facade_type(options) {}

  /** This default constructor sets up the default options. */
  basic_client() : base_facade_type(options()) {}
};

#ifndef BOOST_NETWORK_HTTP_CLIENT_DEFAULT_TAG
#define BOOST_NETWORK_HTTP_CLIENT_DEFAULT_TAG tags::http_async_8bit_udp_resolve
#endif

/**
 * The default HTTP client type is an asynchronous UDP-resolving HTTP/1.1
 * client.
 */
typedef basic_client<BOOST_NETWORK_HTTP_CLIENT_DEFAULT_TAG, 1, 1> client;

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_20091215
