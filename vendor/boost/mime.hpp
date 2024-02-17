//
//          Copyright Marshall Clow 2009-2010
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
//

#ifndef _BOOST_MIME_HPP
#define _BOOST_MIME_HPP

#include <iosfwd>
#include <list>
#include <string>
#include <vector>

#include <boost/fusion/adapted/struct.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/spirit/include/phoenix.hpp>  // pulls in all of Phoenix
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/support_istream_iterator.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/format.hpp>
#include <boost/shared_ptr.hpp>

// #define	DUMP_MIME_DATA	1

namespace boost {
namespace mime {

//	Errors are reported using this exception class
class mime_parsing_error : public std::runtime_error {
 public:
  explicit mime_parsing_error(const std::string &msg)
      : std::runtime_error(msg) {}
};

template <class traits>
class basic_mime;

namespace detail {

static const char *k_crlf = "\015\012";
static const char *k_package_name = "Proposed.Boost.Mime";
static const char *k_package_version = "0.1";
static const char *k_content_type_header = "Content-Type";
static const char *k_mime_version_header = "Mime-Version";

struct default_types {
  typedef std::string string_type;
  //	typedef std::pair < std::string, string_type > header_type;
  typedef std::vector<char> body_type;
};

template <typename string_type>
struct find_mime_header {
  find_mime_header(const char *str) : searchFor(str) {}
  bool operator()(const std::pair<std::string, string_type> &val) const {
    return boost::iequals(val.first, searchFor);
  }

 private:
  const char *searchFor;
};

#ifdef DUMP_MIME_DATA
struct tracer {
  tracer(const char *fn) : fn_(fn) { std::cout << "->" << fn_ << std::endl; }
  ~tracer() { std::cout << "<-" << fn_ << std::endl; }
  const char *fn_;
};
#else
struct tracer {
  tracer(const char *) {}
  ~tracer() {}
};
#endif

//	Parsing a Content-Type header
typedef std::pair<std::string, std::string> phrase_t;
typedef std::vector<phrase_t> phrase_container_t;
struct mime_content_type {
  std::string type;
  std::string sub_type;
  phrase_container_t phrases;
};

namespace qi = boost::spirit::qi;
namespace phx = boost::phoenix;
using boost::spirit::_val;
using boost::spirit::_1;

template <typename Iterator, typename Container>
struct mime_header_parser : qi::grammar<Iterator, Container()> {
  mime_header_parser() : mime_header_parser::base_type(mime_headerList) {
    mime_headerList = *(mime_header) >> crlf;
    mime_header = token >> qi::lit(':') >> value >> crlf;
    token = qi::char_("a-zA-Z") >> *qi::char_("a-zA-Z_0-9\\-");

    //	In Classifieds/000001, a header begins with a CRLF
    value = (valuePart[_val = _1] | qi::eps) >>
            *(valueCont[_val += "\015\012" + _1]);
    valueCont = crlf >> contWS[_val += _1] >> valuePart[_val += _1];
    valuePart = +qi::char_("\t -~");

    contWS = +qi::char_(" \t");
    crlf = qi::lit(k_crlf);

    /*	mime_headerList.name("mime-header-list");
        mime_header.name    ("mime-header");
        token.name          ("mime-token");
        valuePart.name      ("mime-value-part");
        value.name          ("mime-value");

        qi::on_error<qi::fail> ( mime_headerList,
            std::cout
                << phoenix::val("Error! Expecting ")
                << qi::labels::_4
                << phoenix::val(" here: \"")
                << phoenix::construct<std::string>(qi::labels::_3,
       qi::labels::_2)
                << phoenix::val("\"")
                << std::endl
            );
    */
  }

  qi::rule<Iterator, Container()> mime_headerList;
  qi::rule<Iterator, typename Container::value_type()> mime_header;
  qi::rule<Iterator, std::string()> token, value, valueCont, valuePart, contWS;
  qi::rule<Iterator> crlf;
};

template <typename Container, typename Iterator>
static Container read_headers(Iterator &begin, Iterator end) {
  tracer t(__func__);
  Container retVal;

  mime_header_parser<Iterator, Container> mh_parser;
  bool b = qi::parse(begin, end, mh_parser, retVal);
  if (!b) throw mime_parsing_error("Failed to parse headers");

#ifdef DUMP_MIME_DATA
  std::cout << "******Headers*******" << std::endl;
  for (typename Container::const_iterator iter = retVal.begin();
       iter != retVal.end(); ++iter) {
    std::string val = iter->second;
    size_t idx;
    while (std::string::npos != (idx = val.find(k_crlf)))
      val.replace(idx, std::strlen(k_crlf), "\n");
    std::cout << iter->first << ": " << val << std::endl;
  }
  std::cout << std::endl << "******Headers*******" << std::endl;
#endif

  return retVal;
}

//	The structure of a Content-Type mime header is taken from RFC 2045
//		http://www.ietf.org/rfc/rfc2045.txt, section 5.1

template <typename Iterator>
struct mime_content_type_parser : qi::grammar<Iterator, mime_content_type()> {
  mime_content_type_parser()
      : mime_content_type_parser::base_type(content_type_header) {
    content_type_header = *qi::lit(' ') >> part >> '/' >> sub_part >> *phrase;

    part = token | extension_token;
    sub_part = token | extension_token;

    phrase = qi::lit(';') >> +ws >> attribute >> '=' >> value >> *ws;
    ws = qi::char_(" \t") | line_sep | comment;
    line_sep = qi::lexeme[qi::lit(k_crlf)];

    attribute = token.alias();
    value = token | quoted_string;

    token = +(qi::char_(" -~") - qi::char_(" ()<>@,;:\\\"/[]?="));
    comment = qi::lit('(') >> +(qi::char_(" -~") - ')') >> qi::lit(')');
    quoted_string = qi::lit('"') >> +(qi::char_(" -~") - '"') >> qi::lit('"');
    extension_token = qi::char_("Xx") >> qi::lit('-') >> token;
  }

  qi::rule<Iterator, mime_content_type()> content_type_header;
  qi::rule<Iterator, phrase_t()> phrase;
  qi::rule<Iterator, std::string()> part, sub_part, token, attribute, value,
      quoted_string, extension_token;
  qi::rule<Iterator> ws, line_sep, comment;
};

template <typename string_type>
mime_content_type parse_content_type(const string_type &theHeader) {
  tracer t(__func__);
  mime_content_type retVal;
  typename string_type::const_iterator first = theHeader.begin();
  mime_content_type_parser<typename string_type::const_iterator> ct_parser;
  bool b = qi::parse(first, theHeader.end(), ct_parser, retVal);
  if (!b) throw mime_parsing_error("Failed to parse the 'Content-Type' header");

  return retVal;
}

template <typename string_type>
static string_type get_ct_value(const string_type &ctString, const char *key) {
  tracer t(__func__);
  mime_content_type mc = parse_content_type(ctString);
  for (phrase_container_t::const_iterator iter = mc.phrases.begin();
       iter != mc.phrases.end(); ++iter)
    if (boost::iequals(iter->first, key)) return iter->second;

  throw std::runtime_error(
      str(boost::format("Couldn't find Content-Type phrase (%s)") % key));
}

//	Replace this with a spirit thing later.
//	we're looking for '; boundary="<somevalue>".*'
std::string get_boundary(const std::string &ctString) {
  tracer t(__func__);
  return get_ct_value(ctString, "boundary");
}

//	Read the body of a multipart
//	Return a Container of containers, where the first is the actual
// body,
//	and the rest are the sub-parts.
//	Note that the body of the multipart can be empty.
//	If this is the case, then the first separator need not have a crlf

//	if the marker is "abcde", we could have:
//		Note that the separators are really CRLF--abcdeCRLF and
// CRLF--abcde--CRLF
//
//	multipart body
//	--abcde
//		sub part #1
//	--abcde
//		sub part #2
//	--abcde--
//
//	** or **
//	In this case, the first separator is --abcdeCRLF
//
//	--abcde		(no multipart body!)
//		sub part #1
//	--abcde
//		sub part #2
//	--abcde--

typedef std::vector<char> sub_part_t;
typedef std::vector<sub_part_t> sub_parts_t;

template <typename bodyContainer>
struct multipart_body_type {
  bool prolog_is_missing;
  bodyContainer body_prolog;
  sub_parts_t sub_parts;
  bodyContainer body_epilog;
};

//	Parse a mulitpart body.
//	Either "--boundaryCRLF" -- in which case the body is empty
//	or		<some sequence of chars> "CRLF--boundaryCRLF" -- in which
// case we return the sequence
//
//	I am deliberately not checking for a termination separator here
template <typename Iterator, typename Container>
struct multipart_body_parser : qi::grammar<Iterator, Container()> {
  multipart_body_parser(const std::string &boundary, bool &isMissing)
      : multipart_body_parser::base_type(mimeBody), m_is_missing(isMissing) {
    m_is_missing = false;
    //	Thanks to Michael Caisse for the hint to get this working
    mimeBody %=
        bareSep[phx::ref(m_is_missing) = true] | (+(qi::char_ - sep) >> sep);
    bareSep = qi::lit("--") >> boundary >> crlf;
    sep = crlf >> bareSep;
    crlf = qi::lit(k_crlf);
  }

  bool &m_is_missing;
  qi::rule<Iterator, Container()> mimeBody;
  qi::rule<Iterator> bareSep, sep, crlf;
};

//	Break up a multi-part into its' constituent sub parts.
template <typename Iterator, typename Container>
struct multipart_part_parser : qi::grammar<Iterator, Container()> {
  multipart_part_parser(const std::string &boundary)
      : multipart_part_parser::base_type(mimeParts) {
    mimeParts = (+(qi::char_ - sep) % (sep >> crlf)) > terminator;
    sep = crlf >> qi::lit("--") >> boundary;
    terminator = sep >> qi::lit("--") >> crlf;
    crlf = qi::lit(k_crlf);
  }
  qi::rule<Iterator, Container()> mimeParts;
  qi::rule<Iterator> sep, terminator, crlf;
};

template <typename Iterator, typename bodyContainer>
static void read_multipart_body(Iterator &begin, Iterator end,
                                multipart_body_type<bodyContainer> &mp_body,
                                const std::string &separator) {
  tracer t(__func__);
  typedef bodyContainer innerC;
  innerC mpBody;
  multipart_body_parser<Iterator, innerC> mb_parser(separator,
                                                    mp_body.prolog_is_missing);
  if (!qi::parse(begin, end, mb_parser, mp_body.body_prolog))
    throw mime_parsing_error("Failed to parse mime body(1)");

  multipart_part_parser<Iterator, sub_parts_t> mp_parser(separator);
  if (!qi::parse(begin, end, mp_parser, mp_body.sub_parts))
    throw mime_parsing_error("Failed to parse mime body(2)");
  std::copy(begin, end, std::back_inserter(mp_body.body_epilog));

#ifdef DUMP_MIME_DATA
  std::cout << std::endl << ">>****Multipart Body*******" << std::endl;
  std::cout << str(boost::format(
                       "Body size %d, sub part count = %d, trailer size "
                       "= %d %s") %
                   mp_body.body_prolog.size() % mp_body.sub_parts.size() %
                   mp_body.body_epilog.size() %
                   (mp_body.prolog_is_missing ? "(missing)" : ""))
            << std::endl;
  std::cout << std::endl << "****** Multipart Body Prolog *******" << std::endl;
  std::copy(mp_body.body_prolog.begin(), mp_body.body_prolog.end(),
            std::ostream_iterator<char>(std::cout));
  std::cout << std::endl << "****** Multipart Body Epilog *******" << std::endl;
  std::copy(mp_body.body_epilog.begin(), mp_body.body_epilog.end(),
            std::ostream_iterator<char>(std::cout));
  std::cout << std::endl << "<<****Multipart Body*******" << std::endl;
#endif
}

template <typename Container, typename Iterator>
static Container read_simplepart_body(Iterator &begin, Iterator end) {
  tracer t(__func__);
  Container retVal;
  std::copy(begin, end, std::back_inserter(retVal));

#ifdef DUMP_MIME_DATA
  std::cout << std::endl << ">>****SinglePart Body*******" << std::endl;
  std::cout << str(boost::format("Body size %d") % retVal.size()) << std::endl;
  std::copy(retVal.begin(), retVal.end(),
            std::ostream_iterator<char>(std::cout));
  std::cout << std::endl << "<<****SinglePart Body*******" << std::endl;
#endif
  return retVal;
}

//	FIXME: Need to break the headers at 80 chars...
template <typename headerList>
void write_headers(std::ostream &out, const headerList &headers) {
  if (headers.size() > 0) {
    for (typename headerList::const_iterator iter = headers.begin();
         iter != headers.end(); ++iter)
      out << iter->first << ':' << iter->second << detail::k_crlf;
  }
  out << detail::k_crlf;
}

template <typename bodyContainer>
void write_body(std::ostream &out, const bodyContainer &body) {
  std::copy(body.begin(), body.end(), std::ostream_iterator<char>(out));
}

inline void write_boundary(std::ostream &out, std::string boundary, bool isLast,
                           bool leadingCR = true) {
  if (leadingCR) out << detail::k_crlf;
  out << "--" << boundary;
  if (isLast) out << "--";
  out << detail::k_crlf;
}

template <typename Iterator, typename traits>
static boost::shared_ptr<basic_mime<traits> > parse_mime(
    Iterator &begin, Iterator end,
    const char *default_content_type = "text/plain");
}

template <class traits = detail::default_types>
class basic_mime {
 public:
  typedef enum { simple_part, multi_part, message_part } part_kind;
  //	Types for headers
  typedef typename traits::string_type string_type;
  typedef std::pair<std::string, string_type> headerEntry;
  typedef std::list<headerEntry> headerList;
  typedef typename headerList::iterator headerIter;
  typedef typename headerList::const_iterator constHeaderIter;

  //	Types for the parts
  typedef boost::shared_ptr<basic_mime> mimePtr;
  typedef std::vector<mimePtr> partList;
  typedef typename partList::iterator partIter;
  typedef typename partList::const_iterator constPartIter;

  //	Type for the body
  typedef typename traits::body_type bodyContainer;
  typedef boost::shared_ptr<bodyContainer> mimeBody;

  // -----------------------------------------------------------
  //	Constructors, destructor, assignment, and swap
  // -----------------------------------------------------------

  basic_mime(const char *type, const char *subtype)
      : m_body_prolog_is_missing(false),
        m_body(new bodyContainer),
        m_body_epilog(new bodyContainer) {
    if (NULL == type || NULL == subtype || 0 == std::strlen(type) ||
        0 == std::strlen(subtype))
      throw std::runtime_error(
          "Can't create a mime part w/o a type or subtype");

    //	We start with just two headers, "Content-Type:" and "Mime-Version"
    //	Everything else is optional.
    m_part_kind = part_kind_from_string_pair(type, subtype);
    std::string ctString = str(boost::format("%s/%s") % type % subtype);
    set_header_value(detail::k_content_type_header, ctString);
    set_header_value(detail::k_mime_version_header,
                     str(boost::format("1.0 (%s %s)") % detail::k_package_name %
                         detail::k_package_version));
  }

  basic_mime(const headerList &theHeaders,
             const string_type &default_content_type)
      : m_body_prolog_is_missing(false),
        m_body(new bodyContainer),
        m_body_epilog(new bodyContainer),
        m_default_content_type(default_content_type) {
    string_type ct = m_default_content_type;

    constHeaderIter found = std::find_if(
        theHeaders.begin(), theHeaders.end(),
        detail::find_mime_header<string_type>(detail::k_content_type_header));
    if (found != theHeaders.end()) ct = found->second;

    detail::mime_content_type mct = detail::parse_content_type(ct);
    m_part_kind = part_kind_from_string_pair(mct.type, mct.sub_type);
    m_headers = theHeaders;
  }

  basic_mime(const basic_mime &rhs)
      : m_part_kind(rhs.m_part_kind),
        m_headers(rhs.m_headers),
        m_body_prolog_is_missing(rhs.m_body_prolog_is_missing),
        m_body(new bodyContainer(*rhs.m_body)),
        m_body_epilog(new bodyContainer(*rhs.m_body_epilog)),
        /*	m_subparts ( rhs.m_subparts ), */ m_default_content_type(
            rhs.m_default_content_type) {
    //	Copy the parts -- not just the shared pointers
    for (typename partList::const_iterator iter = rhs.subpart_begin();
         iter != rhs.subpart_end(); ++iter)
      m_subparts.push_back(mimePtr(new basic_mime(**iter)));
  }

  //	Simple, copy constructor-based assignment
  //	If this is not efficient enough, then I can optimize it later
  basic_mime &operator=(const basic_mime &rhs) {
    basic_mime temp(rhs);
    this->swap(temp);
    return *this;
  }

  void swap(basic_mime &rhs) throw() {
    std::swap(m_part_kind, rhs.m_part_kind);
    std::swap(m_headers, rhs.m_headers);
    std::swap(m_body_prolog_is_missing, rhs.m_body_prolog_is_missing);
    std::swap(m_body, rhs.m_body);
    std::swap(m_body_epilog, rhs.m_body_epilog);
    std::swap(m_subparts, rhs.m_subparts);
    std::swap(m_default_content_type, rhs.m_default_content_type);
  }

  ~basic_mime() {}

  //	What kind of part is this (simple, multi, message)
  part_kind get_part_kind() const { return m_part_kind; }

  //	Sub-part information
  //	FIXME: Need some error checking here
  //		No sub-parts for simple parts, for example.
  size_t part_count() const { return m_subparts.size(); }

  boost::shared_ptr<basic_mime> operator[](std::size_t idx) const {
    check_subpart_index(idx);
    return m_subparts[idx];
  }

  void append_part(boost::shared_ptr<basic_mime> newPart) {
    check_subpart_append();
    m_subparts.push_back(newPart);
  }

  partIter subpart_begin() { return m_subparts.begin(); }
  partIter subpart_end() { return m_subparts.end(); }
  constPartIter subpart_begin() const { return m_subparts.begin(); }
  constPartIter subpart_end() const { return m_subparts.end(); }

  //	Reading the raw headers
  headerIter header_begin() { return m_headers.begin(); }
  headerIter header_end() { return m_headers.end(); }
  constHeaderIter header_begin() const { return m_headers.begin(); }
  constHeaderIter header_end() const { return m_headers.end(); }

  // -----------------------------------------------------------
  //	Header manipulation
  // -----------------------------------------------------------

  //	The 'tag' part of the header is still a std::string
  bool header_exists(const char *key) const {
    return header_end() != find_header(key);
  }

  string_type header_value(const char *key) const {
    constHeaderIter found = find_header(key);
    if (found == header_end())
      throw std::runtime_error("'header_value' not found");
    return found->second;
  }

  void set_header_value(const char *key, const string_type &value,
                        bool replace = false) {
    if (!replace)
      m_headers.push_back(std::make_pair(std::string(key), value));
    else {
      headerIter found = find_header(key);
      if (found == m_headers.end())
        throw std::runtime_error("'header_value' not found - can't replace");
      found->second = value;
    }
  }

  string_type get_content_type_header() const {
    constHeaderIter found = find_header(detail::k_content_type_header);
    return found != header_end() ? found->second : m_default_content_type;
  }

  string_type get_content_type() const {
    detail::mime_content_type mct =
        detail::parse_content_type(get_content_type_header());
    return string_type(mct.type) + '/' + mct.sub_type;
  }

  //	Special purpose helper routine
  void append_phrase_to_content_type(const char *key,
                                     const string_type &value) {
    headerIter found = find_header(detail::k_content_type_header);

    //	Create a Content-Type header if there isn't one
    if (m_headers.end() == found) {
      m_headers.push_back(std::make_pair(
          std::string(detail::k_content_type_header), m_default_content_type));
      found = find_header(detail::k_content_type_header);
    }

    detail::mime_content_type mct = detail::parse_content_type(found->second);
    detail::phrase_container_t::const_iterator p_found =
        std::find_if(mct.phrases.begin(), mct.phrases.end(),
                     detail::find_mime_header<std::string>(key));
    if (p_found != mct.phrases.end())
      throw std::runtime_error("phrase already exists");
    found->second += str(boost::format("; %s=\"%s\"") % key % value);
  }

  //	Body get/set methods
  mimeBody body() const { return m_body; }
  mimeBody body_prolog() const { return m_body; }
  mimeBody body_epilog() const { return m_body_epilog; }

  std::size_t body_size() const { return m_body->size(); }

  template <typename Iterator>
  void set_body(Iterator begin, Iterator end) {
    bodyContainer temp;
    std::copy(begin, end, std::back_inserter(temp));
    m_body->swap(temp);
  }

  void set_body(const char *contents, size_t sz) {
    set_body(contents, contents + sz);
  }
  void set_body(std::istream &in) {
    set_body(std::istream_iterator<char>(in), std::istream_iterator<char>());
  }
  void set_body(const bodyContainer &new_body) { *m_body = new_body; }

  void set_multipart_prolog_is_missing(bool isMissing) {
    m_body_prolog_is_missing = isMissing;
  }
  void set_body_prolog(const bodyContainer &new_body_prolog) {
    *m_body = new_body_prolog;
  }
  void set_body_epilog(const bodyContainer &new_body_epilog) {
    *m_body_epilog = new_body_epilog;
  }

  // -----------------------------------------------------------
  //	Output
  // -----------------------------------------------------------
  void stream_out(std::ostream &out) {  // called by operator <<
    if (m_part_kind == simple_part) {
      detail::write_headers(out, m_headers);
      detail::write_body(out, *m_body);
    } else if (m_part_kind == message_part) {
      if (m_subparts.size() != 1)
        throw std::runtime_error(
            "message part w/wrong number of sub-parts - should be 1");

      detail::write_headers(out, m_headers);
      m_subparts[0]->stream_out(out);
    } else {  //	multi-part
              //	Find or invent a boundary string
      std::string boundary;
      try {
        boundary = detail::get_boundary(get_content_type_header());
      } catch (std::runtime_error &) {
        //	FIXME: Make boundary strings (more?) unique
        boundary = str(boost::format("------=_NextPart-%s.%08ld") %
                       detail::k_package_name % std::clock());
        append_phrase_to_content_type("boundary", boundary);
      }

      //	If the body prolog is missing, we don't want a CRLF on the front
      // of the first sub-part.
      //	Note that there's a (subtle) difference between an zero length
      // body and a missing one.
      //	See the comments in the parser code for more information.
      detail::write_headers(out, m_headers);
      bool writeCR = body_prolog()->size() > 0 || !m_body_prolog_is_missing;
      detail::write_body(out, *body_prolog());
      for (typename partList::const_iterator iter = m_subparts.begin();
           iter != m_subparts.end(); ++iter) {
        detail::write_boundary(out, boundary, false, writeCR);
        (*iter)->stream_out(out);
        writeCR = true;
      }
      detail::write_boundary(out, boundary, true);
      detail::write_body(out, *body_epilog());
    }
    //	out << detail::k_crlf;
  }

  //	Build a simple mime part
  template <typename Iterator>
  static basic_mime make_simple_part(const char *type, const char *subtype,
                                     Iterator begin, Iterator end) {
    basic_mime retval(type, subtype);
    retval.set_body(begin, end);
    return retval;
  }

  //	Build a mime part from a pair of iterators
  template <typename Iterator>
  static boost::shared_ptr<basic_mime<traits> > parse_mime(Iterator &begin,
                                                           Iterator end) {
    return detail::parse_mime<Iterator, traits>(begin, end);
  }

  //	Build a mime part from a stream
  static boost::shared_ptr<basic_mime> parse_mime(std::istream &in) {
    boost::spirit::istream_iterator first(in);
    boost::spirit::istream_iterator last;
    return parse_mime(first, last);
  }

 private:
  basic_mime();  // Can't create a part w/o a type

  headerIter find_header(const char *key) {
    return std::find_if(header_begin(), header_end(),
                        detail::find_mime_header<string_type>(key));
  }

  constHeaderIter find_header(const char *key) const {
    return std::find_if(header_begin(), header_end(),
                        detail::find_mime_header<string_type>(key));
  }

  static part_kind part_kind_from_string_pair(const std::string &type,
                                              const std::string &sub_type) {
    if (boost::iequals(type, "multipart")) return multi_part;

    part_kind retVal = simple_part;
    //	I expect that this will get more complicated as time goes on....
    //
    //	message/delivery-status is a simple type.
    //	RFC 3464 defines message/delivery-status
    //<http://www.faqs.org/rfcs/rfc3464.html>
    //	The body of a message/delivery-status consists of one or more
    //	   "fields" formatted according to the ABNF of RFC 822 header
    //"fields"
    //	   (see [RFC822]).
    if (boost::iequals(type, "message"))
      if (!boost::iequals(sub_type, "delivery-status")) retVal = message_part;
    return retVal;
  }

  void check_subpart_index(size_t idx) const {
    if (get_part_kind() == simple_part)
      throw std::runtime_error("Simple Mime parts don't have sub-parts");
    else if (get_part_kind() == multi_part) {
      if (idx >= m_subparts.size())
        throw std::runtime_error(
            str(boost::format("Trying to access part %d (of %d) sub-part to a "
                              "multipart/xxx mime part") %
                idx % m_subparts.size()));
    } else {  // message-part
      if (get_part_kind() == message_part)
        if (m_subparts.size() > 1)
          throw std::runtime_error(
              "How did a message/xxx mime parts get more than one "
              "sub-part?");

      if (idx >= m_subparts.size())
        throw std::runtime_error(
            str(boost::format("Trying to access part %d (of %d) sub-part to a "
                              "message/xxx mime part") %
                idx % m_subparts.size()));
    }
  }

  void check_subpart_append() const {
    if (get_part_kind() == simple_part)
      throw std::runtime_error("Simple Mime parts don't have sub-parts");
    else if (get_part_kind() == message_part) {
      if (m_subparts.size() > 0)
        throw std::runtime_error(
            "Can't add a second sub-part to a message/xxx mime part");
    }
    //	else { /* Multi-part */ }	// We can always add to a multi-part
  }

  part_kind m_part_kind;
  headerList m_headers;
  bool m_body_prolog_is_missing;  // only for multiparts
  mimeBody m_body;
  mimeBody m_body_epilog;  // only for multiparts
  partList m_subparts;     // only for multiparts or message
  string_type m_default_content_type;
};

namespace detail {

template <typename Iterator, typename traits>
static boost::shared_ptr<basic_mime<traits> > parse_mime(
    Iterator &begin, Iterator end, const char *default_content_type) {
  tracer t(__func__);
  typedef typename boost::mime::basic_mime<traits> mime_part;

  shared_ptr<mime_part> retVal(new mime_part(
      detail::read_headers<typename mime_part::headerList>(begin, end),
      default_content_type));

  std::string content_type = retVal->get_content_type();

#ifdef DUMP_MIME_DATA
  std::cout << "Content-Type: " << content_type << std::endl;
  std::cout << str(boost::format("retVal->get_part_kind () = %d") %
                   ((int)retVal->get_part_kind()))
            << std::endl;
#endif

  if (retVal->get_part_kind() == mime_part::simple_part)
    retVal->set_body(
        detail::read_simplepart_body<typename mime_part::bodyContainer,
                                     Iterator>(begin, end));
  else if (retVal->get_part_kind() == mime_part::message_part) {
    //	If we've got a message/xxxx, then there is no body, and we have
    // a single
    //	embedded mime_part (which, of course, could be a multipart)
    retVal->append_part(parse_mime<Iterator, traits>(begin, end));
  } else /* multi_part */ {
    //	Find or invent a boundary string
    std::string part_separator =
        detail::get_boundary(retVal->get_content_type_header());
    const char *cont_type = boost::iequals(content_type, "multipart/digest")
                                ? "message/rfc822"
                                : "text/plain";

    detail::multipart_body_type<typename traits::body_type> body_and_subParts;
    detail::read_multipart_body(begin, end, body_and_subParts, part_separator);

    retVal->set_body_prolog(body_and_subParts.body_prolog);
    retVal->set_multipart_prolog_is_missing(
        body_and_subParts.prolog_is_missing);
    for (typename sub_parts_t::const_iterator iter =
             body_and_subParts.sub_parts.begin();
         iter != body_and_subParts.sub_parts.end(); ++iter) {
      typedef typename sub_part_t::const_iterator iter_type;
      iter_type b = iter->begin();
      iter_type e = iter->end();
      retVal->append_part(parse_mime<iter_type, traits>(b, e, cont_type));
    }
    retVal->set_body_epilog(body_and_subParts.body_epilog);
  }

  return retVal;
}
}

// -----------------------------------------------------------
//
//	Streaming
//
// -----------------------------------------------------------

template <typename traits>
inline std::ostream &operator<<(std::ostream &stream,
                                basic_mime<traits> &part) {
  part.stream_out(stream);
  return stream;
}

template <typename traits>
inline std::ostream &operator<<(std::ostream &stream,
                                boost::shared_ptr<basic_mime<traits> > part) {
  return stream << *part;
}
}
}

BOOST_FUSION_ADAPT_STRUCT(boost::mime::detail::mime_content_type,
                          (std::string, type)(std::string, sub_type)(
                              boost::mime::detail::phrase_container_t, phrases))

#endif  // _BOOST_MIME_HPP
