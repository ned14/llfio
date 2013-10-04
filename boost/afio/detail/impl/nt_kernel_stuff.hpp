/* async_file_io
Provides a threadpool and asynchronous file i/o infrastructure based on Boost.ASIO, Boost.Iostreams and filesystem
(C) 2013 Niall Douglas http://www.nedprod.com/
File Created: Mar 2013
*/

#ifdef WIN32
#include <Windows.h>
#include <winioctl.h>

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

    static inline void doinit()
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
        static BOOST_CONSTEXPR_OR_CONST unsigned long long FILETIME_OFFSET_TO_1970=((27111902ULL << 32U) + 3577643008ULL);
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

#endif
