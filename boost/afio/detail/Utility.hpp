/* 
 * File:   Utility.hpp
 * Author: atlas
 *
 * Created on June 25, 2013, 12:43 PM
 */

#ifndef UTILITY_HPP
#define	UTILITY_HPP


#include <unordered_map>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef WIN32
#ifndef S_IFLNK
#define S_IFLNK 0x1400
#endif
#endif
#include "Undoer.hpp"
#include "ErrorHandling.hpp"

// We'll need some future checking before relying or including  std_filesystem.hpp
#include "std_filesystem.hpp"



#endif	/* UTILITY_HPP */

