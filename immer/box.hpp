//
// immer - immutable data structures for C++
// Copyright (C) 2017 Juan Pedro Bolivar Puente
//
// This file is part of immer.
//
// immer is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// immer is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with immer.  If not, see <http://www.gnu.org/licenses/>.
//

#pragma once

#include <immer/detail/util.hpp>
#include <immer/memory_policy.hpp>

namespace immer {

/*!
 * Immutable box for a single value of type `T`.
 *
 * The box is always copiable and movable. The `T` copy or move
 * operations are never called.  Since a box is immutable, copying or
 * moving just copy the underlying pointers.
 */
template <typename T,
          typename MemoryPolicy   = default_memory_policy>
class box
{
    struct holder : MemoryPolicy::refcount
    {
        T value;

        template <typename... Args>
        holder(Args&&... args) : value{std::forward<Args>(args)...} {}
    };

    using heap = typename MemoryPolicy::heap::
        template apply<sizeof(holder)>::type;

    holder* impl_ = nullptr;

    box(holder* impl) : impl_{impl} {}

public:
    using value_type    = T;
    using memory_policy = MemoryPolicy;

    /*!
     * Constructs a box holding `T{}`.
     */
    box() : impl_{detail::make<heap, holder>()} {}

    /*!
     * Constructs a box holding `T{arg}`
     */
    template <typename Arg,
              typename Enable=std::enable_if_t<
                  !std::is_same<box, std::decay_t<Arg>>::value>>
    box(Arg&& arg)
        : impl_{detail::make<heap, holder>(std::forward<Arg>(arg))} {}

    /*!
     * Constructs a box holding `T{arg1, arg2, args...}`
     */
    template <typename Arg1, typename Arg2, typename... Args>
    box(Arg1&& arg1, Arg2&& arg2, Args&& ...args)
        : impl_{detail::make<heap, holder>(
            std::forward<Arg1>(arg1),
            std::forward<Arg2>(arg2),
            std::forward<Args>(args)...)}
    {}

    friend void swap(box& a, box& b)
    { using std::swap; swap(a.impl_, b.impl_); }

    box(box&& other) { swap(*this, other); }
    box(const box& other) : impl_(other.impl_) { impl_->inc(); }
    box& operator=(box&& other) { swap(*this, other); return *this; }
    box& operator=(const box& other)
    {
        auto aux = other;
        swap(*this, aux);
        return *this;
    }
    ~box()
    {
        if (impl_ && impl_->dec()) {
            impl_->~holder();
            heap::deallocate(impl_);
        }
    }

    /*! Query the current value. */
    const T& get() const { return impl_->value; }

    /*! Conversion to the boxed type. */
    operator const T&() const { return get(); }

    /*! Access via dereference */
    const T& operator* () const { return get(); }

    /*! Access via pointer member access */
    const T* operator-> () const { return &get(); }

    /*! Comparison. */
    bool operator==(detail::exact_t<const box&> other) const
    { return impl_ == other.value.impl_ || get() == other.value.get(); }
    // Note that the `exact_t` disambiguates comparisons against `T{}`
    // directly.  In that case we want to use `operator T&` and
    // compare directly.  We definitely never want to convert a value
    // to a box (which causes an allocation) just to compare it.
    bool operator!=(detail::exact_t<const box&> other) const
    { return !(*this == other.value); }

    /*!
     * Returns a new box built by applying the `fn` to the underlying
     * value.
     *
     * @rst
     *
     * **Example**
     *   .. literalinclude:: ../example/box/box.cpp
     *      :language: c++
     *      :dedent: 8
     *      :start-after: update/start
     *      :end-before:  update/end
     *
     * @endrst
     */
    template <typename Fn>
    box update(Fn&& fn) const&
    {
        return std::forward<Fn>(fn)(get());
    }
    template <typename Fn>
    box&& update(Fn&& fn) &&
    {
        if (impl_->unique())
            impl_->value = std::forward<Fn>(fn)(std::move(impl_->value));
        else
            *this = std::forward<Fn>(fn)(impl_->value);
        return std::move(*this);
    }
};

} // namespace immer
