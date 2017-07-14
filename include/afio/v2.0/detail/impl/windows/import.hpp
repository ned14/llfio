/* Declarations for Microsoft Windows system APIs
(C) 2015-2017 Niall Douglas <http://www.nedproductions.biz/> (14 commits)
File Created: Dec 2015


Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License in the accompanying file
Licence.txt or at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.


Distributed under the Boost Software License, Version 1.0.
    (See accompanying file Licence.txt or copy at
          http://www.boost.org/LICENSE_1_0.txt)
*/

#ifndef AFIO_WINDOWS_H
#define AFIO_WINDOWS_H

#include "../../../handle.hpp"
#include <memory>  // for unique_ptr
#include <mutex>

#ifndef _WIN32
#error You should not include windows/import.hpp on not Windows platforms
#endif

#include <sal.h>

// At some future point we will not do this, and instead import symbols manually
// to avoid the windows.h inclusion
#if 1
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winternl.h>

#else
#error todo
#endif

AFIO_V2_NAMESPACE_BEGIN

namespace windows_nt_kernel
{
// Weirdly these appear to be undefined sometimes?
#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((LONG) 0x00000000L)
#endif
#ifndef STATUS_ALERTED
#define STATUS_ALERTED ((LONG) 0x00000101L)
#endif
#ifndef STATUS_DELETE_PENDING
#define STATUS_DELETE_PENDING ((LONG) 0xC0000056)
#endif

  // From http://undocumented.ntinternals.net/UserMode/Undocumented%20Functions/NT%20Objects/File/FILE_INFORMATION_CLASS.html
  typedef enum _FILE_INFORMATION_CLASS {
    FileDirectoryInformation = 1,
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
  } FILE_INFORMATION_CLASS,
  *PFILE_INFORMATION_CLASS;

  typedef enum {
    FileFsVolumeInformation = 1,
    FileFsLabelInformation = 2,
    FileFsSizeInformation = 3,
    FileFsDeviceInformation = 4,
    FileFsAttributeInformation = 5,
    FileFsControlInformation = 6,
    FileFsFullSizeInformation = 7,
    FileFsObjectIdInformation = 8,
    FileFsDriverPathInformation = 9,
    FileFsVolumeFlagsInformation = 10,
    FileFsSectorSizeInformation = 11
  } FS_INFORMATION_CLASS;

  typedef enum { ObjectBasicInformation = 0, ObjectNameInformation = 1, ObjectTypeInformation = 2 } OBJECT_INFORMATION_CLASS;

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
  typedef struct _IO_STATUS_BLOCK
  {
    union {
      NTSTATUS Status;
      PVOID Pointer;
    };
    ULONG_PTR Information;
  } IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

  // From http://msdn.microsoft.com/en-us/library/windows/desktop/aa380518(v=vs.85).aspx
  typedef struct _LSA_UNICODE_STRING
  {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
  } LSA_UNICODE_STRING, *PLSA_UNICODE_STRING, UNICODE_STRING, *PUNICODE_STRING;

  // From http://msdn.microsoft.com/en-us/library/windows/hardware/ff557749(v=vs.85).aspx
  typedef struct _OBJECT_ATTRIBUTES
  {
    ULONG Length;
    HANDLE RootDirectory;
    PUNICODE_STRING ObjectName;
    ULONG Attributes;
    PVOID SecurityDescriptor;
    PVOID SecurityQualityOfService;
  } OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;

  typedef VOID(NTAPI *PIO_APC_ROUTINE)(IN PVOID ApcContext, IN PIO_STATUS_BLOCK IoStatusBlock, IN ULONG Reserved);

  typedef struct _IMAGEHLP_LINE64
  {
    DWORD SizeOfStruct;
    PVOID Key;
    DWORD LineNumber;
    PTSTR FileName;
    DWORD64 Address;
  } IMAGEHLP_LINE64, *PIMAGEHLP_LINE64;

  typedef enum _SECTION_INHERIT { ViewShare = 1, ViewUnmap = 2 } SECTION_INHERIT, *PSECTION_INHERIT;

  typedef struct _WIN32_MEMORY_RANGE_ENTRY
  {
    PVOID VirtualAddress;
    SIZE_T NumberOfBytes;
  } WIN32_MEMORY_RANGE_ENTRY, *PWIN32_MEMORY_RANGE_ENTRY;

  // From https://msdn.microsoft.com/en-us/library/bb432383%28v=vs.85%29.aspx
  typedef NTSTATUS(NTAPI *NtQueryObject_t)(_In_opt_ HANDLE Handle, _In_ OBJECT_INFORMATION_CLASS ObjectInformationClass, _Out_opt_ PVOID ObjectInformation, _In_ ULONG ObjectInformationLength, _Out_opt_ PULONG ReturnLength);

  // From http://undocumented.ntinternals.net/UserMode/Undocumented%20Functions/NT%20Objects/File/NtQueryInformationFile.html
  // and http://msdn.microsoft.com/en-us/library/windows/hardware/ff567052(v=vs.85).aspx
  typedef NTSTATUS(NTAPI *NtQueryInformationFile_t)(_In_ HANDLE FileHandle, _Out_ PIO_STATUS_BLOCK IoStatusBlock, _Out_ PVOID FileInformation, _In_ ULONG Length, _In_ FILE_INFORMATION_CLASS FileInformationClass);

  // From http://msdn.microsoft.com/en-us/library/windows/hardware/ff567070(v=vs.85).aspx
  typedef NTSTATUS(NTAPI *NtQueryVolumeInformationFile_t)(_In_ HANDLE FileHandle, _Out_ PIO_STATUS_BLOCK IoStatusBlock, _Out_ PVOID FsInformation, _In_ ULONG Length, _In_ FS_INFORMATION_CLASS FsInformationClass);

  // From http://msdn.microsoft.com/en-us/library/windows/hardware/ff566492(v=vs.85).aspx
  typedef NTSTATUS(NTAPI *NtOpenDirectoryObject_t)(_Out_ PHANDLE DirectoryHandle, _In_ ACCESS_MASK DesiredAccess, _In_ POBJECT_ATTRIBUTES ObjectAttributes);


  // From http://msdn.microsoft.com/en-us/library/windows/hardware/ff567011(v=vs.85).aspx
  typedef NTSTATUS(NTAPI *NtOpenFile_t)(_Out_ PHANDLE FileHandle, _In_ ACCESS_MASK DesiredAccess, _In_ POBJECT_ATTRIBUTES ObjectAttributes, _Out_ PIO_STATUS_BLOCK IoStatusBlock, _In_ ULONG ShareAccess, _In_ ULONG OpenOptions);

  // From http://msdn.microsoft.com/en-us/library/windows/hardware/ff566424(v=vs.85).aspx
  typedef NTSTATUS(NTAPI *NtCreateFile_t)(_Out_ PHANDLE FileHandle, _In_ ACCESS_MASK DesiredAccess, _In_ POBJECT_ATTRIBUTES ObjectAttributes, _Out_ PIO_STATUS_BLOCK IoStatusBlock, _In_opt_ PLARGE_INTEGER AllocationSize, _In_ ULONG FileAttributes, _In_ ULONG ShareAccess, _In_ ULONG CreateDisposition,
                                          _In_ ULONG CreateOptions, _In_opt_ PVOID EaBuffer, _In_ ULONG EaLength);

  typedef NTSTATUS(NTAPI *NtDeleteFile_t)(_In_ POBJECT_ATTRIBUTES ObjectAttributes);

  typedef NTSTATUS(NTAPI *NtClose_t)(_Out_ HANDLE FileHandle);

  // From http://undocumented.ntinternals.net/UserMode/Undocumented%20Functions/NT%20Objects/File/NtQueryDirectoryFile.html
  // and http://msdn.microsoft.com/en-us/library/windows/hardware/ff567047(v=vs.85).aspx
  typedef NTSTATUS(NTAPI *NtQueryDirectoryFile_t)(_In_ HANDLE FileHandle, _In_opt_ HANDLE Event, _In_opt_ PIO_APC_ROUTINE ApcRoutine, _In_opt_ PVOID ApcContext, _Out_ PIO_STATUS_BLOCK IoStatusBlock, _Out_ PVOID FileInformation, _In_ ULONG Length, _In_ FILE_INFORMATION_CLASS FileInformationClass,
                                                  _In_ BOOLEAN ReturnSingleEntry, _In_opt_ PUNICODE_STRING FileName, _In_ BOOLEAN RestartScan);

  // From http://undocumented.ntinternals.net/UserMode/Undocumented%20Functions/NT%20Objects/File/NtSetInformationFile.html
  // and http://msdn.microsoft.com/en-us/library/windows/hardware/ff567096(v=vs.85).aspx
  typedef NTSTATUS(NTAPI *NtSetInformationFile_t)(_In_ HANDLE FileHandle, _Out_ PIO_STATUS_BLOCK IoStatusBlock, _In_ PVOID FileInformation, _In_ ULONG Length, _In_ FILE_INFORMATION_CLASS FileInformationClass);

  // From http://msdn.microsoft.com/en-us/library/ms648412(v=vs.85).aspx
  typedef NTSTATUS(NTAPI *NtWaitForSingleObject_t)(_In_ HANDLE Handle, _In_ BOOLEAN Alertable, _In_opt_ PLARGE_INTEGER Timeout);

  typedef enum _OBJECT_WAIT_TYPE { WaitAllObject, WaitAnyObject } OBJECT_WAIT_TYPE, *POBJECT_WAIT_TYPE;

  typedef NTSTATUS(NTAPI *NtWaitForMultipleObjects_t)(_In_ ULONG Count, _In_ HANDLE Object[], _In_ OBJECT_WAIT_TYPE WaitType, _In_ BOOLEAN Alertable, _In_opt_ PLARGE_INTEGER Time);

  typedef NTSTATUS(NTAPI *NtDelayExecution_t)(_In_ BOOLEAN Alertable, _In_opt_ LARGE_INTEGER *Interval);

  // From https://msdn.microsoft.com/en-us/library/windows/hardware/ff566474(v=vs.85).aspx
  typedef NTSTATUS(NTAPI *NtLockFile_t)(_In_ HANDLE FileHandle, _In_opt_ HANDLE Event, _In_opt_ PIO_APC_ROUTINE ApcRoutine, _In_opt_ PVOID ApcContext, _Out_ PIO_STATUS_BLOCK IoStatusBlock, _In_ PLARGE_INTEGER ByteOffset, _In_ PLARGE_INTEGER Length, _In_ ULONG Key, _In_ BOOLEAN FailImmediately,
                                        _In_ BOOLEAN ExclusiveLock);

  // From https://msdn.microsoft.com/en-us/library/windows/hardware/ff567118(v=vs.85).aspx
  typedef NTSTATUS(NTAPI *NtUnlockFile_t)(_In_ HANDLE FileHandle, _Out_ PIO_STATUS_BLOCK IoStatusBlock, _In_ PLARGE_INTEGER ByteOffset, _In_ PLARGE_INTEGER Length, _In_ ULONG Key);

  typedef NTSTATUS(NTAPI *NtCreateSection_t)(_Out_ PHANDLE SectionHandle, _In_ ACCESS_MASK DesiredAccess, _In_opt_ POBJECT_ATTRIBUTES ObjectAttributes, _In_opt_ PLARGE_INTEGER MaximumSize, _In_ ULONG SectionPageProtection, _In_ ULONG AllocationAttributes, _In_opt_ HANDLE FileHandle);

  typedef NTSTATUS(NTAPI *NtExtendSection_t)(_In_ HANDLE SectionHandle, _In_opt_ PLARGE_INTEGER MaximumSize);

  typedef NTSTATUS(NTAPI *NtMapViewOfSection_t)(_In_ HANDLE SectionHandle, _In_ HANDLE ProcessHandle, _Inout_ PVOID *BaseAddress, _In_ ULONG_PTR ZeroBits, _In_ SIZE_T CommitSize, _Inout_opt_ PLARGE_INTEGER SectionOffset, _Inout_ PSIZE_T ViewSize, _In_ SECTION_INHERIT InheritDisposition, _In_ ULONG AllocationType,
                                                _In_ ULONG Win32Protect);

  typedef NTSTATUS(NTAPI *NtUnmapViewOfSection_t)(_In_ HANDLE ProcessHandle, _In_opt_ PVOID BaseAddress);

  typedef NTSTATUS(NTAPI *NtFlushBuffersFileEx_t)(_In_ HANDLE FileHandle, _In_ ULONG Flags, _Out_ PIO_STATUS_BLOCK IoStatusBlock);

  typedef BOOLEAN(NTAPI *RtlGenRandom_t)(_Out_ PVOID RandomBuffer, _In_ ULONG RandomBufferLength);

  typedef BOOL(WINAPI *OpenProcessToken_t)(_In_ HANDLE ProcessHandle, _In_ DWORD DesiredAccess, _Out_ PHANDLE TokenHandle);

  typedef BOOL(WINAPI *LookupPrivilegeValue_t)(_In_opt_ LPCTSTR lpSystemName, _In_ LPCTSTR lpName, _Out_ PLUID lpLuid);

  typedef BOOL(WINAPI *AdjustTokenPrivileges_t)(_In_ HANDLE TokenHandle, _In_ BOOL DisableAllPrivileges, _In_opt_ PTOKEN_PRIVILEGES NewState, _In_ DWORD BufferLength, _Out_opt_ PTOKEN_PRIVILEGES PreviousState, _Out_opt_ PDWORD ReturnLength);

  typedef BOOL(WINAPI *PrefetchVirtualMemory_t)(_In_ HANDLE hProcess, _In_ ULONG_PTR NumberOfEntries, _In_ PWIN32_MEMORY_RANGE_ENTRY VirtualAddresses, _In_ ULONG Flags);

  typedef BOOL(WINAPI *DiscardVirtualMemory_t)(_In_ PVOID VirtualAddress, _In_ SIZE_T Size);

  typedef USHORT(WINAPI *RtlCaptureStackBackTrace_t)(_In_ ULONG FramesToSkip, _In_ ULONG FramesToCapture, _Out_ PVOID *BackTrace, _Out_opt_ PULONG BackTraceHash);

  typedef BOOL(WINAPI *SymInitialize_t)(_In_ HANDLE hProcess, _In_opt_ PCTSTR UserSearchPath, _In_ BOOL fInvadeProcess);

  typedef BOOL(WINAPI *SymGetLineFromAddr64_t)(_In_ HANDLE hProcess, _In_ DWORD64 dwAddr, _Out_ PDWORD pdwDisplacement, _Out_ PIMAGEHLP_LINE64 Line);

  typedef BOOLEAN(WINAPI *RtlDosPathNameToNtPathName_U_t)(__in PCWSTR DosFileName, __out PUNICODE_STRING NtFileName, __out_opt PWSTR *FilePart, __out_opt PVOID RelativeName);


  typedef struct _FILE_BASIC_INFORMATION
  {
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    ULONG FileAttributes;
  } FILE_BASIC_INFORMATION, *PFILE_BASIC_INFORMATION;

  typedef struct _FILE_STANDARD_INFORMATION
  {
    LARGE_INTEGER AllocationSize;
    LARGE_INTEGER EndOfFile;
    ULONG NumberOfLinks;
    BOOLEAN DeletePending;
    BOOLEAN Directory;
  } FILE_STANDARD_INFORMATION, *PFILE_STANDARD_INFORMATION;

  typedef struct _FILE_INTERNAL_INFORMATION
  {
    LARGE_INTEGER IndexNumber;
  } FILE_INTERNAL_INFORMATION, *PFILE_INTERNAL_INFORMATION;

  typedef struct _FILE_EA_INFORMATION
  {
    union {
      ULONG EaSize;
      ULONG ReparsePointTag;
    };
  } FILE_EA_INFORMATION, *PFILE_EA_INFORMATION;

  typedef struct _FILE_ACCESS_INFORMATION
  {
    ACCESS_MASK AccessFlags;
  } FILE_ACCESS_INFORMATION, *PFILE_ACCESS_INFORMATION;

  typedef struct _FILE_POSITION_INFORMATION
  {
    LARGE_INTEGER CurrentByteOffset;
  } FILE_POSITION_INFORMATION, *PFILE_POSITION_INFORMATION;

  typedef struct _FILE_MODE_INFORMATION
  {
    ULONG Mode;
  } FILE_MODE_INFORMATION, *PFILE_MODE_INFORMATION;

  typedef struct _FILE_ALIGNMENT_INFORMATION
  {
    ULONG AlignmentRequirement;
  } FILE_ALIGNMENT_INFORMATION, *PFILE_ALIGNMENT_INFORMATION;

  typedef struct _FILE_NAME_INFORMATION
  {
    ULONG FileNameLength;
    WCHAR FileName[1];
  } FILE_NAME_INFORMATION, *PFILE_NAME_INFORMATION;

  typedef struct _FILE_RENAME_INFORMATION
  {
    BOOLEAN ReplaceIfExists;
    HANDLE RootDirectory;
    ULONG FileNameLength;
    WCHAR FileName[1];
  } FILE_RENAME_INFORMATION, *PFILE_RENAME_INFORMATION;

  typedef struct _FILE_LINK_INFORMATION
  {
    BOOLEAN ReplaceIfExists;
    HANDLE RootDirectory;
    ULONG FileNameLength;
    WCHAR FileName[1];
  } FILE_LINK_INFORMATION, *PFILE_LINK_INFORMATION;

  typedef struct _FILE_DISPOSITION_INFORMATION
  {
    BOOLEAN _DeleteFile;
  } FILE_DISPOSITION_INFORMATION, *PFILE_DISPOSITION_INFORMATION;

  typedef struct _FILE_ALL_INFORMATION
  {
    FILE_BASIC_INFORMATION BasicInformation;
    FILE_STANDARD_INFORMATION StandardInformation;
    FILE_INTERNAL_INFORMATION InternalInformation;
    FILE_EA_INFORMATION EaInformation;
    FILE_ACCESS_INFORMATION AccessInformation;
    FILE_POSITION_INFORMATION PositionInformation;
    FILE_MODE_INFORMATION ModeInformation;
    FILE_ALIGNMENT_INFORMATION AlignmentInformation;
    FILE_NAME_INFORMATION NameInformation;
  } FILE_ALL_INFORMATION, *PFILE_ALL_INFORMATION;

  typedef struct _FILE_FS_ATTRIBUTE_INFORMATION
  {
    ULONG FileSystemAttributes;
    LONG MaximumComponentNameLength;
    ULONG FileSystemNameLength;
    WCHAR FileSystemName[1];
  } FILE_FS_ATTRIBUTE_INFORMATION, *PFILE_FS_ATTRIBUTE_INFORMATION;

  typedef struct _FILE_FS_FULL_SIZE_INFORMATION
  {
    LARGE_INTEGER TotalAllocationUnits;
    LARGE_INTEGER CallerAvailableAllocationUnits;
    LARGE_INTEGER ActualAvailableAllocationUnits;
    ULONG SectorsPerAllocationUnit;
    ULONG BytesPerSector;
  } FILE_FS_FULL_SIZE_INFORMATION, *PFILE_FS_FULL_SIZE_INFORMATION;

  typedef struct _FILE_FS_OBJECTID_INFORMATION
  {
    UCHAR ObjectId[16];
    UCHAR ExtendedInfo[48];
  } FILE_FS_OBJECTID_INFORMATION, *PFILE_FS_OBJECTID_INFORMATION;

  typedef struct _FILE_FS_SECTOR_SIZE_INFORMATION
  {
    ULONG LogicalBytesPerSector;
    ULONG PhysicalBytesPerSectorForAtomicity;
    ULONG PhysicalBytesPerSectorForPerformance;
    ULONG FileSystemEffectivePhysicalBytesPerSectorForAtomicity;
    ULONG Flags;
    ULONG ByteOffsetForSectorAlignment;
    ULONG ByteOffsetForPartitionAlignment;
  } FILE_FS_SECTOR_SIZE_INFORMATION, *PFILE_FS_SECTOR_SIZE_INFORMATION;

  // From http://msdn.microsoft.com/en-us/library/windows/hardware/ff540310(v=vs.85).aspx
  typedef struct _FILE_ID_FULL_DIR_INFORMATION
  {
    ULONG NextEntryOffset;
    ULONG FileIndex;
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    LARGE_INTEGER EndOfFile;
    LARGE_INTEGER AllocationSize;
    ULONG FileAttributes;
    ULONG FileNameLength;
    union {
      ULONG EaSize;
      ULONG ReparsePointTag;
    };
    LARGE_INTEGER FileId;
    WCHAR FileName[1];
  } FILE_ID_FULL_DIR_INFORMATION, *PFILE_ID_FULL_DIR_INFORMATION;

  // From https://msdn.microsoft.com/en-us/library/windows/hardware/ff540354(v=vs.85).aspx
  typedef struct _FILE_REPARSE_POINT_INFORMATION
  {
    LARGE_INTEGER FileReference;
    ULONG Tag;
  } FILE_REPARSE_POINT_INFORMATION, *PFILE_REPARSE_POINT_INFORMATION;

  // From http://msdn.microsoft.com/en-us/library/windows/hardware/ff552012(v=vs.85).aspx
  typedef struct _REPARSE_DATA_BUFFER
  {
    ULONG ReparseTag;
    USHORT ReparseDataLength;
    USHORT Reserved;
    union {
      struct
      {
        USHORT SubstituteNameOffset;
        USHORT SubstituteNameLength;
        USHORT PrintNameOffset;
        USHORT PrintNameLength;
        ULONG Flags;
        WCHAR PathBuffer[1];
      } SymbolicLinkReparseBuffer;
      struct
      {
        USHORT SubstituteNameOffset;
        USHORT SubstituteNameLength;
        USHORT PrintNameOffset;
        USHORT PrintNameLength;
        WCHAR PathBuffer[1];
      } MountPointReparseBuffer;
      struct
      {
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
  static NtWaitForMultipleObjects_t NtWaitForMultipleObjects;
  static NtDelayExecution_t NtDelayExecution;
  static NtLockFile_t NtLockFile;
  static NtUnlockFile_t NtUnlockFile;
  static NtCreateSection_t NtCreateSection;
  static NtExtendSection_t NtExtendSection;
  static NtMapViewOfSection_t NtMapViewOfSection;
  static NtUnmapViewOfSection_t NtUnmapViewOfSection;
  static NtFlushBuffersFileEx_t NtFlushBuffersFileEx;
  static RtlGenRandom_t RtlGenRandom;
  static OpenProcessToken_t OpenProcessToken;
  static LookupPrivilegeValue_t LookupPrivilegeValue;
  static AdjustTokenPrivileges_t AdjustTokenPrivileges;
  static PrefetchVirtualMemory_t PrefetchVirtualMemory_;
  static DiscardVirtualMemory_t DiscardVirtualMemory_;
  static SymInitialize_t SymInitialize;
  static SymGetLineFromAddr64_t SymGetLineFromAddr64;
  static RtlCaptureStackBackTrace_t RtlCaptureStackBackTrace;
  static RtlDosPathNameToNtPathName_U_t RtlDosPathNameToNtPathName_U;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4706)  // assignment within conditional
#pragma warning(disable : 6387)  // MSVC sanitiser warns that GetModuleHandleA() might fail (hah!)
#endif
  static inline void doinit()
  {
    if(RtlDosPathNameToNtPathName_U)
      return;
    static std::mutex lock;
    std::lock_guard<decltype(lock)> g(lock);
    static HMODULE ntdllh = GetModuleHandleA("NTDLL.DLL");
    static HMODULE kernel32 = GetModuleHandleA("KERNEL32.DLL");
    if(!NtQueryObject)
      if(!(NtQueryObject = (NtQueryObject_t) GetProcAddress(ntdllh, "NtQueryObject")))
        abort();
    if(!NtQueryInformationFile)
      if(!(NtQueryInformationFile = (NtQueryInformationFile_t) GetProcAddress(ntdllh, "NtQueryInformationFile")))
        abort();
    if(!NtQueryVolumeInformationFile)
      if(!(NtQueryVolumeInformationFile = (NtQueryVolumeInformationFile_t) GetProcAddress(ntdllh, "NtQueryVolumeInformationFile")))
        abort();
    if(!NtOpenDirectoryObject)
      if(!(NtOpenDirectoryObject = (NtOpenDirectoryObject_t) GetProcAddress(ntdllh, "NtOpenDirectoryObject")))
        abort();
    if(!NtOpenFile)
      if(!(NtOpenFile = (NtOpenFile_t) GetProcAddress(ntdllh, "NtOpenFile")))
        abort();
    if(!NtCreateFile)
      if(!(NtCreateFile = (NtCreateFile_t) GetProcAddress(ntdllh, "NtCreateFile")))
        abort();
    if(!NtDeleteFile)
      if(!(NtDeleteFile = (NtDeleteFile_t) GetProcAddress(ntdllh, "NtDeleteFile")))
        abort();
    if(!NtClose)
      if(!(NtClose = (NtClose_t) GetProcAddress(ntdllh, "NtClose")))
        abort();
    if(!NtQueryDirectoryFile)
      if(!(NtQueryDirectoryFile = (NtQueryDirectoryFile_t) GetProcAddress(ntdllh, "NtQueryDirectoryFile")))
        abort();
    if(!NtSetInformationFile)
      if(!(NtSetInformationFile = (NtSetInformationFile_t) GetProcAddress(ntdllh, "NtSetInformationFile")))
        abort();
    if(!NtWaitForSingleObject)
      if(!(NtWaitForSingleObject = (NtWaitForSingleObject_t) GetProcAddress(ntdllh, "NtWaitForSingleObject")))
        abort();
    if(!NtWaitForMultipleObjects)
      if(!(NtWaitForMultipleObjects = (NtWaitForMultipleObjects_t) GetProcAddress(ntdllh, "NtWaitForMultipleObjects")))
        abort();
    if(!NtDelayExecution)
      if(!(NtDelayExecution = (NtDelayExecution_t) GetProcAddress(ntdllh, "NtDelayExecution")))
        abort();
    if(!NtLockFile)
      if(!(NtLockFile = (NtLockFile_t) GetProcAddress(ntdllh, "NtLockFile")))
        abort();
    if(!NtUnlockFile)
      if(!(NtUnlockFile = (NtUnlockFile_t) GetProcAddress(ntdllh, "NtUnlockFile")))
        abort();
    if(!NtCreateSection)
      if(!(NtCreateSection = (NtCreateSection_t) GetProcAddress(ntdllh, "NtCreateSection")))
        abort();
    if(!NtExtendSection)
      if(!(NtExtendSection = (NtExtendSection_t) GetProcAddress(ntdllh, "NtExtendSection")))
        abort();
    if(!NtMapViewOfSection)
      if(!(NtMapViewOfSection = (NtMapViewOfSection_t) GetProcAddress(ntdllh, "NtMapViewOfSection")))
        abort();
    if(!NtUnmapViewOfSection)
      if(!(NtUnmapViewOfSection = (NtUnmapViewOfSection_t) GetProcAddress(ntdllh, "NtUnmapViewOfSection")))
        abort();
    if(!NtFlushBuffersFileEx)
      if(!(NtFlushBuffersFileEx = (NtFlushBuffersFileEx_t) GetProcAddress(ntdllh, "NtFlushBuffersFileEx")))
        abort();
    HMODULE advapi32 = LoadLibraryA("ADVAPI32.DLL");
    if(!RtlGenRandom)
      if(!(RtlGenRandom = (RtlGenRandom_t) GetProcAddress(advapi32, "SystemFunction036")))
        abort();
    if(!OpenProcessToken)
      if(!(OpenProcessToken = (OpenProcessToken_t) GetProcAddress(advapi32, "OpenProcessToken")))
        abort();
    if(!LookupPrivilegeValue)
      if(!(LookupPrivilegeValue = (LookupPrivilegeValue_t) GetProcAddress(advapi32, "LookupPrivilegeValueW")))
        abort();
    if(!AdjustTokenPrivileges)
      if(!(AdjustTokenPrivileges = (AdjustTokenPrivileges_t) GetProcAddress(advapi32, "AdjustTokenPrivileges")))
        abort();
    // Only provided on Windows 8 and above
    if(!PrefetchVirtualMemory_)
      PrefetchVirtualMemory_ = (PrefetchVirtualMemory_t) GetProcAddress(kernel32, "PrefetchVirtualMemory");
    if(!DiscardVirtualMemory_)
      DiscardVirtualMemory_ = (DiscardVirtualMemory_t) GetProcAddress(kernel32, "DiscardVirtualMemory");
#ifdef AFIO_OP_STACKBACKTRACEDEPTH
    if(dbghelp)
    {
      HMODULE dbghelp = LoadLibraryA("DBGHELP.DLL");
      if(!(SymInitialize = (SymInitialize_t) GetProcAddress(dbghelp, "SymInitializeW")))
        abort();
      if(!SymInitialize(GetCurrentProcess(), nullptr, true))
        abort();
      if(!(SymGetLineFromAddr64 = (SymGetLineFromAddr64_t) GetProcAddress(dbghelp, "SymGetLineFromAddrW64")))
        abort();
    }
#endif
    if(!RtlCaptureStackBackTrace)
      if(!(RtlCaptureStackBackTrace = (RtlCaptureStackBackTrace_t) GetProcAddress(ntdllh, "RtlCaptureStackBackTrace")))
        abort();

    if(!RtlDosPathNameToNtPathName_U)
      if(!(RtlDosPathNameToNtPathName_U = (RtlDosPathNameToNtPathName_U_t) GetProcAddress(ntdllh, "RtlDosPathNameToNtPathName_U")))
        abort();
    // MAKE SURE you update the early exit check at the top to whatever the last of these is!
  }
#ifdef _MSC_VER
#pragma warning(pop)
#endif
  static inline void init()
  {
    static bool initialised = false;
    if(!initialised)
    {
      doinit();
      initialised = true;
    }
  }

  static inline filesystem::file_type to_st_type(ULONG FileAttributes, ULONG ReparsePointTag)
  {
#ifdef AFIO_USE_LEGACY_FILESYSTEM_SEMANTICS
    if(FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT && (ReparsePointTag == IO_REPARSE_TAG_MOUNT_POINT || ReparsePointTag == IO_REPARSE_TAG_SYMLINK))
      return filesystem::file_type::symlink_file;
    // return filesystem::file_type::reparse_file;
    else if(FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      return filesystem::file_type::directory_file;
    else
      return filesystem::file_type::regular_file;
#else
    if(FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT && (ReparsePointTag == IO_REPARSE_TAG_MOUNT_POINT || ReparsePointTag == IO_REPARSE_TAG_SYMLINK))
      return filesystem::file_type::symlink;
    // return filesystem::file_type::reparse_file;
    else if(FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      return filesystem::file_type::directory;
    else
      return filesystem::file_type::regular;
#endif
  }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 6326)  // comparison of constants
#endif
  static inline std::chrono::system_clock::time_point to_timepoint(LARGE_INTEGER time)
  {
    // For speed we make the big assumption that the STL's system_clock is based on the time_t epoch 1st Jan 1970.
    static constexpr unsigned long long FILETIME_OFFSET_TO_1970 = ((27111902ULL << 32U) + 3577643008ULL);
    // Need to have this self-adapt to the STL being used
    static constexpr unsigned long long STL_TICKS_PER_SEC = (unsigned long long) std::chrono::system_clock::period::den / std::chrono::system_clock::period::num;
    static constexpr unsigned long long multiplier = STL_TICKS_PER_SEC >= 10000000ULL ? STL_TICKS_PER_SEC / 10000000ULL : 1;
    static constexpr unsigned long long divider = STL_TICKS_PER_SEC >= 10000000ULL ? 1 : 10000000ULL / STL_TICKS_PER_SEC;

    unsigned long long ticks_since_1970 = (time.QuadPart - FILETIME_OFFSET_TO_1970);  // In 100ns increments
    std::chrono::system_clock::duration duration(ticks_since_1970 * multiplier / divider);
    return std::chrono::system_clock::time_point(duration);
  }
  static inline LARGE_INTEGER from_timepoint(std::chrono::system_clock::time_point time)
  {
    // For speed we make the big assumption that the STL's system_clock is based on the time_t epoch 1st Jan 1970.
    static constexpr unsigned long long FILETIME_OFFSET_TO_1970 = ((27111902ULL << 32U) + 3577643008ULL);
    static const std::chrono::system_clock::time_point time_point_1970 = std::chrono::system_clock::from_time_t(0);

    LARGE_INTEGER ret;
    ret.QuadPart = FILETIME_OFFSET_TO_1970 + std::chrono::duration_cast<std::chrono::nanoseconds>(time - time_point_1970).count() / 100;
    return ret;
  }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

// Adapted from http://www.cprogramming.com/snippets/source-code/convert-ntstatus-win32-error
// Could use RtlNtStatusToDosError() instead
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 6387)  // MSVC sanitiser warns on misuse of GetOverlappedResult
#endif
  static inline DWORD win32_error_from_nt_status(NTSTATUS ntstatus)
  {
    DWORD br;
    OVERLAPPED o;

    o.Internal = ntstatus;
    o.InternalHigh = 0;
    o.Offset = 0;
    o.OffsetHigh = 0;
    o.hEvent = 0;
    GetOverlappedResult(NULL, &o, &br, FALSE);
    return GetLastError();
  }
#ifdef _MSC_VER
#pragma warning(pop)
#endif
}  // namespace

// disable to prevent accidental usage
template <class T> inline result<T> make_errored_result(NTSTATUS e, const char *extended = nullptr)
{
  (void) e;
  (void) extended;
  static_assert(!std::is_same<T, T>::value, "Use make_errored_result_nt<T>(NTSTATUS).");
}
template <class T> inline outcome<T> make_errored_outcome(NTSTATUS e, const char *extended = nullptr)
{
  (void) e;
  (void) extended;
  static_assert(!std::is_same<T, T>::value, "Use make_errored_outcome_nt<T>(NTSTATUS).");
}
template <class T> inline result<T> make_errored_result_nt(NTSTATUS e, const char *extended = nullptr)
{
  return make_errored_result<T>(windows_nt_kernel::win32_error_from_nt_status(e), extended);
}
template <class T> inline outcome<T> make_errored_outcome_nt(NTSTATUS e, const char *extended = nullptr)
{
  return make_errored_outcome<T>(windows_nt_kernel::win32_error_from_nt_status(e), extended);
}

#if 0
static inline void fill_stat_t(stat_t &stat, AFIO_POSIX_STAT_STRUCT s, metadata_flags wanted)
{
#ifndef _WIN32
  if (!!(wanted&metadata_flags::dev)) { stat.st_dev = s.st_dev; }
#endif
  if (!!(wanted&metadata_flags::ino)) { stat.st_ino = s.st_ino; }
  if (!!(wanted&metadata_flags::type)) { stat.st_type = to_st_type(s.st_mode); }
#ifndef _WIN32
  if (!!(wanted&metadata_flags::perms)) { stat.st_mode = s.st_perms; }
#endif
  if (!!(wanted&metadata_flags::nlink)) { stat.st_nlink = s.st_nlink; }
#ifndef _WIN32
  if (!!(wanted&metadata_flags::uid)) { stat.st_uid = s.st_uid; }
  if (!!(wanted&metadata_flags::gid)) { stat.st_gid = s.st_gid; }
  if (!!(wanted&metadata_flags::rdev)) { stat.st_rdev = s.st_rdev; }
#endif
  if (!!(wanted&metadata_flags::atim)) { stat.st_atim = chrono::system_clock::from_time_t(s.st_atime); }
  if (!!(wanted&metadata_flags::mtim)) { stat.st_mtim = chrono::system_clock::from_time_t(s.st_mtime); }
  if (!!(wanted&metadata_flags::birthtim)) { stat.st_birthtim = chrono::system_clock::from_time_t(s.st_ctime); }
  if (!!(wanted&metadata_flags::size)) { stat.st_size = s.st_size; }
}
#endif

// Utility routines for implementing deadline sleeps on Windows which only provides interval sleeps
#if 0  // This is the win32 edition. The NT kernel edition is much cleaner and lower overhead.
struct win_handle_deleter
{
  void operator()(HANDLE h) { CloseHandle(h); }
};
static inline std::unique_ptr<void, win_handle_deleter> create_waitable_timer()
{
  HANDLE ret = CreateWaitableTimer(nullptr, true, nullptr);
  if(INVALID_HANDLE_VALUE == ret)
    throw std::system_error(GetLastError(), std::system_category());
  return std::unique_ptr<void, win_handle_deleter>(ret);
}
static inline HANDLE get_thread_local_waitable_timer()
{
  static thread_local auto self = create_waitable_timer();
  return self.get();
}

/*! Defines a number of variables into its scope:
- began_steady: Set to the steady clock at the beginning of a sleep
- end_utc: Set to the system clock when the sleep must end
- sleep_interval: Set to the number of steady milliseconds until the sleep must end
- sleep_object: Set to a primed deadline timer HANDLE which will signal when the system clock reaches the deadline
*/
#define AFIO_WIN_DEADLINE_TO_SLEEP_INIT(d)                                                                                                                                                                                                                                                                                     \
  std::chrono::steady_clock::time_point began_steady;                                                                                                                                                                                                                                                                          \
  \
std::chrono::system_clock::time_point end_utc;                                                                                                                                                                                                                                                                                 \
  \
if(d)                                                                                                                                                                                                                                                                                                                          \
  \
{                                                                                                                                                                                                                                                                                                                         \
    if((d).steady)                                                                                                                                                                                                                                                                                                             \
      began_steady = std::chrono::steady_clock::now();                                                                                                                                                                                                                                                                         \
    else                                                                                                                                                                                                                                                                                                                       \
      end_utc = (d).to_time_point();                                                                                                                                                                                                                                                                                           \
  \
}                                                                                                                                                                                                                                                                                                                         \
  \
DWORD sleep_interval = INFINITE;                                                                                                                                                                                                                                                                                               \
  \
HANDLE sleep_object = nullptr;

#define AFIO_WIN_DEADLINE_TO_SLEEP_LOOP(d)                                                                                                                                                                                                                                                                                     \
  \
if(d)                                                                                                                                                                                                                                                                                                                          \
  \
{                                                                                                                                                                                                                                                                                                                         \
    if((d).steady)                                                                                                                                                                                                                                                                                                             \
    {                                                                                                                                                                                                                                                                                                                          \
      std::chrono::milliseconds ms;                                                                                                                                                                                                                                                                                            \
      ms = std::chrono::duration_cast<std::chrono::milliseconds>((began_steady + std::chrono::nanoseconds(d.nsecs)) - std::chrono::steady_clock::now());                                                                                                                                                                       \
      if(ms.count() < 0)                                                                                                                                                                                                                                                                                                       \
        sleep_interval = 0;                                                                                                                                                                                                                                                                                                    \
      else                                                                                                                                                                                                                                                                                                                     \
        sleep_interval = (DWORD) ms.count();                                                                                                                                                                                                                                                                                   \
    }                                                                                                                                                                                                                                                                                                                          \
    else                                                                                                                                                                                                                                                                                                                       \
    {                                                                                                                                                                                                                                                                                                                          \
      sleep_object = get_thread_local_waitable_timer();                                                                                                                                                                                                                                                                        \
      LARGE_INTEGER due_time = windows_nt_kernel::from_timepoint(end_utc);                                                                                                                                                                                                                                                     \
      if(!SetWaitableTimer(sleep_object, &due_time, 0, nullptr, nullptr, false))                                                                                                                                                                                                                                               \
        throw std::system_error(GetLastError(), std::system_category());                                                                                                                                                                                                                                                       \
    }                                                                                                                                                                                                                                                                                                                          \
  \
}

#define AFIO_WIN_DEADLINE_TO_TIMEOUT(type, d)                                                                                                                                                                                                                                                                                  \
  \
if(d)                                                                                                                                                                                                                                                                                                                          \
  \
{                                                                                                                                                                                                                                                                                                                         \
    if((d).steady)                                                                                                                                                                                                                                                                                                             \
    {                                                                                                                                                                                                                                                                                                                          \
      if(std::chrono::steady_clock::now() >= (began_steady + std::chrono::nanoseconds((d).nsecs)))                                                                                                                                                                                                                             \
        return make_errored_result<type>(std::errc::timed_out);                                                                                                                                                                                                                                                                \
    }                                                                                                                                                                                                                                                                                                                          \
    else                                                                                                                                                                                                                                                                                                                       \
    {                                                                                                                                                                                                                                                                                                                          \
      if(std::chrono::system_clock::now() >= end_utc)                                                                                                                                                                                                                                                                          \
        return make_errored_result<type>(std::errc::timed_out);                                                                                                                                                                                                                                                                \
    }                                                                                                                                                                                                                                                                                                                          \
  \
}
#else
/*! Defines a number of variables into its scope:
- began_steady: Set to the steady clock at the beginning of a sleep
- end_utc: Set to the system clock when the sleep must end
- sleep_interval: Set to the number of steady milliseconds until the sleep must end
- sleep_object: Set to a primed deadline timer HANDLE which will signal when the system clock reaches the deadline
*/
#define AFIO_WIN_DEADLINE_TO_SLEEP_INIT(d)                                                                                                                                                                                                                                                                                     \
  std::chrono::steady_clock::time_point began_steady;                                                                                                                                                                                                                                                                          \
  \
std::chrono::system_clock::time_point end_utc;                                                                                                                                                                                                                                                                                 \
  \
alignas(8) LARGE_INTEGER _timeout;                                                                                                                                                                                                                                                                                             \
  \
memset(&_timeout, 0, sizeof(_timeout));                                                                                                                                                                                                                                                                                        \
  \
LARGE_INTEGER *timeout = nullptr;                                                                                                                                                                                                                                                                                              \
  \
if(d)                                                                                                                                                                                                                                                                                                                          \
  \
{                                                                                                                                                                                                                                                                                                                         \
    if((d).steady)                                                                                                                                                                                                                                                                                                             \
      began_steady = std::chrono::steady_clock::now();                                                                                                                                                                                                                                                                         \
    else                                                                                                                                                                                                                                                                                                                       \
    {                                                                                                                                                                                                                                                                                                                          \
      end_utc = (d).to_time_point();                                                                                                                                                                                                                                                                                           \
      _timeout = windows_nt_kernel::from_timepoint(end_utc);                                                                                                                                                                                                                                                                   \
    }                                                                                                                                                                                                                                                                                                                          \
    timeout = &_timeout;                                                                                                                                                                                                                                                                                                       \
  \
}

#define AFIO_WIN_DEADLINE_TO_SLEEP_LOOP(d)                                                                                                                                                                                                                                                                                     \
  if((d) && (d).steady)                                                                                                                                                                                                                                                                                                        \
  {                                                                                                                                                                                                                                                                                                                            \
    std::chrono::nanoseconds ns;                                                                                                                                                                                                                                                                                               \
    ns = std::chrono::duration_cast<std::chrono::nanoseconds>((began_steady + std::chrono::nanoseconds(d.nsecs)) - std::chrono::steady_clock::now());                                                                                                                                                                          \
    if(ns.count() < 0)                                                                                                                                                                                                                                                                                                         \
      _timeout.QuadPart = 0;                                                                                                                                                                                                                                                                                                   \
    else                                                                                                                                                                                                                                                                                                                       \
      _timeout.QuadPart = ns.count() / -100;                                                                                                                                                                                                                                                                                   \
  }

#define AFIO_WIN_DEADLINE_TO_PARTIAL_DEADLINE(nd, d)                                                                                                                                                                                                                                                                           \
  if(d)                                                                                                                                                                                                                                                                                                                        \
  {                                                                                                                                                                                                                                                                                                                            \
    if((d).steady)                                                                                                                                                                                                                                                                                                             \
    {                                                                                                                                                                                                                                                                                                                          \
      std::chrono::nanoseconds ns = std::chrono::duration_cast<std::chrono::nanoseconds>((began_steady + std::chrono::nanoseconds((d).nsecs)) - std::chrono::steady_clock::now());                                                                                                                                             \
      if(ns.count() < 0)                                                                                                                                                                                                                                                                                                       \
        (nd).nsecs = 0;                                                                                                                                                                                                                                                                                                        \
      else                                                                                                                                                                                                                                                                                                                     \
        (nd).nsecs = ns.count();                                                                                                                                                                                                                                                                                               \
    }                                                                                                                                                                                                                                                                                                                          \
    else                                                                                                                                                                                                                                                                                                                       \
      (nd) = (d);                                                                                                                                                                                                                                                                                                              \
  }

#define AFIO_WIN_DEADLINE_TO_TIMEOUT(d)                                                                                                                                                                                                                                                                                        \
  \
if(d)                                                                                                                                                                                                                                                                                                                          \
  \
{                                                                                                                                                                                                                                                                                                                         \
    if((d).steady)                                                                                                                                                                                                                                                                                                             \
    {                                                                                                                                                                                                                                                                                                                          \
      if(std::chrono::steady_clock::now() >= (began_steady + std::chrono::nanoseconds((d).nsecs)))                                                                                                                                                                                                                             \
        return std::errc::timed_out;                                                                                                                                                                                                                                                                                           \
    }                                                                                                                                                                                                                                                                                                                          \
    else                                                                                                                                                                                                                                                                                                                       \
    {                                                                                                                                                                                                                                                                                                                          \
      if(std::chrono::system_clock::now() >= end_utc)                                                                                                                                                                                                                                                                          \
        return std::errc::timed_out;                                                                                                                                                                                                                                                                                           \
    }                                                                                                                                                                                                                                                                                                                          \
  \
}
#endif

// Initialise an IO_STATUS_BLOCK for later wait operations
static inline windows_nt_kernel::IO_STATUS_BLOCK make_iostatus() noexcept
{
  windows_nt_kernel::IO_STATUS_BLOCK isb;
  memset(&isb, 0, sizeof(isb));
  isb.Status = -1;
  return isb;
}

// Wait for an overlapped handle to complete a specific operation
static inline NTSTATUS ntwait(HANDLE h, windows_nt_kernel::IO_STATUS_BLOCK &isb, const deadline &d) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  AFIO_WIN_DEADLINE_TO_SLEEP_INIT(d);
  do  // needs to be a do, not while in order to flip auto reset event objects etc.
  {
    AFIO_WIN_DEADLINE_TO_SLEEP_LOOP(d);
    // Pump alerts and APCs
    NTSTATUS ntstat = NtWaitForSingleObject(h, true, timeout);
    if(STATUS_TIMEOUT == ntstat)
    {
      DWORD expected = (DWORD) -1;
      // Have to be very careful here, atomically swap timed out for the -1 only
      InterlockedCompareExchange(&isb.Status, ntstat, expected);
      // If it's no longer -1 or the i/o completes, that's fine.
      return isb.Status;
    }
  } while(isb.Status == -1);
  return isb.Status;
}
static inline NTSTATUS ntwait(HANDLE h, OVERLAPPED &ol, const deadline &d) noexcept
{
  return ntwait(h, reinterpret_cast<windows_nt_kernel::IO_STATUS_BLOCK &>(ol), d);
}

// Sleep the thread until some deadline
static inline bool ntsleep(const deadline &d, bool return_on_alert = false) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  AFIO_WIN_DEADLINE_TO_SLEEP_INIT(d);
  for(;;)
  {
    AFIO_WIN_DEADLINE_TO_SLEEP_LOOP(d);
    // Pump alerts and APCs
    NTSTATUS ntstat = NtDelayExecution(true, timeout);
    (void) ntstat;
    if((d).steady)
    {
      if(std::chrono::steady_clock::now() >= (began_steady + std::chrono::nanoseconds((d).nsecs)))
        return false;
    }
    else
    {
      if(std::chrono::system_clock::now() >= end_utc)
        return false;
    }
    if(return_on_alert)
      return true;
  }
}


// Utility routines for building an ACCESS_MASK from a handle::mode
static inline result<ACCESS_MASK> access_mask_from_handle_mode(native_handle_type &nativeh, handle::mode _mode)
{
  ACCESS_MASK access = SYNCHRONIZE;
  switch(_mode)
  {
  case handle::mode::unchanged:
    return std::errc::invalid_argument;
  case handle::mode::none:
    break;
  case handle::mode::attr_read:
    access |= FILE_READ_ATTRIBUTES;
    break;
  case handle::mode::attr_write:
    access |= FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES;
    break;
  case handle::mode::read:
    access |= GENERIC_READ;
    nativeh.behaviour |= native_handle_type::disposition::seekable | native_handle_type::disposition::readable;
    break;
  case handle::mode::write:
    access |= GENERIC_WRITE | GENERIC_READ;
    nativeh.behaviour |= native_handle_type::disposition::seekable | native_handle_type::disposition::readable | native_handle_type::disposition::writable;
    break;
  case handle::mode::append:
    access |= FILE_APPEND_DATA;
    nativeh.behaviour |= native_handle_type::disposition::writable | native_handle_type::disposition::append_only;
    break;
  }
  return access;
}

static inline result<DWORD> attributes_from_handle_caching_and_flags(native_handle_type &nativeh, handle::caching _caching, handle::flag flags)
{
  DWORD attribs = 0;
  if(flags & handle::flag::overlapped)
  {
    attribs |= FILE_FLAG_OVERLAPPED;
    nativeh.behaviour |= native_handle_type::disposition::overlapped;
  }
  switch(_caching)
  {
  case handle::caching::unchanged:
    return std::errc::invalid_argument;
  case handle::caching::none:
    attribs |= FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH;
    nativeh.behaviour |= native_handle_type::disposition::aligned_io;
    break;
  case handle::caching::only_metadata:
    attribs |= FILE_FLAG_NO_BUFFERING;
    nativeh.behaviour |= native_handle_type::disposition::aligned_io;
    break;
  case handle::caching::reads:
  case handle::caching::reads_and_metadata:
    attribs |= FILE_FLAG_WRITE_THROUGH;
    break;
  case handle::caching::all:
  case handle::caching::safety_fsyncs:
    break;
  case handle::caching::temporary:
    attribs |= FILE_ATTRIBUTE_TEMPORARY;
    break;
  }
  if(flags & handle::flag::unlink_on_close)
    attribs |= FILE_FLAG_DELETE_ON_CLOSE;
  return attribs;
}

/* Our own custom CreateFileW() implementation.

The Win32 CreateFileW() implementation is unfortunately slow. It also, very annoyingly,
maps STATUS_DELETE_PENDING onto ERROR_ACCESS_DENIED instead of to ERROR_DELETE_PENDING
which means our file open routines return EACCES, which is useless to us for detecting
when we are trying to open files being deleted. We therefore reimplement CreateFileW()
with a non-broken version.

This edition does pretty much the same as the Win32 edition, minus support for file
templates and lpFileName being anything but a file path.
*/
static inline HANDLE CreateFileW_(_In_ LPCTSTR lpFileName, _In_ DWORD dwDesiredAccess, _In_ DWORD dwShareMode, _In_opt_ LPSECURITY_ATTRIBUTES lpSecurityAttributes, _In_ DWORD dwCreationDisposition, _In_ DWORD dwFlagsAndAttributes, _In_opt_ HANDLE hTemplateFile)
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  if(!lpFileName || !lpFileName[0])
  {
    SetLastError(ERROR_PATH_NOT_FOUND);
    return INVALID_HANDLE_VALUE;
  }
  if(hTemplateFile || !lstrcmpW(lpFileName, L"CONIN$") || !lstrcmpW(lpFileName, L"CONOUT$"))
  {
    SetLastError(ERROR_NOT_SUPPORTED);
    return INVALID_HANDLE_VALUE;
  }
  switch(dwCreationDisposition)
  {
  case CREATE_NEW:
    dwCreationDisposition = FILE_CREATE;
    break;
  case CREATE_ALWAYS:
    dwCreationDisposition = FILE_OVERWRITE_IF;
    break;
  case OPEN_EXISTING:
    dwCreationDisposition = FILE_OPEN;
    break;
  case OPEN_ALWAYS:
    dwCreationDisposition = FILE_OPEN_IF;
    break;
  case TRUNCATE_EXISTING:
    dwCreationDisposition = FILE_OVERWRITE;
    break;
  default:
    SetLastError(ERROR_INVALID_PARAMETER);
    return INVALID_HANDLE_VALUE;
  }
  ULONG flags = 0;
  if(!(dwFlagsAndAttributes & FILE_FLAG_OVERLAPPED))
    flags |= FILE_SYNCHRONOUS_IO_NONALERT;
  if(dwFlagsAndAttributes & FILE_FLAG_WRITE_THROUGH)
    flags |= FILE_WRITE_THROUGH;
  if(dwFlagsAndAttributes & FILE_FLAG_NO_BUFFERING)
    flags |= FILE_NO_INTERMEDIATE_BUFFERING;
  if(dwFlagsAndAttributes & FILE_FLAG_RANDOM_ACCESS)
    flags |= FILE_RANDOM_ACCESS;
  if(dwFlagsAndAttributes & FILE_FLAG_SEQUENTIAL_SCAN)
    flags |= FILE_SEQUENTIAL_ONLY;
  if(dwFlagsAndAttributes & FILE_FLAG_DELETE_ON_CLOSE)
  {
    flags |= FILE_DELETE_ON_CLOSE;
    dwDesiredAccess |= DELETE;
  }
  if(dwFlagsAndAttributes & FILE_FLAG_BACKUP_SEMANTICS)
  {
    if(dwDesiredAccess & GENERIC_ALL)
      flags |= FILE_OPEN_FOR_BACKUP_INTENT | FILE_OPEN_REMOTE_INSTANCE;
    else
    {
      if(dwDesiredAccess & GENERIC_READ)
        flags |= FILE_OPEN_FOR_BACKUP_INTENT;
      if(dwDesiredAccess & GENERIC_WRITE)
        flags |= FILE_OPEN_REMOTE_INSTANCE;
    }
  }
  else
    flags |= FILE_NON_DIRECTORY_FILE;
  if(dwFlagsAndAttributes & FILE_FLAG_OPEN_REPARSE_POINT)
    flags |= FILE_OPEN_REPARSE_POINT;
  if(dwFlagsAndAttributes & FILE_FLAG_OPEN_NO_RECALL)
    flags |= FILE_OPEN_NO_RECALL;

  UNICODE_STRING NtPath;
  if(!RtlDosPathNameToNtPathName_U(lpFileName, &NtPath, NULL, NULL))
  {
    SetLastError(ERROR_FILE_NOT_FOUND);
    return INVALID_HANDLE_VALUE;
  }
  auto unntpath = undoer([&NtPath] {
    if(!HeapFree(GetProcessHeap(), 0, NtPath.Buffer))
      abort();
  });

  OBJECT_ATTRIBUTES ObjectAttributes;
  InitializeObjectAttributes(&ObjectAttributes, &NtPath, 0, NULL, NULL);
  if(lpSecurityAttributes)
  {
    if(lpSecurityAttributes->bInheritHandle)
      ObjectAttributes.Attributes |= OBJ_INHERIT;
    ObjectAttributes.SecurityDescriptor = lpSecurityAttributes->lpSecurityDescriptor;
  }
  if(!(dwFlagsAndAttributes & FILE_FLAG_POSIX_SEMANTICS))
    ObjectAttributes.Attributes |= OBJ_CASE_INSENSITIVE;

  HANDLE ret = INVALID_HANDLE_VALUE;
  IO_STATUS_BLOCK isb = make_iostatus();
  dwFlagsAndAttributes &= ~0xfff80000;
  NTSTATUS ntstat = NtCreateFile(&ret, dwDesiredAccess, &ObjectAttributes, &isb, NULL, dwFlagsAndAttributes, dwShareMode, dwCreationDisposition, flags, NULL, 0);
  if(STATUS_SUCCESS == ntstat)
    return ret;

  win32_error_from_nt_status(ntstat);
  if(STATUS_DELETE_PENDING == ntstat)
  {
    SetLastError(ERROR_DELETE_PENDING);
  }
  return INVALID_HANDLE_VALUE;
}

AFIO_V2_NAMESPACE_END

#endif
