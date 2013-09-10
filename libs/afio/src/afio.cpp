/* async_file_io
Provides a threadpool and asynchronous file i/o infrastructure based on Boost.ASIO, Boost.Iostreams and filesystem
(C) 2013 Niall Douglas http://www.nedprod.com/
File Created: Mar 2013
*/

//#define BOOST_AFIO_MAX_NON_ASYNC_QUEUE_DEPTH 1

#ifndef BOOST_AFIO_MAX_NON_ASYNC_QUEUE_DEPTH
#define BOOST_AFIO_MAX_NON_ASYNC_QUEUE_DEPTH 8
#endif
//#define USE_POSIX_ON_WIN32 // Useful for testing

#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_DEPRECATE(a)

// This always compiles in input validation for this file only (the header file
// disables at the point of instance validation in release builds)
#ifndef BOOST_AFIO_NEVER_VALIDATE_INPUTS
#define BOOST_AFIO_VALIDATE_INPUTS 1
#endif

// Define this to have every allocated op take a backtrace from where it was allocated
//#ifndef BOOST_AFIO_OP_STACKBACKTRACEDEPTH
//#ifndef NDEBUG
//#define BOOST_AFIO_OP_STACKBACKTRACEDEPTH 8
//#endif
//#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef BOOST_AFIO_OP_STACKBACKTRACEDEPTH
#include <dlfcn.h>
// Set to 1 to use libunwind instead of glibc's stack backtracer
#if 0
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#else
#ifdef __linux__
#include <execinfo.h>
#else
#undef BOOST_AFIO_OP_STACKBACKTRACEDEPTH
#endif
#endif
#endif

// Get Mingw to assume we are on at least Windows 2000
#define __MSVCRT_VERSION__ 0x601

#include "../../../boost/afio/afio.hpp"
#include "../../../boost/afio/detail/Aligned_Allocator.hpp"
#include "boost/smart_ptr/detail/spinlock.hpp"
#include "../../../boost/afio/detail/valgrind/memcheck.h"
#include "../../../boost/afio/detail/valgrind/helgrind.h"

#include <fcntl.h>
#ifdef WIN32
#include <Windows.h>
#include <winioctl.h>

// We also compile the posix compat layer for catching silly compile errors for POSIX
#include <io.h>
#include <direct.h>
#define BOOST_AFIO_POSIX_MKDIR(path, mode) _wmkdir(path)
#define BOOST_AFIO_POSIX_RMDIR _wrmdir
#define BOOST_AFIO_POSIX_STAT_STRUCT struct __stat64
#define BOOST_AFIO_POSIX_STAT _wstat64
#define BOOST_AFIO_POSIX_LSTAT _wstat64
#define BOOST_AFIO_POSIX_OPEN _wopen
#define BOOST_AFIO_POSIX_CLOSE _close
#define BOOST_AFIO_POSIX_UNLINK _wunlink
#define BOOST_AFIO_POSIX_FSYNC _commit
#define BOOST_AFIO_POSIX_FTRUNCATE winftruncate

namespace windows_nt_kernel
{
	// From http://undocumented.ntinternals.net/UserMode/Undocumented%20Functions/NT%20Objects/File/FILE_INFORMATION_CLASS.html
	typedef enum _FILE_INFORMATION_CLASS {
		FileDirectoryInformation                 = 1,
		FileFullDirectoryInformation,
		FileBothDirectoryInformation,
		FileBasicInformation,
		FileStandardInformation,
		FileInternalInformation,
		FileEaInformation,
		FileAccessInformation,
		FileNameInformation,
		FileRenameInformation,
		FileLinkInformation,
		FileNamesInformation,
		FileDispositionInformation,
		FilePositionInformation,
		FileFullEaInformation,
		FileModeInformation,
		FileAlignmentInformation,
		FileAllInformation,
		FileAllocationInformation,
		FileEndOfFileInformation,
		FileAlternateNameInformation,
		FileStreamInformation,
		FilePipeInformation,
		FilePipeLocalInformation,
		FilePipeRemoteInformation,
		FileMailslotQueryInformation,
		FileMailslotSetInformation,
		FileCompressionInformation,
		FileObjectIdInformation,
		FileCompletionInformation,
		FileMoveClusterInformation,
		FileQuotaInformation,
		FileReparsePointInformation,
		FileNetworkOpenInformation,
		FileAttributeTagInformation,
		FileTrackingInformation,
		FileIdBothDirectoryInformation,
		FileIdFullDirectoryInformation,
		FileValidDataLengthInformation,
		FileShortNameInformation,
		FileIoCompletionNotificationInformation,
		FileIoStatusBlockRangeInformation,
		FileIoPriorityHintInformation,
		FileSfioReserveInformation,
		FileSfioVolumeInformation,
		FileHardLinkInformation,
		FileProcessIdsUsingFileInformation,
		FileNormalizedNameInformation,
		FileNetworkPhysicalNameInformation,
		FileIdGlobalTxDirectoryInformation,
		FileIsRemoteDeviceInformation,
		FileAttributeCacheInformation,
		FileNumaNodeInformation,
		FileStandardLinkInformation,
		FileRemoteProtocolInformation,
		FileMaximumInformation
	} FILE_INFORMATION_CLASS, *PFILE_INFORMATION_CLASS;

	typedef enum  { 
	  FileFsVolumeInformation       = 1,
	  FileFsLabelInformation        = 2,
	  FileFsSizeInformation         = 3,
	  FileFsDeviceInformation       = 4,
	  FileFsAttributeInformation    = 5,
	  FileFsControlInformation      = 6,
	  FileFsFullSizeInformation     = 7,
	  FileFsObjectIdInformation     = 8,
	  FileFsDriverPathInformation   = 9,
	  FileFsVolumeFlagsInformation  = 10,
	  FileFsSectorSizeInformation   = 11
	} FS_INFORMATION_CLASS;
#ifndef NTSTATUS
#define NTSTATUS LONG
#endif
#ifndef STATUS_TIMEOUT
#define STATUS_TIMEOUT ((NTSTATUS) 0x00000102)
#endif
#ifndef STATUS_PENDING
#define STATUS_PENDING ((NTSTATUS) 0x00000103)
#endif
#ifndef STATUS_BUFFER_OVERFLOW
#define STATUS_BUFFER_OVERFLOW ((NTSTATUS) 0x80000005)
#endif

	// From http://msdn.microsoft.com/en-us/library/windows/hardware/ff550671(v=vs.85).aspx
	typedef struct _IO_STATUS_BLOCK {
		union {
			NTSTATUS Status;
			PVOID    Pointer;
		};
		ULONG_PTR Information;
	} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

	// From http://msdn.microsoft.com/en-us/library/windows/desktop/aa380518(v=vs.85).aspx
	typedef struct _LSA_UNICODE_STRING {
	  USHORT Length;
	  USHORT MaximumLength;
	  PWSTR  Buffer;
	} LSA_UNICODE_STRING, *PLSA_UNICODE_STRING, UNICODE_STRING, *PUNICODE_STRING;

	// From http://msdn.microsoft.com/en-us/library/windows/hardware/ff557749(v=vs.85).aspx
	typedef struct _OBJECT_ATTRIBUTES {
	  ULONG           Length;
	  HANDLE          RootDirectory;
	  PUNICODE_STRING ObjectName;
	  ULONG           Attributes;
	  PVOID           SecurityDescriptor;
	  PVOID           SecurityQualityOfService;
	}  OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;


	// From http://undocumented.ntinternals.net/UserMode/Undocumented%20Functions/NT%20Objects/File/NtQueryInformationFile.html
	// and http://msdn.microsoft.com/en-us/library/windows/hardware/ff567052(v=vs.85).aspx
	typedef NTSTATUS (NTAPI *NtQueryInformationFile_t)(
		/*_In_*/   HANDLE FileHandle,
		/*_Out_*/  PIO_STATUS_BLOCK IoStatusBlock,
		/*_Out_*/  PVOID FileInformation,
		/*_In_*/   ULONG Length,
		/*_In_*/   FILE_INFORMATION_CLASS FileInformationClass
		);

	// From http://msdn.microsoft.com/en-us/library/windows/hardware/ff567070(v=vs.85).aspx
	typedef NTSTATUS (NTAPI *NtQueryVolumeInformationFile_t)(
		/*_In_*/   HANDLE FileHandle,
		/*_Out_*/  PIO_STATUS_BLOCK IoStatusBlock,
		/*_Out_*/  PVOID FsInformation,
		/*_In_*/   ULONG Length,
		/*_In_*/   FS_INFORMATION_CLASS FsInformationClass
		);

	// From http://msdn.microsoft.com/en-us/library/windows/hardware/ff566492(v=vs.85).aspx
	typedef NTSTATUS (NTAPI *NtOpenDirectoryObject_t)(
	  /*_Out_*/  PHANDLE DirectoryHandle,
	  /*_In_*/   ACCESS_MASK DesiredAccess,
	  /*_In_*/   POBJECT_ATTRIBUTES ObjectAttributes
	);


	// From http://msdn.microsoft.com/en-us/library/windows/hardware/ff567011(v=vs.85).aspx
	typedef NTSTATUS (NTAPI *NtOpenFile_t)(
	  /*_Out_*/  PHANDLE FileHandle,
	  /*_In_*/   ACCESS_MASK DesiredAccess,
	  /*_In_*/   POBJECT_ATTRIBUTES ObjectAttributes,
	  /*_Out_*/  PIO_STATUS_BLOCK IoStatusBlock,
	  /*_In_*/   ULONG ShareAccess,
	  /*_In_*/   ULONG OpenOptions
	);

	// From http://msdn.microsoft.com/en-us/library/windows/hardware/ff566424(v=vs.85).aspx
	typedef NTSTATUS (NTAPI *NtCreateFile_t)(
	  /*_Out_*/     PHANDLE FileHandle,
	  /*_In_*/      ACCESS_MASK DesiredAccess,
	  /*_In_*/      POBJECT_ATTRIBUTES ObjectAttributes,
	  /*_Out_*/     PIO_STATUS_BLOCK IoStatusBlock,
	  /*_In_opt_*/  PLARGE_INTEGER AllocationSize,
	  /*_In_*/      ULONG FileAttributes,
	  /*_In_*/      ULONG ShareAccess,
	  /*_In_*/      ULONG CreateDisposition,
	  /*_In_*/      ULONG CreateOptions,
	  /*_In_opt_*/  PVOID EaBuffer,
	  /*_In_*/      ULONG EaLength
	);

	typedef NTSTATUS (NTAPI *NtClose_t)(
	  /*_Out_*/  HANDLE FileHandle
	);

	// From http://undocumented.ntinternals.net/UserMode/Undocumented%20Functions/NT%20Objects/File/NtQueryDirectoryFile.html
	// and http://msdn.microsoft.com/en-us/library/windows/hardware/ff567047(v=vs.85).aspx
	typedef NTSTATUS (NTAPI *NtQueryDirectoryFile_t)(
		/*_In_*/      HANDLE FileHandle,
		/*_In_opt_*/  HANDLE Event,
		/*_In_opt_*/  void *ApcRoutine,
		/*_In_opt_*/  PVOID ApcContext,
		/*_Out_*/     PIO_STATUS_BLOCK IoStatusBlock,
		/*_Out_*/     PVOID FileInformation,
		/*_In_*/      ULONG Length,
		/*_In_*/      FILE_INFORMATION_CLASS FileInformationClass,
		/*_In_*/      BOOLEAN ReturnSingleEntry,
		/*_In_opt_*/  PUNICODE_STRING FileName,
		/*_In_*/      BOOLEAN RestartScan
		);

	// From http://undocumented.ntinternals.net/UserMode/Undocumented%20Functions/NT%20Objects/File/NtSetInformationFile.html
	// and http://msdn.microsoft.com/en-us/library/windows/hardware/ff567096(v=vs.85).aspx
	typedef NTSTATUS (NTAPI *NtSetInformationFile_t)(
		/*_In_*/   HANDLE FileHandle,
		/*_Out_*/  PIO_STATUS_BLOCK IoStatusBlock,
		/*_In_*/   PVOID FileInformation,
		/*_In_*/   ULONG Length,
		/*_In_*/   FILE_INFORMATION_CLASS FileInformationClass
		);

	// From http://msdn.microsoft.com/en-us/library/ms648412(v=vs.85).aspx
	typedef NTSTATUS (NTAPI *NtWaitForSingleObject_t)(
		/*_In_*/  HANDLE Handle,
		/*_In_*/  BOOLEAN Alertable,
		/*_In_*/  PLARGE_INTEGER Timeout
		);

	typedef struct _FILE_BASIC_INFORMATION {
	  LARGE_INTEGER CreationTime;
	  LARGE_INTEGER LastAccessTime;
	  LARGE_INTEGER LastWriteTime;
	  LARGE_INTEGER ChangeTime;
	  ULONG         FileAttributes;
	} FILE_BASIC_INFORMATION, *PFILE_BASIC_INFORMATION;

	typedef struct _FILE_STANDARD_INFORMATION {
	  LARGE_INTEGER AllocationSize;
	  LARGE_INTEGER EndOfFile;
	  ULONG         NumberOfLinks;
	  BOOLEAN       DeletePending;
	  BOOLEAN       Directory;
	} FILE_STANDARD_INFORMATION, *PFILE_STANDARD_INFORMATION;

	typedef struct _FILE_INTERNAL_INFORMATION {
	  LARGE_INTEGER IndexNumber;
	} FILE_INTERNAL_INFORMATION, *PFILE_INTERNAL_INFORMATION;

	typedef struct _FILE_EA_INFORMATION {
	  ULONG EaSize;
	} FILE_EA_INFORMATION, *PFILE_EA_INFORMATION;

	typedef struct _FILE_ACCESS_INFORMATION {
	  ACCESS_MASK AccessFlags;
	} FILE_ACCESS_INFORMATION, *PFILE_ACCESS_INFORMATION;

	typedef struct _FILE_POSITION_INFORMATION {
	  LARGE_INTEGER CurrentByteOffset;
	} FILE_POSITION_INFORMATION, *PFILE_POSITION_INFORMATION;

	typedef struct _FILE_MODE_INFORMATION {
	  ULONG Mode;
	} FILE_MODE_INFORMATION, *PFILE_MODE_INFORMATION;

	typedef struct _FILE_ALIGNMENT_INFORMATION {
	  ULONG AlignmentRequirement;
	} FILE_ALIGNMENT_INFORMATION, *PFILE_ALIGNMENT_INFORMATION;

	typedef struct _FILE_NAME_INFORMATION {
	  ULONG FileNameLength;
	  WCHAR FileName[1];
	} FILE_NAME_INFORMATION, *PFILE_NAME_INFORMATION;

	typedef struct _FILE_ALL_INFORMATION {
	  FILE_BASIC_INFORMATION     BasicInformation;
	  FILE_STANDARD_INFORMATION  StandardInformation;
	  FILE_INTERNAL_INFORMATION  InternalInformation;
	  FILE_EA_INFORMATION        EaInformation;
	  FILE_ACCESS_INFORMATION    AccessInformation;
	  FILE_POSITION_INFORMATION  PositionInformation;
	  FILE_MODE_INFORMATION      ModeInformation;
	  FILE_ALIGNMENT_INFORMATION AlignmentInformation;
	  FILE_NAME_INFORMATION      NameInformation;
	} FILE_ALL_INFORMATION, *PFILE_ALL_INFORMATION;

	typedef struct _FILE_FS_SECTOR_SIZE_INFORMATION {
	  ULONG LogicalBytesPerSector;
	  ULONG PhysicalBytesPerSectorForAtomicity;
	  ULONG PhysicalBytesPerSectorForPerformance;
	  ULONG FileSystemEffectivePhysicalBytesPerSectorForAtomicity;
	  ULONG Flags;
	  ULONG ByteOffsetForSectorAlignment;
	  ULONG ByteOffsetForPartitionAlignment;
	} FILE_FS_SECTOR_SIZE_INFORMATION, *PFILE_FS_SECTOR_SIZE_INFORMATION;

	// From http://msdn.microsoft.com/en-us/library/windows/hardware/ff540310(v=vs.85).aspx
	typedef struct _FILE_ID_FULL_DIR_INFORMATION  {
	  ULONG         NextEntryOffset;
	  ULONG         FileIndex;
	  LARGE_INTEGER CreationTime;
	  LARGE_INTEGER LastAccessTime;
	  LARGE_INTEGER LastWriteTime;
	  LARGE_INTEGER ChangeTime;
	  LARGE_INTEGER EndOfFile;
	  LARGE_INTEGER AllocationSize;
	  ULONG         FileAttributes;
	  ULONG         FileNameLength;
	  ULONG         EaSize;
	  LARGE_INTEGER FileId;
	  WCHAR         FileName[1];
	} FILE_ID_FULL_DIR_INFORMATION, *PFILE_ID_FULL_DIR_INFORMATION;

	// From http://msdn.microsoft.com/en-us/library/windows/hardware/ff552012(v=vs.85).aspx
	typedef struct _REPARSE_DATA_BUFFER {
	  ULONG  ReparseTag;
	  USHORT ReparseDataLength;
	  USHORT Reserved;
	  union {
		struct {
		  USHORT SubstituteNameOffset;
		  USHORT SubstituteNameLength;
		  USHORT PrintNameOffset;
		  USHORT PrintNameLength;
		  ULONG  Flags;
		  WCHAR  PathBuffer[1];
		} SymbolicLinkReparseBuffer;
		struct {
		  USHORT SubstituteNameOffset;
		  USHORT SubstituteNameLength;
		  USHORT PrintNameOffset;
		  USHORT PrintNameLength;
		  WCHAR  PathBuffer[1];
		} MountPointReparseBuffer;
		struct {
		  UCHAR DataBuffer[1];
		} GenericReparseBuffer;
	  };
	} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;

	static NtQueryInformationFile_t NtQueryInformationFile;
	static NtQueryVolumeInformationFile_t NtQueryVolumeInformationFile;
	static NtOpenDirectoryObject_t NtOpenDirectoryObject;
	static NtOpenFile_t NtOpenFile;
	static NtCreateFile_t NtCreateFile;
	static NtClose_t NtClose;
	static NtQueryDirectoryFile_t NtQueryDirectoryFile;
	static NtSetInformationFile_t NtSetInformationFile;
	static NtWaitForSingleObject_t NtWaitForSingleObject;

	static void doinit()
	{
		if(!NtQueryInformationFile)
			if(!(NtQueryInformationFile=(NtQueryInformationFile_t) GetProcAddress(GetModuleHandleA("NTDLL.DLL"), "NtQueryInformationFile")))
				abort();
		if(!NtQueryVolumeInformationFile)
			if(!(NtQueryVolumeInformationFile=(NtQueryVolumeInformationFile_t) GetProcAddress(GetModuleHandleA("NTDLL.DLL"), "NtQueryVolumeInformationFile")))
				abort();
		if(!NtOpenDirectoryObject)
			if(!(NtOpenDirectoryObject=(NtOpenDirectoryObject_t) GetProcAddress(GetModuleHandleA("NTDLL.DLL"), "NtOpenDirectoryObject")))
				abort();
		if(!NtOpenFile)
			if(!(NtOpenFile=(NtOpenFile_t) GetProcAddress(GetModuleHandleA("NTDLL.DLL"), "NtOpenFile")))
				abort();
		if(!NtCreateFile)
			if(!(NtCreateFile=(NtCreateFile_t) GetProcAddress(GetModuleHandleA("NTDLL.DLL"), "NtCreateFile")))
				abort();
		if(!NtClose)
			if(!(NtClose=(NtClose_t) GetProcAddress(GetModuleHandleA("NTDLL.DLL"), "NtClose")))
				abort();
		if(!NtQueryDirectoryFile)
			if(!(NtQueryDirectoryFile=(NtQueryDirectoryFile_t) GetProcAddress(GetModuleHandleA("NTDLL.DLL"), "NtQueryDirectoryFile")))
				abort();
		if(!NtSetInformationFile)
			if(!(NtSetInformationFile=(NtSetInformationFile_t) GetProcAddress(GetModuleHandleA("NTDLL.DLL"), "NtSetInformationFile")))
				abort();
		if(!NtWaitForSingleObject)
			if(!(NtWaitForSingleObject=(NtWaitForSingleObject_t) GetProcAddress(GetModuleHandleA("NTDLL.DLL"), "NtWaitForSingleObject")))
				abort();
	}
	static inline void init()
	{
		static bool initialised=false;
		if(!initialised)
		{
			doinit();
			initialised=true;
		}
	}

	static inline std::filesystem::path ntpath_from_dospath(std::filesystem::path p)
	{
		// This is pretty easy thanks to a convenient symlink in the NT kernel root directory ...
		std::filesystem::path base("\\??");
		base/=p;
		return base;
	}

	static inline std::filesystem::path dospath_from_ntpath(std::filesystem::path p)
	{
		auto first=++p.begin();
		if(*first=="??")
			p=std::filesystem::path(p.native().begin()+4, p.native().end());
		return p;
	}

	static inline uint16_t to_st_type(ULONG FileAttributes, uint16_t mode=0)
	{
		if(FileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)
			mode|=S_IFLNK;
		else if(FileAttributes&FILE_ATTRIBUTE_DIRECTORY)
			mode|=S_IFDIR;
		else
			mode|=S_IFREG;
		return mode;
	}

	static inline boost::afio::chrono::system_clock::time_point to_timepoint(LARGE_INTEGER time)
	{
		// We make the big assumption that the STL's system_clock is based on the time_t epoch 1st Jan 1970.
		static BOOST_CONSTEXPR_OR_CONST unsigned long long FILETIME_OFFSET_TO_1970=(((unsigned long long)27111902 << 32) + (unsigned long long)3577643008);
		// Need to have this self-adapt to the STL being used
		static BOOST_CONSTEXPR_OR_CONST unsigned long long STL_TICKS_PER_SEC=(unsigned long long) boost::afio::chrono::system_clock::period::den/boost::afio::chrono::system_clock::period::num;

		unsigned long long ticks_since_1970=(time.QuadPart - FILETIME_OFFSET_TO_1970); // In 100ns increments
		boost::afio::chrono::system_clock::duration duration(ticks_since_1970/(10000000ULL/STL_TICKS_PER_SEC));
		return boost::afio::chrono::system_clock::time_point(duration);
	}

	// Adapted from http://www.cprogramming.com/snippets/source-code/convert-ntstatus-win32-error
	// Could use RtlNtStatusToDosError() instead
	static inline void SetWin32LastErrorFromNtStatus(NTSTATUS ntstatus)
	{
		DWORD br;
		OVERLAPPED o;
 
		o.Internal = ntstatus;
		o.InternalHigh = 0;
		o.Offset = 0;
		o.OffsetHigh = 0;
		o.hEvent = 0;
		GetOverlappedResult(NULL, &o, &br, FALSE);
	}
} // namespace

// WinVista and later have the SetFileInformationByHandle() function, but for WinXP
// compatibility we use the kernel syscall directly
static inline bool wintruncate(HANDLE h, boost::afio::off_t newsize)
{
	windows_nt_kernel::init();
	using namespace windows_nt_kernel;
	IO_STATUS_BLOCK isb={ 0 };
	BOOST_AFIO_ERRHNT(NtSetInformationFile(h, &isb, &newsize, sizeof(newsize), FileEndOfFileInformation));
	return true;
}
static inline int winftruncate(int fd, boost::afio::off_t _newsize)
{
#if 1
	return wintruncate((HANDLE) _get_osfhandle(fd), _newsize) ? 0 : -1;
#else
	// This is a bit tricky ... overlapped files ignore their file position except in this one
	// case, but clearly here we have a race condition. No choice but to rinse and repeat I guess.
	LARGE_INTEGER size={0}, newsize={0};
	HANDLE h=(HANDLE) _get_osfhandle(fd);
	newsize.QuadPart=_newsize;
	while(size.QuadPart!=newsize.QuadPart)
	{
		BOOST_AFIO_ERRHWIN(SetFilePointerEx(h, newsize, NULL, FILE_BEGIN));
		BOOST_AFIO_ERRHWIN(SetEndOfFile(h));
		BOOST_AFIO_ERRHWIN(GetFileSizeEx(h, &size));
	}
#endif
}
static inline void fill_stat_t(boost::afio::stat_t &stat, BOOST_AFIO_POSIX_STAT_STRUCT s, boost::afio::metadata_flags wanted)
{
	using namespace boost::afio;
	if(!!(wanted&metadata_flags::dev)) { stat.st_dev=s.st_dev; }
	if(!!(wanted&metadata_flags::ino)) { stat.st_ino=s.st_ino; }
	if(!!(wanted&metadata_flags::type)) { stat.st_type=s.st_mode; }
	if(!!(wanted&metadata_flags::mode)) { stat.st_mode=s.st_mode; }
	if(!!(wanted&metadata_flags::nlink)) { stat.st_nlink=s.st_nlink; }
	if(!!(wanted&metadata_flags::uid)) { stat.st_uid=s.st_uid; }
	if(!!(wanted&metadata_flags::gid)) { stat.st_gid=s.st_gid; }
	if(!!(wanted&metadata_flags::rdev)) { stat.st_rdev=s.st_rdev; }
	if(!!(wanted&metadata_flags::atim)) { stat.st_atim=chrono::system_clock::from_time_t(s.st_atime); }
	if(!!(wanted&metadata_flags::mtim)) { stat.st_mtim=chrono::system_clock::from_time_t(s.st_mtime); }
	if(!!(wanted&metadata_flags::birthtim)) { stat.st_birthtim=chrono::system_clock::from_time_t(s.st_ctime); }
	if(!!(wanted&metadata_flags::size)) { stat.st_size=s.st_size; }
}
#else
#include <dirent.h>     /* Defines DT_* constants */
#include <sys/syscall.h>
#include <fnmatch.h>
#include <sys/uio.h>
#include <limits.h>
#define BOOST_AFIO_POSIX_MKDIR mkdir
#define BOOST_AFIO_POSIX_RMDIR ::rmdir
#define BOOST_AFIO_POSIX_STAT_STRUCT struct stat 
#define BOOST_AFIO_POSIX_STAT stat
#define BOOST_AFIO_POSIX_LSTAT ::lstat
#define BOOST_AFIO_POSIX_OPEN open
#define BOOST_AFIO_POSIX_SYMLINK ::symlink
#define BOOST_AFIO_POSIX_CLOSE ::close
#define BOOST_AFIO_POSIX_UNLINK unlink
#define BOOST_AFIO_POSIX_FSYNC fsync
#define BOOST_AFIO_POSIX_FTRUNCATE ftruncate

static inline boost::afio::chrono::system_clock::time_point to_timepoint(struct timespec ts)
{
	// Need to have this self-adapt to the STL being used
	static BOOST_CONSTEXPR_OR_CONST unsigned long long STL_TICKS_PER_SEC=(unsigned long long) boost::afio::chrono::system_clock::period::den/boost::afio::chrono::system_clock::period::num;
	// We make the big assumption that the STL's system_clock is based on the time_t epoch 1st Jan 1970.
	boost::afio::chrono::system_clock::duration duration(ts.tv_sec*STL_TICKS_PER_SEC+ts.tv_nsec/(1000000000ULL/STL_TICKS_PER_SEC));
	return boost::afio::chrono::system_clock::time_point(duration);
}
static inline void fill_stat_t(boost::afio::stat_t &stat, BOOST_AFIO_POSIX_STAT_STRUCT s, boost::afio::metadata_flags wanted)
{
	using namespace boost::afio;
	if(!!(wanted&metadata_flags::dev)) { stat.st_dev=s.st_dev; }
	if(!!(wanted&metadata_flags::ino)) { stat.st_ino=s.st_ino; }
	if(!!(wanted&metadata_flags::type)) { stat.st_type=s.st_mode; }
	if(!!(wanted&metadata_flags::mode)) { stat.st_mode=s.st_mode; }
	if(!!(wanted&metadata_flags::nlink)) { stat.st_nlink=s.st_nlink; }
	if(!!(wanted&metadata_flags::uid)) { stat.st_uid=s.st_uid; }
	if(!!(wanted&metadata_flags::gid)) { stat.st_gid=s.st_gid; }
	if(!!(wanted&metadata_flags::rdev)) { stat.st_rdev=s.st_rdev; }
	if(!!(wanted&metadata_flags::atim)) { stat.st_atim=to_timepoint(s.st_atim); }
	if(!!(wanted&metadata_flags::mtim)) { stat.st_mtim=to_timepoint(s.st_mtim); }
	if(!!(wanted&metadata_flags::ctim)) { stat.st_ctim=to_timepoint(s.st_ctim); }
	if(!!(wanted&metadata_flags::size)) { stat.st_size=s.st_size; }
	if(!!(wanted&metadata_flags::allocated)) { stat.st_allocated=s.st_blocks*s.st_blksize; }
	if(!!(wanted&metadata_flags::blocks)) { stat.st_blocks=s.st_blocks; }
	if(!!(wanted&metadata_flags::blksize)) { stat.st_blksize=s.st_blksize; }
#ifdef HAVE_STAT_FLAGS
	if(!!(wanted&metadata_flags::flags)) { stat.st_flags=s.st_flags; }
#endif
#ifdef HAVE_STAT_GEN
	if(!!(wanted&metadata_flags::gen)) { stat.st_gen=s.st_gen; }
#endif
#ifdef HAVE_BIRTHTIMESPEC
	if(!!(wanted&metadata_flags::birthtim)) { stat.st_birthtim=to_timepoint(s.st_birthtim); }
#endif
}
#endif

// libstdc++ doesn't come with std::lock_guard
#define BOOST_AFIO_LOCK_GUARD boost::lock_guard

#if defined(DEBUG) && 0
#define BOOST_AFIO_DEBUG_PRINTING 1
#ifdef WIN32
#define BOOST_AFIO_DEBUG_PRINT(...) \
	{ \
		char buffer[16384]; \
		sprintf(buffer, __VA_ARGS__); \
		OutputDebugStringA(buffer); \
	}
#else
#define BOOST_AFIO_DEBUG_PRINT(...) \
	{ \
		fprintf(stderr, __VA_ARGS__); \
	}
#endif
#else
#define BOOST_AFIO_DEBUG_PRINT(...)
#endif


#ifdef WIN32
#ifndef IOV_MAX
#define IOV_MAX 1024
#endif
struct iovec {
    void  *iov_base;    /* Starting address */
    size_t iov_len;     /* Number of bytes to transfer */
};
typedef ptrdiff_t ssize_t;
static boost::detail::spinlock preadwritelock;
ssize_t preadv(int fd, const struct iovec *iov, int iovcnt, boost::afio::off_t offset)
{
	boost::afio::off_t at=offset;
	ssize_t transferred;
	BOOST_AFIO_LOCK_GUARD<boost::detail::spinlock> lockh(preadwritelock);
	if(-1==_lseeki64(fd, offset, SEEK_SET)) return -1;
	for(; iovcnt; iov++, iovcnt--, at+=(boost::afio::off_t) transferred)
		if(-1==(transferred=_read(fd, iov->iov_base, (unsigned) iov->iov_len))) return -1;
	return (ssize_t)(at-offset);
}
ssize_t pwritev(int fd, const struct iovec *iov, int iovcnt, boost::afio::off_t offset)
{
	boost::afio::off_t at=offset;
	ssize_t transferred;
	BOOST_AFIO_LOCK_GUARD<boost::detail::spinlock> lockh(preadwritelock);
	if(-1==_lseeki64(fd, offset, SEEK_SET)) return -1;
	for(; iovcnt; iov++, iovcnt--, at+=(boost::afio::off_t) transferred)
		if(-1==(transferred=_write(fd, iov->iov_base, (unsigned) iov->iov_len))) return -1;
	return (ssize_t)(at-offset);
}
#endif


namespace boost { namespace afio {

#ifdef BOOST_AFIO_NEED_CURRENT_EXCEPTION_HACK
	namespace detail { boost::exception_ptr &vs2010_lack_of_decent_current_exception_support_hack() { static boost::exception_ptr v; return v; } }
#endif

size_t async_file_io_dispatcher_base::page_size() BOOST_NOEXCEPT_OR_NOTHROW
{
	static size_t pagesize;
	if(!pagesize)
	{
#ifdef WIN32
		SYSTEM_INFO si={ 0 };
		GetSystemInfo(&si);
		pagesize=si.dwPageSize;
#else
		pagesize=getpagesize();
#endif
	}
	return pagesize;
}

std::shared_ptr<std_thread_pool> process_threadpool()
{
	// This is basically how many file i/o operations can occur at once
	// Obviously the kernel also has a limit
	static std::weak_ptr<std_thread_pool> shared;
	static boost::detail::spinlock lock;
	std::shared_ptr<std_thread_pool> ret(shared.lock());
	if(!ret)
	{
		BOOST_AFIO_LOCK_GUARD<boost::detail::spinlock> lockh(lock);
		ret=shared.lock();
		if(!ret)
		{
			shared=ret=std::make_shared<std_thread_pool>(BOOST_AFIO_MAX_NON_ASYNC_QUEUE_DEPTH);
		}
	}
	return ret;
}


namespace detail {

#if defined(WIN32)
	struct async_io_handle_windows : public async_io_handle
	{
		std::shared_ptr<async_file_io_dispatcher_base> parent;
		std::unique_ptr<boost::asio::windows::random_access_handle> h;
		void *myid;
		bool has_been_added, SyncOnClose;

		static HANDLE int_checkHandle(HANDLE h, const std::filesystem::path &path)
		{
			BOOST_AFIO_ERRHWINFN(INVALID_HANDLE_VALUE!=h, path);
			return h;
		}
		async_io_handle_windows(std::shared_ptr<async_file_io_dispatcher_base> _parent, std::shared_ptr<async_io_handle> _dirh, const std::filesystem::path &path, file_flags flags) : async_io_handle(_parent.get(), std::move(_dirh), path, flags), parent(_parent), myid(nullptr), has_been_added(false), SyncOnClose(false) { }
		inline async_io_handle_windows(std::shared_ptr<async_file_io_dispatcher_base> _parent, std::shared_ptr<async_io_handle> _dirh, const std::filesystem::path &path, file_flags flags, bool _SyncOnClose, HANDLE _h);
		virtual void *native_handle() const { return myid; }

		// You can't use shared_from_this() in a constructor so ...
		void do_add_io_handle_to_parent()
		{
			if(h)
			{
				parent->int_add_io_handle(myid, shared_from_this());
				has_been_added=true;
			}
		}
		~async_io_handle_windows()
		{
			BOOST_AFIO_DEBUG_PRINT("D %p\n", this);
			if(has_been_added)
				parent->int_del_io_handle(myid);
			if(h)
			{
				if(SyncOnClose && write_count_since_fsync())
					BOOST_AFIO_ERRHWINFN(FlushFileBuffers(h->native_handle()), path());
				h->close();
			}
		}
		virtual stat_t lstat(metadata_flags wanted) const
		{
			windows_nt_kernel::init();
			using namespace windows_nt_kernel;
			stat_t stat(nullptr);
			IO_STATUS_BLOCK isb={ 0 };
			NTSTATUS ntstat;

			HANDLE h=myid;
			BOOST_AFIO_TYPEALIGNMENT(8) std::filesystem::path::value_type buffer[sizeof(FILE_ALL_INFORMATION)/sizeof(std::filesystem::path::value_type)+32769];
			FILE_ALL_INFORMATION &fai=*(FILE_ALL_INFORMATION *)buffer;
			FILE_FS_SECTOR_SIZE_INFORMATION ffssi={0};
			bool needInternal=!!(wanted&metadata_flags::ino);
			bool needBasic=(!!(wanted&metadata_flags::type) || !!(wanted&metadata_flags::atim) || !!(wanted&metadata_flags::mtim) || !!(wanted&metadata_flags::ctim) || !!(wanted&metadata_flags::birthtim));
			bool needStandard=(!!(wanted&metadata_flags::nlink) || !!(wanted&metadata_flags::size) || !!(wanted&metadata_flags::allocated) || !!(wanted&metadata_flags::blocks));
			// It's not widely known that the NT kernel supplies a stat() equivalent i.e. get me everything in a single syscall
			// However fetching FileAlignmentInformation which comes with FILE_ALL_INFORMATION is slow as it touches the device driver,
			// so only use if we need more than one item
			if((needInternal+needBasic+needStandard)>=2)
			{
				ntstat=NtQueryInformationFile(h, &isb, &fai, sizeof(buffer), FileAllInformation);
				if(STATUS_PENDING==ntstat)
					ntstat=NtWaitForSingleObject(h, FALSE, NULL);
				BOOST_AFIO_ERRHNTFN(ntstat, path());
			}
			else
			{
				if(needInternal)
				{
					ntstat=NtQueryInformationFile(h, &isb, &fai.InternalInformation, sizeof(fai.InternalInformation), FileInternalInformation);
					if(STATUS_PENDING==ntstat)
						ntstat=NtWaitForSingleObject(h, FALSE, NULL);
					BOOST_AFIO_ERRHNTFN(ntstat, path());
				}
				if(needBasic)
				{
					ntstat=NtQueryInformationFile(h, &isb, &fai.BasicInformation, sizeof(fai.BasicInformation), FileBasicInformation);
					if(STATUS_PENDING==ntstat)
						ntstat=NtWaitForSingleObject(h, FALSE, NULL);
					BOOST_AFIO_ERRHNTFN(ntstat, path());
				}
				if(needStandard)
				{
					ntstat=NtQueryInformationFile(h, &isb, &fai.StandardInformation, sizeof(fai.StandardInformation), FileStandardInformation);
					if(STATUS_PENDING==ntstat)
						ntstat=NtWaitForSingleObject(h, FALSE, NULL);
					BOOST_AFIO_ERRHNTFN(ntstat, path());
				}
			}
			if(!!(wanted&metadata_flags::blocks) || !!(wanted&metadata_flags::blksize))
			{
				ntstat=NtQueryVolumeInformationFile(h, &isb, &ffssi, sizeof(ffssi), FileFsSectorSizeInformation);
				if(STATUS_PENDING==ntstat)
					ntstat=NtWaitForSingleObject(h, FALSE, NULL);
				//BOOST_AFIO_ERRHNTFN(ntstat, path());
                if(0/*STATUS_SUCCESS*/!=ntstat)
                {
                    // Windows XP and Vista don't support the FILE_FS_SECTOR_SIZE_INFORMATION
                    // API, so we'll just hardcode 512 bytes
                    ffssi.PhysicalBytesPerSectorForPerformance=512;
                }
			}
			if(!!(wanted&metadata_flags::ino)) { stat.st_ino=fai.InternalInformation.IndexNumber.QuadPart; }
			if(!!(wanted&metadata_flags::type)) { stat.st_type=to_st_type(fai.BasicInformation.FileAttributes); }
			if(!!(wanted&metadata_flags::nlink)) { stat.st_nlink=(int16_t) fai.StandardInformation.NumberOfLinks; }
			if(!!(wanted&metadata_flags::atim)) { stat.st_atim=to_timepoint(fai.BasicInformation.LastAccessTime); }
			if(!!(wanted&metadata_flags::mtim)) { stat.st_mtim=to_timepoint(fai.BasicInformation.LastWriteTime); }
			if(!!(wanted&metadata_flags::ctim)) { stat.st_ctim=to_timepoint(fai.BasicInformation.ChangeTime); }
			if(!!(wanted&metadata_flags::size)) { stat.st_size=fai.StandardInformation.EndOfFile.QuadPart; }
			if(!!(wanted&metadata_flags::allocated)) { stat.st_allocated=fai.StandardInformation.AllocationSize.QuadPart; }
			if(!!(wanted&metadata_flags::blocks)) { stat.st_blocks=fai.StandardInformation.AllocationSize.QuadPart/ffssi.PhysicalBytesPerSectorForPerformance; }
			if(!!(wanted&metadata_flags::blksize)) { stat.st_blksize=(uint16_t) ffssi.PhysicalBytesPerSectorForPerformance; }
			if(!!(wanted&metadata_flags::birthtim)) { stat.st_birthtim=to_timepoint(fai.BasicInformation.CreationTime); }
			return stat;
		}
		virtual std::filesystem::path target() const
		{
			if(!opened_as_symlink())
				return std::filesystem::path();
			windows_nt_kernel::init();
			using namespace windows_nt_kernel;
			using windows_nt_kernel::REPARSE_DATA_BUFFER;
			BOOST_AFIO_TYPEALIGNMENT(8) char buffer[sizeof(REPARSE_DATA_BUFFER)+32769];
			DWORD written=0;
			REPARSE_DATA_BUFFER *rpd=(REPARSE_DATA_BUFFER *) buffer;
			memset(rpd, 0, sizeof(*rpd));
			BOOST_AFIO_ERRHWIN(DeviceIoControl(myid, FSCTL_GET_REPARSE_POINT, NULL, 0, rpd, sizeof(buffer), &written, NULL));
			switch(rpd->ReparseTag)
			{
			case IO_REPARSE_TAG_MOUNT_POINT:
				return dospath_from_ntpath(std::filesystem::path::string_type(rpd->MountPointReparseBuffer.PathBuffer+rpd->MountPointReparseBuffer.SubstituteNameOffset/sizeof(std::filesystem::path::value_type), rpd->MountPointReparseBuffer.SubstituteNameLength/sizeof(std::filesystem::path::value_type)));
			case IO_REPARSE_TAG_SYMLINK:
				return dospath_from_ntpath(std::filesystem::path::string_type(rpd->SymbolicLinkReparseBuffer.PathBuffer+rpd->SymbolicLinkReparseBuffer.SubstituteNameOffset/sizeof(std::filesystem::path::value_type), rpd->SymbolicLinkReparseBuffer.SubstituteNameLength/sizeof(std::filesystem::path::value_type)));
			}
			BOOST_AFIO_THROW(std::runtime_error("Unknown type of symbolic link."));
		}
	};
#endif
	struct async_io_handle_posix : public async_io_handle
	{
		std::shared_ptr<async_file_io_dispatcher_base> parent;
		int fd;
		bool has_been_added, SyncOnClose, has_ever_been_fsynced;

		async_io_handle_posix(std::shared_ptr<async_file_io_dispatcher_base> _parent, std::shared_ptr<async_io_handle> _dirh, const std::filesystem::path &path, file_flags flags, bool _SyncOnClose, int _fd) : async_io_handle(_parent.get(), std::move(_dirh), path, flags), parent(_parent), fd(_fd), has_been_added(false), SyncOnClose(_SyncOnClose),has_ever_been_fsynced(false)
		{
			if(fd!=-999)
				BOOST_AFIO_ERRHOSFN(fd, path);
		}
		virtual void *native_handle() const { return (void *)(size_t) fd; }

		// You can't use shared_from_this() in a constructor so ...
		void do_add_io_handle_to_parent()
		{
			parent->int_add_io_handle((void *)(size_t)fd, shared_from_this());
			has_been_added=true;
		}
		~async_io_handle_posix()
		{
			if(has_been_added)
				parent->int_del_io_handle((void *)(size_t)fd);
			if(fd>=0)
			{
				// Flush synchronously here? I guess ...
				if(SyncOnClose && write_count_since_fsync())
					BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_FSYNC(fd), path());
				BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_CLOSE(fd), path());
				fd=-1;
			}
		}
		virtual stat_t lstat(metadata_flags wanted) const
		{
			stat_t stat(nullptr);
			BOOST_AFIO_POSIX_STAT_STRUCT s={0};
			BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_LSTAT(path().c_str(), &s), path());
			fill_stat_t(stat, s, wanted);
			return stat;
		}
		virtual std::filesystem::path target() const
		{
#ifdef WIN32
			return std::filesystem::path();
#else
			if(!opened_as_symlink())
				return std::filesystem::path();
			char buffer[PATH_MAX+1];
			ssize_t len;
			if((len = readlink(path().c_str(), buffer, sizeof(buffer)-1)) == -1)
			    BOOST_AFIO_ERRGOS(-1);
			return std::filesystem::path::string_type(buffer, len);
#endif
		}
	};

    #ifdef DOXYGEN_NO_CLASS_ENUMS
    enum file_flags
    #elif defined(BOOST_NO_CXX11_SCOPED_ENUMS)
    BOOST_SCOPED_ENUM_DECLARE_BEGIN(OpType)
    #else
    enum class OpType 
    #endif
	{
		Unknown,
		UserCompletion,
		dir,
		rmdir,
		file,
		rmfile,
		symlink,
		rmsymlink,
		sync,
		close,
		read,
		write,
		truncate,
		barrier,
		enumerate,

		Last
	}
   #ifdef BOOST_NO_CXX11_SCOPED_ENUMS
    BOOST_SCOPED_ENUM_DECLARE_END(OpType)
    #else
    ;
    #endif
	static const char *optypes[]={
		"unknown",
		"UserCompletion",
		"dir",
		"rmdir",
		"file",
		"rmfile",
		"symlink",
		"rmsymlink",
		"sync",
		"close",
		"read",
		"write",
		"truncate",
		"barrier",
		"enumerate"
	};
	static_assert(static_cast<size_t>(OpType::Last)==sizeof(optypes)/sizeof(*optypes), "You forgot to fix up the strings matching OpType");
	struct async_file_io_dispatcher_op
	{
		OpType optype;
		async_op_flags flags;
		std::shared_ptr<shared_future<std::shared_ptr<async_io_handle>>> h;
		std::unique_ptr<promise<std::shared_ptr<async_io_handle>>> detached_promise;
		typedef std::pair<size_t, std::function<std::shared_ptr<async_io_handle> (std::shared_ptr<async_io_handle>, exception_ptr *)>> completion_t;
		std::vector<completion_t> completions;
#ifndef NDEBUG
		completion_t boundf; // Useful for debugging
#endif
#ifdef BOOST_AFIO_OP_STACKBACKTRACEDEPTH
		std::vector<void *> stack;
		void fillStack()
		{
#ifdef UNW_LOCAL_ONLY
			// libunwind seems a bit more accurate than glibc's backtrace()
			unw_context_t uc;
			unw_cursor_t cursor;
			unw_getcontext(&uc);
			stack.reserve(BOOST_AFIO_OP_STACKBACKTRACEDEPTH);
			unw_init_local(&cursor, &uc);
			size_t n=0;
			while(unw_step(&cursor)>0 && n++<BOOST_AFIO_OP_STACKBACKTRACEDEPTH)
			{
				unw_word_t ip;
				unw_get_reg(&cursor, UNW_REG_IP, &ip);
				stack.push_back((void *) ip);
			}
#else
			stack.reserve(BOOST_AFIO_OP_STACKBACKTRACEDEPTH);
			stack.resize(BOOST_AFIO_OP_STACKBACKTRACEDEPTH);
			stack.resize(backtrace(&stack.front(), stack.size()));
#endif
		}
#else
		void fillStack() { }
#endif
		async_file_io_dispatcher_op(OpType _optype, async_op_flags _flags, std::shared_ptr<shared_future<std::shared_ptr<async_io_handle>>> _h, completion_t _boundf)
			: optype(_optype), flags(_flags), h(_h)
#ifndef NDEBUG
			, boundf(_boundf)
#endif
		{ fillStack(); }
		async_file_io_dispatcher_op(async_file_io_dispatcher_op &&o) BOOST_NOEXCEPT_OR_NOTHROW : optype(o.optype), flags(std::move(o.flags)), h(std::move(o.h)),
			detached_promise(std::move(o.detached_promise)), completions(std::move(o.completions))
#ifndef NDEBUG
			, boundf(std::move(o.boundf))
#endif
#ifdef BOOST_AFIO_OP_STACKBACKTRACEDEPTH
			, stack(std::move(o.stack))
#endif
			{
			}
	private:
		async_file_io_dispatcher_op(const async_file_io_dispatcher_op &o)
#ifndef BOOST_NO_CXX11_DELETED_FUNCTIONS
			= delete
#endif
			;
	};
	struct async_file_io_dispatcher_base_p
	{
		std::shared_ptr<thread_source> pool;
		file_flags flagsforce, flagsmask;

		typedef boost::detail::spinlock fdslock_t;
		typedef recursive_mutex opslock_t;
		typedef recursive_mutex dircachelock_t;
		fdslock_t fdslock; std::unordered_map<void *, std::weak_ptr<async_io_handle>> fds;
		opslock_t opslock; size_t monotoniccount; std::unordered_map<size_t, async_file_io_dispatcher_op> ops;
		dircachelock_t dircachelock; std::unordered_map<std::filesystem::path, std::weak_ptr<async_io_handle>, std::filesystem_hash> dirhcache;

		async_file_io_dispatcher_base_p(std::shared_ptr<thread_source> _pool, file_flags _flagsforce, file_flags _flagsmask) : pool(_pool),
			flagsforce(_flagsforce), flagsmask(_flagsmask), monotoniccount(0)
		{
			// Boost's spinlock is so lightweight it has no constructor ...
			fdslock.unlock();
			ANNOTATE_RWLOCK_CREATE(&fdslock);
        
#if !defined(BOOST_MSVC)|| BOOST_MSVC >= 1700 // MSVC 2010 doesn't support reserve
			ops.reserve(10000);
#else
			ops.rehash((size_t) std::ceil(10000 / ops.max_load_factor()));
#endif
		}
		~async_file_io_dispatcher_base_p()
		{
			ANNOTATE_RWLOCK_DESTROY(&fdslock);
		}

		// Returns a handle to a directory from the cache, or creates a new directory handle.
		template<class F> std::shared_ptr<async_io_handle> get_handle_to_dir(F *parent, size_t id, exception_ptr *e, async_path_op_req req, typename async_file_io_dispatcher_base::completion_returntype (F::*dofile)(size_t, std::shared_ptr<async_io_handle>, exception_ptr *, async_path_op_req))
		{
			std::shared_ptr<async_io_handle> dirh;
			BOOST_AFIO_LOCK_GUARD<dircachelock_t> dircachelockh(dircachelock);
			do
			{
				std::unordered_map<std::filesystem::path, std::weak_ptr<async_io_handle>, std::filesystem_hash>::iterator it=dirhcache.find(req.path);
				if(dirhcache.end()==it || it->second.expired())
				{
					if(dirhcache.end()!=it) dirhcache.erase(it);
					auto result=(parent->*dofile)(id, /* known null */dirh, e, req);
					if(!result.first) abort();
					dirh=std::move(result.second);
					if(dirh)
					{
						auto _it=dirhcache.insert(std::make_pair(req.path, std::weak_ptr<async_io_handle>(dirh)));
						return dirh;
					}
					else
						abort();
				}
				else
					dirh=std::shared_ptr<async_io_handle>(it->second);
			} while(!dirh);
			return dirh;
		}
		// Returns a handle to a containing directory from the cache, or creates a new directory handle.
		template<class F> std::shared_ptr<async_io_handle> get_handle_to_containing_dir(F *parent, size_t id, exception_ptr *e, async_path_op_req req, typename async_file_io_dispatcher_base::completion_returntype (F::*dofile)(size_t, std::shared_ptr<async_io_handle>, exception_ptr *, async_path_op_req))
		{
			req.path=req.path.parent_path();
			req.flags=req.flags&~file_flags::FastDirectoryEnumeration;
			return get_handle_to_dir(parent, id, e, req, dofile);
		}
	};
#if defined(WIN32)
	inline async_io_handle_windows::async_io_handle_windows(std::shared_ptr<async_file_io_dispatcher_base> _parent, std::shared_ptr<async_io_handle> _dirh, const std::filesystem::path &path, file_flags flags, bool _SyncOnClose, HANDLE _h) : async_io_handle(_parent.get(), std::move(_dirh), path, flags), parent(_parent), h(new boost::asio::windows::random_access_handle(_parent->p->pool->io_service(), int_checkHandle(_h, path))), myid(_h), has_been_added(false), SyncOnClose(_SyncOnClose) { }
#endif
	class async_file_io_dispatcher_compat;
	class async_file_io_dispatcher_windows;
	class async_file_io_dispatcher_linux;
	class async_file_io_dispatcher_qnx;
	struct immediate_async_ops
	{
		typedef std::shared_ptr<async_io_handle> rettype;
		typedef rettype retfuncttype();
		std::vector<packaged_task<retfuncttype>> toexecute;

		immediate_async_ops() { }
		// Returns a promise which is fulfilled when this is destructed
		future<rettype> enqueue(std::function<retfuncttype> f)
		{
			packaged_task<retfuncttype> t(std::move(f));
			toexecute.push_back(std::move(t));
			return toexecute.back().get_future();
		}
		~immediate_async_ops()
		{
			BOOST_FOREACH(auto &i, toexecute)
			{
				i();
			}
		}
	private:
		immediate_async_ops(const immediate_async_ops &);
		immediate_async_ops &operator=(const immediate_async_ops &);
		immediate_async_ops(immediate_async_ops &&);
		immediate_async_ops &operator=(immediate_async_ops &&);
	};
}

metadata_flags directory_entry::metadata_supported() BOOST_NOEXCEPT_OR_NOTHROW
{
	metadata_flags ret;
#ifdef WIN32
	ret=metadata_flags::None
		//| metadata_flags::dev
		| metadata_flags::ino        // FILE_INTERNAL_INFORMATION, enumerated
		| metadata_flags::type       // FILE_BASIC_INFORMATION, enumerated
		//| metadata_flags::mode
		| metadata_flags::nlink      // FILE_STANDARD_INFORMATION
		//| metadata_flags::uid
		//| metadata_flags::gid
		//| metadata_flags::rdev
		| metadata_flags::atim       // FILE_BASIC_INFORMATION, enumerated
		| metadata_flags::mtim       // FILE_BASIC_INFORMATION, enumerated
		| metadata_flags::ctim       // FILE_BASIC_INFORMATION, enumerated
		| metadata_flags::size       // FILE_STANDARD_INFORMATION, enumerated
		| metadata_flags::allocated  // FILE_STANDARD_INFORMATION, enumerated
		| metadata_flags::blocks
		| metadata_flags::blksize    // FILE_ALIGNMENT_INFORMATION
		//| metadata_flags::flags
		//| metadata_flags::gen
		| metadata_flags::birthtim   // FILE_BASIC_INFORMATION, enumerated
		;
#elif defined(__linux__)
	ret=metadata_flags::None
		| metadata_flags::dev
		| metadata_flags::ino
		| metadata_flags::type
		| metadata_flags::mode
		| metadata_flags::nlink
		| metadata_flags::uid
		| metadata_flags::gid
		| metadata_flags::rdev
		| metadata_flags::atim
		| metadata_flags::mtim
		| metadata_flags::ctim
		| metadata_flags::size
		| metadata_flags::allocated
		| metadata_flags::blocks
		| metadata_flags::blksize
	// Sadly these must wait until someone fixes the Linux stat() call e.g. the xstat() proposal.
		//| metadata_flags::flags
		//| metadata_flags::gen
		//| metadata_flags::birthtim
	// According to http://computer-forensics.sans.org/blog/2011/03/14/digital-forensics-understanding-ext4-part-2-timestamps
	// ext4 keeps birth time at offset 144 to 151 in the inode. If we ever got round to it, birthtime could be hacked.
		;
#else
	// Kinda assumes FreeBSD or OS X really ...
	ret=metadata_flags::None
		| metadata_flags::dev
		| metadata_flags::ino
		| metadata_flags::type
		| metadata_flags::mode
		| metadata_flags::nlink
		| metadata_flags::uid
		| metadata_flags::gid
		| metadata_flags::rdev
		| metadata_flags::atim
		| metadata_flags::mtim
		| metadata_flags::ctim
		| metadata_flags::size
		| metadata_flags::allocated
		| metadata_flags::blocks
		| metadata_flags::blksize
#define HAVE_STAT_FLAGS
		| metadata_flags::flags
#define HAVE_STAT_GEN
		| metadata_flags::gen
#define HAVE_BIRTHTIMESPEC
		| metadata_flags::birthtim
		;
#endif
	return ret;
}

metadata_flags directory_entry::metadata_fastpath() BOOST_NOEXCEPT_OR_NOTHROW
{
	metadata_flags ret;
#ifdef WIN32
	ret=metadata_flags::None
		//| metadata_flags::dev
		| metadata_flags::ino        // FILE_INTERNAL_INFORMATION, enumerated
		| metadata_flags::type       // FILE_BASIC_INFORMATION, enumerated
		//| metadata_flags::mode
		//| metadata_flags::nlink      // FILE_STANDARD_INFORMATION
		//| metadata_flags::uid
		//| metadata_flags::gid
		//| metadata_flags::rdev
		| metadata_flags::atim       // FILE_BASIC_INFORMATION, enumerated
		| metadata_flags::mtim       // FILE_BASIC_INFORMATION, enumerated
		| metadata_flags::ctim       // FILE_BASIC_INFORMATION, enumerated
		| metadata_flags::size       // FILE_STANDARD_INFORMATION, enumerated
		| metadata_flags::allocated  // FILE_STANDARD_INFORMATION, enumerated
		//| metadata_flags::blocks
		//| metadata_flags::blksize    // FILE_ALIGNMENT_INFORMATION
		//| metadata_flags::flags
		//| metadata_flags::gen
		| metadata_flags::birthtim   // FILE_BASIC_INFORMATION, enumerated
		;
#elif defined(__linux__)
	ret=metadata_flags::None
		| metadata_flags::dev
		| metadata_flags::ino
		| metadata_flags::type
		| metadata_flags::mode
		| metadata_flags::nlink
		| metadata_flags::uid
		| metadata_flags::gid
		| metadata_flags::rdev
		| metadata_flags::atim
		| metadata_flags::mtim
		| metadata_flags::ctim
		| metadata_flags::size
		| metadata_flags::allocated
		| metadata_flags::blocks
		| metadata_flags::blksize
	// Sadly these must wait until someone fixes the Linux stat() call e.g. the xstat() proposal.
		//| metadata_flags::flags
		//| metadata_flags::gen
		//| metadata_flags::birthtim
	// According to http://computer-forensics.sans.org/blog/2011/03/14/digital-forensics-understanding-ext4-part-2-timestamps
	// ext4 keeps birth time at offset 144 to 151 in the inode. If we ever got round to it, birthtime could be hacked.
		;
#else
	// Kinda assumes FreeBSD or OS X really ...
	ret=metadata_flags::None
		| metadata_flags::dev
		| metadata_flags::ino
		| metadata_flags::type
		| metadata_flags::mode
		| metadata_flags::nlink
		| metadata_flags::uid
		| metadata_flags::gid
		| metadata_flags::rdev
		| metadata_flags::atim
		| metadata_flags::mtim
		| metadata_flags::ctim
		| metadata_flags::size
		| metadata_flags::allocated
		| metadata_flags::blocks
		| metadata_flags::blksize
#define HAVE_STAT_FLAGS
		| metadata_flags::flags
#define HAVE_STAT_GEN
		| metadata_flags::gen
#define HAVE_BIRTHTIMESPEC
		| metadata_flags::birthtim
		;
#endif
	return ret;
}

size_t directory_entry::compatibility_maximum() BOOST_NOEXCEPT_OR_NOTHROW
{
#ifdef WIN32
	// Let's choose 100k entries. Why not!
	return 100000;
#else
	return 100000;
	// This is what glibc uses, a 32Kb buffer.
	//return 32768/sizeof(dirent);
#endif
}

async_file_io_dispatcher_base::async_file_io_dispatcher_base(std::shared_ptr<thread_source> threadpool, file_flags flagsforce, file_flags flagsmask) : p(new detail::async_file_io_dispatcher_base_p(threadpool, flagsforce, flagsmask))
{
}

async_file_io_dispatcher_base::~async_file_io_dispatcher_base()
{
#ifndef BOOST_AFIO_COMPILING_FOR_GCOV
	std::unordered_map<shared_future<std::shared_ptr<async_io_handle>> *, std::pair<size_t, future_status>> reallyoutstanding;
	for(;;)
	{
		std::vector<std::pair<size_t, std::shared_ptr<shared_future<std::shared_ptr<async_io_handle>>>>> outstanding;
		{
			BOOST_AFIO_LOCK_GUARD<detail::async_file_io_dispatcher_base_p::opslock_t> opslockh(p->opslock);
			if(!p->ops.empty())
			{
				outstanding.reserve(p->ops.size());
				BOOST_FOREACH(auto &op, p->ops)
				{
					if(op.second.h->valid())
					{
						auto it=reallyoutstanding.find(op.second.h.get());
						if(reallyoutstanding.end()!=it)
						{
							if(it->second.first>=5)
							{
								static const char *statuses[]={"ready", "timeout", "deferred", "unknown"};
								std::cerr << "WARNING: ~async_file_dispatcher_base() detects stuck async_io_op in total of " << p->ops.size() << " extant ops\n"
								"   id=" << op.first << " type=" << detail::optypes[static_cast<size_t>(op.second.optype)] << " flags=0x" << std::hex << static_cast<size_t>(op.second.flags) << std::dec << " status=" << statuses[(((int) it->second.second)>=0 && ((int) it->second.second)<=2) ? (int) it->second.second : 3] << " handle_usecount=" << op.second.h.use_count() << " failcount=" << it->second.first << " Completions:";
								BOOST_FOREACH(auto &c, op.second.completions)
								{
									std::cerr << " id=" << c.first;
								}
								std::cerr << std::endl;
#ifdef BOOST_AFIO_OP_STACKBACKTRACEDEPTH
								std::cerr << "  Allocation backtrace:" << std::endl;
								size_t n=0;
								BOOST_FOREACH(void *addr, op.second.stack)
								{
									Dl_info info;
									std::cerr << "    " << ++n << ". 0x" << std::hex << addr << std::dec << ": ";
									if(dladdr(addr, &info))
									{
										// This is hacky ...
										if(info.dli_fname)
										{
											char buffer2[4096];
											sprintf(buffer2, "/usr/bin/addr2line -C -f -i -e %s %lx", info.dli_fname, (long)((size_t) addr - (size_t) info.dli_fbase));
											FILE *ih=popen(buffer2, "r");
											auto unih=boost::afio::detail::Undoer([&ih]{ if(ih) pclose(ih); });
											if(ih)
											{
												size_t length=fread(buffer2, 1, sizeof(buffer2), ih);
												buffer2[length]=0;
												std::string buffer(buffer2, length-1);
												boost::replace_all(buffer, "\n", "\n       ");
												std::cerr << buffer << " (+0x" << std::hex << ((size_t) addr - (size_t) info.dli_saddr) << ")" << std::dec;
											}
											else std::cerr << info.dli_fname << ":0 (+0x" << std::hex << ((size_t) addr - (size_t) info.dli_fbase) << ")" << std::dec;
										}
										else std::cerr << "unknown:0";
									}
									else
										std::cerr << "completely unknown";
									std::cerr << std::endl;
								}
#endif
							}
						}
						outstanding.push_back(std::make_pair(op.first, op.second.h));
					}
				}
			}
		}
		if(outstanding.empty()) break;
		size_t mincount=(size_t)-1;
		BOOST_FOREACH(auto &op, outstanding)
		{
			future_status status=op.second->wait_for(chrono::duration<int, ratio<1, 10>>(1).toBoost());
			switch(status)
			{
			case future_status::ready:
				reallyoutstanding.erase(op.second.get());
				break;
			case future_status::deferred:
				// Probably pending on others, but log
			case future_status::timeout:
				auto it=reallyoutstanding.find(op.second.get());
				if(reallyoutstanding.end()==it)
					it=reallyoutstanding.insert(std::make_pair(op.second.get(), std::make_pair(0, status))).first;
				it->second.first++;
				if(it->second.first<mincount) mincount=it->second.first;
				break;
			}
		}
		if(mincount>=10) // i.e. nothing is changing
		{
			std::cerr << "WARNING: ~async_file_dispatcher_base() sees no change in " << reallyoutstanding.size() << " stuck async_io_ops, so exiting destructor wait" << std::endl;
			break;
		}
	}
#endif
	delete p;
}

void async_file_io_dispatcher_base::int_add_io_handle(void *key, std::shared_ptr<async_io_handle> h)
{
	BOOST_AFIO_LOCK_GUARD<detail::async_file_io_dispatcher_base_p::fdslock_t> lockh(p->fdslock);
	ANNOTATE_RWLOCK_ACQUIRED(&p->fdslock, 1);
	p->fds.insert(std::make_pair(key, std::weak_ptr<async_io_handle>(h)));
	ANNOTATE_RWLOCK_RELEASED(&p->fdslock, 1);
}

void async_file_io_dispatcher_base::int_del_io_handle(void *key)
{
	BOOST_AFIO_LOCK_GUARD<detail::async_file_io_dispatcher_base_p::fdslock_t> lockh(p->fdslock);
	ANNOTATE_RWLOCK_ACQUIRED(&p->fdslock, 1);
	p->fds.erase(key);
	ANNOTATE_RWLOCK_RELEASED(&p->fdslock, 1);
}

std::weak_ptr<thread_source> async_file_io_dispatcher_base::threadsource() const
{
	return p->pool;
}

file_flags async_file_io_dispatcher_base::fileflags(file_flags flags) const
{
	file_flags ret=(flags&~p->flagsmask)|p->flagsforce;
	if(!!(ret&file_flags::EnforceDependencyWriteOrder))
	{
		// The logic (for now) is this:
		// If the data is sequentially accessed, we won't be seeking much
		// so turn on AlwaysSync.
		// If not sequentially accessed and we might therefore be seeking,
		// turn on SyncOnClose.
		if(!!(ret&file_flags::WillBeSequentiallyAccessed))
			ret=ret|file_flags::AlwaysSync;
		else
			ret=ret|file_flags::SyncOnClose;
	}
	return ret;
}

size_t async_file_io_dispatcher_base::wait_queue_depth() const
{
	BOOST_AFIO_LOCK_GUARD<detail::async_file_io_dispatcher_base_p::opslock_t> opslockh(p->opslock);
	return p->ops.size();
}

size_t async_file_io_dispatcher_base::count() const
{
	size_t ret;
	BOOST_AFIO_LOCK_GUARD<detail::async_file_io_dispatcher_base_p::fdslock_t> lockh(p->fdslock);
	ANNOTATE_RWLOCK_ACQUIRED(&p->fdslock, 1);
	ret=p->fds.size();
	ANNOTATE_RWLOCK_RELEASED(&p->fdslock, 1);
	return ret;
}

// Called in unknown thread
async_file_io_dispatcher_base::completion_returntype async_file_io_dispatcher_base::invoke_user_completion(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *e, std::function<async_file_io_dispatcher_base::completion_t> callback)
{
	return callback(id, h, e);
}

std::vector<async_io_op> async_file_io_dispatcher_base::completion(const std::vector<async_io_op> &ops, const std::vector<std::pair<async_op_flags, std::function<async_file_io_dispatcher_base::completion_t>>> &callbacks)
{
	if(!ops.empty() && ops.size()!=callbacks.size())
		BOOST_AFIO_THROW(std::runtime_error("The sequence of preconditions must either be empty or exactly the same length as callbacks."));
	std::vector<async_io_op> ret;
	ret.reserve(callbacks.size());
	std::vector<async_io_op>::const_iterator i;
	std::vector<std::pair<async_op_flags, std::function<async_file_io_dispatcher_base::completion_t>>>::const_iterator c;
	BOOST_AFIO_LOCK_GUARD<detail::async_file_io_dispatcher_base_p::opslock_t> opslockh(p->opslock);
	exception_ptr he;
	detail::immediate_async_ops immediates;
	if(ops.empty())
	{
		async_io_op empty;
		BOOST_FOREACH(auto & c, callbacks)
		{
			ret.push_back(chain_async_op(&he, immediates, (int) detail::OpType::UserCompletion, empty, c.first, &async_file_io_dispatcher_base::invoke_user_completion, c.second));
		}
	}
	else for(i=ops.begin(), c=callbacks.begin(); i!=ops.end() && c!=callbacks.end(); ++i, ++c)
			ret.push_back(chain_async_op(&he, immediates, (int) detail::OpType::UserCompletion, *i, c->first, &async_file_io_dispatcher_base::invoke_user_completion, c->second));
	return ret;
}

// Called in unknown thread
void async_file_io_dispatcher_base::complete_async_op(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr e)
{
	BOOST_AFIO_LOCK_GUARD<detail::async_file_io_dispatcher_base_p::opslock_t> opslockh(p->opslock);
	detail::immediate_async_ops immediates;
	// Find me in ops, remove my completions and delete me from extant ops
	std::unordered_map<size_t, detail::async_file_io_dispatcher_op>::iterator it=p->ops.find(id);
	if(p->ops.end()==it)
	{
#ifndef NDEBUG
		std::vector<size_t> opsids;
		BOOST_FOREACH(auto &i, p->ops)
		{
			opsids.push_back(i.first);
		}
		std::sort(opsids.begin(), opsids.end());
#endif
		BOOST_AFIO_THROW_FATAL(std::runtime_error("Failed to find this operation in list of currently executing operations"));
	}
	// Erase me from ops on exit from this function
	auto eraseit=boost::afio::detail::Undoer([this, id]{
		std::unordered_map<size_t, detail::async_file_io_dispatcher_op>::iterator it=p->ops.find(id);
		if(p->ops.end()==it)
		{
			BOOST_AFIO_THROW_FATAL(std::runtime_error("Failed to find this operation in list of currently executing operations"));
		}
		p->ops.erase(it);
	});
	if(!it->second.completions.empty())
	{
		// Remove completions as we're about to modify p->ops which will invalidate it
		std::vector<detail::async_file_io_dispatcher_op::completion_t> completions(std::move(it->second.completions));
		assert(it->second.completions.empty());
		BOOST_FOREACH(auto &c, completions)
		{
			exception_ptr *immediate_e=(!!e) ? &e : nullptr;
			// Enqueue each completion
			it=p->ops.find(c.first);
			if(p->ops.end()==it)
				BOOST_AFIO_THROW_FATAL(std::runtime_error("Failed to find this completion operation in list of currently executing operations"));
			if(!!(it->second.flags & async_op_flags::ImmediateCompletion))
			{
				// If he was set up with a detached future, use that instead
				if(it->second.detached_promise)
				{
					*it->second.h=it->second.detached_promise->get_future();
					immediates.enqueue(std::bind(c.second, h, immediate_e));
				}
				else
					*it->second.h=immediates.enqueue(std::bind(c.second, h, immediate_e));
			}
			else
			{
				// If he was set up with a detached future, use that instead
				if(it->second.detached_promise)
				{
					*it->second.h=it->second.detached_promise->get_future();
					p->pool->enqueue(std::bind(c.second, h, nullptr));
				}
				else
					*it->second.h=p->pool->enqueue(std::bind(c.second, h, nullptr));
			}
			BOOST_AFIO_DEBUG_PRINT("C %u > %u %p\n", (unsigned) id, (unsigned) c.first, h.get());
		}
		// Restore it to my id
		it=p->ops.find(id);
		if(p->ops.end()==it)
		{
	#ifndef NDEBUG
			std::vector<size_t> opsids;
			BOOST_FOREACH(auto &i, p->ops)
			{
				opsids.push_back(i.first);
			}
			std::sort(opsids.begin(), opsids.end());
	#endif
			BOOST_AFIO_THROW_FATAL(std::runtime_error("Failed to find this operation in list of currently executing operations"));
		}
	}
	if(it->second.detached_promise)
	{
		auto delpromise=boost::afio::detail::Undoer([&it]{ it->second.detached_promise.reset(); });
		if(e)
			it->second.detached_promise->set_exception(e);
		else
			it->second.detached_promise->set_value(h);
	}
	BOOST_AFIO_DEBUG_PRINT("R %u %p\n", (unsigned) id, h.get());
}


#ifndef BOOST_NO_CXX11_VARIADIC_TEMPLATES
// Called in unknown thread 
template<class F, class... Args> std::shared_ptr<async_io_handle> async_file_io_dispatcher_base::invoke_async_op_completions(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *e, completion_returntype (F::*f)(size_t, std::shared_ptr<async_io_handle>, exception_ptr *, Args...), Args... args)
{
	try
	{
		completion_returntype ret((static_cast<F *>(this)->*f)(id, h, e, args...));
		// If boolean is false, reschedule completion notification setting it to ret.second, otherwise complete now
		if(ret.first)
		{
			complete_async_op(id, ret.second);
		}
		else
		{
			// Make sure this was set up for deferred completion
	#ifndef NDEBUG
			BOOST_AFIO_LOCK_GUARD<detail::async_file_io_dispatcher_base_p::opslock_t> opslockh(p->opslock);
			std::unordered_map<size_t, detail::async_file_io_dispatcher_op>::iterator it(p->ops.find(id));
			if(p->ops.end()==it)
			{
	#ifndef NDEBUG
				std::vector<size_t> opsids;
				BOOST_FOREACH(auto &i, p->ops)
                {
					opsids.push_back(i.first);
                }
				std::sort(opsids.begin(), opsids.end());
	#endif
				BOOST_AFIO_THROW_FATAL(std::runtime_error("Failed to find this operation in list of currently executing operations"));
			}
			if(!it->second.detached_promise)
			{
				// If this trips, it means a completion handler tried to defer signalling
				// completion but it hadn't been set up with a detached future
				assert(0);
				std::terminate();
			}
	#endif
		}
		return ret.second;
	}
	catch(...)
	{
		exception_ptr e(afio::make_exception_ptr(afio::current_exception()));
		assert(e);
		BOOST_AFIO_DEBUG_PRINT("E %u begin\n", (unsigned) id);
		complete_async_op(id, h, e);
		BOOST_AFIO_DEBUG_PRINT("E %u end\n", (unsigned) id);
		BOOST_AFIO_RETHROW;
	}
}
#else

#define BOOST_PP_LOCAL_MACRO(N)                                                                     \
    template <class F                                                                               \
    BOOST_PP_COMMA_IF(N)                                                                            \
    BOOST_PP_ENUM_PARAMS(N, class A)>                                                               \
    std::shared_ptr<async_io_handle>                                                        \
    async_file_io_dispatcher_base::invoke_async_op_completions                                      \
    (size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *e,                        \
    completion_returntype (F::*f)(size_t, std::shared_ptr<async_io_handle>, exception_ptr * \
    BOOST_PP_COMMA_IF(N)                                                                            \
    BOOST_PP_ENUM_PARAMS(N, A))                                                                     \
    BOOST_PP_COMMA_IF(N)                                                                            \
    BOOST_PP_ENUM_BINARY_PARAMS(N, A, a))     /* parameters end */                                  \
    {                                                                                               \
	    try\
	    {\
		    completion_returntype ret((static_cast<F *>(this)->*f)(id, h, e BOOST_PP_COMMA_IF(N) BOOST_PP_ENUM_PARAMS(N, a)));\
		    /* If boolean is false, reschedule completion notification setting it to ret.second, otherwise complete now */ \
		    if(ret.first)   \
		    {\
			    complete_async_op(id, ret.second);\
		    }\
		    else\
		    {\
			    /* Make sure this was set up for deferred completion*/ \
			    BOOST_AFIO_LOCK_GUARD<detail::async_file_io_dispatcher_base_p::opslock_t> opslockh(p->opslock);\
			    std::unordered_map<size_t, detail::async_file_io_dispatcher_op>::iterator it(p->ops.find(id));\
			    if(p->ops.end()==it)\
			    {\
				    std::vector<size_t> opsids;\
				    BOOST_FOREACH(auto &i, p->ops)\
                    {\
					    opsids.push_back(i.first);\
                    }\
				    std::sort(opsids.begin(), opsids.end());\
				    BOOST_AFIO_THROW_FATAL(std::runtime_error("Failed to find this operation in list of currently executing operations"));\
			    }\
			    if(!it->second.detached_promise)\
			    {\
				    /* If this trips, it means a completion handler tried to defer signalling*/ \
				    /* completion but it hadn't been set up with a detached future*/ \
				    assert(0);\
				    std::terminate();\
			    }\
		    }\
		    return ret.second;\
	    }\
        catch(...)\
	    {\
		    exception_ptr e(afio::make_exception_ptr(afio::current_exception()));\
		    BOOST_AFIO_DEBUG_PRINT("E %u begin\n", (unsigned) id);\
		    complete_async_op(id, h, e);\
		    BOOST_AFIO_DEBUG_PRINT("E %u end\n", (unsigned) id);\
		    BOOST_AFIO_RETHROW;\
	    }\
    } //end of invoke_async_op_completions
  
#define BOOST_PP_LOCAL_LIMITS     (0, BOOST_AFIO_MAX_PARAMETERS) //should this be 0 or 1 for the min????
#include BOOST_PP_LOCAL_ITERATE()
#endif




#ifndef BOOST_NO_CXX11_VARIADIC_TEMPLATES
// You MUST hold opslock before entry!
template<class F, class... Args> async_io_op async_file_io_dispatcher_base::chain_async_op(exception_ptr *he, detail::immediate_async_ops &immediates, int optype, const async_io_op &precondition, async_op_flags flags, completion_returntype (F::*f)(size_t, std::shared_ptr<async_io_handle>, exception_ptr *, Args...), Args... args)
{	
	size_t thisid=0;
	while(!(thisid=++p->monotoniccount));
#if 0 //ndef NDEBUG
	if(!p->ops.empty())
	{
		std::vector<size_t> opsids;
		BOOST_FOREACH(auto &i, p->ops)
        {
			opsids.push_back(i.first);
        }
		std::sort(opsids.begin(), opsids.end());
		assert(thisid==opsids.back()+1);
	}
#endif
	// Wrap supplied implementation routine with a completion dispatcher
	auto wrapperf=&async_file_io_dispatcher_base::invoke_async_op_completions<F, Args...>;
	// Bind supplied implementation routine to this, unique id and any args they passed
	typename detail::async_file_io_dispatcher_op::completion_t boundf(std::make_pair(thisid, std::bind(wrapperf, this, thisid, std::placeholders::_1, std::placeholders::_2, f, args...)));
	// Make a new async_io_op ready for returning
	async_io_op ret(this, thisid);
	bool done=false;
	if(precondition.id)
	{
		// If still in flight, chain boundf to be executed when precondition completes
		auto dep(p->ops.find(precondition.id));
		if(p->ops.end()!=dep)
		{
			dep->second.completions.push_back(boundf);
			done=true;
		}
	}
	auto undep=boost::afio::detail::Undoer([done, this, precondition](){
		if(done)
		{
			auto dep(p->ops.find(precondition.id));
			dep->second.completions.pop_back();
		}
	});
	if(!done)
	{
		// Bind input handle now and queue immediately to next available thread worker
		std::shared_ptr<async_io_handle> h;
		exception_ptr *_he=nullptr;
		if(precondition.h->valid())
		{
			// Boost's shared_future has get() as non-const which is weird, because it doesn't
			// delete the data after retrieval.
			shared_future<std::shared_ptr<async_io_handle>> &f=const_cast<shared_future<std::shared_ptr<async_io_handle>> &>(*precondition.h);
			// The reason we don't throw here is consistency: we throw at point of batch API entry
			// if any of the preconditions are errored, but if they became errored between then and
			// now we can hardly abort half way through a batch op without leaving around stuck ops.
			// No, instead we simulate as if the precondition had not yet become ready.
			if(precondition.h->has_exception())
				*he=get_exception_ptr(f), _he=he;
			else
				h=f.get();
		}
		else if(precondition.id)
		{
			// It should never happen that precondition.id is valid but removed from extant ops
			// which indicates it completed and yet h remains invalid
			assert(0);
			std::terminate();
		}
		if(!!(flags & async_op_flags::ImmediateCompletion))
		{
			*ret.h=immediates.enqueue(std::bind(boundf.second, h, _he)).share();
		}
		else
			*ret.h=p->pool->enqueue(std::bind(boundf.second, h, nullptr)).share();
	}
	auto opsit=p->ops.insert(std::move(std::make_pair(thisid, std::move(detail::async_file_io_dispatcher_op((detail::OpType) optype, flags, ret.h, boundf)))));
	assert(opsit.second);
	BOOST_AFIO_DEBUG_PRINT("I %u < %u (%s)\n", (unsigned) thisid, (unsigned) precondition.id, detail::optypes[static_cast<int>(optype)]);
	auto unopsit=boost::afio::detail::Undoer([this, opsit, thisid](){
		p->ops.erase(opsit.first);
		BOOST_AFIO_DEBUG_PRINT("E R %u\n", (unsigned) thisid);
	});
	if(!!(flags & async_op_flags::DetachedFuture))
	{
		opsit.first->second.detached_promise.reset(new promise<std::shared_ptr<async_io_handle>>);
		if(!done)
			*opsit.first->second.h=opsit.first->second.detached_promise->get_future();
	}
	unopsit.dismiss();
	undep.dismiss();
	return ret;
}

#else

// removed because we can't have preprocessor directives inside of a macro
  #if 0 //ndef NDEBUG
	    if(!p->ops.empty())
	    {
		    std::vector<size_t> opsids;
		    BOOST_FOREACH(auto &i, p->ops)
            {
			    opsids.push_back(i.first);
            }
		    std::sort(opsids.begin(), opsids.end());
		    assert(thisid==opsids.back()+1);
	    }
    #endif

#define BOOST_PP_LOCAL_MACRO(N)                                                                     \
    template <class F                                /* template start */                           \
    BOOST_PP_COMMA_IF(N)                                                                            \
    BOOST_PP_ENUM_PARAMS(N, class A)>                /* template end */                             \
    async_io_op                                      /* return type */                              \
    async_file_io_dispatcher_base::chain_async_op       /* function name */             \
    (exception_ptr *he, detail::immediate_async_ops &immediates, int optype,           /* parameters start */          \
    const async_io_op &precondition,async_op_flags flags,                                           \
    completion_returntype (F::*f)(size_t, std::shared_ptr<async_io_handle>, exception_ptr * \
    BOOST_PP_COMMA_IF(N)                                                                            \
    BOOST_PP_ENUM_PARAMS(N, A))                                                                     \
    BOOST_PP_COMMA_IF(N)                                                                            \
    BOOST_PP_ENUM_BINARY_PARAMS(N, A, a))     /* parameters end */                                  \
    {\
	    size_t thisid=0;\
	    while(!(thisid=++p->monotoniccount));\
	    /* Wrap supplied implementation routine with a completion dispatcher*/ \
	    auto wrapperf=&async_file_io_dispatcher_base::invoke_async_op_completions<F BOOST_PP_COMMA_IF(N) BOOST_PP_ENUM_PARAMS(N, A)>;\
	    /* Bind supplied implementation routine to this, unique id and any args they passed*/ \
	    typename detail::async_file_io_dispatcher_op::completion_t boundf(std::make_pair(thisid, std::bind(wrapperf, this, thisid, std::placeholders::_1, std::placeholders::_2, f BOOST_PP_COMMA_IF(N) BOOST_PP_ENUM_PARAMS(N, a))));\
	    /* Make a new async_io_op ready for returning*/\
	    async_io_op ret(this, thisid);\
	    bool done=false;\
	    if(precondition.id)\
	    {\
		    /* If still in flight, chain boundf to be executed when precondition completes*/ \
		    auto dep(p->ops.find(precondition.id));\
		    if(p->ops.end()!=dep)\
		    {\
			    dep->second.completions.push_back(boundf);\
			    done=true;\
		    }\
	    }\
	    auto undep=boost::afio::detail::Undoer([done, this, precondition](){\
		    if(done)\
		    {\
			    auto dep(p->ops.find(precondition.id));\
			    dep->second.completions.pop_back();\
		    }\
	    });\
	    if(!done)\
	    {\
		    /* Bind input handle now and queue immediately to next available thread worker*/ \
		    std::shared_ptr<async_io_handle> h;\
			exception_ptr *_he=nullptr; \
			if(precondition.h->valid()) \
			{ \
				/* Boost's shared_future has get() as non-const which is weird, because it doesn't*/ \
				/* delete the data after retrieval.*/ \
				shared_future<std::shared_ptr<async_io_handle>> &f=const_cast<shared_future<std::shared_ptr<async_io_handle>> &>(*precondition.h);\
				/* The reason we don't throw here is consistency: we throw at point of batch API entry*/ \
				/* if any of the preconditions are errored, but if they became errored between then and*/ \
				/* now we can hardly abort half way through a batch op without leaving around stuck ops.*/ \
				/* No, instead we simulate as if the precondition had not yet become ready.*/ \
				if(precondition.h->has_exception()) \
					*he=get_exception_ptr(f), _he=he;\
				else \
					h=f.get();\
			}\
			else if(precondition.id)\
			{\
				/* It should never happen that precondition.id is valid but removed from extant ops*/\
				/* which indicates it completed and yet h remains invalid*/\
				assert(0);\
				std::terminate();\
			}\
			if(!!(flags & async_op_flags::ImmediateCompletion))\
			{\
				*ret.h=immediates.enqueue(std::bind(boundf.second, h, _he)).share();\
			}\
			else\
				*ret.h=p->pool->enqueue(std::bind(boundf.second, h, nullptr)).share();\
		}\
	    auto opsit=p->ops.insert(std::move(std::make_pair(thisid, std::move(detail::async_file_io_dispatcher_op((detail::OpType) optype, flags, ret.h, boundf)))));\
	    assert(opsit.second);\
	    BOOST_AFIO_DEBUG_PRINT("I %u < %u (%s)\n", (unsigned) thisid, (unsigned) precondition.id, detail::optypes[static_cast<int>(optype)]);\
	    auto unopsit=boost::afio::detail::Undoer([this, opsit, thisid](){\
		    p->ops.erase(opsit.first);\
		    BOOST_AFIO_DEBUG_PRINT("E R %u\n", (unsigned) thisid);\
	    });\
	    if(!!(flags & async_op_flags::DetachedFuture))\
	    {\
		    opsit.first->second.detached_promise.reset(new promise<std::shared_ptr<async_io_handle>>);\
		    if(!done)\
			    *opsit.first->second.h=opsit.first->second.detached_promise->get_future();\
	    }\
	    unopsit.dismiss();\
	    undep.dismiss();\
	    return ret;\
    }

  
#define BOOST_PP_LOCAL_LIMITS     (0, BOOST_AFIO_MAX_PARAMETERS) //should this be 0 or 1 for the min????
#include BOOST_PP_LOCAL_ITERATE()

#endif

// General non-specialised implementation taking some arbitrary parameter T
template<class F, class T> std::vector<async_io_op> async_file_io_dispatcher_base::chain_async_ops(int optype, const std::vector<async_io_op> &preconditions, const std::vector<T> &container, async_op_flags flags, completion_returntype (F::*f)(size_t, std::shared_ptr<async_io_handle>, exception_ptr *, T))
{
	std::vector<async_io_op> ret;
	ret.reserve(container.size());
	assert(preconditions.size()==container.size());
	if(preconditions.size()!=container.size())
		BOOST_AFIO_THROW(std::runtime_error("preconditions size does not match size of ops data"));
	BOOST_AFIO_LOCK_GUARD<detail::async_file_io_dispatcher_base_p::opslock_t> opslockh(p->opslock);
	exception_ptr he;
	detail::immediate_async_ops immediates;
	auto precondition_it=preconditions.cbegin();
	auto container_it=container.cbegin();
	for(; precondition_it!=preconditions.cend() && container_it!=container.cend(); ++precondition_it, ++container_it)
		ret.push_back(chain_async_op(&he, immediates, optype, *precondition_it, flags, f, *container_it));
	return ret;
}
// Generic op receiving specialisation i.e. precondition is also input op. Skips sanity checking.
template<class F> std::vector<async_io_op> async_file_io_dispatcher_base::chain_async_ops(int optype, const std::vector<async_io_op> &container, async_op_flags flags, completion_returntype (F::*f)(size_t, std::shared_ptr<async_io_handle>, exception_ptr *, async_io_op))
{
	std::vector<async_io_op> ret;
	ret.reserve(container.size());
	BOOST_AFIO_LOCK_GUARD<detail::async_file_io_dispatcher_base_p::opslock_t> opslockh(p->opslock);
	exception_ptr he;
	detail::immediate_async_ops immediates;
	BOOST_FOREACH(auto &i, container)
    {
		ret.push_back(chain_async_op(&he, immediates, optype, i, flags, f, i));
    }
	return ret;
}
// Dir/file open specialisation
template<class F> std::vector<async_io_op> async_file_io_dispatcher_base::chain_async_ops(int optype, const std::vector<async_path_op_req> &container, async_op_flags flags, completion_returntype (F::*f)(size_t, std::shared_ptr<async_io_handle>, exception_ptr *, async_path_op_req))
{
	std::vector<async_io_op> ret;
	ret.reserve(container.size());
	BOOST_AFIO_LOCK_GUARD<detail::async_file_io_dispatcher_base_p::opslock_t> opslockh(p->opslock);
	exception_ptr he;
	detail::immediate_async_ops immediates;
	BOOST_FOREACH(auto &i, container)
    {
		ret.push_back(chain_async_op(&he, immediates, optype, i.precondition, flags, f, i));
    }
	return ret;
}
// Data read and write specialisation
template<class F, bool iswrite> std::vector<async_io_op> async_file_io_dispatcher_base::chain_async_ops(int optype, const std::vector<detail::async_data_op_req_impl<iswrite>> &container, async_op_flags flags, completion_returntype(F::*f)(size_t, std::shared_ptr<async_io_handle>, exception_ptr *, detail::async_data_op_req_impl<iswrite>))
{
	std::vector<async_io_op> ret;
	ret.reserve(container.size());
	BOOST_AFIO_LOCK_GUARD<detail::async_file_io_dispatcher_base_p::opslock_t> opslockh(p->opslock);
	exception_ptr he;
	detail::immediate_async_ops immediates;
	BOOST_FOREACH(auto &i, container)
    {
		ret.push_back(chain_async_op(&he, immediates, optype, i.precondition, flags, f, i));
    }
	return ret;
}
// Directory enumerate specialisation
template<class F> std::pair<std::vector<future<std::pair<std::vector<directory_entry>, bool>>>, std::vector<async_io_op>> async_file_io_dispatcher_base::chain_async_ops(int optype, const std::vector<async_enumerate_op_req> &container, async_op_flags flags, completion_returntype (F::*f)(size_t, std::shared_ptr<async_io_handle>, exception_ptr *, async_enumerate_op_req, std::shared_ptr<promise<std::pair<std::vector<directory_entry>, bool>>> ret))
{
	typedef std::pair<std::vector<directory_entry>, bool> retitemtype;
	std::vector<async_io_op> ret;
	std::vector<future<retitemtype>> retfutures;
	ret.reserve(container.size());
	retfutures.reserve(container.size());
	BOOST_AFIO_LOCK_GUARD<detail::async_file_io_dispatcher_base_p::opslock_t> opslockh(p->opslock);
	exception_ptr he;
	detail::immediate_async_ops immediates;
	BOOST_FOREACH(auto &i, container)
    {
		// Unfortunately older C++0x compilers don't cope well with feeding move only std::future<> into std::bind
		auto transport=std::make_shared<promise<retitemtype>>();
		retfutures.push_back(std::move(transport->get_future()));
		ret.push_back(chain_async_op(&he, immediates, optype, i.precondition, flags, f, i, transport));
    }
	return std::make_pair(std::move(retfutures), std::move(ret));
}

namespace detail
{
	struct barrier_count_completed_state
	{
		atomic<size_t> togo;
		std::vector<std::pair<size_t, std::shared_ptr<async_io_handle>>> out;
		std::vector<std::shared_ptr<shared_future<std::shared_ptr<async_io_handle>>>> insharedstates;
		barrier_count_completed_state(const std::vector<async_io_op> &ops) : togo(ops.size()), out(ops.size())
		{
			insharedstates.reserve(ops.size());
			BOOST_FOREACH(auto &i, ops)
			{
				insharedstates.push_back(i.h);
			}
		}
	};
}

/* This is extremely naughty ... you really shouldn't be using templates to hide implementation
types, but hey it works and is non-header so so what ...
*/
//template<class T> async_file_io_dispatcher_base::completion_returntype async_file_io_dispatcher_base::dobarrier(size_t id, std::shared_ptr<async_io_handle> h, T state);
template<> async_file_io_dispatcher_base::completion_returntype async_file_io_dispatcher_base::dobarrier<std::pair<std::shared_ptr<detail::barrier_count_completed_state>, size_t>>(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *e, std::pair<std::shared_ptr<detail::barrier_count_completed_state>, size_t> state)
{
	size_t idx=state.second;
	detail::barrier_count_completed_state &s=*state.first;
	s.out[idx]=std::make_pair(id, h); // This might look thread unsafe, but each idx is unique
	if(--state.first->togo)
		return std::make_pair(false, h);
#if 1
	// On the basis that probably the preceding decrementing thread has yet to signal their future,
	// give up my timeslice
	boost::this_thread::yield();
#endif
#if 1
	// Rather than potentially expend a syscall per wait on each input op to complete, compose a list of input futures and wait on them all
	std::vector<shared_future<std::shared_ptr<async_io_handle>>> notready;
	notready.reserve(s.insharedstates.size()-1);
	for(idx=0; idx<s.insharedstates.size(); idx++)
	{
		shared_future<std::shared_ptr<async_io_handle>> &f=*s.insharedstates[idx];
		if(idx==state.second || f.is_ready()) continue;
		notready.push_back(f);
	}
	if(!notready.empty())
		boost::wait_for_all(notready.begin(), notready.end());
#endif
	// Last one just completed, so issue completions for everything in out except me
	for(idx=0; idx<s.out.size(); idx++)
	{
		if(idx==state.second) continue;
		shared_future<std::shared_ptr<async_io_handle>> &thisresult=*s.insharedstates[idx];
		// TODO: If I include the wait_for_all above, we need only fetch this if thisresult.has_exception().
		exception_ptr e(get_exception_ptr(thisresult));
		complete_async_op(s.out[idx].first, s.out[idx].second, e);
	}
	// Am I being called because my precondition threw an exception so we're actually currently inside an exception catch?
	// If so then duplicate the same exception throw
	if(e && *e)
		rethrow_exception(*e);
	else
		return std::make_pair(true, h);
}

std::vector<async_io_op> async_file_io_dispatcher_base::barrier(const std::vector<async_io_op> &ops)
{
#if BOOST_AFIO_VALIDATE_INPUTS
		BOOST_FOREACH(auto &i, ops)
        {
			if(!i.validate(false))
				BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
        }
#endif
	// Create a shared state for the completions to be attached to all the items we are waiting upon
	auto state(std::make_shared<detail::barrier_count_completed_state>(ops));
	// Create each of the parameters to be sent to each dobarrier
	std::vector<std::pair<std::shared_ptr<detail::barrier_count_completed_state>, size_t>> statev;
	statev.reserve(ops.size());
	size_t idx=0;
	BOOST_FOREACH(auto &op, ops)
	{
		statev.push_back(std::make_pair(state, idx++));
	}
	return chain_async_ops((int) detail::OpType::barrier, ops, statev, async_op_flags::ImmediateCompletion|async_op_flags::DetachedFuture, &async_file_io_dispatcher_base::dobarrier<std::pair<std::shared_ptr<detail::barrier_count_completed_state>, size_t>>);
}


#if defined(WIN32)
namespace detail {
	class async_file_io_dispatcher_windows : public async_file_io_dispatcher_base
	{
		friend class directory_entry;
		friend void directory_entry::_int_fetch(metadata_flags wanted, std::shared_ptr<async_io_handle> dirh);

		size_t pagesize;
		// Called in unknown thread
		completion_returntype dodir(size_t id, std::shared_ptr<async_io_handle> _, exception_ptr *e, async_path_op_req req)
		{
			BOOL ret=0;
			req.flags=fileflags(req.flags)|file_flags::int_opening_dir|file_flags::Read;
			if(!(req.flags & file_flags::UniqueDirectoryHandle) && !!(req.flags & file_flags::Read) && !(req.flags & file_flags::Write))
			{
				// Return a copy of the one in the dir cache if available
				return std::make_pair(true, p->get_handle_to_dir(this, id, e, req, &async_file_io_dispatcher_windows::dofile));
			}
			else
			{
				// With the NT kernel, you create a directory by creating a file.
				return dofile(id, _, e, req);
			}
		}
		// Called in unknown thread
		completion_returntype dormdir(size_t id, std::shared_ptr<async_io_handle> _, exception_ptr *, async_path_op_req req)
		{
			req.flags=fileflags(req.flags);
			BOOST_AFIO_ERRHWINFN(RemoveDirectory(req.path.c_str()), req.path);
			auto ret=std::make_shared<async_io_handle_windows>(shared_from_this(), std::shared_ptr<async_io_handle>(), req.path, req.flags);
			return std::make_pair(true, ret);
		}
		// Called in unknown thread
		completion_returntype dofile(size_t id, std::shared_ptr<async_io_handle>, exception_ptr *e, async_path_op_req req)
		{
			std::shared_ptr<async_io_handle> dirh;
			DWORD access=FILE_READ_ATTRIBUTES, creatdisp=0, flags=0x4000/*FILE_OPEN_FOR_BACKUP_INTENT*/|0x00200000/*FILE_OPEN_REPARSE_POINT*/, attribs=FILE_ATTRIBUTE_NORMAL;
			req.flags=fileflags(req.flags);
			if(!!(req.flags & file_flags::int_opening_dir))
			{
				flags|=0x01/*FILE_DIRECTORY_FILE*/;
				access|=FILE_LIST_DIRECTORY|FILE_TRAVERSE;
				if(!!(req.flags & file_flags::Read)) access|=GENERIC_READ;
				if(!!(req.flags & file_flags::Write)) access|=GENERIC_WRITE;
				// Windows doesn't like opening directories without buffering.
				if(!!(req.flags & file_flags::OSDirect)) req.flags=req.flags & ~file_flags::OSDirect;
			}
			else
			{
				flags|=0x040/*FILE_NON_DIRECTORY_FILE*/;
				if(!!(req.flags & file_flags::Append)) access|=FILE_APPEND_DATA|SYNCHRONIZE;
				else
				{
					if(!!(req.flags & file_flags::Read)) access|=GENERIC_READ;
					if(!!(req.flags & file_flags::Write)) access|=GENERIC_WRITE;
				}
			}
			if(!!(req.flags & file_flags::CreateOnlyIfNotExist)) creatdisp|=0x00000002/*FILE_CREATE*/;
			else if(!!(req.flags & file_flags::Create)) creatdisp|=0x00000003/*FILE_OPEN_IF*/;
			else if(!!(req.flags & file_flags::Truncate)) creatdisp|=0x00000005/*FILE_OVERWRITE_IF*/;
			else creatdisp|=0x00000001/*FILE_OPEN*/;
			if(!!(req.flags & file_flags::WillBeSequentiallyAccessed))
				flags|=0x00000004/*FILE_SEQUENTIAL_ONLY*/;
			else if(!!(req.flags & file_flags::WillBeRandomlyAccessed))
				flags|=0x00000800/*FILE_RANDOM_ACCESS*/;
			if(!!(req.flags & file_flags::OSDirect)) flags|=0x00000008/*FILE_NO_INTERMEDIATE_BUFFERING*/;
			if(!!(req.flags & file_flags::AlwaysSync)) flags|=0x00000002/*FILE_WRITE_THROUGH*/;
			if(!!(req.flags & file_flags::FastDirectoryEnumeration))
				dirh=p->get_handle_to_containing_dir(this, id, e, req, &async_file_io_dispatcher_windows::dofile);

			windows_nt_kernel::init();
			using namespace windows_nt_kernel;
			HANDLE h=nullptr;
			IO_STATUS_BLOCK isb={ 0 };
			OBJECT_ATTRIBUTES oa={sizeof(OBJECT_ATTRIBUTES)};
			std::filesystem::path path(req.path.make_preferred());
			UNICODE_STRING _path;
			if(isalpha(path.native()[0]) && path.native()[1]==':')
			{
				path=ntpath_from_dospath(path);
				// If it's a DOS path, ignore case differences
				oa.Attributes=0x40/*OBJ_CASE_INSENSITIVE*/;
			}
			_path.Buffer=const_cast<std::filesystem::path::value_type *>(path.c_str());
			_path.MaximumLength=(_path.Length=(USHORT) (path.native().size()*sizeof(std::filesystem::path::value_type)))+sizeof(std::filesystem::path::value_type);
			oa.ObjectName=&_path;
			// Should I bother with oa.RootDirectory? For now, no.
			LARGE_INTEGER AllocationSize={0};
			BOOST_AFIO_ERRHNTFN(NtCreateFile(&h, access, &oa, &isb, &AllocationSize, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
				creatdisp, flags, NULL, 0), req.path);
			// If writing and SyncOnClose and NOT synchronous, turn on SyncOnClose
			auto ret=std::make_shared<async_io_handle_windows>(shared_from_this(), dirh, req.path, req.flags,
				(file_flags::SyncOnClose|file_flags::Write)==(req.flags & (file_flags::SyncOnClose|file_flags::Write|file_flags::AlwaysSync)),
				h);
			static_cast<async_io_handle_windows *>(ret.get())->do_add_io_handle_to_parent();
			return std::make_pair(true, ret);
		}
		// Called in unknown thread
		completion_returntype dormfile(size_t id, std::shared_ptr<async_io_handle> _, exception_ptr *, async_path_op_req req)
		{
			req.flags=fileflags(req.flags);
			BOOST_AFIO_ERRHWINFN(DeleteFile(req.path.c_str()), req.path);
			auto ret=std::make_shared<async_io_handle_windows>(shared_from_this(), std::shared_ptr<async_io_handle>(), req.path, req.flags);
			return std::make_pair(true, ret);
		}
		// Called in unknown thread
		void boost_asio_symlink_completion_handler(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *, std::shared_ptr<std::unique_ptr<std::filesystem::path::value_type[]>> buffer, const boost::system::error_code &ec, size_t bytes_transferred)
		{
			if(ec)
			{
				exception_ptr e;
				// boost::system::system_error makes no attempt to ask windows for what the error code means :(
				try
				{
					BOOST_AFIO_ERRGWINFN(ec.value(), h->path());
				}
				catch(...)
				{
					e=afio::make_exception_ptr(afio::current_exception());
				}
				complete_async_op(id, h, e);
			}
			else
			{
				complete_async_op(id, h);
			}
		}
		// Called in unknown thread
		completion_returntype dosymlink(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *e, async_path_op_req req)
		{
			async_io_handle_windows *p=static_cast<async_io_handle_windows *>(h.get());
			req.flags=fileflags(req.flags);
			req.flags=req.flags|file_flags::int_opening_link;
			// For safety, best not create unless doesn't exist
			if(!!(req.flags&file_flags::Create))
			{
				req.flags=req.flags&~file_flags::Create;
				req.flags=req.flags|file_flags::CreateOnlyIfNotExist;
			}
			// If not creating, simply open
			if(!(req.flags&file_flags::CreateOnlyIfNotExist))
			{
				return dodir(id, h, e, req);
			}
			if(!!(h->flags()&file_flags::int_opening_dir))
			{
				// Create a directory junction
				windows_nt_kernel::init();
				using namespace windows_nt_kernel;
				using windows_nt_kernel::REPARSE_DATA_BUFFER;
				// First we need a new directory with write access
				req.flags=req.flags|file_flags::Write;
				completion_returntype ret=dodir(id, h, e, req);
				assert(ret.first);
				std::filesystem::path destpath(h->path());
				size_t destpathbytes=destpath.native().size()*sizeof(std::filesystem::path::value_type);
				size_t buffersize=sizeof(REPARSE_DATA_BUFFER)+destpathbytes*2+256;
				auto buffer=std::make_shared<std::unique_ptr<std::filesystem::path::value_type[]>>(new std::filesystem::path::value_type[buffersize]);
				REPARSE_DATA_BUFFER *rpd=(REPARSE_DATA_BUFFER *) buffer->get();
				memset(rpd, 0, sizeof(*rpd));
				rpd->ReparseTag=IO_REPARSE_TAG_MOUNT_POINT;
				if(isalpha(destpath.native()[0]) && destpath.native()[1]==':')
				{
					destpath=ntpath_from_dospath(destpath);
					destpathbytes=destpath.native().size()*sizeof(std::filesystem::path::value_type);
					memcpy(rpd->MountPointReparseBuffer.PathBuffer, destpath.c_str(), destpathbytes+sizeof(std::filesystem::path::value_type));
					rpd->MountPointReparseBuffer.SubstituteNameOffset=0;
					rpd->MountPointReparseBuffer.SubstituteNameLength=(USHORT)destpathbytes;
					rpd->MountPointReparseBuffer.PrintNameOffset=(USHORT)(destpathbytes+sizeof(std::filesystem::path::value_type));
					rpd->MountPointReparseBuffer.PrintNameLength=(USHORT)(h->path().native().size()*sizeof(std::filesystem::path::value_type));
					memcpy(rpd->MountPointReparseBuffer.PathBuffer+rpd->MountPointReparseBuffer.PrintNameOffset/sizeof(std::filesystem::path::value_type), h->path().c_str(), rpd->MountPointReparseBuffer.PrintNameLength+sizeof(std::filesystem::path::value_type));
				}
				else
				{
					memcpy(rpd->MountPointReparseBuffer.PathBuffer, destpath.c_str(), destpathbytes+sizeof(std::filesystem::path::value_type));
					rpd->MountPointReparseBuffer.SubstituteNameOffset=0;
					rpd->MountPointReparseBuffer.SubstituteNameLength=(USHORT)destpathbytes;
					rpd->MountPointReparseBuffer.PrintNameOffset=(USHORT)(destpathbytes+sizeof(std::filesystem::path::value_type));
					rpd->MountPointReparseBuffer.PrintNameLength=(USHORT)destpathbytes;
					memcpy(rpd->MountPointReparseBuffer.PathBuffer+rpd->MountPointReparseBuffer.PrintNameOffset/sizeof(std::filesystem::path::value_type), h->path().c_str(), rpd->MountPointReparseBuffer.PrintNameLength+sizeof(std::filesystem::path::value_type));
				}
				size_t headerlen=offsetof(REPARSE_DATA_BUFFER, MountPointReparseBuffer);
				size_t reparsebufferheaderlen=offsetof(REPARSE_DATA_BUFFER, MountPointReparseBuffer.PathBuffer)-headerlen;
				rpd->ReparseDataLength=(USHORT)(rpd->MountPointReparseBuffer.SubstituteNameLength+rpd->MountPointReparseBuffer.PrintNameLength+2*sizeof(std::filesystem::path::value_type)+reparsebufferheaderlen);
				boost::asio::windows::overlapped_ptr ol(p->h->get_io_service(), boost::bind(&async_file_io_dispatcher_windows::boost_asio_symlink_completion_handler, this, id, ret.second, nullptr, buffer, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
				BOOL ok=DeviceIoControl(ret.second->native_handle(), FSCTL_SET_REPARSE_POINT, rpd, (DWORD)(rpd->ReparseDataLength+headerlen), NULL, 0, NULL, ol.get());
				DWORD errcode=GetLastError();
				if(!ok && ERROR_IO_PENDING!=errcode)
				{
					//std::cerr << "ERROR " << errcode << std::endl;
					boost::system::error_code ec(errcode, boost::asio::error::get_system_category());
					ol.complete(ec, 0);
				}
				else
					ol.release();
				// Indicate we're not finished yet
				return std::make_pair(false, h);
			}
			else
			{
				// Create a symbolic link to a file
				BOOST_AFIO_THROW(std::runtime_error("Creating symbolic links to files is not yet supported on Windows. It wouldn't work without Administrator privileges anyway."));
			}
		}
		// Called in unknown thread
		completion_returntype dormsymlink(size_t id, std::shared_ptr<async_io_handle> _, exception_ptr *, async_path_op_req req)
		{
			req.flags=fileflags(req.flags);
			BOOST_AFIO_ERRHWINFN(RemoveDirectory(req.path.c_str()), req.path);
			auto ret=std::make_shared<async_io_handle_windows>(shared_from_this(), std::shared_ptr<async_io_handle>(), req.path, req.flags);
			return std::make_pair(true, ret);
		}
		// Called in unknown thread
		completion_returntype dosync(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *, async_io_op)
		{
			async_io_handle_windows *p=static_cast<async_io_handle_windows *>(h.get());
			off_t bytestobesynced=p->write_count_since_fsync();
			assert(p);
			if(bytestobesynced)
				BOOST_AFIO_ERRHWINFN(FlushFileBuffers(p->h->native_handle()), p->path());
			p->byteswrittenatlastfsync+=bytestobesynced;
			return std::make_pair(true, h);
		}
		// Called in unknown thread
		completion_returntype doclose(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *, async_io_op)
		{
			async_io_handle_windows *p=static_cast<async_io_handle_windows *>(h.get());
			assert(p);
			// Windows doesn't provide an async fsync so do it synchronously
			if(p->SyncOnClose && p->write_count_since_fsync())
				BOOST_AFIO_ERRHWINFN(FlushFileBuffers(p->h->native_handle()), p->path());
			p->h->close();
			p->h.reset();
			return std::make_pair(true, h);
		}
		// Called in unknown thread
		void boost_asio_readwrite_completion_handler(bool is_write, size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *, std::shared_ptr<std::pair<boost::afio::atomic<bool>, boost::afio::atomic<size_t>>> bytes_to_transfer, const boost::system::error_code &ec, size_t bytes_transferred)
		{
			if(ec)
			{
				exception_ptr e;
				// boost::system::system_error makes no attempt to ask windows for what the error code means :(
				try
				{
					BOOST_AFIO_ERRGWINFN(ec.value(), h->path());
				}
				catch(...)
				{
					e=afio::make_exception_ptr(afio::current_exception());
					bool exp=false;
					// If someone else has already returned an error, simply exit
					if(bytes_to_transfer->first.compare_exchange_strong(exp, true))
						return;
				}
				complete_async_op(id, h, e);
			}
			else
			{
				async_io_handle_windows *p=static_cast<async_io_handle_windows *>(h.get());
				if(is_write)
					p->byteswritten+=bytes_transferred;
				else
					p->bytesread+=bytes_transferred;
				if(!(bytes_to_transfer->second-=bytes_transferred))
					complete_async_op(id, h);
				BOOST_AFIO_DEBUG_PRINT("H %u e=%u\n", (unsigned) id, (unsigned) ec.value());
			}
			//std::cout << "id=" << id << " total=" << bytes_to_transfer->second << " this=" << bytes_transferred << std::endl;
		}
		template<bool iswrite> void doreadwrite(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *, detail::async_data_op_req_impl<iswrite> req, async_io_handle_windows *p)
		{
			// boost::asio::async_read_at() seems to have a bug and only transfers 64Kb per buffer
			// boost::asio::windows::random_access_handle::async_read_some_at() clearly bothers
			// with the first buffer only. Same goes for both async write functions.
			//
			// So we implement by hand and skip ASIO altogether.
			size_t amount=0;
			BOOST_FOREACH(auto &b, req.buffers)
			{
                amount+=boost::asio::buffer_size(b);
            }
			auto bytes_to_transfer=std::make_shared<std::pair<boost::afio::atomic<bool>, boost::afio::atomic<size_t>>>();
			bytes_to_transfer->second.store(amount);//mingw choked on operator=, thought amount was atomic&, so changed to store to avoid issue
			// Are we using direct i/o, because then we get the magic scatter/gather special functions?
			if(!!(p->flags() & file_flags::OSDirect))
			{
				// Yay we can use the direct i/o scatter gather functions which are far more efficient
				size_t pages=amount/pagesize, thisbufferoffset;
				std::vector<FILE_SEGMENT_ELEMENT> elems(1+pages);
				auto bufferit=req.buffers.begin();
				thisbufferoffset=0;
				for(size_t n=0; n<pages; n++)
				{
					// Be careful of 32 bit void * sign extension here ...
					elems[n].Alignment=((size_t) boost::asio::buffer_cast<const void *>(*bufferit))+thisbufferoffset;
					thisbufferoffset+=pagesize;
					if(thisbufferoffset>=boost::asio::buffer_size(*bufferit))
					{
						++bufferit;
						thisbufferoffset=0;
					}
				}
				elems[pages].Alignment=0;
				boost::asio::windows::overlapped_ptr ol(p->h->get_io_service(), boost::bind(&async_file_io_dispatcher_windows::boost_asio_readwrite_completion_handler, this, iswrite, id, h, nullptr, bytes_to_transfer, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
				ol.get()->Offset=(DWORD) (req.where & 0xffffffff);
				ol.get()->OffsetHigh=(DWORD) ((req.where>>32) & 0xffffffff);
				BOOL ok=iswrite ? WriteFileGather
					(p->h->native_handle(), &elems.front(), (DWORD) amount, NULL, ol.get())
					: ReadFileScatter
					(p->h->native_handle(), &elems.front(), (DWORD) amount, NULL, ol.get());
				DWORD errcode=GetLastError();
				if(!ok && ERROR_IO_PENDING!=errcode)
				{
					//std::cerr << "ERROR " << errcode << std::endl;
					boost::system::error_code ec(errcode, boost::asio::error::get_system_category());
					ol.complete(ec, 0);
				}
				else
					ol.release();
			}
			else
			{
				size_t offset=0;
				BOOST_FOREACH(auto &b, req.buffers)
				{
					boost::asio::windows::overlapped_ptr ol(p->h->get_io_service(), boost::bind(&async_file_io_dispatcher_windows::boost_asio_readwrite_completion_handler, this, iswrite, id, h, nullptr, bytes_to_transfer, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
					ol.get()->Offset=(DWORD) ((req.where+offset) & 0xffffffff);
					ol.get()->OffsetHigh=(DWORD) (((req.where+offset)>>32) & 0xffffffff);
					BOOL ok=iswrite ? WriteFile
						(p->h->native_handle(), boost::asio::buffer_cast<const void *>(b), (DWORD) boost::asio::buffer_size(b), NULL, ol.get())
						: ReadFile
						(p->h->native_handle(), (LPVOID) boost::asio::buffer_cast<const void *>(b), (DWORD) boost::asio::buffer_size(b), NULL, ol.get());
					DWORD errcode=GetLastError();
					if(!ok && ERROR_IO_PENDING!=errcode)
					{
						//std::cerr << "ERROR " << errcode << std::endl;
						boost::system::error_code ec(errcode, boost::asio::error::get_system_category());
						ol.complete(ec, 0);
					}
					else
						ol.release();
					offset+=boost::asio::buffer_size(b);
				}
			}
		}
		// Called in unknown thread
		completion_returntype doread(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *e, detail::async_data_op_req_impl<false> req)
		{
			async_io_handle_windows *p=static_cast<async_io_handle_windows *>(h.get());
			assert(p);
			BOOST_AFIO_DEBUG_PRINT("R %u %p (%c) @ %u, b=%u\n", (unsigned) id, h.get(), p->path().native().back(), (unsigned) req.where, (unsigned) req.buffers.size());
#ifdef DEBUG_PRINTING
			BOOST_FOREACH(auto &b, req.buffers)
            {	BOOST_AFIO_DEBUG_PRINT("  R %u: %p %u\n", (unsigned) id, boost::asio::buffer_cast<const void *>(b), (unsigned) boost::asio::buffer_size(b)); }
#endif
			doreadwrite(id, h, e, req, p);
			// Indicate we're not finished yet
			return std::make_pair(false, h);
		}
		// Called in unknown thread
		completion_returntype dowrite(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *e, detail::async_data_op_req_impl<true> req)
		{
			async_io_handle_windows *p=static_cast<async_io_handle_windows *>(h.get());
			assert(p);
			BOOST_AFIO_DEBUG_PRINT("W %u %p (%c) @ %u, b=%u\n", (unsigned) id, h.get(), p->path().native().back(), (unsigned) req.where, (unsigned) req.buffers.size());
#ifdef DEBUG_PRINTING
			BOOST_FOREACH(auto &b, req.buffers)
            {	BOOST_AFIO_DEBUG_PRINT("  W %u: %p %u\n", (unsigned) id, boost::asio::buffer_cast<const void *>(b), (unsigned) boost::asio::buffer_size(b)); }
#endif
			doreadwrite(id, h, e, req, p);
			// Indicate we're not finished yet
			return std::make_pair(false, h);
		}
		// Called in unknown thread
		completion_returntype dotruncate(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *, off_t _newsize)
		{
			async_io_handle_windows *p=static_cast<async_io_handle_windows *>(h.get());
			assert(p);
			BOOST_AFIO_DEBUG_PRINT("T %u %p (%c)\n", (unsigned) id, h.get(), p->path().native().back());
#if 1
			BOOST_AFIO_ERRHWINFN(wintruncate(p->h->native_handle(), _newsize), p->path());
#else
			// This is a bit tricky ... overlapped files ignore their file position except in this one
			// case, but clearly here we have a race condition. No choice but to rinse and repeat I guess.
			LARGE_INTEGER size={0}, newsize={0};
			newsize.QuadPart=_newsize;
			while(size.QuadPart!=newsize.QuadPart)
			{
				BOOST_AFIO_ERRHWINFN(SetFilePointerEx(p->h->native_handle(), newsize, NULL, FILE_BEGIN), p->path());
				BOOST_AFIO_ERRHWINFN(SetEndOfFile(p->h->native_handle()), p->path());
				BOOST_AFIO_ERRHWINFN(GetFileSizeEx(p->h->native_handle(), &size), p->path());
			}
#endif
			return std::make_pair(true, h);
		}
		// Called in unknown thread
		typedef std::shared_ptr<std::tuple<std::shared_ptr<promise<std::pair<std::vector<directory_entry>, bool>>>, std::unique_ptr<windows_nt_kernel::FILE_ID_FULL_DIR_INFORMATION[]>, async_enumerate_op_req>> enumerate_state_t;
		void boost_asio_enumerate_completion_handler(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *e, enumerate_state_t state, const boost::system::error_code &ec, size_t bytes_transferred)
		{
			using windows_nt_kernel::FILE_ID_FULL_DIR_INFORMATION;
			std::shared_ptr<promise<std::pair<std::vector<directory_entry>, bool>>> &ret=std::get<0>(*state);
			std::unique_ptr<FILE_ID_FULL_DIR_INFORMATION[]> &buffer=std::get<1>(*state);
			async_enumerate_op_req &req=std::get<2>(*state);
			if(ec && ERROR_MORE_DATA==ec.value() && bytes_transferred<(sizeof(FILE_ID_FULL_DIR_INFORMATION)+buffer.get()->FileNameLength))
			{
				// Bump maxitems by one and reschedule.
				req.maxitems++;
				buffer=std::unique_ptr<FILE_ID_FULL_DIR_INFORMATION[]>(new FILE_ID_FULL_DIR_INFORMATION[req.maxitems]);
				doenumerate(id, h, e, state);
				return;
			}
			if(ec && ERROR_MORE_DATA!=ec.value())
			{
				if(ERROR_NO_MORE_FILES==ec.value())
				{
					ret->set_value(std::make_pair(std::vector<directory_entry>(), false));
					complete_async_op(id, h);
					return;
				}
				exception_ptr e;
				// boost::system::system_error makes no attempt to ask windows for what the error code means :(
				try
				{
					BOOST_AFIO_ERRGWINFN(ec.value(), h->path());
				}
				catch(...)
				{
					e=afio::make_exception_ptr(afio::current_exception());
				}
				ret->set_exception(e);
				complete_async_op(id, h, e);
			}
			else
			{
				async_io_handle_windows *p=static_cast<async_io_handle_windows *>(h.get());
				using windows_nt_kernel::to_st_type;
				using windows_nt_kernel::to_timepoint;
				std::vector<directory_entry> _ret;
				_ret.reserve(req.maxitems);
				bool thisbatchdone=(sizeof(FILE_ID_FULL_DIR_INFORMATION)*req.maxitems-bytes_transferred)>sizeof(FILE_ID_FULL_DIR_INFORMATION);
				directory_entry item;
				// This is what windows returns with each enumeration
				item.have_metadata=directory_entry::metadata_fastpath();
				bool done=false;
				for(FILE_ID_FULL_DIR_INFORMATION *ffdi=buffer.get(); !done; ffdi=(FILE_ID_FULL_DIR_INFORMATION *)((size_t) ffdi + ffdi->NextEntryOffset))
				{
					size_t length=ffdi->FileNameLength/sizeof(std::filesystem::path::value_type);
					if(length<=2 && '.'==ffdi->FileName[0])
						if(1==length || '.'==ffdi->FileName[1]) continue;
					std::filesystem::path::string_type leafname(ffdi->FileName, length);
					item.leafname=std::move(leafname);
					item.stat.st_ino=ffdi->FileId.QuadPart;
					item.stat.st_type=to_st_type(ffdi->FileAttributes);
					item.stat.st_atim=to_timepoint(ffdi->LastAccessTime);
					item.stat.st_mtim=to_timepoint(ffdi->LastWriteTime);
					item.stat.st_ctim=to_timepoint(ffdi->ChangeTime);
					item.stat.st_size=ffdi->EndOfFile.QuadPart;
					item.stat.st_allocated=ffdi->AllocationSize.QuadPart;
					item.stat.st_birthtim=to_timepoint(ffdi->CreationTime);
					_ret.push_back(std::move(item));
					if(!ffdi->NextEntryOffset) done=true;
				}
				ret->set_value(std::make_pair(std::move(_ret), !thisbatchdone));
				complete_async_op(id, h);
			}
		}
		// Called in unknown thread
		void doenumerate(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *, enumerate_state_t state)
		{
			async_io_handle_windows *p=static_cast<async_io_handle_windows *>(h.get());
			using namespace windows_nt_kernel;
			std::unique_ptr<FILE_ID_FULL_DIR_INFORMATION[]> &buffer=std::get<1>(*state);
			async_enumerate_op_req &req=std::get<2>(*state);
			NTSTATUS ntstat;
			UNICODE_STRING _glob;
			if(!req.glob.empty())
			{
				_glob.Buffer=const_cast<std::filesystem::path::value_type *>(req.glob.c_str());
				_glob.Length=_glob.MaximumLength=(USHORT) req.glob.native().size();
			}
			boost::asio::windows::overlapped_ptr ol(p->h->get_io_service(), boost::bind(&async_file_io_dispatcher_windows::boost_asio_enumerate_completion_handler, this, id, h, nullptr, state, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
			bool done;
			do
			{
				// It's not tremendously obvious how to call the kernel directly with an OVERLAPPED. ReactOS's sources told me how ...
				ntstat=NtQueryDirectoryFile(p->h->native_handle(), ol.get()->hEvent, NULL, ol.get(), (PIO_STATUS_BLOCK) ol.get(),
					buffer.get(), (ULONG)(sizeof(FILE_ID_FULL_DIR_INFORMATION)*req.maxitems),
					FileIdFullDirectoryInformation, FALSE, req.glob.empty() ? NULL : &_glob, req.restart);
				if(STATUS_BUFFER_OVERFLOW==ntstat)
				{
					req.maxitems++;
					std::get<1>(*state)=std::unique_ptr<FILE_ID_FULL_DIR_INFORMATION[]>(new FILE_ID_FULL_DIR_INFORMATION[req.maxitems]);
					done=false;
				}
				else done=true;
			} while(!done);
			if(STATUS_PENDING!=ntstat)
			{
				//std::cerr << "ERROR " << errcode << std::endl;
				SetWin32LastErrorFromNtStatus(ntstat);
				boost::system::error_code ec(GetLastError(), boost::asio::error::get_system_category());
				ol.complete(ec, 0);
			}
			else
				ol.release();
		}
		// Called in unknown thread
		completion_returntype doenumerate(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *e, async_enumerate_op_req req, std::shared_ptr<promise<std::pair<std::vector<directory_entry>, bool>>> ret)
		{
			async_io_handle_windows *p=static_cast<async_io_handle_windows *>(h.get());
			windows_nt_kernel::init();
			using namespace windows_nt_kernel;

			// A bit messy this, but necessary
			enumerate_state_t state=std::make_shared<std::tuple<std::shared_ptr<promise<std::pair<std::vector<directory_entry>, bool>>>,
				std::unique_ptr<windows_nt_kernel::FILE_ID_FULL_DIR_INFORMATION[]>, async_enumerate_op_req>>(
				std::move(ret),
				std::unique_ptr<FILE_ID_FULL_DIR_INFORMATION[]>(new FILE_ID_FULL_DIR_INFORMATION[req.maxitems]),
				std::move(req)
				);
			doenumerate(id, std::move(h), e, std::move(state));

			// Indicate we're not finished yet
			return std::make_pair(false, h);
		}

	public:
		async_file_io_dispatcher_windows(std::shared_ptr<thread_source> threadpool, file_flags flagsforce, file_flags flagsmask) : async_file_io_dispatcher_base(threadpool, flagsforce, flagsmask), pagesize(page_size())
		{
		}

		virtual std::vector<async_io_op> dir(const std::vector<async_path_op_req> &reqs)
		{
#if BOOST_AFIO_VALIDATE_INPUTS
			BOOST_FOREACH(auto &i, reqs)
            {
				if(!i.validate())
					BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
			return chain_async_ops((int) detail::OpType::dir, reqs, async_op_flags::None, &async_file_io_dispatcher_windows::dodir);
		}
		virtual std::vector<async_io_op> rmdir(const std::vector<async_path_op_req> &reqs)
		{
#if BOOST_AFIO_VALIDATE_INPUTS
			BOOST_FOREACH(auto &i, reqs)
            {
				if(!i.validate())
					BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
			return chain_async_ops((int) detail::OpType::rmdir, reqs, async_op_flags::None, &async_file_io_dispatcher_windows::dormdir);
		}
		virtual std::vector<async_io_op> file(const std::vector<async_path_op_req> &reqs)
		{
#if BOOST_AFIO_VALIDATE_INPUTS
			BOOST_FOREACH(auto &i, reqs)
            {
				if(!i.validate())
					BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
			return chain_async_ops((int) detail::OpType::file, reqs, async_op_flags::None, &async_file_io_dispatcher_windows::dofile);
		}
		virtual std::vector<async_io_op> rmfile(const std::vector<async_path_op_req> &reqs)
		{
#if BOOST_AFIO_VALIDATE_INPUTS
			BOOST_FOREACH(auto &i, reqs)
            {
				if(!i.validate())
					BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
			return chain_async_ops((int) detail::OpType::rmfile, reqs, async_op_flags::None, &async_file_io_dispatcher_windows::dormfile);
		}
		virtual std::vector<async_io_op> symlink(const std::vector<async_path_op_req> &reqs)
		{
#if BOOST_AFIO_VALIDATE_INPUTS
			BOOST_FOREACH(auto &i, reqs)
            {
				if(!i.validate())
					BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
			return chain_async_ops((int) detail::OpType::symlink, reqs, async_op_flags::DetachedFuture, &async_file_io_dispatcher_windows::dosymlink);
		}
		virtual std::vector<async_io_op> rmsymlink(const std::vector<async_path_op_req> &reqs)
		{
#if BOOST_AFIO_VALIDATE_INPUTS
			BOOST_FOREACH(auto &i, reqs)
            {
				if(!i.validate())
					BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
			return chain_async_ops((int) detail::OpType::rmsymlink, reqs, async_op_flags::None, &async_file_io_dispatcher_windows::dormsymlink);
		}
		virtual std::vector<async_io_op> sync(const std::vector<async_io_op> &ops)
		{
#if BOOST_AFIO_VALIDATE_INPUTS
			BOOST_FOREACH(auto &i, ops)
            {
				if(!i.validate())
					BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
			return chain_async_ops((int) detail::OpType::sync, ops, async_op_flags::None, &async_file_io_dispatcher_windows::dosync);
		}
		virtual std::vector<async_io_op> close(const std::vector<async_io_op> &ops)
		{
#if BOOST_AFIO_VALIDATE_INPUTS
			BOOST_FOREACH(auto &i, ops)
            {
				if(!i.validate())
					BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
			return chain_async_ops((int) detail::OpType::close, ops, async_op_flags::None, &async_file_io_dispatcher_windows::doclose);
		}
		virtual std::vector<async_io_op> read(const std::vector<detail::async_data_op_req_impl<false>> &reqs)
		{
#if BOOST_AFIO_VALIDATE_INPUTS
			BOOST_FOREACH(auto &i, reqs)
            {
				if(!i.validate())
					BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
			return chain_async_ops((int) detail::OpType::read, reqs, async_op_flags::DetachedFuture|async_op_flags::ImmediateCompletion, &async_file_io_dispatcher_windows::doread);
		}
		virtual std::vector<async_io_op> write(const std::vector<detail::async_data_op_req_impl<true>> &reqs)
		{
#if BOOST_AFIO_VALIDATE_INPUTS
			BOOST_FOREACH(auto &i, reqs)
            {
				if(!i.validate())
					BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
			return chain_async_ops((int) detail::OpType::write, reqs, async_op_flags::DetachedFuture|async_op_flags::ImmediateCompletion, &async_file_io_dispatcher_windows::dowrite);
		}
		virtual std::vector<async_io_op> truncate(const std::vector<async_io_op> &ops, const std::vector<off_t> &sizes)
		{
#if BOOST_AFIO_VALIDATE_INPUTS
			BOOST_FOREACH(auto &i, ops)
            {
				if(!i.validate())
					BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
			return chain_async_ops((int) detail::OpType::truncate, ops, sizes, async_op_flags::None, &async_file_io_dispatcher_windows::dotruncate);
		}
		virtual std::pair<std::vector<future<std::pair<std::vector<directory_entry>, bool>>>, std::vector<async_io_op>> enumerate(const std::vector<async_enumerate_op_req> &reqs)
		{
#if BOOST_AFIO_VALIDATE_INPUTS
			BOOST_FOREACH(auto &i, reqs)
            {
				if(!i.validate())
					BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
			return chain_async_ops((int) detail::OpType::enumerate, reqs, async_op_flags::DetachedFuture|async_op_flags::ImmediateCompletion, &async_file_io_dispatcher_windows::doenumerate);
		}
	};
} //namespace
#endif
namespace detail {
	class async_file_io_dispatcher_compat : public async_file_io_dispatcher_base
	{
		// Called in unknown thread
		completion_returntype dodir(size_t id, std::shared_ptr<async_io_handle> _, exception_ptr *e, async_path_op_req req)
		{
			int ret=0;
			req.flags=fileflags(req.flags)|file_flags::int_opening_dir|file_flags::Read;
			if(!!(req.flags & (file_flags::Create|file_flags::CreateOnlyIfNotExist)))
			{
				ret=BOOST_AFIO_POSIX_MKDIR(req.path.c_str(), 0x1f8/*770*/);
				if(-1==ret && EEXIST==errno)
				{
					// Ignore already exists unless we were asked otherwise
					if(!(req.flags & file_flags::CreateOnlyIfNotExist))
						ret=0;
				}
				BOOST_AFIO_ERRHOSFN(ret, req.path);
				req.flags=req.flags&~(file_flags::Create|file_flags::CreateOnlyIfNotExist);
			}

			BOOST_AFIO_POSIX_STAT_STRUCT s={0};
			ret=BOOST_AFIO_POSIX_STAT(req.path.c_str(), &s);
			if(0==ret && S_IFDIR!=(s.st_mode&S_IFDIR))
				BOOST_AFIO_THROW(std::runtime_error("Not a directory"));
			if(!(req.flags & file_flags::UniqueDirectoryHandle) && !!(req.flags & file_flags::Read) && !(req.flags & file_flags::Write))
			{
				// Return a copy of the one in the dir cache if available
				return std::make_pair(true, p->get_handle_to_dir(this, id, e, req, &async_file_io_dispatcher_compat::dofile));
			}
			else
			{
				return dofile(id, _, e, req);
			}
		}
		// Called in unknown thread
		completion_returntype dormdir(size_t id, std::shared_ptr<async_io_handle> _, exception_ptr *, async_path_op_req req)
		{
			req.flags=fileflags(req.flags);
			BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_RMDIR(req.path.c_str()), req.path);
			auto ret=std::make_shared<async_io_handle_posix>(shared_from_this(), std::shared_ptr<async_io_handle>(), req.path, req.flags, false, -999);
			return std::make_pair(true, ret);
		}
		// Called in unknown thread
		completion_returntype dofile(size_t id, std::shared_ptr<async_io_handle>, exception_ptr *e, async_path_op_req req)
		{
			int flags=0;
			std::shared_ptr<async_io_handle> dirh;
			req.flags=fileflags(req.flags);
			if(!!(req.flags & file_flags::Read) && !!(req.flags & file_flags::Write)) flags|=O_RDWR;
			else if(!!(req.flags & file_flags::Read)) flags|=O_RDONLY;
			else if(!!(req.flags & file_flags::Write)) flags|=O_WRONLY;
			if(!!(req.flags & file_flags::Append)) flags|=O_APPEND;
			if(!!(req.flags & file_flags::Truncate)) flags|=O_TRUNC;
			if(!!(req.flags & file_flags::CreateOnlyIfNotExist)) flags|=O_EXCL|O_CREAT;
			else if(!!(req.flags & file_flags::Create)) flags|=O_CREAT;
#ifdef O_DIRECT
			if(!!(req.flags & file_flags::OSDirect)) flags|=O_DIRECT;
#endif
#ifdef O_SYNC
			if(!!(req.flags & file_flags::AlwaysSync)) flags|=O_SYNC;
#endif
			if(!!(req.flags & file_flags::int_opening_dir))
			{
#ifdef O_DIRECTORY
				flags|=O_DIRECTORY;
#endif
				// Some POSIXs don't like opening directories without buffering.
				if(!!(req.flags & file_flags::OSDirect))
				{
					req.flags=req.flags & ~file_flags::OSDirect;
#ifdef O_DIRECT
					flags&=~O_DIRECT;
#endif
				}
			}
			if(!!(req.flags & file_flags::FastDirectoryEnumeration))
				dirh=p->get_handle_to_containing_dir(this, id, e, req, &async_file_io_dispatcher_compat::dofile);
			// If writing and SyncOnClose and NOT synchronous, turn on SyncOnClose
			auto ret=std::make_shared<async_io_handle_posix>(shared_from_this(), dirh, req.path, req.flags, (file_flags::SyncOnClose|file_flags::Write)==(req.flags & (file_flags::SyncOnClose|file_flags::Write|file_flags::AlwaysSync)),
				BOOST_AFIO_POSIX_OPEN(req.path.c_str(), flags, 0x1b0/*660*/));
			static_cast<async_io_handle_posix *>(ret.get())->do_add_io_handle_to_parent();
			return std::make_pair(true, ret);
		}
		// Called in unknown thread
		completion_returntype dormfile(size_t id, std::shared_ptr<async_io_handle> _, exception_ptr *, async_path_op_req req)
		{
			req.flags=fileflags(req.flags);
			BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_UNLINK(req.path.c_str()), req.path);
			auto ret=std::make_shared<async_io_handle_posix>(shared_from_this(), std::shared_ptr<async_io_handle>(), req.path, req.flags, false, -999);
			return std::make_pair(true, ret);
		}
		// Called in unknown thread
		completion_returntype dosymlink(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *e, async_path_op_req req)
		{
#ifdef WIN32
			BOOST_AFIO_THROW(std::runtime_error("Creating symbolic links via MSVCRT is not supported on Windows."));
#else
			req.flags=fileflags(req.flags)|file_flags::int_opening_link;
			BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_SYMLINK(h->path().c_str(), req.path.c_str()), req.path);
			BOOST_AFIO_POSIX_STAT_STRUCT s={0};
			BOOST_AFIO_ERRHOS(BOOST_AFIO_POSIX_STAT(req.path.c_str(), &s));
			if(S_IFDIR!=(s.st_mode&S_IFDIR))
				return dofile(id, h, e, req);
			else
				return dodir(id, h, e, req);
#endif
		}
		// Called in unknown thread
		completion_returntype dormsymlink(size_t id, std::shared_ptr<async_io_handle> _, exception_ptr *, async_path_op_req req)
		{
			req.flags=fileflags(req.flags);
			BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_UNLINK(req.path.c_str()), req.path);
			auto ret=std::make_shared<async_io_handle_posix>(shared_from_this(), std::shared_ptr<async_io_handle>(), req.path, req.flags, false, -999);
			return std::make_pair(true, ret);
		}
		// Called in unknown thread
		completion_returntype dosync(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *, async_io_op)
		{
			async_io_handle_posix *p=static_cast<async_io_handle_posix *>(h.get());
			off_t bytestobesynced=p->write_count_since_fsync();
			if(bytestobesynced)
				BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_FSYNC(p->fd), p->path());
			p->has_ever_been_fsynced=true;
			p->byteswrittenatlastfsync+=bytestobesynced;
			return std::make_pair(true, h);
		}
		// Called in unknown thread
		completion_returntype doclose(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *, async_io_op)
		{
			async_io_handle_posix *p=static_cast<async_io_handle_posix *>(h.get());
			if(p->SyncOnClose && p->write_count_since_fsync())
				BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_FSYNC(p->fd), p->path());
			BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_CLOSE(p->fd), p->path());
			p->fd=-1;
			return std::make_pair(true, h);
		}
		// Called in unknown thread
		completion_returntype doread(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *, detail::async_data_op_req_impl<false> req)
		{
			async_io_handle_posix *p=static_cast<async_io_handle_posix *>(h.get());
			ssize_t bytesread=0, bytestoread=0;
			iovec v;
			std::vector<iovec> vecs;
			vecs.reserve(req.buffers.size());
			BOOST_AFIO_DEBUG_PRINT("R %u %p (%c) @ %u, b=%u\n", (unsigned) id, h.get(), p->path().native().back(), (unsigned) req.where, (unsigned) req.buffers.size());
#ifdef DEBUG_PRINTING
			BOOST_FOREACH(auto &b, req.buffers)
            {	
                BOOST_AFIO_DEBUG_PRINT("  R %u: %p %u\n", (unsigned) id, boost::asio::buffer_cast<const void *>(b), (unsigned) boost::asio::buffer_size(b));
            }
#endif
			BOOST_FOREACH(auto &b, req.buffers)
			{
				v.iov_base=boost::asio::buffer_cast<void *>(b);
				v.iov_len=boost::asio::buffer_size(b);
				bytestoread+=v.iov_len;
				vecs.push_back(v);
			}
			for(size_t n=0; n<vecs.size(); n+=IOV_MAX)
			{
				ssize_t _bytesread;
				BOOST_AFIO_ERRHOSFN((int) (_bytesread=preadv(p->fd, (&vecs.front())+n, std::min((int) (vecs.size()-n), IOV_MAX), req.where+bytesread)), p->path());
				p->bytesread+=_bytesread;
				bytesread+=_bytesread;
			}
			if(bytesread!=bytestoread)
				BOOST_AFIO_THROW_FATAL(std::runtime_error("Failed to read all buffers"));
			return std::make_pair(true, h);
		}
		// Called in unknown thread
		completion_returntype dowrite(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *, detail::async_data_op_req_impl<true> req)
		{
			async_io_handle_posix *p=static_cast<async_io_handle_posix *>(h.get());
			ssize_t byteswritten=0, bytestowrite=0;
			iovec v;
			std::vector<iovec> vecs;
			vecs.reserve(req.buffers.size());
			BOOST_AFIO_DEBUG_PRINT("W %u %p (%c) @ %u, b=%u\n", (unsigned) id, h.get(), p->path().native().back(), (unsigned) req.where, (unsigned) req.buffers.size());
#ifdef DEBUG_PRINTING
			BOOST_FOREACH(auto &b, req.buffers)
            {	
                BOOST_AFIO_DEBUG_PRINT("  W %u: %p %u\n", (unsigned) id, boost::asio::buffer_cast<const void *>(b), (unsigned) boost::asio::buffer_size(b));
            }
#endif
			BOOST_FOREACH(auto &b, req.buffers)
			{
				v.iov_base=(void *) boost::asio::buffer_cast<const void *>(b);
				v.iov_len=boost::asio::buffer_size(b);
				bytestowrite+=v.iov_len;
				vecs.push_back(v);
			}
			for(size_t n=0; n<vecs.size(); n+=IOV_MAX)
			{
				ssize_t _byteswritten;
				BOOST_AFIO_ERRHOSFN((int) (_byteswritten=pwritev(p->fd, (&vecs.front())+n, std::min((int) (vecs.size()-n), IOV_MAX), req.where+byteswritten)), p->path());
				p->byteswritten+=_byteswritten;
				byteswritten+=_byteswritten;
			}
			if(byteswritten!=bytestowrite)
				BOOST_AFIO_THROW_FATAL(std::runtime_error("Failed to write all buffers"));
			return std::make_pair(true, h);
		}
		// Called in unknown thread
		completion_returntype dotruncate(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *, off_t newsize)
		{
			async_io_handle_posix *p=static_cast<async_io_handle_posix *>(h.get());
			BOOST_AFIO_DEBUG_PRINT("T %u %p (%c)\n", (unsigned) id, h.get(), p->path().native().back());
			BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_FTRUNCATE(p->fd, newsize), p->path());
			return std::make_pair(true, h);
		}
		// Called in unknown thread
		completion_returntype doenumerate(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *, async_enumerate_op_req req, std::shared_ptr<promise<std::pair<std::vector<directory_entry>, bool>>> ret)
		{
			async_io_handle_posix *p=static_cast<async_io_handle_posix *>(h.get());
			try
			{
#ifdef WIN32
				BOOST_AFIO_THROW(std::runtime_error("Enumerating directories via MSVCRT is not supported."));
#else
#ifdef __linux__
				// Unlike FreeBSD, Linux doesn't define a getdents() function, so we'll do that here.
				typedef int (*getdents64_t)(int, char *, int);
				getdents64_t getdents=(getdents64_t)([](int fd, char *buf, int count) -> int { return syscall(SYS_getdents64, fd, buf, count); });
				typedef dirent64 dirent;
#endif
				auto buffer=std::unique_ptr<dirent[]>(new dirent[req.maxitems]);
				if(req.restart)
				{
					BOOST_AFIO_ERRHOS(lseek64(p->fd, 0, SEEK_SET));
				}
				int bytes;
				bool done;
				do
				{
					bytes=getdents(p->fd, (char *) buffer.get(), sizeof(dirent)*req.maxitems);
					if(-1==bytes && EINVAL==errno)
					{
						req.maxitems++;
						buffer=std::unique_ptr<dirent[]>(new dirent[req.maxitems]);
						done=false;
					}
					else done=true;
				} while(!done);
				BOOST_AFIO_ERRHOS(bytes);
				if(!bytes)
				{
					ret->set_value(std::make_pair(std::vector<directory_entry>(), false));
					return std::make_pair(true, h);
				}
				VALGRIND_MAKE_MEM_DEFINED_IF_ADDRESSABLE(buffer.get(), bytes);
				bool thisbatchdone=(sizeof(dirent)*req.maxitems-bytes)>sizeof(dirent);
				std::vector<directory_entry> _ret;
				_ret.reserve(req.maxitems);
				directory_entry item;
				// This is what POSIX returns with getdents()
				item.have_metadata=item.have_metadata|metadata_flags::ino|metadata_flags::type;
				done=false;
				for(dirent *dent=buffer.get(); !done; dent=(dirent *)((size_t) dent + dent->d_reclen))
				{
					if((bytes-=dent->d_reclen)<=0) done=true;
					if(!dent->d_ino)
						continue;
					size_t length=strchr(dent->d_name, 0)-dent->d_name;
					if(length<=2 && '.'==dent->d_name[0])
						if(1==length || '.'==dent->d_name[1]) continue;
					if(!req.glob.empty() && fnmatch(req.glob.native().c_str(), dent->d_name, 0)) continue;
					std::filesystem::path::string_type leafname(dent->d_name, length);
					item.leafname=std::move(leafname);
					item.stat.st_ino=dent->d_ino;
					char d_type=dent->d_type;
					if(DT_UNKNOWN==d_type)
						item.have_metadata=item.have_metadata&~metadata_flags::type;
					else
					{
						item.have_metadata=item.have_metadata|metadata_flags::type;
						switch(d_type)
						{
						case DT_BLK:
							item.stat.st_type=S_IFBLK;
							break;
						case DT_CHR:
							item.stat.st_type=S_IFCHR;
							break;
						case DT_DIR:
							item.stat.st_type=S_IFDIR;
							break;
						case DT_FIFO:
							item.stat.st_type=S_IFIFO;
							break;
						case DT_LNK:
							item.stat.st_type=S_IFLNK;
							break;
						case DT_REG:
							item.stat.st_type=S_IFREG;
							break;
						case DT_SOCK:
							item.stat.st_type=S_IFSOCK;
							break;
						default:
							item.have_metadata=item.have_metadata&~metadata_flags::type;
							item.stat.st_type=0;
							break;
						}
					}
					_ret.push_back(std::move(item));
				}
				ret->set_value(std::make_pair(std::move(_ret), !thisbatchdone));
#endif
				return std::make_pair(true, h);
			}
			catch(...)
			{
				ret->set_exception(afio::make_exception_ptr(afio::current_exception()));
				throw;
			}
		}

	public:
		async_file_io_dispatcher_compat(std::shared_ptr<thread_source> threadpool, file_flags flagsforce, file_flags flagsmask) : async_file_io_dispatcher_base(threadpool, flagsforce, flagsmask)
		{
		}


		virtual std::vector<async_io_op> dir(const std::vector<async_path_op_req> &reqs)
		{
#if BOOST_AFIO_VALIDATE_INPUTS
			BOOST_FOREACH(auto &i, reqs)
            {
				if(!i.validate())
					BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
			return chain_async_ops((int) detail::OpType::dir, reqs, async_op_flags::None, &async_file_io_dispatcher_compat::dodir);
		}
		virtual std::vector<async_io_op> rmdir(const std::vector<async_path_op_req> &reqs)
		{
#if BOOST_AFIO_VALIDATE_INPUTS
			BOOST_FOREACH(auto &i, reqs)
            {
				if(!i.validate())
					BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
			return chain_async_ops((int) detail::OpType::rmdir, reqs, async_op_flags::None, &async_file_io_dispatcher_compat::dormdir);
		}
		virtual std::vector<async_io_op> file(const std::vector<async_path_op_req> &reqs)
		{
#if BOOST_AFIO_VALIDATE_INPUTS
			BOOST_FOREACH(auto &i, reqs)
            {
				if(!i.validate())
					BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
			return chain_async_ops((int) detail::OpType::file, reqs, async_op_flags::None, &async_file_io_dispatcher_compat::dofile);
		}
		virtual std::vector<async_io_op> rmfile(const std::vector<async_path_op_req> &reqs)
		{
#if BOOST_AFIO_VALIDATE_INPUTS
			BOOST_FOREACH(auto &i, reqs)
            {
				if(!i.validate())
					BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
			return chain_async_ops((int) detail::OpType::rmfile, reqs, async_op_flags::None, &async_file_io_dispatcher_compat::dormfile);
		}
		virtual std::vector<async_io_op> symlink(const std::vector<async_path_op_req> &reqs)
		{
#if BOOST_AFIO_VALIDATE_INPUTS
			BOOST_FOREACH(auto &i, reqs)
            {
				if(!i.validate())
					BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
			return chain_async_ops((int) detail::OpType::symlink, reqs, async_op_flags::None, &async_file_io_dispatcher_compat::dosymlink);
		}
		virtual std::vector<async_io_op> rmsymlink(const std::vector<async_path_op_req> &reqs)
		{
#if BOOST_AFIO_VALIDATE_INPUTS
			BOOST_FOREACH(auto &i, reqs)
            {
				if(!i.validate())
					BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
			return chain_async_ops((int) detail::OpType::rmsymlink, reqs, async_op_flags::None, &async_file_io_dispatcher_compat::dormsymlink);
		}
		virtual std::vector<async_io_op> sync(const std::vector<async_io_op> &ops)
		{
#if BOOST_AFIO_VALIDATE_INPUTS
			BOOST_FOREACH(auto &i, ops)
            {
				if(!i.validate())
					BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
			return chain_async_ops((int) detail::OpType::sync, ops, async_op_flags::None, &async_file_io_dispatcher_compat::dosync);
		}
		virtual std::vector<async_io_op> close(const std::vector<async_io_op> &ops)
		{
#if BOOST_AFIO_VALIDATE_INPUTS
			BOOST_FOREACH(auto &i, ops)
            {
				if(!i.validate())
					BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
			return chain_async_ops((int) detail::OpType::close, ops, async_op_flags::None, &async_file_io_dispatcher_compat::doclose);
		}
		virtual std::vector<async_io_op> read(const std::vector<detail::async_data_op_req_impl<false>> &reqs)
		{
#if BOOST_AFIO_VALIDATE_INPUTS
			BOOST_FOREACH(auto &i, reqs)
            {
				if(!i.validate())
					BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
			return chain_async_ops((int) detail::OpType::read, reqs, async_op_flags::None, &async_file_io_dispatcher_compat::doread);
		}
		virtual std::vector<async_io_op> write(const std::vector<detail::async_data_op_req_impl<true>> &reqs)
		{
#if BOOST_AFIO_VALIDATE_INPUTS
			BOOST_FOREACH(auto &i, reqs)
            {
				if(!i.validate())
					BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
			return chain_async_ops((int) detail::OpType::write, reqs, async_op_flags::None, &async_file_io_dispatcher_compat::dowrite);
		}
		virtual std::vector<async_io_op> truncate(const std::vector<async_io_op> &ops, const std::vector<off_t> &sizes)
		{
#if BOOST_AFIO_VALIDATE_INPUTS
			BOOST_FOREACH(auto &i, ops)
            {
				if(!i.validate())
					BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
			return chain_async_ops((int) detail::OpType::truncate, ops, sizes, async_op_flags::None, &async_file_io_dispatcher_compat::dotruncate);
		}
		virtual std::pair<std::vector<future<std::pair<std::vector<directory_entry>, bool>>>, std::vector<async_io_op>> enumerate(const std::vector<async_enumerate_op_req> &reqs)
		{
#if BOOST_AFIO_VALIDATE_INPUTS
			BOOST_FOREACH(auto &i, reqs)
            {
				if(!i.validate())
					BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
			return chain_async_ops((int) detail::OpType::enumerate, reqs, async_op_flags::None, &async_file_io_dispatcher_compat::doenumerate);
		}
	};
}

void directory_entry::_int_fetch(metadata_flags wanted, std::shared_ptr<async_io_handle> dirh)
{
#ifdef WIN32
	detail::async_file_io_dispatcher_windows *dispatcher=dynamic_cast<detail::async_file_io_dispatcher_windows *>(dirh->parent());
	if(dispatcher)
	{
		windows_nt_kernel::init();
		using namespace windows_nt_kernel;
		bool slowPath=(!!(wanted&metadata_flags::nlink) || !!(wanted&metadata_flags::blocks) || !!(wanted&metadata_flags::blksize));
		if(!slowPath)
		{
			// Fast path skips opening a handle per file by enumerating the containing directory using a glob
			// exactly matching the leafname. This is about 10x quicker, so it's very much worth it.
			BOOST_AFIO_TYPEALIGNMENT(8) std::filesystem::path::value_type buffer[sizeof(FILE_ALL_INFORMATION)/sizeof(std::filesystem::path::value_type)+32769];
			IO_STATUS_BLOCK isb={ 0 };
			UNICODE_STRING _glob;
			NTSTATUS ntstat;
			_glob.Buffer=const_cast<std::filesystem::path::value_type *>(leafname.c_str());
			_glob.MaximumLength=(_glob.Length=(USHORT) (leafname.native().size()*sizeof(std::filesystem::path::value_type)))+sizeof(std::filesystem::path::value_type);
			FILE_ID_FULL_DIR_INFORMATION *ffdi=(FILE_ID_FULL_DIR_INFORMATION *) buffer;
			ntstat=NtQueryDirectoryFile(dirh->native_handle(), NULL, NULL, NULL, &isb, ffdi, sizeof(buffer),
				FileIdFullDirectoryInformation, TRUE, &_glob, FALSE);
			if(STATUS_PENDING==ntstat)
				ntstat=NtWaitForSingleObject(dirh->native_handle(), FALSE, NULL);
			BOOST_AFIO_ERRHNTFN(ntstat, dirh->path());
			if(!!(wanted&metadata_flags::ino)) { stat.st_ino=ffdi->FileId.QuadPart; }
			if(!!(wanted&metadata_flags::type)) { stat.st_type=to_st_type(ffdi->FileAttributes); }
			if(!!(wanted&metadata_flags::atim)) { stat.st_atim=to_timepoint(ffdi->LastAccessTime); }
			if(!!(wanted&metadata_flags::mtim)) { stat.st_mtim=to_timepoint(ffdi->LastWriteTime); }
			if(!!(wanted&metadata_flags::ctim)) { stat.st_ctim=to_timepoint(ffdi->ChangeTime); }
			if(!!(wanted&metadata_flags::size)) { stat.st_size=ffdi->EndOfFile.QuadPart; }
			if(!!(wanted&metadata_flags::allocated)) { stat.st_allocated=ffdi->AllocationSize.QuadPart; }
			if(!!(wanted&metadata_flags::birthtim)) { stat.st_birthtim=to_timepoint(ffdi->CreationTime); }
		}
		else
		{
			// No choice here, open a handle and stat it.
			async_path_op_req req(dirh->path()/name(), file_flags::Read);
			auto fileh=dispatcher->dofile(0, std::shared_ptr<async_io_handle>(), nullptr, req).second;
			stat=fileh->lstat(wanted);
		}
	}
	else
#endif
	{
		BOOST_AFIO_POSIX_STAT_STRUCT s={0};
		std::filesystem::path path(dirh->path());
		path/=leafname;
		BOOST_AFIO_ERRHOSFN(BOOST_AFIO_POSIX_LSTAT(path.c_str(), &s), path);
		fill_stat_t(stat, s, wanted);
	}
}

std::shared_ptr<async_file_io_dispatcher_base> make_async_file_io_dispatcher(std::shared_ptr<thread_source> threadpool, file_flags flagsforce, file_flags flagsmask)
{
#if defined(WIN32) && !defined(USE_POSIX_ON_WIN32)
	return std::make_shared<detail::async_file_io_dispatcher_windows>(threadpool, flagsforce, flagsmask);
#else
	return std::make_shared<detail::async_file_io_dispatcher_compat>(threadpool, flagsforce, flagsmask);
#endif
}

} } // namespace
