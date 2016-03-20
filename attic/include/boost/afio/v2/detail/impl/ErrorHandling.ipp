/* NiallsCPP11Utilities
(C) 2012 Niall Douglas http://www.nedprod.com/
File Created: Nov 2012
*/

#define _CRT_SECURE_NO_WARNINGS

#include "../../afio.hpp"
#include <locale>
#include <cstring>

BOOST_AFIO_V2_NAMESPACE_BEGIN
namespace detail {
  inline void push_exception(std::string name) noexcept
  {
#ifdef BOOST_AFIO_OP_STACKBACKTRACEDEPTH
    try
    {
      if(!afio_exception_stack())
        afio_exception_stack()=new afio_exception_stack_t;
      afio_exception_stack_entry se;
      se.name=std::move(name);
      collect_stack(se.stack);
      afio_exception_stack()->push_back(std::move(se));
    } catch(...) { }
#endif
  }
}
BOOST_AFIO_V2_NAMESPACE_END

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <Windows.h>

BOOST_AFIO_V2_NAMESPACE_BEGIN
  namespace detail{
            
            BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC void int_throwWinError(const char *file, const char *function, int lineno, unsigned code, std::function<BOOST_AFIO_V2_NAMESPACE::path()> filename)
            {
                if(ERROR_NOT_ENOUGH_MEMORY==code || ERROR_OUTOFMEMORY==code) BOOST_AFIO_THROW(std::bad_alloc());
                error_code ec(code, system_category());
                DWORD len;
                char buffer[1024];
                if(!(len=FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 0, code, 0, buffer, sizeof(buffer), 0)))
                {
                    strcpy(buffer, "unknown error code");
                    len=(DWORD) strlen(buffer);
                }
                // Remove annoying CRLF at end of message sometimes
                while(10==buffer[len-1])
                {
                    buffer[len-1]=0;
                    len--;
                    if(13==buffer[len-1])
                    {
                        buffer[len-1]=0;
                        len--;
                    }
                }
                std::string errstr(buffer, buffer+len);
                errstr.append(" ("+to_string(code)+") in '"+std::string(file)+"':"+std::string(function)+":"+to_string(lineno));
                // Add the filename where appropriate. This helps debugging a lot.
                if(filename)
                {
                    if(ERROR_FILE_NOT_FOUND==code || ERROR_PATH_NOT_FOUND==code)
                    {
                            errstr="File '"+filename().generic_string()+"' not found [Host OS Error: "+errstr+"]";
                    }
                    else if(ERROR_ACCESS_DENIED==code || ERROR_EA_ACCESS_DENIED==code)
                    {
                            errstr="Access to '"+filename().generic_string()+"' denied [Host OS Error: "+errstr+"]";
                    }
                    else
                    {
                      errstr.append(" [Path: '"+filename().generic_string()+"']");
                    }
                }
                BOOST_AFIO_DEBUG_PRINT("! %s\n", errstr.c_str());
                push_exception(errstr);
                BOOST_AFIO_THROW(system_error(ec, errstr));
            }

            BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC void int_throwNTError(const char *file, const char *function, int lineno, unsigned code, std::function<BOOST_AFIO_V2_NAMESPACE::path()> filename)
            {
                // system_category needs a win32 code, not NT kernel code
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 6387) // MSVC sanitiser warns on misuse of GetOverlappedResult
#endif
                {
                    DWORD br;
                    OVERLAPPED o;
             
                    o.Internal = code;
                    o.InternalHigh = 0;
                    o.Offset = 0;
                    o.OffsetHigh = 0;
                    o.hEvent = 0;
                    GetOverlappedResult(NULL, &o, &br, FALSE);
                }
#ifdef _MSC_VER
#pragma warning(pop)
#endif
                error_code ec(GetLastError(), system_category());
                DWORD len;
                char buffer[1024];
                if(!(len=FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_HMODULE,
                    GetModuleHandleA("NTDLL.DLL"), code, 0, buffer, sizeof(buffer), 0)))
                {
                    strcpy(buffer, "unknown error code");
                    len=(DWORD) strlen(buffer);
                }
                // Remove annoying CRLF at end of message sometimes
                while(10==buffer[len-1])
                {
                    buffer[len-1]=0;
                    len--;
                    if(13==buffer[len-1])
                    {
                        buffer[len-1]=0;
                        len--;
                    }
                }
                std::string errstr(buffer, buffer+len);
                errstr.append(" ("+to_string(code)+") in '"+std::string(file)+"':"+std::string(function)+":"+to_string(lineno));
                // Add the filename where appropriate. This helps debugging a lot.
                if(filename)
                {
                    if(0xC000000F/*STATUS_NO_SUCH_FILE*/==code || 0xC000003A/*STATUS_OBJECT_PATH_NOT_FOUND*/==code)
                    {
                        errstr="File '"+filename().generic_string()+"' not found [Host OS Error: "+errstr+"]";
                    }
                    else if(0xC0000022/*STATUS_ACCESS_DENIED*/==code)
                    {
                        errstr="Access to '"+filename().generic_string()+"' denied [Host OS Error: "+errstr+"]";
                    }
                    else
                    {
                      errstr.append(" [Path: '"+filename().generic_string()+"']");
                    }
                }
                BOOST_AFIO_DEBUG_PRINT("! %s\n", errstr.c_str());
                push_exception(errstr);
                BOOST_AFIO_THROW(system_error(ec, errstr));
            }

  } // namespace detail
BOOST_AFIO_V2_NAMESPACE_END

#endif

BOOST_AFIO_V2_NAMESPACE_BEGIN
  namespace detail{

            BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC void int_throwOSError(const char *file, const char *function, int lineno, int code, std::function<BOOST_AFIO_V2_NAMESPACE::path()> filename)
            {
                if(ENOMEM==code) BOOST_AFIO_THROW(std::bad_alloc());
                error_code ec(code, generic_category());
                std::string errstr(strerror(code));
                errstr.append(" ("+to_string(code)+") in '"+std::string(file)+"':"+std::string(function)+":"+to_string(lineno));
                // Add the filename where appropriate. This helps debugging a lot.
                if(filename)
                {
                    if(ENOENT==code || ENOTDIR==code)
                    {
                            errstr="File '"+filename().generic_string()+"' not found [Host OS Error: "+errstr+"]";
                    }
                    else if(EACCES==code)
                    {
                            errstr="Access to '"+filename().generic_string()+"' denied [Host OS Error: "+errstr+"]";
                    }
                    else
                    {
                      errstr.append(" [Path: '"+filename().generic_string()+"']");
                    }
                }
                BOOST_AFIO_DEBUG_PRINT("! %s\n", errstr.c_str());
                push_exception(errstr);
                BOOST_AFIO_THROW(system_error(ec, errstr));
            }// end int_throwOSError
  }// namespace detail
BOOST_AFIO_V2_NAMESPACE_END
