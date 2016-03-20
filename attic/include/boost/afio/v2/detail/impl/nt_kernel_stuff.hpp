/* async_file_io
Provides a threadpool and asynchronous file i/o infrastructure based on Boost.ASIO, Boost.Iostreams and filesystem
(C) 2013 Niall Douglas http://www.nedprod.com/
File Created: Mar 2013
*/

#ifdef WIN32
#include <Windows.h>
#include <winioctl.h>

BOOST_AFIO_V2_NAMESPACE_BEGIN

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
    
    typedef enum {
        ObjectBasicInformation = 0,
        ObjectNameInformation = 1,
        ObjectTypeInformation = 2
    } OBJECT_INFORMATION_CLASS;

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

    typedef
      VOID
      (NTAPI *PIO_APC_ROUTINE) (
      IN PVOID ApcContext,
      IN PIO_STATUS_BLOCK IoStatusBlock,
      IN ULONG Reserved
      );

    typedef struct _IMAGEHLP_LINE64 {
      DWORD   SizeOfStruct;
      PVOID   Key;
      DWORD   LineNumber;
      PTSTR   FileName;
      DWORD64 Address;
    } IMAGEHLP_LINE64, *PIMAGEHLP_LINE64;

    // From https://msdn.microsoft.com/en-us/library/bb432383%28v=vs.85%29.aspx
    typedef NTSTATUS (NTAPI *NtQueryObject_t)(
      /*_In_opt_*/   HANDLE Handle,
      /*_In_*/       OBJECT_INFORMATION_CLASS ObjectInformationClass,
      /*_Out_opt_*/  PVOID ObjectInformation,
      /*_In_*/       ULONG ObjectInformationLength,
      /*_Out_opt_*/  PULONG ReturnLength
    );
    
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

    typedef NTSTATUS (NTAPI *NtDeleteFile_t)(
      /*_In_*/      POBJECT_ATTRIBUTES ObjectAttributes
    );

    typedef NTSTATUS(NTAPI *NtClose_t)(
      /*_Out_*/  HANDLE FileHandle
      );

    // From http://undocumented.ntinternals.net/UserMode/Undocumented%20Functions/NT%20Objects/File/NtQueryDirectoryFile.html
    // and http://msdn.microsoft.com/en-us/library/windows/hardware/ff567047(v=vs.85).aspx
    typedef NTSTATUS (NTAPI *NtQueryDirectoryFile_t)(
        /*_In_*/      HANDLE FileHandle,
        /*_In_opt_*/  HANDLE Event,
        /*_In_opt_*/  PIO_APC_ROUTINE ApcRoutine,
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

    // From https://msdn.microsoft.com/en-us/library/windows/hardware/ff566474(v=vs.85).aspx
    typedef NTSTATUS (NTAPI *NtLockFile_t)(
      /*_In_*/      HANDLE FileHandle,
      /*_In_opt_*/  HANDLE Event,
      /*_In_opt_*/  PIO_APC_ROUTINE ApcRoutine,
      /*_In_opt_*/  PVOID ApcContext,
      /*_Out_*/     PIO_STATUS_BLOCK IoStatusBlock,
      /*_In_*/      PLARGE_INTEGER ByteOffset,
      /*_In_*/      PLARGE_INTEGER Length,
      /*_In_*/      ULONG Key,
      /*_In_*/      BOOLEAN FailImmediately,
      /*_In_*/      BOOLEAN ExclusiveLock
      );

    // From https://msdn.microsoft.com/en-us/library/windows/hardware/ff567118(v=vs.85).aspx
    typedef NTSTATUS (NTAPI *NtUnlockFile_t)(
          /*_In_*/   HANDLE FileHandle,
          /*_Out_*/  PIO_STATUS_BLOCK IoStatusBlock,
          /*_In_*/   PLARGE_INTEGER ByteOffset,
          /*_In_*/   PLARGE_INTEGER Length,
          /*_In_*/   ULONG Key
          );
          
    typedef BOOLEAN (NTAPI *RtlGenRandom_t)(
          /*_Out_*/  PVOID RandomBuffer,
          /*_In_*/   ULONG RandomBufferLength
          );
          
    typedef BOOL (WINAPI *OpenProcessToken_t)(
          /*_In_*/   HANDLE ProcessHandle,
          /*_In_*/   DWORD DesiredAccess,
          /*_Out_*/  PHANDLE TokenHandle
        );
        
    typedef BOOL (WINAPI *LookupPrivilegeValue_t)(
          /*_In_opt_*/  LPCTSTR lpSystemName,
          /*_In_*/      LPCTSTR lpName,
          /*_Out_*/     PLUID lpLuid
        );
        
    typedef BOOL (WINAPI *AdjustTokenPrivileges_t)(
          /*_In_*/       HANDLE TokenHandle,
          /*_In_*/       BOOL DisableAllPrivileges,
          /*_In_opt_*/   PTOKEN_PRIVILEGES NewState,
          /*_In_*/       DWORD BufferLength,
          /*_Out_opt_*/  PTOKEN_PRIVILEGES PreviousState,
          /*_Out_opt_*/  PDWORD ReturnLength
        );

    typedef USHORT (WINAPI *RtlCaptureStackBackTrace_t)(
      /*_In_*/       ULONG FramesToSkip,
      /*_In_*/       ULONG FramesToCapture,
      /*_Out_*/      PVOID *BackTrace,
      /*_Out_opt_*/  PULONG BackTraceHash
      );

    typedef BOOL (WINAPI *SymInitialize_t)(
      /*_In_*/      HANDLE hProcess,
      /*_In_opt_*/  PCTSTR UserSearchPath,
      /*_In_*/      BOOL fInvadeProcess
      );

    typedef BOOL (WINAPI *SymGetLineFromAddr64_t)(
      /*_In_*/   HANDLE hProcess,
      /*_In_*/   DWORD64 dwAddr,
      /*_Out_*/  PDWORD pdwDisplacement,
      /*_Out_*/  PIMAGEHLP_LINE64 Line
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
      union {
        ULONG EaSize;
        ULONG ReparsePointTag;
      };
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

    typedef struct _FILE_RENAME_INFORMATION {
      BOOLEAN ReplaceIfExists;
      HANDLE  RootDirectory;
      ULONG   FileNameLength;
      WCHAR   FileName[1];
    } FILE_RENAME_INFORMATION, *PFILE_RENAME_INFORMATION;

    typedef struct _FILE_LINK_INFORMATION {
      BOOLEAN ReplaceIfExists;
      HANDLE  RootDirectory;
      ULONG   FileNameLength;
      WCHAR   FileName[1];
    } FILE_LINK_INFORMATION, *PFILE_LINK_INFORMATION;

    typedef struct _FILE_DISPOSITION_INFORMATION {
      BOOLEAN DeleteFile;
    } FILE_DISPOSITION_INFORMATION, *PFILE_DISPOSITION_INFORMATION;

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

    typedef struct _FILE_FS_ATTRIBUTE_INFORMATION {
      ULONG FileSystemAttributes;
      LONG  MaximumComponentNameLength;
      ULONG FileSystemNameLength;
      WCHAR FileSystemName[1];
    } FILE_FS_ATTRIBUTE_INFORMATION, *PFILE_FS_ATTRIBUTE_INFORMATION;

    typedef struct _FILE_FS_FULL_SIZE_INFORMATION {
      LARGE_INTEGER TotalAllocationUnits;
      LARGE_INTEGER CallerAvailableAllocationUnits;
      LARGE_INTEGER ActualAvailableAllocationUnits;
      ULONG         SectorsPerAllocationUnit;
      ULONG         BytesPerSector;
    } FILE_FS_FULL_SIZE_INFORMATION, *PFILE_FS_FULL_SIZE_INFORMATION;

    typedef struct _FILE_FS_OBJECTID_INFORMATION {
      UCHAR ObjectId[16];
      UCHAR ExtendedInfo[48];
    } FILE_FS_OBJECTID_INFORMATION, *PFILE_FS_OBJECTID_INFORMATION;

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
      union {
        ULONG       EaSize;
        ULONG       ReparsePointTag;
      };
      LARGE_INTEGER FileId;
      WCHAR         FileName[1];
    } FILE_ID_FULL_DIR_INFORMATION, *PFILE_ID_FULL_DIR_INFORMATION;

    // From https://msdn.microsoft.com/en-us/library/windows/hardware/ff540354(v=vs.85).aspx
    typedef struct _FILE_REPARSE_POINT_INFORMATION {
      LARGE_INTEGER FileReference;
      ULONG    Tag;
    } FILE_REPARSE_POINT_INFORMATION, *PFILE_REPARSE_POINT_INFORMATION;

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

    static NtQueryObject_t NtQueryObject;
    static NtQueryInformationFile_t NtQueryInformationFile;
    static NtQueryVolumeInformationFile_t NtQueryVolumeInformationFile;
    static NtOpenDirectoryObject_t NtOpenDirectoryObject;
    static NtOpenFile_t NtOpenFile;
    static NtCreateFile_t NtCreateFile;
    static NtDeleteFile_t NtDeleteFile;
    static NtClose_t NtClose;
    static NtQueryDirectoryFile_t NtQueryDirectoryFile;
    static NtSetInformationFile_t NtSetInformationFile;
    static NtWaitForSingleObject_t NtWaitForSingleObject;
    static NtLockFile_t NtLockFile;
    static NtUnlockFile_t NtUnlockFile;
    static RtlGenRandom_t RtlGenRandom;
    static OpenProcessToken_t OpenProcessToken;
    static LookupPrivilegeValue_t LookupPrivilegeValue;
    static AdjustTokenPrivileges_t AdjustTokenPrivileges;
    static SymInitialize_t SymInitialize;
    static SymGetLineFromAddr64_t SymGetLineFromAddr64;
    static RtlCaptureStackBackTrace_t RtlCaptureStackBackTrace;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 6387) // MSVC sanitiser warns that GetModuleHandleA() might fail (hah!)
#endif
    static inline void doinit()
    {
      if(RtlCaptureStackBackTrace)
        return;
      static spinlock<bool> lock;
      lock_guard<decltype(lock)> g(lock);
      if(!NtQueryObject)
          if(!(NtQueryObject=(NtQueryObject_t) GetProcAddress(GetModuleHandleA("NTDLL.DLL"), "NtQueryObject")))
              abort();
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
      if(!NtDeleteFile)
          if(!(NtDeleteFile =(NtDeleteFile_t) GetProcAddress(GetModuleHandleA("NTDLL.DLL"), "NtDeleteFile")))
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
      if(!NtLockFile)
          if(!(NtLockFile=(NtLockFile_t) GetProcAddress(GetModuleHandleA("NTDLL.DLL"), "NtLockFile")))
              abort();
      if(!NtUnlockFile)
          if(!(NtUnlockFile=(NtUnlockFile_t) GetProcAddress(GetModuleHandleA("NTDLL.DLL"), "NtUnlockFile")))
              abort();
      HMODULE advapi32=LoadLibraryA("ADVAPI32.DLL");
      if(!RtlGenRandom)
          if(!(RtlGenRandom=(RtlGenRandom_t) GetProcAddress(advapi32, "SystemFunction036")))
              abort();
      if(!OpenProcessToken)
          if(!(OpenProcessToken=(OpenProcessToken_t) GetProcAddress(advapi32, "OpenProcessToken")))
              abort();
      if(!LookupPrivilegeValue)
          if(!(LookupPrivilegeValue=(LookupPrivilegeValue_t) GetProcAddress(advapi32, "LookupPrivilegeValueW")))
              abort();
      if(!AdjustTokenPrivileges)
          if(!(AdjustTokenPrivileges=(AdjustTokenPrivileges_t) GetProcAddress(advapi32, "AdjustTokenPrivileges")))
              abort();
      HMODULE dbghelp = LoadLibraryA("DBGHELP.DLL");
#ifdef BOOST_AFIO_OP_STACKBACKTRACEDEPTH
      if (dbghelp)
      {
        if (!(SymInitialize = (SymInitialize_t) GetProcAddress(dbghelp, "SymInitializeW")))
          abort();
        if(!SymInitialize(GetCurrentProcess(), nullptr, true))
          abort();
        if (!(SymGetLineFromAddr64 = (SymGetLineFromAddr64_t) GetProcAddress(dbghelp, "SymGetLineFromAddrW64")))
          abort();
      }
#endif
      if (!RtlCaptureStackBackTrace)
        if (!(RtlCaptureStackBackTrace = (RtlCaptureStackBackTrace_t) GetProcAddress(GetModuleHandleA("NTDLL.DLL"), "RtlCaptureStackBackTrace")))
          abort();
      // MAKE SURE you update the early exit check at the top to whatever the last of these is!
    }
#ifdef _MSC_VER
#pragma warning(pop)
#endif
    static inline void init()
    {
        static bool initialised=false;
        if(!initialised)
        {
            doinit();
            initialised=true;
        }
    }

    static inline filesystem::file_type to_st_type(ULONG FileAttributes, ULONG ReparsePointTag)
    {
#ifdef BOOST_AFIO_USE_LEGACY_FILESYSTEM_SEMANTICS
        if(FileAttributes&FILE_ATTRIBUTE_REPARSE_POINT && (ReparsePointTag == IO_REPARSE_TAG_MOUNT_POINT || ReparsePointTag == IO_REPARSE_TAG_SYMLINK))
            return filesystem::file_type::symlink_file;
            //return filesystem::file_type::reparse_file;
        else if(FileAttributes&FILE_ATTRIBUTE_DIRECTORY)
            return filesystem::file_type::directory_file;
        else
            return filesystem::file_type::regular_file;
#else
        if(FileAttributes&FILE_ATTRIBUTE_REPARSE_POINT && (ReparsePointTag == IO_REPARSE_TAG_MOUNT_POINT || ReparsePointTag == IO_REPARSE_TAG_SYMLINK))
            return filesystem::file_type::symlink;
            //return filesystem::file_type::reparse_file;
        else if(FileAttributes&FILE_ATTRIBUTE_DIRECTORY)
            return filesystem::file_type::directory;
        else
            return filesystem::file_type::regular;
#endif
    }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 6326) // comparison of constants
#endif
    static inline chrono::system_clock::time_point to_timepoint(LARGE_INTEGER time)
    {
        // We make the big assumption that the STL's system_clock is based on the time_t epoch 1st Jan 1970.
        static BOOST_CONSTEXPR_OR_CONST unsigned long long FILETIME_OFFSET_TO_1970=((27111902ULL << 32U) + 3577643008ULL);
        // Need to have this self-adapt to the STL being used
        static BOOST_CONSTEXPR_OR_CONST unsigned long long STL_TICKS_PER_SEC=(unsigned long long) chrono::system_clock::period::den/chrono::system_clock::period::num;
        static BOOST_CONSTEXPR_OR_CONST unsigned long long multiplier=STL_TICKS_PER_SEC>=10000000ULL ? STL_TICKS_PER_SEC/10000000ULL : 1;
        static BOOST_CONSTEXPR_OR_CONST unsigned long long divider=STL_TICKS_PER_SEC>=10000000ULL ? 1 : 10000000ULL/STL_TICKS_PER_SEC;

        unsigned long long ticks_since_1970=(time.QuadPart - FILETIME_OFFSET_TO_1970); // In 100ns increments
        chrono::system_clock::duration duration(ticks_since_1970*multiplier/divider);
        return chrono::system_clock::time_point(duration);
    }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

    // Adapted from http://www.cprogramming.com/snippets/source-code/convert-ntstatus-win32-error
    // Could use RtlNtStatusToDosError() instead
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 6387) // MSVC sanitiser warns on misuse of GetOverlappedResult
#endif
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
#ifdef _MSC_VER
#pragma warning(pop)
#endif

    // C++ error code from GetLastError()
    static inline error_code make_error_code()
    {
      return error_code((int) GetLastError(), system_category());
    }

    // C++ error code from NTSTATUS
    static inline error_code make_error_code(NTSTATUS ntstat)
    {
      SetWin32LastErrorFromNtStatus(ntstat);
      return error_code((int)GetLastError(), system_category());
    }
} // namespace

// WinVista and later have the SetFileInformationByHandle() function, but for WinXP
// compatibility we use the kernel syscall directly
static inline bool wintruncate(HANDLE h, off_t newsize)
{
    windows_nt_kernel::init();
    using namespace windows_nt_kernel;
    IO_STATUS_BLOCK isb={ 0 };
    BOOST_AFIO_ERRHNT(NtSetInformationFile(h, &isb, &newsize, sizeof(newsize), FileEndOfFileInformation));
    return true;
}
static inline int winftruncate(int fd, off_t _newsize)
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
static inline void fill_stat_t(stat_t &stat, BOOST_AFIO_POSIX_STAT_STRUCT s, metadata_flags wanted)
{
    using namespace boost::afio;
#ifndef WIN32
    if(!!(wanted&metadata_flags::dev)) { stat.st_dev=s.st_dev; }
#endif
    if(!!(wanted&metadata_flags::ino)) { stat.st_ino=s.st_ino; }
    if(!!(wanted&metadata_flags::type)) { stat.st_type=to_st_type(s.st_mode); }
#ifndef WIN32
    if(!!(wanted&metadata_flags::perms)) { stat.st_mode=s.st_perms; }
#endif
    if(!!(wanted&metadata_flags::nlink)) { stat.st_nlink=s.st_nlink; }
#ifndef WIN32
    if(!!(wanted&metadata_flags::uid)) { stat.st_uid=s.st_uid; }
    if(!!(wanted&metadata_flags::gid)) { stat.st_gid=s.st_gid; }
    if(!!(wanted&metadata_flags::rdev)) { stat.st_rdev=s.st_rdev; }
#endif
    if(!!(wanted&metadata_flags::atim)) { stat.st_atim=chrono::system_clock::from_time_t(s.st_atime); }
    if(!!(wanted&metadata_flags::mtim)) { stat.st_mtim=chrono::system_clock::from_time_t(s.st_mtime); }
    if(!!(wanted&metadata_flags::birthtim)) { stat.st_birthtim=chrono::system_clock::from_time_t(s.st_ctime); }
    if(!!(wanted&metadata_flags::size)) { stat.st_size=s.st_size; }
}

BOOST_AFIO_V2_NAMESPACE_END

#endif
