
//          Copyright Dean Michael Berris 2007.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_MESSAGE_TRANSFORMERS_SELECTORS_HPP__
#define BOOST_NETWORK_MESSAGE_TRANSFORMERS_SELECTORS_HPP__

namespace boost {
namespace network {
namespace selectors {
struct source_selector;
struct destination_selector;
}  // namespace selectors

selectors::source_selector source_(selectors::source_selector /*unused*/);
selectors::destination_selector destination_(
    selectors::destination_selector /*unused*/);

namespace selectors {
struct source_selector {
 private:
  source_selector() = default;
  source_selector(source_selector const& /*unused*/) {};
  friend source_selector boost::network::source_(source_selector /*unused*/);
};

struct destination_selector {
 private:
  destination_selector() = default;
  destination_selector(destination_selector const& /*unused*/) {};
  friend destination_selector boost::network::destination_(
      destination_selector /*unused*/);
};
}  // namespace selectors

typedef selectors::source_selector (*source_selector_t)(
    selectors::source_selector);
typedef selectors::destination_selector (*destination_selector_t)(
    selectors::destination_selector);

inline selectors::source_selector source_(
    selectors::source_selector /*unused*/) {
  return selectors::source_selector();
}

inline selectors::destination_selector destination_(
    selectors::destination_selector /*unused*/) {
  return selectors::destination_selector();
}

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_MESSAGE_TRANSFORMERS_SELECTORS_HPP__
