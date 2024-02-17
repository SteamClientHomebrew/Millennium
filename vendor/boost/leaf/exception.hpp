#ifndef BOOST_LEAF_EXCEPTION_HPP_INCLUDED
#define BOOST_LEAF_EXCEPTION_HPP_INCLUDED

// Copyright 2018-2022 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf/config.hpp>
#include <boost/leaf/error.hpp>

#ifndef BOOST_LEAF_NO_EXCEPTIONS

#include <exception>

#define BOOST_LEAF_EXCEPTION ::boost::leaf::leaf_detail::inject_loc{__FILE__,__LINE__,__FUNCTION__}+::boost::leaf::exception
#define BOOST_LEAF_THROW_EXCEPTION ::boost::leaf::leaf_detail::throw_with_loc{__FILE__,__LINE__,__FUNCTION__}+::boost::leaf::exception

////////////////////////////////////////

namespace boost { namespace leaf {

namespace leaf_detail
{
    struct throw_with_loc
    {
        char const * const file;
        int const line;
        char const * const fn;

        template <class Ex>
        [[noreturn]] friend void operator+( throw_with_loc loc, Ex const & ex )
        {
            ex.load_source_location_(loc.file, loc.line, loc.fn);
            ::boost::leaf::throw_exception(ex);
        }
    };
}

} }

////////////////////////////////////////

namespace boost { namespace leaf {

namespace leaf_detail
{
    inline void enforce_std_exception( std::exception const & ) noexcept { }

    class BOOST_LEAF_SYMBOL_VISIBLE exception_base
    {
        std::shared_ptr<void const> auto_id_bump_;
    public:

        virtual error_id get_error_id() const noexcept = 0;

    protected:

        exception_base():
            auto_id_bump_(0, [](void const *) { (void) new_id(); })
        {
        }

        ~exception_base() noexcept { }
    };

    template <class Ex>
    class BOOST_LEAF_SYMBOL_VISIBLE exception:
        public Ex,
        public exception_base,
        public error_id
    {
        error_id get_error_id() const noexcept final override
        {
            return *this;
        }

    public:

        exception( exception const & ) = default;
        exception( exception && ) = default;

        BOOST_LEAF_CONSTEXPR exception( error_id id, Ex const & ex ) noexcept:
            Ex(ex),
            error_id(id)
        {
            enforce_std_exception(*this);
        }

        BOOST_LEAF_CONSTEXPR exception( error_id id, Ex && ex ) noexcept:
            Ex(std::move(ex)),
            error_id(id)
        {
            enforce_std_exception(*this);
        }

        explicit BOOST_LEAF_CONSTEXPR exception( error_id id ) noexcept:
            error_id(id)
        {
            enforce_std_exception(*this);
        }
    };

    template <class... T>
    struct at_least_one_derives_from_std_exception;

    template <>
    struct at_least_one_derives_from_std_exception<>: std::false_type { };

    template <class T, class... Rest>
    struct at_least_one_derives_from_std_exception<T, Rest...>
    {
        constexpr static const bool value = std::is_base_of<std::exception,typename std::remove_reference<T>::type>::value || at_least_one_derives_from_std_exception<Rest...>::value;
    };
}

template <class Ex, class... E>
inline
typename std::enable_if<std::is_base_of<std::exception,typename std::remove_reference<Ex>::type>::value, leaf_detail::exception<typename std::remove_reference<Ex>::type>>::type
exception( error_id err, Ex && ex, E && ... e ) noexcept
{
    static_assert(!leaf_detail::at_least_one_derives_from_std_exception<E...>::value, "Error objects passed to leaf::exception may not derive from std::exception");
    return leaf_detail::exception<typename std::remove_reference<Ex>::type>( err.load(std::forward<E>(e)...), std::forward<Ex>(ex) );
}

template <class E1, class... E>
inline
typename std::enable_if<!std::is_base_of<std::exception,typename std::remove_reference<E1>::type>::value, leaf_detail::exception<std::exception>>::type
exception( error_id err, E1 && car, E && ... cdr ) noexcept
{
    static_assert(!leaf_detail::at_least_one_derives_from_std_exception<E...>::value, "Error objects passed to leaf::exception may not derive from std::exception");
    return leaf_detail::exception<std::exception>( err.load(std::forward<E1>(car), std::forward<E>(cdr)...) );
}

inline leaf_detail::exception<std::exception> exception( error_id err ) noexcept
{
    return leaf_detail::exception<std::exception>(err);
}

template <class Ex, class... E>
inline
typename std::enable_if<std::is_base_of<std::exception,typename std::remove_reference<Ex>::type>::value, leaf_detail::exception<typename std::remove_reference<Ex>::type>>::type
exception( Ex && ex, E && ... e ) noexcept
{
    static_assert(!leaf_detail::at_least_one_derives_from_std_exception<E...>::value, "Error objects passed to leaf::exception may not derive from std::exception");
    return leaf_detail::exception<typename std::remove_reference<Ex>::type>( new_error().load(std::forward<E>(e)...), std::forward<Ex>(ex) );
}

template <class E1, class... E>
inline
typename std::enable_if<!std::is_base_of<std::exception,typename std::remove_reference<E1>::type>::value, leaf_detail::exception<std::exception>>::type
exception( E1 && car, E && ... cdr ) noexcept
{
    static_assert(!leaf_detail::at_least_one_derives_from_std_exception<E...>::value, "Error objects passed to leaf::exception may not derive from std::exception");
    return leaf_detail::exception<std::exception>( new_error().load(std::forward<E1>(car), std::forward<E>(cdr)...) );
}

inline leaf_detail::exception<std::exception> exception() noexcept
{
    return leaf_detail::exception<std::exception>(leaf::new_error());
}

////////////////////////////////////////

#ifndef BOOST_LEAF_NO_EXCEPTIONS

template <class T>
class result;

namespace leaf_detail
{
    inline error_id catch_exceptions_helper( std::exception const & ex, leaf_detail_mp11::mp_list<> )
    {
        return leaf::new_error(std::current_exception());
    }

    template <class Ex1, class... Ex>
    inline error_id catch_exceptions_helper( std::exception const & ex, leaf_detail_mp11::mp_list<Ex1,Ex...> )
    {
        if( Ex1 const * p = dynamic_cast<Ex1 const *>(&ex) )
            return catch_exceptions_helper(ex, leaf_detail_mp11::mp_list<Ex...>{ }).load(*p);
        else
            return catch_exceptions_helper(ex, leaf_detail_mp11::mp_list<Ex...>{ });
    }

    template <class T>
    struct deduce_exception_to_result_return_type_impl
    {
        using type = result<T>;
    };

    template <class T>
    struct deduce_exception_to_result_return_type_impl<result<T>>
    {
        using type = result<T>;
    };

    template <class T>
    using deduce_exception_to_result_return_type = typename deduce_exception_to_result_return_type_impl<T>::type;
}

template <class... Ex, class F>
inline
leaf_detail::deduce_exception_to_result_return_type<leaf_detail::fn_return_type<F>>
exception_to_result( F && f ) noexcept
{
    try
    {
        return std::forward<F>(f)();
    }
    catch( std::exception const & ex )
    {
        return leaf_detail::catch_exceptions_helper(ex, leaf_detail_mp11::mp_list<Ex...>());
    }
    catch(...)
    {
        return leaf::new_error(std::current_exception());
    }
}

#endif

} }

#endif

#endif
