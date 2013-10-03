/* NiallsCPP11Utilities
(C) 2012 Niall Douglas http://www.nedprod.com/
File Created: Nov 2012
*/

#define _CRT_SECURE_NO_WARNINGS

#include "../ErrorHandling.hpp"
#include "../../afio.hpp"
#include <locale>
#include <cstring>
#include "boost/exception/to_string.hpp"

using boost::to_string;

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <Windows.h>

namespace boost {
    namespace afio{
        namespace detail{

            
            BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC void int_throwWinError(const char *file, const char *function, int lineno, unsigned code, const std::filesystem::path *filename)
            {
                    using namespace std;
                    DWORD len;
                    char buffer[1024];
                    len=FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 0, code, 0, buffer, sizeof(buffer), 0);
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
                    string errstr(buffer, buffer+len);
                    errstr.append(" ("+to_string(code)+") in '"+string(file)+"':"+string(function)+":"+to_string(lineno));
                    if(ERROR_FILE_NOT_FOUND==code || ERROR_PATH_NOT_FOUND==code)
                    {
                            errstr="File '"+filename->generic_string()+"' not found [Host OS Error: "+errstr+"]";
                            BOOST_AFIO_THROW(ios_base::failure(errstr));
                    }
                    else if(ERROR_ACCESS_DENIED==code || ERROR_EA_ACCESS_DENIED==code)
                    {
                            errstr="Access to '"+filename->generic_string()+"' denied [Host OS Error: "+errstr+"]";
                            BOOST_AFIO_THROW(ios_base::failure(errstr));
                    }
                    else if(ERROR_NO_DATA==code || ERROR_BROKEN_PIPE==code || ERROR_PIPE_NOT_CONNECTED==code || ERROR_PIPE_LISTENING==code)
                    {
                            BOOST_AFIO_THROW(ios_base::failure(errstr));
                    }
                    else
                    {
                            BOOST_AFIO_THROW(ios_base::failure(errstr));
                    }
            }

            BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC void int_throwNTError(const char *file, const char *function, int lineno, unsigned code, const std::filesystem::path *filename)
            {
                    using namespace std;
                    DWORD len;
                    char buffer[1024];
                    len=FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_HMODULE,
                        GetModuleHandleA("NTDLL.DLL"), code, 0, buffer, sizeof(buffer), 0);
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
                    string errstr(buffer, buffer+len);
                    errstr.append(" ("+to_string(code)+") in '"+string(file)+"':"+string(function)+":"+to_string(lineno));
                    if(0xC000000F/*STATUS_NO_SUCH_FILE*/==code || 0xC000003A/*STATUS_OBJECT_PATH_NOT_FOUND*/==code)
                    {
                            errstr="File '"+filename->generic_string()+"' not found [Host OS Error: "+errstr+"]";
                            BOOST_AFIO_THROW(ios_base::failure(errstr));
                    }
                    else if(0xC0000022/*STATUS_ACCESS_DENIED*/==code)
                    {
                            errstr="Access to '"+filename->generic_string()+"' denied [Host OS Error: "+errstr+"]";
                            BOOST_AFIO_THROW(ios_base::failure(errstr));
                    }
                    else if(0xC00000B1/*STATUS_PIPE_CLOSING*/==code || 0xC00000AD/*STATUS_INVALID_PIPE_STATE*/==code || 0xC00000B0/*STATUS_PIPE_DISCONNECTED*/==code || 0xC00000B3/*STATUS_PIPE_LISTENING*/==code)
                    {
                            BOOST_AFIO_THROW(ios_base::failure(errstr));
                    }
                    else
                    {
                            BOOST_AFIO_THROW(ios_base::failure(errstr));
                    }
            }

        } // namespace detail
    }//namespace afio
}//namespace boost

#endif

namespace boost {
    namespace afio{
        namespace detail{

            BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC void int_throwOSError(const char *file, const char *function, int lineno, int code, const std::filesystem::path *filename)
            {
                    using namespace std;
                    string errstr(strerror(code));
                    errstr.append(" ("+to_string(code)+") in '"+string(file)+"':"+string(function)+":"+to_string(lineno));
                    if(ENOENT==code || ENOTDIR==code)
                    {
                            errstr="File '"+filename->generic_string()+"' not found [Host OS Error: "+errstr+"]";
                            BOOST_AFIO_THROW(ios_base::failure(errstr));
                    }
                    else if(EACCES==code)
                    {
                            errstr="Access to '"+filename->generic_string()+"' denied [Host OS Error: "+errstr+"]";
                            BOOST_AFIO_THROW(ios_base::failure(errstr));
                    }
                    else
                    {
                            BOOST_AFIO_THROW(ios_base::failure(errstr));
                    }
            }// end int_throwOSError
        }// namespace detail
    }// namespace afio
} // namespace boost
