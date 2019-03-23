//
// Copyright (c) 2018-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/BeastLounge
//

#ifndef LOUNGE_UTILITY_HPP
#define LOUNGE_UTILITY_HPP

#include "config.hpp"
#include <boost/smart_ptr/enable_shared_from_this.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>

//------------------------------------------------------------------------------

template<class T>
boost::shared_ptr<T>
shared_from(T* p)
{
    return boost::shared_ptr<T>(
        p->shared_from_this(), p);
}

template<class T>
boost::weak_ptr<T>
weak_from(T* p)
{
#if 0
    return boost::weak_ptr<T>(
        p->weak_from_this(), p);
#else
    return boost::weak_ptr<T>(
        boost::static_pointer_cast<T>(
            p->weak_from_this().lock()));
#endif
}

//------------------------------------------------------------------------------

namespace detail {

template<class T>
struct self_handler
{
    boost::shared_ptr<T> self;

    template<class... Args>
    void
    operator()(Args&&... args) const
    {
        (*self)(std::forward<Args>(args)...);
    }
};

template<class T, class MFn>
struct self_mfn_handler
{
    MFn mfn_;
    boost::shared_ptr<T> self;

    template<class... Args>
    void
    operator()(Args&&... args) const
    {
        ((*self).*mfn_)(std::forward<Args>(args)...);
    }
};

} // detail

/** Return an invocable handler for `this_`.

    This work-around for C++11 missing lambda-capture expression
    helps with implementations that use macro-based coroutines.
*/
template<class T>
detail::self_handler<T>
bind_self(T* this_)
{
    return {shared_from(this_)};
}

/** Return an invocable handler for `this_`.

    This is a very cheap version of std::bind_front which works
    for class member functions using shared_ptr.
*/
template<class T, class MFn>
detail::self_mfn_handler<T, MFn>
bind_self(T* this_, MFn mfn)
{
    return {mfn, shared_from(this_)};
}

//------------------------------------------------------------------------------

template<class T, class U>
boost::weak_ptr<T>
weak_ptr_cast(boost::weak_ptr<U> const& wp) noexcept
{
    return boost::static_pointer_cast<T>(wp.lock());
}

#endif
