/* 
 * File:   Undoer.hpp
 * Author: Niall Douglas
 *(C) 2012 Niall Douglas http://www.nedprod.com/
 * Created on June 25, 2013, 11:35 AM
 */

#ifndef UNDOER_HPP
#define	UNDOER_HPP

/*! \file Undoer.hpp
\brief Declares Undoer class and implementation
*/

#include<boost/config.hpp>
#include <utility>

#if defined(BOOST_MSVC) && BOOST_MSVC<=1700 && !defined(noexcept)
#define noexcept throw()
#endif
#if defined(BOOST_MSVC) && BOOST_MSVC<=1700 && !defined(constexpr)
#define constexpr const
#endif
#if defined(__GNUC__) && !defined(GCC_VERSION)
#define GCC_VERSION (__GNUC__ * 10000 \
				   + __GNUC_MINOR__ * 100 \
				   + __GNUC_PATCHLEVEL__)
#endif



namespace boost{
    namespace afio{
        namespace detail{
            
            namespace Impl {
                    template<typename T, bool iscomparable> struct is_nullptr { bool operator()(T c) const noexcept { return !c; } };
                    template<typename T> struct is_nullptr<T, false> { bool operator()(T) const noexcept { return false; } };
            }
            //! Compile-time safe detector of if \em v is nullptr (can cope with non-pointer convertibles)
            #if defined(__GNUC__) && GCC_VERSION<40900
            template<typename T> bool is_nullptr(T v) noexcept { return Impl::is_nullptr<T, std::is_constructible<bool, T>::value>()(std::forward<T>(v)); }
            #else
            template<typename T> bool is_nullptr(T v) noexcept { return Impl::is_nullptr<T, std::is_trivially_constructible<bool, T>::value>()(std::forward<T>(v)); }
            #endif

            
            template<typename callable> class UndoerImpl
            {
                    callable undoer;
                    bool _dismissed;

            #if !defined(BOOST_MSVC) || BOOST_MSVC>1700
                    UndoerImpl() = delete;
                    UndoerImpl(const UndoerImpl &) = delete;
                    UndoerImpl &operator=(const UndoerImpl &) = delete;
            #else
                    UndoerImpl();
                    UndoerImpl(const UndoerImpl &);
                    UndoerImpl &operator=(const UndoerImpl &);
            #endif
                    explicit UndoerImpl(callable &&c) : undoer(std::move(c)), _dismissed(false) { }
                    void int_trigger() { if(!_dismissed && !is_nullptr(undoer)) { undoer(); _dismissed=true; } }
            public:
                    UndoerImpl(UndoerImpl &&o) : undoer(std::move(o.undoer)), _dismissed(o._dismissed) { o._dismissed=true; }
                    UndoerImpl &operator=(UndoerImpl &&o) { int_trigger(); undoer=std::move(o.undoer); _dismissed=o._dismissed; o._dismissed=true; return *this; }
                    template<typename _callable> friend UndoerImpl<_callable> Undoer(_callable c);
                    ~UndoerImpl() { int_trigger(); }
                    //! Returns if the Undoer is dismissed
                    bool dismissed() const { return _dismissed; }
                    //! Dismisses the Undoer
                    void dismiss(bool d=true) { _dismissed=d; }
                    //! Undismisses the Undoer
                    void undismiss(bool d=true) { _dismissed=!d; }
            };//UndoerImpl


            /*! \brief Alexandrescu style rollbacks, a la C++ 11.

            Example of usage:
            \code
            auto resetpos=Undoer([&s]() { s.seekg(0, std::ios::beg); });
            ...
            resetpos.dismiss();
            \endcode
            */
            template<typename callable> inline UndoerImpl<callable> Undoer(callable c)
            {
                    //static_assert(!std::is_function<callable>::value && !std::is_member_function_pointer<callable>::value && !std::is_member_object_pointer<callable>::value && !has_call_operator<callable>::value, "Undoer applied to a type not providing a call operator");
                    auto foo=UndoerImpl<callable>(std::move(c));
                    return foo;
            }//Undoer

        }//namespace detail
    } // namesapce afio
}// namespace boost


#endif	/* UNDOER_HPP */

