#ifndef BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_FACADE_HPP_20100623
#define BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_FACADE_HPP_20100623

// Copyright 2013 Google, Inc.
// Copyright 2010 Dean Michael Berris <dberris@google.com>
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <memory>
#include <functional>
#include <boost/network/protocol/http/client/pimpl.hpp>
#include <boost/network/protocol/http/request.hpp>
#include <boost/network/protocol/http/response.hpp>

namespace boost {
namespace network {
namespace http {

template <class Tag>
struct basic_request;

template <class Tag>
struct basic_response;

template <class Tag, unsigned version_major, unsigned version_minor>
class basic_client_facade {
  typedef basic_client_impl<Tag, version_major, version_minor> pimpl_type;

 public:
  /**
   * The type to use for strings associated with this client. Typically resolves
   * to `std::string`.
   */
  typedef typename string<Tag>::type string_type;

  /** The request type. This models the HTTP Request concept. */
  typedef basic_request<Tag> request;

  /** The response type. This models the HTTP Response concept.*/
  typedef basic_response<Tag> response;

  typedef
      typename std::array<typename char_<Tag>::type,
                          BOOST_NETWORK_HTTP_CLIENT_CONNECTION_BUFFER_SIZE>::
          const_iterator const_iterator;
  typedef iterator_range<const_iterator> char_const_range;

  /**
   * This callback is invoked with a range representing part of the response's
   * body as it comes in. In case of errors, the second argument is an error
   * code.
   */
  typedef std::function<void(char_const_range,
                             boost::system::error_code const&)>
      body_callback_function_type;

  /**
   * Functions that model this signature will be used to generate part of the
   * body while the request is being performed. This allows users to provide a
   * generator function that will generate the body of the request piece-wise.
   *
   * Implementations should return `true` if there is more to the body of the
   * request, and `false` otherwise.
   */
  typedef std::function<bool(string_type&)> body_generator_function_type;

  explicit basic_client_facade(client_options<Tag> const& options) {
    init_pimpl(options);
  }

  ~basic_client_facade() { pimpl->wait_complete(); }

  /**
   * Perform a HEAD request.
   */
  response head(request const& request) {
    return pimpl->request_skeleton(request, "HEAD", false,
                                   body_callback_function_type(),
                                   body_generator_function_type());
  }

  /**
   * Perform a GET request.
   *
   * @param[in] request The request object including the URI and headers.
   * @param[in] body_handler If provided, a callback invoked for parts of the
   *   response's body.
   * @returns A response object.
   * @throw std::exception May throw exceptions on errors, derived from
   *   `std::exception`.
   */
  response get(request const& request,
               body_callback_function_type body_handler =
                   body_callback_function_type()) {
    return pimpl->request_skeleton(request, "GET", true, body_handler,
                                   body_generator_function_type());
  }

  /**
   * Perform a POST request.
   *
   * @param[in] request A copy of the request object including the URI and
   *   headers.
   * @param[in] body The whole contents of the body. If provided, this overrides
   *   the body in the `request`.
   * @param[in] content_type The content type for the request. This overrides
   *   the content type in the `request`.
   * @param[in] body_handler The callback invoked for parts of the response body
   *   as they come in.
   * @param[in] body_generator If provided, is invoked to generate parts of the
   *   request's body as it is being sent.
   * @returns A response object.
   * @throws std::exception May throw exceptions on errors, derived from
   *   `std::exception`.
   */
  response post(
      request request, string_type const& body = string_type(),
      string_type const& content_type = string_type(),
      body_callback_function_type body_handler = body_callback_function_type(),
      body_generator_function_type body_generator =
          body_generator_function_type()) {
    return perform_request(request, "POST", body, content_type,
                           body_handler, body_generator);
  }

  /**
   * Perform a POST request.
   *
   * @param[in] request The request including the URI and headers.
   * @param[in] body_generator The function to call to generate part of the body
   *   while the request is being performed.
   * @param[in] callback If provided, the function to call for parts of the
   *   response's body as they come in.
   * @returns A response object.
   * @throws std::exception May throw exceptions derived from std::exception in
   *   case of errors.
   */
  response post(request const& request,
                body_generator_function_type body_generator,
                body_callback_function_type body_handler =
                    body_callback_function_type()) {
    return pimpl->request_skeleton(request, "POST", true, body_handler,
                                   body_generator);
  }

  /**
   * Perform a POST request.
   *
   * @param[in] request The request including the URI and headers.
   * @param[in] body_generator The function to call to generate part of the body
   *   while the request is being performed.
   * @param[in] callback If provided, the function to call for parts of the
   *   response's body as they come in.
   * @returns A response object.
   * @throws std::exception May throw exceptions derived from std::exception in
   *   case of errors.
   */
  response post(request const& request,
                body_callback_function_type body_handler,
                body_generator_function_type body_generator =
                    body_generator_function_type()) {
    return post(request, string_type(), string_type(), body_handler,
                body_generator);
  }

  /**
   * Perform a POST request.
   *
   * @param[in] request The request object including the URI and headers.
   * @param[in] body The whole contents of the body.
   * @param[in] body_handler The callback invoked for parts of the response body
   *   as they come in.
   * @param[in] body_generator If provided, is invoked to generate parts of the
   *   request's body as it is being sent.
   * @returns A response object.
   * @throws std::exception May throw exceptions on errors, derived from
   *   `std::exception`.
   */
  response post(request const& request, string_type const& body,
                body_callback_function_type body_handler,
                body_generator_function_type body_generator =
                    body_generator_function_type()) {
    return post(request, body, string_type(), body_handler, body_generator);
  }

  /**
   * Perform a PUT request.
   *
   * @param[in] request A copy of the request object including the URI and
   *   headers.
   * @param[in] body The whole contents of the body. If provided, this overrides
   *   the body in the `request`.
   * @param[in] content_type The content type for the request. This overrides
   *   the content type in the `request`.
   * @param[in] body_handler The callback invoked for parts of the response body
   *   as they come in.
   * @param[in] body_generator If provided, is invoked to generate parts of the
   *   request's body as it is being sent.
   * @returns A response object.
   * @throws std::exception May throw exceptions on errors, derived from
   *   `std::exception`.
   */
  response put(
      request request, string_type const& body = string_type(),
      string_type const& content_type = string_type(),
      body_callback_function_type body_handler = body_callback_function_type(),
      body_generator_function_type body_generator =
          body_generator_function_type()) {
    return perform_request(request, "PUT", body, content_type,
                           body_handler, body_generator);
  }

  /**
   * Perform a PUT request.
   *
   * @param[in] request The request including the URI and headers.
   * @param[in] callback If provided, the function to call for parts of the
   *   response's body as they come in.
   * @param[in] body_generator The function to call to generate part of the body
   *   while the request is being performed.
   * @returns A response object.
   * @throws std::exception May throw exceptions derived from std::exception in
   *   case of errors.
   */
  response put(request const& request, body_callback_function_type callback,
               body_generator_function_type body_generator =
                   body_generator_function_type()) {
    return put(request, string_type(), string_type(), callback, body_generator);
  }

  /**
   * Perform a POST request.
   *
   * @param[in] request The request object including the URI and headers.
   * @param[in] body The whole contents of the body.
   * @param[in] body_handler The callback invoked for parts of the response body
   *   as they come in.
   * @param[in] body_generator If provided, is invoked to generate parts of the
   *   request's body as it is being sent.
   * @returns A response object.
   * @throws std::exception May throw exceptions on errors, derived from
   *   `std::exception`.
   */
  response put(request const& request, string_type body,
               body_callback_function_type body_handler,
               body_generator_function_type body_generator = {}) {
    return put(request, body, string_type(), body_handler, body_generator);
  }

  /**
   * Perform a PATCH request.
   *
   * @param[in] request A copy of the request object including the URI and
   *   headers.
   * @param[in] body The whole contents of the body. If provided, this overrides
   *   the body in the `request`.
   * @param[in] content_type The content type for the request. This overrides
   *   the content type in the `request`.
   * @param[in] body_handler The callback invoked for parts of the response body
   *   as they come in.
   * @param[in] body_generator If provided, is invoked to generate parts of the
   *   request's body as it is being sent.
   * @returns A response object.
   * @throws std::exception May throw exceptions on errors, derived from
   *   `std::exception`.
   */
  response patch(
      request request, string_type const& body = string_type(),
      string_type const& content_type = string_type(),
      body_callback_function_type body_handler = body_callback_function_type(),
      body_generator_function_type body_generator =
          body_generator_function_type()) {
    return perform_request(request, "PATCH", body, content_type,
                           body_handler, body_generator);
  }

  /**
   * Perform a PATCH request.
   *
   * @param[in] request The request including the URI and headers.
   * @param[in] callback If provided, the function to call for parts of the
   *   response's body as they come in.
   * @param[in] body_generator The function to call to generate part of the body
   *   while the request is being performed.
   * @returns A response object.
   * @throws std::exception May throw exceptions derived from std::exception in
   *   case of errors.
   */
  response patch(request const& request, body_callback_function_type callback,
               body_generator_function_type body_generator =
                   body_generator_function_type()) {
    return patch(request, string_type(), string_type(), callback, body_generator);
  }

  /**
   * Perform a PATCH request.
   *
   * @param[in] request The request object including the URI and headers.
   * @param[in] body The whole contents of the body.
   * @param[in] body_handler The callback invoked for parts of the response body
   *   as they come in.
   * @param[in] body_generator If provided, is invoked to generate parts of the
   *   request's body as it is being sent.
   * @returns A response object.
   * @throws std::exception May throw exceptions on errors, derived from
   *   `std::exception`.
   */
  response patch(request const& request, string_type body,
               body_callback_function_type body_handler,
               body_generator_function_type body_generator = {}) {
    return patch(request, body, string_type(), body_handler, body_generator);
  }

  /**
   * Perform a request.
   *
   * @param[in] request A copy of the request object including the URI and
   *   headers.
   * @param[in] method The HTTP method
   * @param[in] body The whole contents of the body. If provided, this overrides
   *   the body in the `request`.
   * @param[in] content_type The content type for the request. This overrides
   *   the content type in the `request`.
   * @param[in] body_handler The callback invoked for parts of the response body
   *   as they come in.
   * @param[in] body_generator If provided, is invoked to generate parts of the
   *   request's body as it is being sent.
   * @returns A response object.
   * @throws std::exception May throw exceptions on errors, derived from
   *   `std::exception`.
   */
  response perform_request(request request, string_type const& method,
                           string_type const& body = string_type(),
                           string_type const& content_type = string_type(),
                           body_callback_function_type body_handler =
                                body_callback_function_type(),
                           body_generator_function_type body_generator =
                                body_generator_function_type()) {
    if (body != string_type()) {
      request << remove_header("Content-Length")
              << header("Content-Length", std::to_string(body.size()))
              << boost::network::body(body);
    }
    typename headers_range<basic_request<Tag> >::type content_type_headers =
        headers(request)["Content-Type"];
    if (content_type != string_type()) {
      if (!boost::empty(content_type_headers))
        request << remove_header("Content-Type");
      request << header("Content-Type", content_type);
    } else {
      if (boost::empty(content_type_headers)) {
        typedef typename char_<Tag>::type char_type;
        static char_type const content_type[] = "x-application/octet-stream";
        request << header("Content-Type", content_type);
      }
    }
    return pimpl->request_skeleton(request, method, true, body_handler,
                                   body_generator);
  }

  /**
   * Perform a DELETE request.
   *
   * @param[in] request The request object including the URI and the headers.
   * @param[in] body_handler The callback invoked for parts of the response body
   *   as they come in, if provided.
   * @returns A response object.
   * @throws std::exception May throw exceptions on errors, derived from
   *   `std::exception`.
   */
  response delete_(request const& request,
                   body_callback_function_type body_handler = {}) {
    return pimpl->request_skeleton(request, "DELETE", true, body_handler,
                                   body_generator_function_type());
  }

  /**
   * Clears the cache of resolved endpoints.
   */
  void clear_resolved_cache() { pimpl->clear_resolved_cache(); }

 protected:
  std::shared_ptr<pimpl_type> pimpl;

  void init_pimpl(client_options<Tag> const& options) {
    pimpl.reset(new pimpl_type(
        options.cache_resolved(), options.follow_redirects(),
        options.always_verify_peer(), options.openssl_certificate(),
        options.openssl_verify_path(), options.openssl_certificate_file(),
        options.openssl_private_key_file(), options.openssl_ciphers(),
        options.openssl_sni_hostname(), options.openssl_options(),
        options.io_service(), options.timeout(),
        options.remove_chunk_markers()));
  }
};

}  // namespace http
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_FACADE_HPP_20100623
