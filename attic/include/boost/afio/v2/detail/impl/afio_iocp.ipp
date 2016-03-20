/* async_file_io
Provides a threadpool and asynchronous file i/o infrastructure based on Boost.ASIO, Boost.Iostreams and filesystem
(C) 2013-2014 Niall Douglas http://www.nedprod.com/
File Created: Mar 2013
*/

#ifdef WIN32
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 6262) // Excessive stack usage
#endif

BOOST_AFIO_V2_NAMESPACE_BEGIN

namespace detail {
    // Helper for opening files. Lightweight means open with no access, it can be faster.
    static inline std::pair<NTSTATUS, HANDLE> ntcreatefile(handle_ptr dirh, BOOST_AFIO_V2_NAMESPACE::path path, file_flags flags, bool overlapped=true) noexcept
    {
      DWORD access=FILE_READ_ATTRIBUTES|SYNCHRONIZE, attribs=FILE_ATTRIBUTE_NORMAL;
      DWORD fileshare=FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE;
      DWORD creatdisp=0, ntflags=0x4000/*FILE_OPEN_FOR_BACKUP_INTENT*/;
      if(!overlapped)
        ntflags|=0x20/*FILE_SYNCHRONOUS_IO_NONALERT*/;
      if(flags==file_flags::none)

        ntflags|=0x00200000/*FILE_OPEN_REPARSE_POINT*/;

      else
      {
        if(!!(flags & file_flags::int_opening_link))
            ntflags|=0x00200000/*FILE_OPEN_REPARSE_POINT*/;
        if(!!(flags & file_flags::int_opening_dir))
        {
            /* NOTE TO SELF: Never, ever, ever again request FILE_DELETE_CHILD perms. It subtly breaks renaming, enumeration
                             and a ton of other stuff in a really weird way. */
            ntflags|=0x01/*FILE_DIRECTORY_FILE*/;
            access|=FILE_LIST_DIRECTORY|FILE_TRAVERSE;  // FILE_READ_DATA|FILE_EXECUTE
            // Match POSIX where read perms are sufficient to use this handle as a relative base for creating new entries
            //access|=FILE_ADD_FILE|FILE_ADD_SUBDIRECTORY;  // FILE_WRITE_DATA|FILE_APPEND_DATA  // Fails where user is not administrator
            if(!!(flags & file_flags::read)) access|=GENERIC_READ;
            // Write access probably means he wants to delete or rename self
            if(!!(flags & file_flags::write)) access|=GENERIC_WRITE|DELETE;
            // Windows doesn't like opening directories without buffering.
            if(!!(flags & file_flags::os_direct)) flags=flags & ~file_flags::os_direct;
        }
        else
        {
            ntflags|=0x040/*FILE_NON_DIRECTORY_FILE*/;
            if(!!(flags & file_flags::append)) access|=FILE_APPEND_DATA|DELETE;
            else
            {
                if(!!(flags & file_flags::read)) access|=GENERIC_READ;
                if(!!(flags & file_flags::write)) access|=GENERIC_WRITE|DELETE;
            }
            if(!!(flags & file_flags::will_be_sequentially_accessed))
                ntflags|=0x00000004/*FILE_SEQUENTIAL_ONLY*/;
            else if(!!(flags & file_flags::will_be_randomly_accessed))
                ntflags|=0x00000800/*FILE_RANDOM_ACCESS*/;
            if(!!(flags & file_flags::temporary_file))
                attribs|=FILE_ATTRIBUTE_TEMPORARY;
        }
        if(!!(flags & file_flags::delete_on_close) && (!!(flags & file_flags::create_only_if_not_exist) || !!(flags & file_flags::int_file_share_delete)))
        {
            ntflags|=0x00001000/*FILE_DELETE_ON_CLOSE*/;
            access|=DELETE;
        }
        if(!!(flags & file_flags::int_file_share_delete))
            access|=DELETE;
      }
      if(!!(flags & file_flags::create_only_if_not_exist))
      {
        creatdisp|=0x00000002/*FILE_CREATE*/;
      }
      else if(!!(flags & file_flags::create))
      {
        creatdisp|=0x00000003/*FILE_OPEN_IF*/;
      }
      else if(!!(flags & file_flags::truncate)) creatdisp|=0x00000005/*FILE_OVERWRITE_IF*/;
      else creatdisp|=0x00000001/*FILE_OPEN*/;
      if(!!(flags & file_flags::os_direct)) ntflags|=0x00000008/*FILE_NO_INTERMEDIATE_BUFFERING*/;
      if(!!(flags & file_flags::always_sync)) ntflags|=0x00000002/*FILE_WRITE_THROUGH*/;

      windows_nt_kernel::init();
      using namespace windows_nt_kernel;
      HANDLE h=nullptr;
      IO_STATUS_BLOCK isb={ 0 };
      OBJECT_ATTRIBUTES oa={sizeof(OBJECT_ATTRIBUTES)};
      oa.RootDirectory=dirh ? dirh->native_handle() : nullptr;
      UNICODE_STRING _path;
      // If relative path, or symlinked DOS path, use case insensitive
      bool isSymlinkedDosPath=(path.native().size()>3 && path.native()[0]=='\\' && path.native()[1]=='?' && path.native()[2]=='?' && path.native()[3]=='\\');
      oa.Attributes=(path.native()[0]!='\\' || isSymlinkedDosPath) ? 0x40/*OBJ_CASE_INSENSITIVE*/ : 0;
      //if(!!(flags & file_flags::int_opening_link))
      //  oa.Attributes|=0x100/*OBJ_OPENLINK*/;
      _path.Buffer=const_cast<path::value_type *>(path.c_str());
      _path.MaximumLength=(_path.Length=(USHORT) (path.native().size()*sizeof(path::value_type)))+sizeof(path::value_type);
      oa.ObjectName=&_path;
      LARGE_INTEGER AllocationSize={0};
      return std::make_pair(NtCreateFile(&h, access, &oa, &isb, &AllocationSize, attribs, fileshare,
          creatdisp, ntflags, NULL, 0), h);
    }

    struct win_actual_lock_file;
    struct win_lock_file : public lock_file<win_actual_lock_file>
    {
      asio::windows::object_handle ev;
      win_lock_file(handle *_h=nullptr) : lock_file<win_actual_lock_file>(_h), ev(_h->parent()->threadsource()->io_service())
      {
        HANDLE evh;
        BOOST_AFIO_ERRHWIN(INVALID_HANDLE_VALUE!=(evh=CreateEvent(nullptr, true, false, nullptr)));
        ev.assign(evh);
      }
    };
    
    // Two modes of calling, either a handle or a leafname + dir handle
    static inline bool isDeletedFile(path::string_type leafname)
    {
      if(leafname.size()==64+6 && !leafname.compare(0, 6, L".afiod"))
      {
        // Could be one of our "deleted" files, is he ".afiod" + all hex?
        for(size_t n=6; n<leafname.size(); n++)
        {
          auto c=leafname[n];
          if(!((c>='0' && c<='9') || (c>='a' && c<='f')))
            return false;
        }
        return true;
      }
      return false;
    }
    static inline bool isDeletedFile(handle_ptr h)
    {
      windows_nt_kernel::init();
      using namespace windows_nt_kernel;
      IO_STATUS_BLOCK isb={ 0 };
      BOOST_AFIO_TYPEALIGNMENT(8) FILE_ALL_INFORMATION fai;
      NTSTATUS ntstat=NtQueryInformationFile(h->native_handle(), &isb, &fai.StandardInformation, sizeof(fai.StandardInformation), FileStandardInformation);
      // If the query succeeded and delete is pending, filter out this entry
      if(!ntstat && fai.StandardInformation.DeletePending)
        return true;
      else
        return false;
    }
    
    static inline path MountPointFromHandle(HANDLE h)
    {
      windows_nt_kernel::init();
      using namespace windows_nt_kernel;
      BOOST_AFIO_TYPEALIGNMENT(8) path::value_type buffer[32769];
      IO_STATUS_BLOCK isb={ 0 };
      NTSTATUS ntstat;
      UNICODE_STRING *objectpath=(UNICODE_STRING *) buffer;
      ULONG length;
      ntstat=NtQueryObject(h, ObjectNameInformation, objectpath, sizeof(buffer), &length);
      if(STATUS_PENDING==ntstat)
        ntstat=NtWaitForSingleObject(h, FALSE, NULL);
      BOOST_AFIO_ERRHNT(ntstat);
      // Now get the subpath of our handle within its mount
      BOOST_AFIO_TYPEALIGNMENT(8) path::value_type buffer2[sizeof(FILE_NAME_INFORMATION)/sizeof(path::value_type)+32769];
      FILE_NAME_INFORMATION *fni=(FILE_NAME_INFORMATION *) buffer2;
      ntstat=NtQueryInformationFile(h, &isb, fni, sizeof(buffer2), FileNameInformation);
      if(STATUS_PENDING==ntstat)
        ntstat=NtWaitForSingleObject(h, FALSE, NULL);
      BOOST_AFIO_ERRHNT(ntstat);
      // The mount path will be the remainder of the objectpath after removing the
      // common ending
      size_t remainder=(objectpath->Length-fni->FileNameLength)/sizeof(path::value_type);
      return path::string_type(objectpath->Buffer, remainder);
    }
    
    static inline path::string_type VolumeNameFromMountPoint(path mountpoint)
    {
      HANDLE vh;
      BOOST_AFIO_TYPEALIGNMENT(8) path::value_type buffer[32769];
      BOOST_AFIO_TYPEALIGNMENT(8) path::value_type volumename[64];
      // Enumerate every volume in the system and the device it maps until we find ours
      BOOST_AFIO_ERRHWIN(INVALID_HANDLE_VALUE!=(vh=FindFirstVolume(volumename, sizeof(volumename)/sizeof(volumename[0]))));
      auto unvh=Undoer([&vh]{FindVolumeClose(vh);});
      size_t volumenamelen;
      do
      {
        DWORD len;
        volumenamelen=wcslen(volumename);
        volumename[volumenamelen-1]=0;  // remove the trailing backslash
        BOOST_AFIO_ERRHWIN((len=QueryDosDevice(volumename+4, buffer, sizeof(buffer)/sizeof(buffer[0]))));
        if(!mountpoint.native().compare(0, len, buffer))
          return path::string_type(volumename, volumenamelen-1);
      } while(FindNextVolume(vh, volumename, sizeof(volumename)/sizeof(volumename[0])));
      return path::string_type();
    }
}

BOOST_AFIO_HEADERS_ONLY_FUNC_SPEC filesystem::path normalise_path(path p, path_normalise type)
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  bool isSymlinkedDosPath=(p.native().size()>3 && p.native()[0]=='\\' && p.native()[1]=='?' && p.native()[2]=='?' && p.native()[3]=='\\');
  bool needToOpen=!isSymlinkedDosPath || path_normalise::guid_all==type;
  // Path is probably \Device\Harddisk...
  NTSTATUS ntstat;
  HANDLE h=nullptr;
  if(needToOpen)
  {
    // Open with no rights and no access
    std::tie(ntstat, h)=detail::ntcreatefile(handle_ptr(), p, file_flags::none, false);
    BOOST_AFIO_ERRHNTFN(ntstat, [&p]{return p;});
  }
  auto unh=detail::Undoer([&h]{if(h) CloseHandle(h);});
  path mountpoint;
  path::string_type volumename;
  if(isSymlinkedDosPath)
  {
    BOOST_AFIO_TYPEALIGNMENT(8) path::value_type buffer[64];
    mountpoint=path(p.native().substr(0, p.native().find('\\', 4)+1), path::direct());
    BOOST_AFIO_ERRHWIN(GetVolumeNameForVolumeMountPoint(mountpoint.c_str()+4, buffer, 64));
    volumename=buffer;
    // Trim the mount point's ending slash
    filesystem::path::string_type &me=const_cast<filesystem::path::string_type &>(mountpoint.native());
    me.resize(me.size()-1);
  }
  else
  {
    mountpoint=detail::MountPointFromHandle(h);
    volumename=detail::VolumeNameFromMountPoint(mountpoint);
  }
  if(path_normalise::guid_volume==type)
    return filesystem::path(volumename)/p.native().substr(mountpoint.native().size());
  else if(path_normalise::guid_all==type)
  {
    FILE_OBJECTID_BUFFER fob;
    DWORD out;
    BOOST_AFIO_ERRHWIN(DeviceIoControl(h, FSCTL_CREATE_OR_GET_OBJECT_ID, nullptr, 0, &fob, sizeof(fob), &out, nullptr));
    GUID *guid=(GUID *) &fob.ObjectId;
    path::value_type buffer[64];
    swprintf_s(buffer, 64, L"{%08x-%04hx-%04hx-%02x%02x-%02x%02x%02x%02x%02x%02x}", guid->Data1, guid->Data2, guid->Data3, guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3], guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]);
    return filesystem::path(volumename)/filesystem::path::string_type(buffer, 38);
  }
  else
  {
    bool needsExtendedPrefix=false;
    // Are there any illegal Win32 characters in here?
    static BOOST_CONSTEXPR_OR_CONST char reserved_chars[]="\"*/:<>?|";
    for(size_t n=isSymlinkedDosPath ? p.native().find('\\', 5) : 0; !needsExtendedPrefix && n<p.native().size(); n++)
    {
      if(p.native()[n]>=1 && p.native()[n]<=31)
        needsExtendedPrefix=true;
      for(size_t x=0; x<sizeof(reserved_chars); x++)
        if(p.native()[n]==reserved_chars[x])
        {
          needsExtendedPrefix=true;
          break;
        }
    }
    if(!needsExtendedPrefix)
    {
      // Are any segments of the filename a reserved name?
      static constexpr const wchar_t *reserved_names[]={L"CON", L"PRN", L"AUX", L"NUL", L"COM1", L"COM2", L"COM3", L"COM4", L"COM5", L"COM6", L"COM7", L"COM8", L"COM9", L"LPT1", L"LPT2", L"LPT3", L"LPT4", L"LPT5", L"LPT6", L"LPT7", L"LPT8", L"LPT9"};
      for(auto &i : p)
      {
        auto s=i.stem().native();
        if(s[0]==' ' || s[s.size()-1]=='.')
        {
          needsExtendedPrefix=true;
          break;
        }
        for(size_t n=0; n<sizeof(reserved_names)/sizeof(reserved_names[0]); n++)
        {
          if(s==reserved_names[n])
          {
            needsExtendedPrefix=true;
            break;
          }
        }
        if(needsExtendedPrefix)
          break;
      }
    }

    DWORD len;
    BOOST_AFIO_TYPEALIGNMENT(8) path::value_type buffer[32769];
    if(volumename.back()!='\\') volumename.push_back('\\');
    BOOST_AFIO_ERRHWIN(GetVolumePathNamesForVolumeName(volumename.c_str(), buffer, sizeof(buffer)/sizeof(buffer[0]), &len));
    // Read the returned DOS mount points into a vector and sort them by size
    std::vector<path::string_type> dosmountpoints;
    for(path::value_type *x=buffer; *x; x+=wcslen(x)+1)
      dosmountpoints.push_back(path::string_type(x));
    if(dosmountpoints.empty())
      BOOST_AFIO_THROW(std::runtime_error("No Win32 mount points returned for volume path"));
    std::sort(dosmountpoints.begin(), dosmountpoints.end(), [](const path::string_type &a, const path::string_type &b){return a.size()<b.size();});
    filesystem::path ret(dosmountpoints.front());
    ret/=p.native().substr(mountpoint.native().size()+1);
    if(ret.native().size()>=260)
      needsExtendedPrefix=true;
    if(needsExtendedPrefix)
      return filesystem::path("\\\\?")/ret;
    else
      return ret;
  }
}

namespace detail
{
    struct async_io_handle_windows : public handle
    {
        std::unique_ptr<asio::windows::random_access_handle> h;
        void *myid;
        bool has_been_added, SyncOnClose;
        HANDLE sectionh;
        typedef spinlock<bool> pathlock_t;
        mutable pathlock_t pathlock; BOOST_AFIO_V2_NAMESPACE::path _path;
        std::unique_ptr<win_lock_file> lockfile;

        static HANDLE int_checkHandle(HANDLE h, const BOOST_AFIO_V2_NAMESPACE::path &path)
        {
            BOOST_AFIO_ERRHWINFN(INVALID_HANDLE_VALUE!=h, [&path]{return path;});
            return h;
        }
        async_io_handle_windows(dispatcher *_parent, const BOOST_AFIO_V2_NAMESPACE::path &path, file_flags flags) : handle(_parent, flags),
          myid(nullptr), has_been_added(false), SyncOnClose(false), sectionh(nullptr), _path(path) { }
        inline async_io_handle_windows(dispatcher *_parent, const BOOST_AFIO_V2_NAMESPACE::path &path, file_flags flags, bool _SyncOnClose, HANDLE _h);
        inline handle_ptr int_verifymyinode();
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC void close() override final
        {
            BOOST_AFIO_DEBUG_PRINT("D %p\n", this);
            if(sectionh)
            {
                BOOST_AFIO_ERRHWINFN(CloseHandle(sectionh), [this]{return path();});
                sectionh=nullptr;
            }
            if(h)
            {
                // Windows doesn't provide an async fsync so do it synchronously
                if(SyncOnClose && write_count_since_fsync())
                    BOOST_AFIO_ERRHWINFN(FlushFileBuffers(myid), [this]{return path();});
                h->close();
                h.reset();
            }
            // Deregister AFTER close of file handle
            if(has_been_added)
            {
                parent()->int_del_io_handle(myid);
                has_been_added=false;
            }
            myid=nullptr;
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC open_states is_open() const override final
        {
          if(!myid)
            return open_states::closed;
          return available_to_directory_cache() ? open_states::opendir : open_states::open;
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC void *native_handle() const override final { return myid; }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC BOOST_AFIO_V2_NAMESPACE::path path(bool refresh=false) override final
        {
          if(refresh)
          {
            if(!myid)
            {
#if 0
              auto mycontainer=container();
              if(mycontainer)
                mycontainer->path(true);
#endif
            }
            else
            {
              BOOST_AFIO_V2_NAMESPACE::path newpath;
              if(!isDeletedFile(shared_from_this()))
              {
                windows_nt_kernel::init();
                using namespace windows_nt_kernel;
                HANDLE h=myid;
                BOOST_AFIO_TYPEALIGNMENT(8) path::value_type buffer[32769];
                UNICODE_STRING *volumepath=(UNICODE_STRING *) buffer;
                ULONG bufferlength;
                NTSTATUS ntstat=NtQueryObject(h, ObjectNameInformation, volumepath, sizeof(buffer), &bufferlength);
                if(STATUS_PENDING==ntstat)
                  ntstat=NtWaitForSingleObject(h, FALSE, NULL);
                BOOST_AFIO_ERRHNT(ntstat);
                newpath=path::string_type(volumepath->Buffer, volumepath->Length/sizeof(path::value_type));
              }
              bool changed=false;
              BOOST_AFIO_V2_NAMESPACE::path oldpath;
              {
                lock_guard<pathlock_t> g(pathlock);
                if((changed=(newpath!=_path)))
                {
                  oldpath=std::move(_path);
                  _path=newpath;
                }
              }
              if(changed)
              {
                // Currently always zap the container if path changes
                if(this->dirh)
                  this->dirh.reset();
                // Need to update the directory cache
                if(available_to_directory_cache())
                  parent()->int_directory_cached_handle_path_changed(oldpath, newpath, shared_from_this());
#if 0
                // Perhaps also need to update my container
                auto containerh(container());
                if(containerh)
                  containerh->path(true);
#endif
              }
            }
          }
          lock_guard<pathlock_t> g(pathlock);
          return _path;
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC BOOST_AFIO_V2_NAMESPACE::path path() const override final
        {
          lock_guard<pathlock_t> g(pathlock);
          return _path;
        }

        // You can't use shared_from_this() in a constructor so ...
        void do_add_io_handle_to_parent()
        {
            if(myid)
            {
                // Canonicalise my path
                auto newpath=path(true);
                parent()->int_add_io_handle(myid, shared_from_this());
                has_been_added=true;
                // If I'm the right sort of directory, register myself with the dircache. path(true) may have
                // already done this, but it's not much harm to repeat myself.
                if(available_to_directory_cache())
                  parent()->int_directory_cached_handle_path_changed(BOOST_AFIO_V2_NAMESPACE::path(), newpath, shared_from_this());
            }
            if(!!(flags() & file_flags::hold_parent_open) && !(flags() & file_flags::int_hold_parent_open_nested))
              dirh=int_verifymyinode();
        }
        ~async_io_handle_windows()
        {
            async_io_handle_windows::close();
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC directory_entry direntry(metadata_flags wanted) override final
        {
            windows_nt_kernel::init();
            using namespace windows_nt_kernel;
            stat_t stat(nullptr);
            IO_STATUS_BLOCK isb={ 0 };
            NTSTATUS ntstat;

            HANDLE h=myid;
            BOOST_AFIO_TYPEALIGNMENT(8) path::value_type buffer[sizeof(FILE_ALL_INFORMATION)/sizeof(path::value_type)+32769];
            buffer[0]=0;
            FILE_ALL_INFORMATION &fai=*(FILE_ALL_INFORMATION *)buffer;
            FILE_FS_SECTOR_SIZE_INFORMATION ffssi={0};
            bool needInternal=!!(wanted&metadata_flags::ino);
            bool needBasic=(!!(wanted&metadata_flags::type) || !!(wanted&metadata_flags::atim) || !!(wanted&metadata_flags::mtim) || !!(wanted&metadata_flags::ctim) || !!(wanted&metadata_flags::birthtim) || !!(wanted&metadata_flags::sparse) || !!(wanted&metadata_flags::compressed) || !!(wanted&metadata_flags::reparse_point));
            bool needStandard=(!!(wanted&metadata_flags::nlink) || !!(wanted&metadata_flags::size) || !!(wanted&metadata_flags::allocated) || !!(wanted&metadata_flags::blocks));
            // It's not widely known that the NT kernel supplies a stat() equivalent i.e. get me everything in a single syscall
            // However fetching FileAlignmentInformation which comes with FILE_ALL_INFORMATION is slow as it touches the device driver,
            // so only use if we need more than one item
            if(((int) needInternal+(int) needBasic+(int) needStandard)>=2)
            {
                ntstat=NtQueryInformationFile(h, &isb, &fai, sizeof(buffer), FileAllInformation);
                if(STATUS_PENDING==ntstat)
                    ntstat=NtWaitForSingleObject(h, FALSE, NULL);
                BOOST_AFIO_ERRHNTFN(ntstat, [this]{return path();});
            }
            else
            {
                if(needInternal)
                {
                    ntstat=NtQueryInformationFile(h, &isb, &fai.InternalInformation, sizeof(fai.InternalInformation), FileInternalInformation);
                    if(STATUS_PENDING==ntstat)
                        ntstat=NtWaitForSingleObject(h, FALSE, NULL);
                    BOOST_AFIO_ERRHNTFN(ntstat, [this]{return path();});
                }
                if(needBasic)
                {
                    ntstat=NtQueryInformationFile(h, &isb, &fai.BasicInformation, sizeof(fai.BasicInformation), FileBasicInformation);
                    if(STATUS_PENDING==ntstat)
                        ntstat=NtWaitForSingleObject(h, FALSE, NULL);
                    BOOST_AFIO_ERRHNTFN(ntstat, [this]{return path();});
                }
                if(needStandard)
                {
                    ntstat=NtQueryInformationFile(h, &isb, &fai.StandardInformation, sizeof(fai.StandardInformation), FileStandardInformation);
                    if(STATUS_PENDING==ntstat)
                        ntstat=NtWaitForSingleObject(h, FALSE, NULL);
                    BOOST_AFIO_ERRHNTFN(ntstat, [this]{return path();});
                }
            }
            if(!!(wanted&metadata_flags::blocks) || !!(wanted&metadata_flags::blksize))
            {
                ntstat=NtQueryVolumeInformationFile(h, &isb, &ffssi, sizeof(ffssi), FileFsSectorSizeInformation);
                if(STATUS_PENDING==ntstat)
                    ntstat=NtWaitForSingleObject(h, FALSE, NULL);
                //BOOST_AFIO_ERRHNTFN(ntstat, [this]{return path();});
                if(0/*STATUS_SUCCESS*/!=ntstat)
                {
                    // Windows XP and Vista don't support the FILE_FS_SECTOR_SIZE_INFORMATION
                    // API, so we'll just hardcode 512 bytes
                    ffssi.PhysicalBytesPerSectorForPerformance=512;
                }
            }
            if(!!(wanted&metadata_flags::ino)) { stat.st_ino=fai.InternalInformation.IndexNumber.QuadPart; }
            if(!!(wanted&metadata_flags::type))
            {
              ULONG ReparsePointTag=fai.EaInformation.ReparsePointTag;
              // We need to get its reparse tag to see if it's a symlink
              if(fai.BasicInformation.FileAttributes&FILE_ATTRIBUTE_REPARSE_POINT && !ReparsePointTag)
              {
                BOOST_AFIO_TYPEALIGNMENT(8) char buffer[sizeof(REPARSE_DATA_BUFFER)+32769];
                DWORD written=0;
                REPARSE_DATA_BUFFER *rpd=(REPARSE_DATA_BUFFER *) buffer;
                memset(rpd, 0, sizeof(*rpd));
                BOOST_AFIO_ERRHWINFN(DeviceIoControl(h, FSCTL_GET_REPARSE_POINT, NULL, 0, rpd, sizeof(buffer), &written, NULL), [this]{return path();});
                ReparsePointTag=rpd->ReparseTag;
              }
              stat.st_type=windows_nt_kernel::to_st_type(fai.BasicInformation.FileAttributes, ReparsePointTag);
            }
            if(!!(wanted&metadata_flags::nlink)) { stat.st_nlink=(int16_t) fai.StandardInformation.NumberOfLinks; }
            if(!!(wanted&metadata_flags::atim)) { stat.st_atim=to_timepoint(fai.BasicInformation.LastAccessTime); }
            if(!!(wanted&metadata_flags::mtim)) { stat.st_mtim=to_timepoint(fai.BasicInformation.LastWriteTime); }
            if(!!(wanted&metadata_flags::ctim)) { stat.st_ctim=to_timepoint(fai.BasicInformation.ChangeTime); }
            if(!!(wanted&metadata_flags::size)) { stat.st_size=fai.StandardInformation.EndOfFile.QuadPart; }
            if(!!(wanted&metadata_flags::allocated)) { stat.st_allocated=fai.StandardInformation.AllocationSize.QuadPart; }
            if(!!(wanted&metadata_flags::blocks)) { stat.st_blocks=fai.StandardInformation.AllocationSize.QuadPart/ffssi.PhysicalBytesPerSectorForPerformance; }
            if(!!(wanted&metadata_flags::blksize)) { stat.st_blksize=(uint16_t) ffssi.PhysicalBytesPerSectorForPerformance; }
            if(!!(wanted&metadata_flags::birthtim)) { stat.st_birthtim=to_timepoint(fai.BasicInformation.CreationTime); }
            if(!!(wanted&metadata_flags::sparse)) { stat.st_sparse=!!(fai.BasicInformation.FileAttributes & FILE_ATTRIBUTE_SPARSE_FILE); }
            if(!!(wanted&metadata_flags::compressed)) { stat.st_compressed=!!(fai.BasicInformation.FileAttributes & FILE_ATTRIBUTE_COMPRESSED); }
            if(!!(wanted&metadata_flags::reparse_point)) { stat.st_reparse_point=!!(fai.BasicInformation.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT); }
            return directory_entry(const_cast<async_io_handle_windows *>(this)->path(true).filename().native(), stat, wanted);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC BOOST_AFIO_V2_NAMESPACE::path target() override final
        {
            if(!opened_as_symlink())
                return path();
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
                return BOOST_AFIO_V2_NAMESPACE::path(path::string_type(rpd->MountPointReparseBuffer.PathBuffer+rpd->MountPointReparseBuffer.SubstituteNameOffset/sizeof(BOOST_AFIO_V2_NAMESPACE::path::value_type), rpd->MountPointReparseBuffer.SubstituteNameLength/sizeof(BOOST_AFIO_V2_NAMESPACE::path::value_type)), BOOST_AFIO_V2_NAMESPACE::path::direct());
            case IO_REPARSE_TAG_SYMLINK:
                return BOOST_AFIO_V2_NAMESPACE::path(path::string_type(rpd->SymbolicLinkReparseBuffer.PathBuffer+rpd->SymbolicLinkReparseBuffer.SubstituteNameOffset/sizeof(BOOST_AFIO_V2_NAMESPACE::path::value_type), rpd->SymbolicLinkReparseBuffer.SubstituteNameLength/sizeof(BOOST_AFIO_V2_NAMESPACE::path::value_type)), BOOST_AFIO_V2_NAMESPACE::path::direct());
            }
            BOOST_AFIO_THROW(system_error(EINVAL, generic_category()));
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::unique_ptr<mapped_file> map_file(size_t length, off_t offset, bool read_only) override final
        {
            if(length==(size_t)-1)
              length=0;
            if(!sectionh)
            {
              lock_guard<pathlock_t> g(pathlock);
              if(!sectionh)
              {
                HANDLE h;
                DWORD prot=!!(flags() & file_flags::write) ? PAGE_READWRITE : PAGE_READONLY;
                if(INVALID_HANDLE_VALUE!=(h=CreateFileMapping(myid, NULL, prot, 0, 0, nullptr)))
                  sectionh=h;
              }
            }
            if(sectionh)
            {
              DWORD prot=FILE_MAP_READ;
              if(!read_only && !!(flags() & file_flags::write))
                prot=FILE_MAP_WRITE;
              void *mapaddr=nullptr;
              if((mapaddr=MapViewOfFile(sectionh, prot, (DWORD)(offset>>32), (DWORD)(offset&0xffffffff), length)))
              {
                return detail::make_unique<mapped_file>(shared_from_this(), mapaddr, length, offset);
              }
            }
            return nullptr;
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC void link(const path_req &_req) override final
        {
            if(!myid)
              BOOST_AFIO_THROW(std::invalid_argument("Currently cannot hard link a closed file by handle."));
            path_req req(_req);
            auto newdirh=decode_relative_path<async_file_io_dispatcher_windows, async_io_handle_windows>(req);
            windows_nt_kernel::init();
            using namespace windows_nt_kernel;
            IO_STATUS_BLOCK isb={ 0 };
            BOOST_AFIO_TYPEALIGNMENT(8) path::value_type buffer[32769];
            FILE_LINK_INFORMATION *fni=(FILE_LINK_INFORMATION *) buffer;
            fni->ReplaceIfExists=false;
            fni->RootDirectory=newdirh ? newdirh->native_handle() : nullptr;
            fni->FileNameLength=(ULONG)(req.path.native().size()*sizeof(path::value_type));
            memcpy(fni->FileName, req.path.c_str(), fni->FileNameLength);
            BOOST_AFIO_ERRHNTFN(NtSetInformationFile(myid, &isb, fni, sizeof(FILE_LINK_INFORMATION)+fni->FileNameLength, FileLinkInformation), [this]{return path();});
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC void unlink() override final
        {
            if(!myid)
              BOOST_AFIO_THROW(std::invalid_argument("Currently cannot unlink a closed file by handle."));
            windows_nt_kernel::init();
            using namespace windows_nt_kernel;
#if 0
            // Despite what is claimed on the internet, NtDeleteFile does NOT delete the file immediately.
            // Moreover, it cannot delete directories nor junction points, so we'll go the more proper path.
            OBJECT_ATTRIBUTES oa={sizeof(OBJECT_ATTRIBUTES)};
            oa.RootDirectory=myid;
            UNICODE_STRING _path;
            path::value_type c=0;
            _path.Buffer=&c;
            _path.MaximumLength=(_path.Length=0)+sizeof(path::value_type);
            oa.ObjectName=&_path;
            //if(opened_as_symlink())
            //  oa.Attributes=0x100/*OBJ_OPENLINK*/;  // Seems to dislike deleting junctions points without this
            NTSTATUS ntstat=NtDeleteFile(&oa);
            // Returns this error if we are deleting a junction point
            if(ntstat!=0xC0000278/*STATUS_IO_REPARSE_DATA_INVALID*/)
              BOOST_AFIO_ERRHNTFN(ntstat, [this]{return path();});
#else
            IO_STATUS_BLOCK isb={ 0 };
            BOOST_AFIO_TYPEALIGNMENT(8) path::value_type buffer[32769+sizeof(FILE_NAME_INFORMATION)/sizeof(path::value_type)];
            // This is done in two steps to stop annoying temporary failures
            // Firstly, get where I am within my volume, NtQueryObject returns too much so simply fetch my current name
            // Then try to rename myself to the closest to the root as possible with a .afiodXXXXX crypto random name
            // If I fail to rename myself there, try the next directory up, usually at some point I'll find some directory
            // I'm allowed write to
            FILE_NAME_INFORMATION *fni=(FILE_NAME_INFORMATION *) buffer;
            BOOST_AFIO_V2_NAMESPACE::path mypath;
            bool success=false;
            do
            {
              auto _randomname(".afiod"+utils::random_string(32 /* 128 bits */));
              filesystem::path::string_type randomname(_randomname.begin(), _randomname.end());
              mypath=path(true);
              BOOST_AFIO_ERRHNTFN(NtQueryInformationFile(myid, &isb, fni, sizeof(buffer), FileNameInformation), [this]{return path(); });
              filesystem::path myloc(filesystem::path::string_type(fni->FileName, fni->FileNameLength/sizeof(path::value_type))), prefix(mypath.native());
              const_cast<filesystem::path::string_type &>(prefix.native()).resize(prefix.native().size()-myloc.native().size());
              auto try_rename=[&]{
                FILE_RENAME_INFORMATION *fri=(FILE_RENAME_INFORMATION *) buffer;
                fri->ReplaceIfExists=false;
                fri->RootDirectory=nullptr;  // rename to the same directory
                size_t n=prefix.make_preferred().native().size();
                memcpy(fri->FileName, prefix.c_str(), prefix.native().size()*sizeof(path::value_type));
                fri->FileName[n++]='\\';
                memcpy(fri->FileName+n, randomname.c_str(), randomname.size()*sizeof(path::value_type));
                n+=randomname.size();
                fri->FileName[n]=0;
                fri->FileNameLength=(ULONG) (n*sizeof(path::value_type));
                NTSTATUS ntstat=NtSetInformationFile(myid, &isb, fri, sizeof(FILE_RENAME_INFORMATION)+fri->FileNameLength, FileRenameInformation);
                // Access denied may come back if we can't rename to the location or we are renaming a directory
                // and that directory contains open files. We can't tell the difference, so keep going
                if(!ntstat)
                  success=true;
#if 0
                else
                  std::wcout << "unlink() failed to rename to " << fri->FileName << " due to " << ntstat << std::endl;
#endif
                return ntstat;
              };
              if(try_rename())
              {
                myloc=myloc.parent_path();
                for(auto &fragment : myloc)
                {
                  prefix/=fragment;
                  if(fragment.native().size()>1 && !try_rename())
                    break;
                }
              }
            } while(!success && mypath!=path(true));
            // By this point maybe the file/directory was renamed, maybe it was not.
            FILE_BASIC_INFORMATION fbi={ 0 };
            fbi.FileAttributes=FILE_ATTRIBUTE_HIDDEN;
            NtSetInformationFile(myid, &isb, &fbi, sizeof(fbi), FileBasicInformation);
            FILE_DISPOSITION_INFORMATION fdi={ true };
            BOOST_AFIO_ERRHNTFN(NtSetInformationFile(myid, &isb, &fdi, sizeof(fdi), FileDispositionInformation), [this]{return path();});
#endif
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC void atomic_relink(const path_req &_req) override final
        {
            if(!myid)
              BOOST_AFIO_THROW(std::invalid_argument("Currently cannot relink a closed file by handle."));
            path_req req(_req);
            auto newdirh=decode_relative_path<async_file_io_dispatcher_windows, async_io_handle_windows>(req);
            windows_nt_kernel::init();
            using namespace windows_nt_kernel;
            IO_STATUS_BLOCK isb={ 0 };
            BOOST_AFIO_TYPEALIGNMENT(8) path::value_type buffer[32769];
            FILE_RENAME_INFORMATION *fni=(FILE_RENAME_INFORMATION *) buffer;
            fni->ReplaceIfExists=true;
            fni->RootDirectory=newdirh ? newdirh->native_handle() : nullptr;
            fni->FileNameLength=(ULONG)(req.path.native().size()*sizeof(path::value_type));
            memcpy(fni->FileName, req.path.c_str(), fni->FileNameLength);
            BOOST_AFIO_ERRHNTFN(NtSetInformationFile(myid, &isb, fni, sizeof(FILE_RENAME_INFORMATION)+fni->FileNameLength, FileRenameInformation), [this]{return path();});
        }
    };
    inline async_io_handle_windows::async_io_handle_windows(dispatcher *_parent,
      const BOOST_AFIO_V2_NAMESPACE::path &path, file_flags flags, bool _SyncOnClose, HANDLE _h)
      : handle(_parent, flags),
        h(new asio::windows::random_access_handle(_parent->p->pool->io_service(), int_checkHandle(_h, path))), myid(_h),
        has_been_added(false), SyncOnClose(_SyncOnClose), sectionh(nullptr), _path(path)
    {
      if(!!(flags & file_flags::os_lockable))
        lockfile=process_lockfile_registry::open<win_lock_file>(this);
    }

    class async_file_io_dispatcher_windows : public dispatcher
    {
        template<class Impl, class Handle> friend handle_ptr detail::decode_relative_path(path_req &req, bool force_absolute);
        friend class dispatcher;
        friend class directory_entry;
        friend void directory_entry::_int_fetch(metadata_flags wanted, handle_ptr dirh);
        friend struct async_io_handle_windows;
        size_t pagesize;
        handle_ptr decode_relative_path(path_req &req, bool force_absolute=false)
        {
          return detail::decode_relative_path<async_file_io_dispatcher_windows, async_io_handle_windows>(req, force_absolute);
        }

        // Called in unknown thread
        completion_returntype dodir(size_t id, future<> op, path_req req)
        {
            req.flags=fileflags(req.flags)|file_flags::int_opening_dir|file_flags::read;
            // TODO FIXME: Currently file_flags::create may duplicate handles in the dirhcache
            if(!(req.flags & file_flags::unique_directory_handle) && !!(req.flags & file_flags::read) && !(req.flags & file_flags::write) && !(req.flags & (file_flags::create|file_flags::create_only_if_not_exist)))
            {
              path_req req2(req);
              // Return a copy of the one in the dir cache if available as a fast path
              decode_relative_path(req2, true);
              try
              {
                return std::make_pair(true, p->get_handle_to_dir(this, id, req2, &async_file_io_dispatcher_windows::dofile));
              }
              catch(...)
              {
                // fall through
              }
            }
            // This will add itself to the dir cache if it's eligible
            return dofile(id, op, req);
        }
        // Called in unknown thread
        completion_returntype dounlink(bool is_dir, size_t id, future<> op, path_req req)
        {
            req.flags=fileflags(req.flags);
            // Deleting the input op?
            if(req.path.empty())
            {
              handle_ptr h(op.get_handle());
              async_io_handle_windows *p=static_cast<async_io_handle_windows *>(h.get());
              if(p->is_open()!=handle::open_states::closed)
              {
                p->unlink();
                return std::make_pair(true, op.get_handle());
              }
            }
            windows_nt_kernel::init();
            using namespace windows_nt_kernel;
            auto dirh=decode_relative_path(req);
#if 0
            OBJECT_ATTRIBUTES oa={sizeof(OBJECT_ATTRIBUTES)};
            oa.RootDirectory=dirh ? dirh->native_handle() : nullptr;
            UNICODE_STRING _path;
            // If relative path, or symlinked DOS path, use case insensitive
            bool isSymlinkedDosPath=(req.path.native().size()>3 && req.path.native()[0]=='\\' && req.path.native()[1]=='?' && req.path.native()[2]=='?' && req.path.native()[3]=='\\');
            oa.Attributes=(req.path.native()[0]!='\\' || isSymlinkedDosPath) ? 0x40/*OBJ_CASE_INSENSITIVE*/ : 0;
            _path.Buffer=const_cast<path::value_type *>(req.path.c_str());
            _path.MaximumLength=(_path.Length=(USHORT) (req.path.native().size()*sizeof(path::value_type)))+sizeof(path::value_type);
            oa.ObjectName=&_path;
            BOOST_AFIO_ERRHNTFN(NtDeleteFile(&oa), [&req]{return req.path;});
#else
            // Open the file with DeleteOnClose, renaming it before closing the handle.
            NTSTATUS ntstat;
            HANDLE temph;
            req.flags=req.flags|file_flags::delete_on_close|file_flags::int_file_share_delete;
            std::tie(ntstat, temph)=ntcreatefile(dirh, req.path, req.flags, false);
            BOOST_AFIO_ERRHNTFN(ntstat, [&req]{return req.path;});
            auto untemph=detail::Undoer([&temph]{if(temph) CloseHandle(temph);});
            IO_STATUS_BLOCK isb={ 0 };
            BOOST_AFIO_TYPEALIGNMENT(8) path::value_type buffer[32769];
            FILE_RENAME_INFORMATION *fni=(FILE_RENAME_INFORMATION *) buffer;
            fni->ReplaceIfExists=false;
            fni->RootDirectory=nullptr;  // same directory
            auto randompath(".afiod"+utils::random_string(32 /* 128 bits */));
            fni->FileNameLength=(ULONG)(randompath.size()*sizeof(path::value_type));
            for(size_t n=0; n<randompath.size(); n++)
              fni->FileName[n]=randompath[n];
            fni->FileName[randompath.size()]=0;
            ntstat=NtSetInformationFile(temph, &isb, fni, sizeof(FILE_RENAME_INFORMATION)+fni->FileNameLength, FileRenameInformation);
            // Access denied may come back if the directory contains any files with open handles
            // If that happens, ignore and mark to delete anyway
            //if(ntstat==0xC0000022/*STATUS_ACCESS_DENIED*/)
            FILE_BASIC_INFORMATION fbi={ 0 };
            fbi.FileAttributes=FILE_ATTRIBUTE_HIDDEN;
            NtSetInformationFile(temph, &isb, &fbi, sizeof(fbi), FileBasicInformation);
            FILE_DISPOSITION_INFORMATION fdi={ true };
            BOOST_AFIO_ERRHNTFN(NtSetInformationFile(temph, &isb, &fdi, sizeof(fdi), FileDispositionInformation), [this]{return path(); });
#endif
            return std::make_pair(true, op.get_handle());
        }
        // Called in unknown thread
        completion_returntype dormdir(size_t id, future<> op, path_req req)
        {
          req.flags=fileflags(req.flags)|file_flags::int_opening_dir;
          return dounlink(true, id, std::move(op), std::move(req));
        }
      public:
      private:
        // Called in unknown thread
        completion_returntype dofile(size_t id, future<> op, path_req req)
        {
            NTSTATUS status=0;
            HANDLE h=nullptr;
            req.flags=fileflags(req.flags);
            handle_ptr dirh=decode_relative_path(req);
            std::tie(status, h)=ntcreatefile(dirh, req.path, req.flags);
            if(dirh)
              req.path=dirh->path()/req.path;
            BOOST_AFIO_ERRHNTFN(status, [&req]{return req.path;});
            // If writing and SyncOnClose and NOT synchronous, turn on SyncOnClose
            auto ret=std::make_shared<async_io_handle_windows>(this, req.path, req.flags,
                (file_flags::sync_on_close|file_flags::write)==(req.flags & (file_flags::sync_on_close|file_flags::write|file_flags::always_sync)),
                h);
            static_cast<async_io_handle_windows *>(ret.get())->do_add_io_handle_to_parent();
            // If creating a file or directory or link and he wants compression, try that now
            if(!!(req.flags & file_flags::create_compressed) && (!!(req.flags & file_flags::create_only_if_not_exist) || !!(req.flags & file_flags::create)))
            {
              DWORD bytesout=0;
              USHORT val=COMPRESSION_FORMAT_DEFAULT;
              BOOST_AFIO_ERRHWINFN(DeviceIoControl(ret->native_handle(), FSCTL_SET_COMPRESSION, &val, sizeof(val), nullptr, 0, &bytesout, nullptr), [&req]{return req.path;});
            }
            if(!(req.flags & file_flags::int_opening_dir) && !(req.flags & file_flags::int_opening_link))
            {
              // If opening existing file for write, try to convert to sparse, ignoring any failures
              if(!(req.flags & file_flags::no_sparse) && !!(req.flags & file_flags::write))
              {
#if defined(__MINGW32__) && !defined(__MINGW64__) && !defined(__MINGW64_VERSION_MAJOR)
                // Mingw32 currently lacks the FILE_SET_SPARSE_BUFFER structure
                typedef struct _FILE_SET_SPARSE_BUFFER {
                  BOOLEAN SetSparse;
                } FILE_SET_SPARSE_BUFFER, *PFILE_SET_SPARSE_BUFFER;
#endif
                DWORD bytesout=0;
                FILE_SET_SPARSE_BUFFER fssb={true};
                DeviceIoControl(ret->native_handle(), FSCTL_SET_SPARSE, &fssb, sizeof(fssb), nullptr, 0, &bytesout, nullptr);
              }
            }
            return std::make_pair(true, ret);
        }
        // Called in unknown thread
        completion_returntype dormfile(size_t id, future<> op, path_req req)
        {
          return dounlink(true, id, std::move(op), std::move(req));
        }
        // Called in unknown thread
        void boost_asio_symlink_completion_handler(size_t id, handle_ptr h, std::shared_ptr<std::unique_ptr<path::value_type[]>> buffer, const error_code &ec, size_t bytes_transferred)
        {
            if(ec)
            {
                exception_ptr e;
                // boost::system::system_error makes no attempt to ask windows for what the error code means :(
                try
                {
                    BOOST_AFIO_ERRGWINFN(ec.value(), [h]{return h->path();});
                }
                catch(...)
                {
                    e=current_exception();
                }
                complete_async_op(id, h, e);
            }
            else
            {
                complete_async_op(id, h);
            }
        }
        // Called in unknown thread
        completion_returntype dosymlink(size_t id, future<> op, path_req req)
        {
            handle_ptr h(op.get_handle());
            async_io_handle_windows *p=static_cast<async_io_handle_windows *>(h.get());
            req.flags=fileflags(req.flags);
            req.flags=req.flags|file_flags::int_opening_link;
            // For safety, best not create unless doesn't exist
            if(!!(req.flags&file_flags::create))
            {
                req.flags=req.flags&~file_flags::create;
                req.flags=req.flags|file_flags::create_only_if_not_exist;
            }
            // If not creating, simply open
            if(!(req.flags&file_flags::create_only_if_not_exist))
                return dofile(id, op, req);
            windows_nt_kernel::init();
            using namespace windows_nt_kernel;
            using windows_nt_kernel::REPARSE_DATA_BUFFER;
            // Our new object needs write access and if we're linking a directory, create a directory
            req.flags=req.flags|file_flags::write;
            if(!!(h->flags()&file_flags::int_opening_dir))
              req.flags=req.flags|file_flags::int_opening_dir;
            completion_returntype ret=dofile(id, op, req);
            assert(ret.first);
            path destpath(h->path(true));
            size_t destpathbytes=destpath.native().size()*sizeof(path::value_type);
            size_t buffersize=sizeof(REPARSE_DATA_BUFFER)+destpathbytes*2+256;
            auto buffer=std::make_shared<std::unique_ptr<path::value_type[]>>(new path::value_type[buffersize]);
            REPARSE_DATA_BUFFER *rpd=(REPARSE_DATA_BUFFER *) buffer->get();
            memset(rpd, 0, sizeof(*rpd));
            // Create a directory junction for directories and symbolic links for files
            if(!!(h->flags()&file_flags::int_opening_dir))
              rpd->ReparseTag=IO_REPARSE_TAG_MOUNT_POINT;
            else
              rpd->ReparseTag=IO_REPARSE_TAG_SYMLINK;
            if(destpath.native().size()>5 && isalpha(destpath.native()[4]) && destpath.native()[5]==':')
            {
                memcpy(rpd->MountPointReparseBuffer.PathBuffer, destpath.c_str(), destpathbytes+sizeof(path::value_type));
                rpd->MountPointReparseBuffer.SubstituteNameOffset=0;
                rpd->MountPointReparseBuffer.SubstituteNameLength=(USHORT)destpathbytes;
                rpd->MountPointReparseBuffer.PrintNameOffset=(USHORT)(destpathbytes+sizeof(path::value_type));
                rpd->MountPointReparseBuffer.PrintNameLength=(USHORT)(destpathbytes-4);
                memcpy(rpd->MountPointReparseBuffer.PathBuffer+rpd->MountPointReparseBuffer.PrintNameOffset/sizeof(path::value_type), destpath.c_str()+4, rpd->MountPointReparseBuffer.PrintNameLength+sizeof(path::value_type));
            }
            else
            {
                memcpy(rpd->MountPointReparseBuffer.PathBuffer, destpath.c_str(), destpathbytes+sizeof(path::value_type));
                rpd->MountPointReparseBuffer.SubstituteNameOffset=0;
                rpd->MountPointReparseBuffer.SubstituteNameLength=(USHORT)destpathbytes;
                rpd->MountPointReparseBuffer.PrintNameOffset=(USHORT)(destpathbytes+sizeof(path::value_type));
                rpd->MountPointReparseBuffer.PrintNameLength=(USHORT)destpathbytes;
                memcpy(rpd->MountPointReparseBuffer.PathBuffer+rpd->MountPointReparseBuffer.PrintNameOffset/sizeof(path::value_type), h->path().c_str(), rpd->MountPointReparseBuffer.PrintNameLength+sizeof(path::value_type));
            }
            size_t headerlen=offsetof(REPARSE_DATA_BUFFER, MountPointReparseBuffer);
            size_t reparsebufferheaderlen=offsetof(REPARSE_DATA_BUFFER, MountPointReparseBuffer.PathBuffer)-headerlen;
            rpd->ReparseDataLength=(USHORT)(rpd->MountPointReparseBuffer.SubstituteNameLength+rpd->MountPointReparseBuffer.PrintNameLength+2*sizeof(path::value_type)+reparsebufferheaderlen);
            auto dirop(ret.second);
            asio::windows::overlapped_ptr ol(threadsource()->io_service(), [this, id,
              BOOST_AFIO_LAMBDA_MOVE_CAPTURE(dirop),
              BOOST_AFIO_LAMBDA_MOVE_CAPTURE(buffer)](const error_code &ec, size_t bytes){ boost_asio_symlink_completion_handler(id, std::move(dirop), std::move(buffer), ec, bytes);});
            DWORD bytesout=0;
            BOOL ok=DeviceIoControl(ret.second->native_handle(), FSCTL_SET_REPARSE_POINT, rpd, (DWORD)(rpd->ReparseDataLength+headerlen), NULL, 0, &bytesout, ol.get());
            DWORD errcode=GetLastError();
            if(!ok && ERROR_IO_PENDING!=errcode)
            {
                //std::cerr << "ERROR " << errcode << std::endl;
                error_code ec(errcode, asio::error::get_system_category());
                ol.complete(ec, ol.get()->InternalHigh);
            }
            else
                ol.release();
            // Indicate we're not finished yet
            return std::make_pair(false, h);
        }
        // Called in unknown thread
        completion_returntype dormsymlink(size_t id, future<> op, path_req req)
        {
          req.flags=fileflags(req.flags)|file_flags::int_opening_link;
          return dounlink(true, id, std::move(op), std::move(req));
        }
        // Called in unknown thread
        completion_returntype dozero(size_t id, future<> op, std::vector<std::pair<off_t, off_t>> ranges)
        {
#if defined(__MINGW32__) && !defined(__MINGW64__) && !defined(__MINGW64_VERSION_MAJOR)
            // Mingw32 currently lacks the FILE_ZERO_DATA_INFORMATION structure and FSCTL_SET_ZERO_DATA
            typedef struct _FILE_ZERO_DATA_INFORMATION {
              LARGE_INTEGER FileOffset;
              LARGE_INTEGER BeyondFinalZero;
            } FILE_ZERO_DATA_INFORMATION, *PFILE_ZERO_DATA_INFORMATION;
#define FSCTL_SET_ZERO_DATA             CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 50, METHOD_BUFFERED, FILE_WRITE_DATA)
#endif
            handle_ptr h(op.get_handle());
            async_io_handle_windows *p=static_cast<async_io_handle_windows *>(h.get());
            assert(p);
            auto buffers=std::make_shared<std::vector<FILE_ZERO_DATA_INFORMATION>>();
            buffers->reserve(ranges.size());
            off_t bytes=0;
            for(auto &i : ranges)
            {
              FILE_ZERO_DATA_INFORMATION fzdi;
              fzdi.FileOffset.QuadPart=i.first;
              fzdi.BeyondFinalZero.QuadPart=i.first+i.second;
              buffers->push_back(std::move(fzdi));
              bytes+=i.second;
            }
            auto bytes_to_transfer=std::make_shared<std::pair<atomic<bool>, atomic<off_t>>>();
            bytes_to_transfer->first=false;
            bytes_to_transfer->second=bytes;
            auto completion_handler=[this, id, h, bytes_to_transfer, buffers](const error_code &ec, size_t bytes, off_t thisbytes)
            {
              if(ec)
              {
                exception_ptr e;
                // boost::system::system_error makes no attempt to ask windows for what the error code means :(
                try
                {
                  BOOST_AFIO_ERRGWINFN(ec.value(), [h]{return h->path();});
                }
                catch(...)
                {
                  e=current_exception();
                  bool exp=false;
                  // If someone else has already returned an error, simply exit
                  if(!bytes_to_transfer->first.compare_exchange_strong(exp, true))
                    return;
                }
                complete_async_op(id, h, e);
              }
              else if(!(bytes_to_transfer->second-=thisbytes))
              {
                complete_async_op(id, h);
              }
            };
            for(auto &i : *buffers)
            {
              asio::windows::overlapped_ptr ol(threadsource()->io_service(), std::bind(completion_handler, std::placeholders::_1, std::placeholders::_2, i.BeyondFinalZero.QuadPart-i.FileOffset.QuadPart));
              DWORD bytesout=0;
              BOOL ok=DeviceIoControl(p->native_handle(), FSCTL_SET_ZERO_DATA, &i, sizeof(i), nullptr, 0, &bytesout, ol.get());
              DWORD errcode=GetLastError();
              if(!ok && ERROR_IO_PENDING!=errcode)
              {
                //std::cerr << "ERROR " << errcode << std::endl;
                error_code ec(errcode, asio::error::get_system_category());
                ol.complete(ec, ol.get()->InternalHigh);
              }
              else
                ol.release();
            }
            // Indicate we're not finished yet
            return std::make_pair(false, h);
        }
        // Called in unknown thread
        completion_returntype dosync(size_t id, future<> op, future<>)
        {
          handle_ptr h(op.get_handle());
          async_io_handle_windows *p=static_cast<async_io_handle_windows *>(h.get());
          assert(p);
          off_t bytestobesynced=p->write_count_since_fsync();
          if(bytestobesynced)
            BOOST_AFIO_ERRHWINFN(FlushFileBuffers(p->native_handle()), [p]{return p->path();});
          p->byteswrittenatlastfsync+=bytestobesynced;
          return std::make_pair(true, h);
        }
        // Called in unknown thread
        completion_returntype doclose(size_t id, future<> op, future<>)
        {
            handle_ptr h(op.get_handle());
            async_io_handle_windows *p=static_cast<async_io_handle_windows *>(h.get());
            assert(p);
            if(!!(p->flags() & file_flags::int_opening_dir) && !(p->flags() & file_flags::unique_directory_handle) && !!(p->flags() & file_flags::read) && !(p->flags() & file_flags::write))
            {
                // As this is a directory which may be a fast directory enumerator, ignore close
            }
            else
            {
                p->close();
            }
            return std::make_pair(true, h);
        }
        // Called in unknown thread
        void boost_asio_readwrite_completion_handler(bool is_write, size_t id, handle_ptr h, std::shared_ptr<std::tuple<atomic<bool>, atomic<size_t>, detail::io_req_impl<true>>> bytes_to_transfer, std::tuple<off_t, size_t, size_t, size_t> pars, const error_code &ec, size_t bytes_transferred)
        {
            if(!this->p->filters_buffers.empty())
            {
                for(auto &i: this->p->filters_buffers)
                {
                    if(i.first==OpType::Unknown || (!is_write && i.first==OpType::read) || (is_write && i.first==OpType::write))
                    {
                        i.second(is_write ? OpType::write : OpType::read, h.get(), std::get<2>(*bytes_to_transfer), std::get<0>(pars), std::get<1>(pars), std::get<2>(pars), ec, bytes_transferred);
                    }
                }
            }
            if(ec)
            {
                exception_ptr e;
                // boost::system::system_error makes no attempt to ask windows for what the error code means :(
                try
                {
                    BOOST_AFIO_ERRGWINFN(ec.value(), [h]{return h->path();});
                }
                catch(...)
                {
                    e=current_exception();
                    bool exp=false;
                    // If someone else has already returned an error, simply exit
                    if(!std::get<0>(*bytes_to_transfer).compare_exchange_strong(exp, true))
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
                size_t togo=(std::get<1>(*bytes_to_transfer)-=std::get<3>(pars));
                if(!togo) // bytes_this_chunk may not equal bytes_transferred if final 4Kb chunk of direct file
                    complete_async_op(id, h);
                if(togo>((size_t)1<<(8*sizeof(size_t)-1)))
                    BOOST_AFIO_THROW_FATAL(std::runtime_error("IOCP returned more bytes than we asked for. This is probably memory corruption."));
                BOOST_AFIO_DEBUG_PRINT("H %u e=%u togo=%u bt=%u bc=%u\n", (unsigned) id, (unsigned) ec.value(), (unsigned) togo, (unsigned) bytes_transferred, (unsigned) std::get<3>(pars));
            }
            //std::cout << "id=" << id << " total=" << bytes_to_transfer->second << " this=" << bytes_transferred << std::endl;
        }
        template<bool iswrite> void doreadwrite(size_t id, handle_ptr h, detail::io_req_impl<iswrite> req, async_io_handle_windows *p)
        {
            // asio::async_read_at() seems to have a bug and only transfers 64Kb per buffer
            // asio::windows::random_access_handle::async_read_some_at() clearly bothers
            // with the first buffer only. Same goes for both async write functions.
            //
            // So we implement by hand and skip ASIO altogether.
            size_t amount=0;
            for(auto &b: req.buffers)
            {
                amount+=asio::buffer_size(b);
            }
            auto bytes_to_transfer=std::make_shared<std::tuple<atomic<bool>, atomic<size_t>, detail::io_req_impl<true>>>();
            //mingw choked on atomic<T>::operator=, thought amount was atomic&, so changed to store to avoid issue
            std::get<1>(*bytes_to_transfer).store(amount);
            std::get<2>(*bytes_to_transfer)=req;
            // Are we using direct i/o, because then we get the magic scatter/gather special functions?
            if(!!(p->flags() & file_flags::os_direct))
            {
                // Yay we can use the direct i/o scatter gather functions which are far more efficient
                size_t pages=amount/pagesize, thisbufferoffset;
                std::vector<FILE_SEGMENT_ELEMENT> elems(1+pages);
                auto bufferit=req.buffers.begin();
                thisbufferoffset=0;
                for(size_t n=0; n<pages; n++)
                {
                    // Be careful of 32 bit void * sign extension here ...
                    elems[n].Alignment=((size_t) asio::buffer_cast<const void *>(*bufferit))+thisbufferoffset;
                    thisbufferoffset+=pagesize;
                    if(thisbufferoffset>=asio::buffer_size(*bufferit))
                    {
                        ++bufferit;
                        thisbufferoffset=0;
                    }
                }
                elems[pages].Alignment=0;
                auto t(std::make_tuple(req.where, 0, req.buffers.size(), amount));
                asio::windows::overlapped_ptr ol(threadsource()->io_service(), [this, id, h, bytes_to_transfer,
                  BOOST_AFIO_LAMBDA_MOVE_CAPTURE(t)](const error_code &ec, size_t bytes) {
                    boost_asio_readwrite_completion_handler(iswrite, id, std::move(h),
                      bytes_to_transfer, std::move(t), ec, bytes); });
                ol.get()->Offset=(DWORD) (req.where & 0xffffffff);
                ol.get()->OffsetHigh=(DWORD) ((req.where>>32) & 0xffffffff);
                BOOL ok=iswrite ? WriteFileGather
                    (p->native_handle(), &elems.front(), (DWORD) amount, NULL, ol.get())
                    : ReadFileScatter
                    (p->native_handle(), &elems.front(), (DWORD) amount, NULL, ol.get());
                DWORD errcode=GetLastError();
                if(!ok && ERROR_IO_PENDING!=errcode)
                {
                    //std::cerr << "ERROR " << errcode << std::endl;
                    error_code ec(errcode, asio::error::get_system_category());
                    ol.complete(ec, ol.get()->InternalHigh);
                }
                else
                    ol.release();
            }
            else
            {
                size_t offset=0, n=0;
                for(auto &b: req.buffers)
                {
                    auto t(std::make_tuple(req.where+offset, n, 1, asio::buffer_size(b)));
                    asio::windows::overlapped_ptr ol(threadsource()->io_service(),  [this, id, h, bytes_to_transfer,
                      BOOST_AFIO_LAMBDA_MOVE_CAPTURE(t)](const error_code &ec, size_t bytes) {
                        boost_asio_readwrite_completion_handler(iswrite, id, std::move(h),
                        bytes_to_transfer, std::move(t), ec, bytes); });
                    ol.get()->Offset=(DWORD) ((req.where+offset) & 0xffffffff);
                    ol.get()->OffsetHigh=(DWORD) (((req.where+offset)>>32) & 0xffffffff);
                    DWORD bytesmoved=0;
                    BOOL ok=iswrite ? WriteFile
                        (p->native_handle(), asio::buffer_cast<const void *>(b), (DWORD) asio::buffer_size(b), &bytesmoved, ol.get())
                        : ReadFile
                        (p->native_handle(), (LPVOID) asio::buffer_cast<const void *>(b), (DWORD) asio::buffer_size(b), &bytesmoved, ol.get());
                    DWORD errcode=GetLastError();
                    if(!ok && ERROR_IO_PENDING!=errcode)
                    {
                        //std::cerr << "ERROR " << errcode << std::endl;
                        error_code ec(errcode, asio::error::get_system_category());
                        ol.complete(ec, ol.get()->InternalHigh);
                    }
                    else
                        ol.release();
                    offset+=asio::buffer_size(b);
                    n++;
                }
            }
        }
        // Called in unknown thread
        completion_returntype doread(size_t id, future<> op, detail::io_req_impl<false> req)
        {
            handle_ptr h(op.get_handle());
            async_io_handle_windows *p=static_cast<async_io_handle_windows *>(h.get());
            assert(p);
            BOOST_AFIO_DEBUG_PRINT("R %u %p (%c) @ %u, b=%u\n", (unsigned) id, h.get(), p->path().native().back(), (unsigned) req.where, (unsigned) req.buffers.size());
#ifdef DEBUG_PRINTING
            for(auto &b: req.buffers)
            {   BOOST_AFIO_DEBUG_PRINT("  R %u: %p %u\n", (unsigned) id, asio::buffer_cast<const void *>(b), (unsigned) asio::buffer_size(b)); }
#endif
            doreadwrite(id, h, req, p);
            // Indicate we're not finished yet
            return std::make_pair(false, h);
        }
        // Called in unknown thread
        completion_returntype dowrite(size_t id, future<> op, detail::io_req_impl<true> req)
        {
            handle_ptr h(op.get_handle());
            async_io_handle_windows *p=static_cast<async_io_handle_windows *>(h.get());
            assert(p);
            BOOST_AFIO_DEBUG_PRINT("W %u %p (%c) @ %u, b=%u\n", (unsigned) id, h.get(), p->path().native().back(), (unsigned) req.where, (unsigned) req.buffers.size());
#ifdef DEBUG_PRINTING
            for(auto &b: req.buffers)
            {   BOOST_AFIO_DEBUG_PRINT("  W %u: %p %u\n", (unsigned) id, asio::buffer_cast<const void *>(b), (unsigned) asio::buffer_size(b)); }
#endif
            doreadwrite(id, h, req, p);
            // Indicate we're not finished yet
            return std::make_pair(false, h);
        }
        // Called in unknown thread
        completion_returntype dotruncate(size_t id, future<> op, off_t _newsize)
        {
            handle_ptr h(op.get_handle());
            async_io_handle_windows *p=static_cast<async_io_handle_windows *>(h.get());
            assert(p);
            BOOST_AFIO_DEBUG_PRINT("T %u %p (%c)\n", (unsigned) id, h.get(), p->path().native().back());
#if 1
            BOOST_AFIO_ERRHWINFN(wintruncate(p->native_handle(), _newsize), [p]{return p->path();});
#else
            // This is a bit tricky ... overlapped files ignore their file position except in this one
            // case, but clearly here we have a race condition. No choice but to rinse and repeat I guess.
            LARGE_INTEGER size={0}, newsize={0};
            newsize.QuadPart=_newsize;
            while(size.QuadPart!=newsize.QuadPart)
            {
                BOOST_AFIO_ERRHWINFN(SetFilePointerEx(p->native_handle(), newsize, NULL, FILE_BEGIN), [p]{return p->path();});
                BOOST_AFIO_ERRHWINFN(SetEndOfFile(p->native_handle()), [p]{return p->path();});
                BOOST_AFIO_ERRHWINFN(GetFileSizeEx(p->native_handle(), &size), [p]{return p->path();});
            }
#endif
            return std::make_pair(true, h);
        }
        // Called in unknown thread
        struct enumerate_state : std::enable_shared_from_this<enumerate_state>
        {
            size_t id;
            future<> op;
            std::shared_ptr<promise<std::pair<std::vector<directory_entry>, bool>>> ret;
            std::unique_ptr<windows_nt_kernel::FILE_ID_FULL_DIR_INFORMATION[]> buffer;
            enumerate_req req;
            enumerate_state(size_t _id, future<> _op, std::shared_ptr<promise<std::pair<std::vector<directory_entry>, bool>>> _ret,
              enumerate_req _req) : id(_id), op(std::move(_op)), ret(std::move(_ret)), req(std::move(_req)) { reallocate_buffer(); }
            void reallocate_buffer()
            {
              using windows_nt_kernel::FILE_ID_FULL_DIR_INFORMATION;
              buffer=std::unique_ptr<FILE_ID_FULL_DIR_INFORMATION[]>(new FILE_ID_FULL_DIR_INFORMATION[req.maxitems]);
            }
        };
        typedef std::shared_ptr<enumerate_state> enumerate_state_t;
        void boost_asio_enumerate_completion_handler(enumerate_state_t state, const error_code &ec, size_t bytes_transferred)
        {
            using namespace windows_nt_kernel;
            size_t id=state->id;
            handle_ptr h(state->op.get_handle());
            std::shared_ptr<promise<std::pair<std::vector<directory_entry>, bool>>> &ret=state->ret;
            std::unique_ptr<FILE_ID_FULL_DIR_INFORMATION[]> &buffer=state->buffer;
            enumerate_req &req=state->req;
            if(ec && ERROR_MORE_DATA==ec.value() && bytes_transferred<(sizeof(FILE_ID_FULL_DIR_INFORMATION)+buffer.get()->FileNameLength))
            {
                // Bump maxitems by one and reschedule.
                req.maxitems++;
                state->reallocate_buffer();
                doenumerate(state);
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
                    BOOST_AFIO_ERRGWINFN(ec.value(), [h]{return h->path();});
                }
                catch(...)
                {
                    e=current_exception();
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
                bool needmoremetadata=!!(req.metadata&~item.have_metadata);
                bool done=false;
                for(FILE_ID_FULL_DIR_INFORMATION *ffdi=buffer.get(); !done; ffdi=(FILE_ID_FULL_DIR_INFORMATION *)((size_t) ffdi + ffdi->NextEntryOffset))
                {
                    if(!ffdi->NextEntryOffset) done=true;
                    size_t length=ffdi->FileNameLength/sizeof(path::value_type);
                    if(length<=2 && '.'==ffdi->FileName[0])
                        if(1==length || '.'==ffdi->FileName[1])
                            continue;
                    path::string_type leafname(ffdi->FileName, length);
                    if(req.filtering==enumerate_req::filter::fastdeleted && isDeletedFile(leafname))
                      continue;
                    item.leafname=std::move(leafname);
                    item.stat.st_ino=ffdi->FileId.QuadPart;
                    item.stat.st_type=to_st_type(ffdi->FileAttributes, ffdi->ReparsePointTag);
                    item.stat.st_atim=to_timepoint(ffdi->LastAccessTime);
                    item.stat.st_mtim=to_timepoint(ffdi->LastWriteTime);
                    item.stat.st_ctim=to_timepoint(ffdi->ChangeTime);
                    item.stat.st_size=ffdi->EndOfFile.QuadPart;
                    item.stat.st_allocated=ffdi->AllocationSize.QuadPart;
                    item.stat.st_birthtim=to_timepoint(ffdi->CreationTime);
                    item.stat.st_sparse=!!(ffdi->FileAttributes & FILE_ATTRIBUTE_SPARSE_FILE);
                    item.stat.st_compressed=!!(ffdi->FileAttributes & FILE_ATTRIBUTE_COMPRESSED);
                    item.stat.st_reparse_point=!!(ffdi->FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT);
                    _ret.push_back(std::move(item));
                }
                if(needmoremetadata)
                {
                    for(auto &i: _ret)
                    {
                      try
                      {
                        i.fetch_metadata(h, req.metadata);
                      }
                      catch(...) { } // File may have vanished between enumerate and now
                    }
                }
                ret->set_value(std::make_pair(std::move(_ret), !thisbatchdone));
                complete_async_op(id, h);
            }
        }
        // Called in unknown thread
        void doenumerate(enumerate_state_t state)
        {
            size_t id=state->id;
            handle_ptr h(state->op.get_handle());
            async_io_handle_windows *p=static_cast<async_io_handle_windows *>(h.get());
            using namespace windows_nt_kernel;
            std::unique_ptr<FILE_ID_FULL_DIR_INFORMATION[]> &buffer=state->buffer;
            enumerate_req &req=state->req;
            NTSTATUS ntstat;
            UNICODE_STRING _glob={ 0 };
            if(!req.glob.empty())
            {
                _glob.Buffer=const_cast<path::value_type *>(req.glob.c_str());
                _glob.MaximumLength=(_glob.Length=(USHORT) (req.glob.native().size()*sizeof(path::value_type)))+sizeof(path::value_type);
            }
            asio::windows::overlapped_ptr ol(threadsource()->io_service(), [this, state](const error_code &ec, size_t bytes)
            {
              if(!state)
                abort();
              boost_asio_enumerate_completion_handler(state, ec, bytes);
            });
            bool done;
            do
            {
                // 2015-01-03 ned: I've been battling this stupid memory corruption bug for a while now, and it's really a bug in NT because they're probably not
                //                 testing asynchronous direction enumeration so hence this evil workaround. Basically 32 bit kernels will corrupt memory if
                //                 ApcContext is the i/o status block, and they demand ApcContext to be null or else! Unfortunately 64 bit kernels, including
                //                 when a 32 bit process is running under WoW64, pass ApcContext not IoStatusBlock as the completion port overlapped output,
                //                 so on 64 bit kernels we have no choice and must always set ApcContext to IoStatusBlock!
                //
                //                 So, if I'm a 32 bit build, check IsWow64Process() to see if I'm really 32 bit and don't set ApcContext, else set ApcContext.
                void *ApcContext=ol.get();
#ifndef _WIN64
                BOOL isWow64=false;
                if(IsWow64Process(GetCurrentProcess(), &isWow64), !isWow64)
                  ApcContext=nullptr;
#endif
                ntstat=NtQueryDirectoryFile(p->native_handle(), ol.get()->hEvent, NULL, ApcContext, (PIO_STATUS_BLOCK) ol.get(),
                    buffer.get(), (ULONG)(sizeof(FILE_ID_FULL_DIR_INFORMATION)*req.maxitems),
                    FileIdFullDirectoryInformation, FALSE, req.glob.empty() ? NULL : &_glob, req.restart);
                if(STATUS_BUFFER_OVERFLOW==ntstat)
                {
                    req.maxitems++;
                    state->reallocate_buffer();
                    done=false;
                }
                else done=true;
            } while(!done);
            if(STATUS_PENDING!=ntstat)
            {
                //std::cerr << "ERROR " << errcode << std::endl;
                SetWin32LastErrorFromNtStatus(ntstat);
                error_code ec(GetLastError(), asio::error::get_system_category());
                ol.complete(ec, ol.get()->InternalHigh);
            }
            else
                ol.release();
        }
        // Called in unknown thread
        completion_returntype doenumerate(size_t id, future<> op, enumerate_req req, std::shared_ptr<promise<std::pair<std::vector<directory_entry>, bool>>> ret)
        {
            windows_nt_kernel::init();
            using namespace windows_nt_kernel;

            // A bit messy this, but necessary
            enumerate_state_t state=std::make_shared<enumerate_state>(
                id,
                std::move(op),
                std::move(ret),
                std::move(req)
                );
            doenumerate(std::move(state));

            // Indicate we're not finished yet
            return std::make_pair(false, handle_ptr());
        }
        // Called in unknown thread
        completion_returntype doextents(size_t id, future<> op, std::shared_ptr<promise<std::vector<std::pair<off_t, off_t>>>> ret, size_t entries)
        {
#if defined(__MINGW32__) && !defined(__MINGW64__) && !defined(__MINGW64_VERSION_MAJOR)
            // Mingw32 currently lacks the FILE_ALLOCATED_RANGE_BUFFER structure and FSCTL_QUERY_ALLOCATED_RANGES
            typedef struct _FILE_ALLOCATED_RANGE_BUFFER {
              LARGE_INTEGER FileOffset;
              LARGE_INTEGER Length;
            } FILE_ALLOCATED_RANGE_BUFFER, *PFILE_ALLOCATED_RANGE_BUFFER;
#define FSCTL_QUERY_ALLOCATED_RANGES    CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 51,  METHOD_NEITHER, FILE_READ_DATA)
#endif
            handle_ptr h(op.get_handle());
            async_io_handle_windows *p=static_cast<async_io_handle_windows *>(h.get());
            assert(p);
            auto buffers=std::make_shared<std::vector<FILE_ALLOCATED_RANGE_BUFFER>>(entries);
            auto completion_handler=[this, id, op, ret, entries, buffers](const error_code &ec, size_t bytes)
            {
              handle_ptr h(op.get_handle());
              if(ec)
              {
                if(ERROR_MORE_DATA==ec.value())
                {
                    doextents(id, std::move(op), std::move(ret), entries*2);
                    return;
                }
                exception_ptr e;
                // boost::system::system_error makes no attempt to ask windows for what the error code means :(
                try
                {
                  BOOST_AFIO_ERRGWINFN(ec.value(), [h]{return h->path();});
                }
                catch(...)
                {
                  e=current_exception();
                }
                ret->set_exception(e);
                complete_async_op(id, h, e);
              }
              else
              {
                std::vector<std::pair<off_t, off_t>> out(bytes/sizeof(FILE_ALLOCATED_RANGE_BUFFER));
                for(size_t n=0; n<out.size(); n++)
                {
                  out[n].first=buffers->data()[n].FileOffset.QuadPart;
                  out[n].second=buffers->data()[n].Length.QuadPart;
                }
                ret->set_value(std::move(out));
                complete_async_op(id, h);
              }
            };
            asio::windows::overlapped_ptr ol(threadsource()->io_service(), completion_handler);
            do
            {
                DWORD bytesout=0;
                // Search entire file
                buffers->front().FileOffset.QuadPart=0;
                buffers->front().Length.QuadPart=((off_t)1<<63)-1; // Microsoft claims this is 1<<64-1024 for NTFS, but I get bad parameter error with anything higher than 1<<63-1.
                BOOL ok=DeviceIoControl(p->native_handle(), FSCTL_QUERY_ALLOCATED_RANGES, buffers->data(), sizeof(FILE_ALLOCATED_RANGE_BUFFER), buffers->data(), (DWORD)(buffers->size()*sizeof(FILE_ALLOCATED_RANGE_BUFFER)), &bytesout, ol.get());
                DWORD errcode=GetLastError();
                if(!ok && ERROR_IO_PENDING!=errcode)
                {
                  if(ERROR_INSUFFICIENT_BUFFER==errcode || ERROR_MORE_DATA==errcode)
                  {
                    buffers->resize(buffers->size()*2);
                    continue;
                  }
                  //std::cerr << "ERROR " << errcode << std::endl;
                  error_code ec(errcode, asio::error::get_system_category());
                  ol.complete(ec, ol.get()->InternalHigh);
                }
                else
                  ol.release();
                break;
            } while(true);

            // Indicate we're not finished yet
            return std::make_pair(false, h);
        }
        // Called in unknown thread
        completion_returntype doextents(size_t id, future<> op, std::shared_ptr<promise<std::vector<std::pair<off_t, off_t>>>> ret)
        {
          return doextents(id, op, std::move(ret), 16);
        }
        // Called in unknown thread
        completion_returntype dostatfs(size_t id, future<> op, fs_metadata_flags req, std::shared_ptr<promise<statfs_t>> out)
        {
          try
          {
            windows_nt_kernel::init();
            using namespace windows_nt_kernel;
            handle_ptr h(op.get_handle());
            async_io_handle_windows *p=static_cast<async_io_handle_windows *>(h.get());
            assert(p);
            statfs_t ret;

            // Probably not worth doing these asynchronously, so execute synchronously
            BOOST_AFIO_TYPEALIGNMENT(8) path::value_type buffer[32769];
            IO_STATUS_BLOCK isb={ 0 };
            NTSTATUS ntstat;
            if(!!(req&fs_metadata_flags::flags) || !!(req&fs_metadata_flags::namemax) || !!(req&fs_metadata_flags::fstypename))
            {
              FILE_FS_ATTRIBUTE_INFORMATION *ffai=(FILE_FS_ATTRIBUTE_INFORMATION *) buffer;
              ntstat=NtQueryVolumeInformationFile(h->native_handle(), &isb, ffai, sizeof(buffer), FileFsAttributeInformation);
              if(STATUS_PENDING==ntstat)
                  ntstat=NtWaitForSingleObject(h->native_handle(), FALSE, NULL);
              BOOST_AFIO_ERRHNTFN(ntstat, [h]{return h->path();});
              if(!!(req&fs_metadata_flags::flags))
              {
                ret.f_flags.rdonly=!!(ffai->FileSystemAttributes & FILE_READ_ONLY_VOLUME);
                ret.f_flags.acls=!!(ffai->FileSystemAttributes & FILE_PERSISTENT_ACLS);
                ret.f_flags.xattr=!!(ffai->FileSystemAttributes & FILE_NAMED_STREAMS);
                ret.f_flags.compression=!!(ffai->FileSystemAttributes & FILE_VOLUME_IS_COMPRESSED);
                ret.f_flags.extents=!!(ffai->FileSystemAttributes & FILE_SUPPORTS_SPARSE_FILES);
                ret.f_flags.filecompression=!!(ffai->FileSystemAttributes & FILE_FILE_COMPRESSION);
              }
              if(!!(req&fs_metadata_flags::namemax)) ret.f_namemax=ffai->MaximumComponentNameLength;
              if(!!(req&fs_metadata_flags::fstypename))
              {
                ret.f_fstypename.resize(ffai->FileSystemNameLength/sizeof(path::value_type));
                for(size_t n=0; n<ffai->FileSystemNameLength/sizeof(path::value_type); n++)
                  ret.f_fstypename[n]=(char) ffai->FileSystemName[n];
              }
            }
            if(!!(req&fs_metadata_flags::bsize) || !!(req&fs_metadata_flags::blocks) || !!(req&fs_metadata_flags::bfree) || !!(req&fs_metadata_flags::bavail))
            {
              FILE_FS_FULL_SIZE_INFORMATION *fffsi=(FILE_FS_FULL_SIZE_INFORMATION *) buffer;
              ntstat=NtQueryVolumeInformationFile(h->native_handle(), &isb, fffsi, sizeof(buffer), FileFsFullSizeInformation);
              if(STATUS_PENDING==ntstat)
                  ntstat=NtWaitForSingleObject(h->native_handle(), FALSE, NULL);
              BOOST_AFIO_ERRHNTFN(ntstat, [h]{return h->path();});
              if(!!(req&fs_metadata_flags::bsize)) ret.f_bsize=fffsi->BytesPerSector*fffsi->SectorsPerAllocationUnit;
              if(!!(req&fs_metadata_flags::blocks)) ret.f_blocks=fffsi->TotalAllocationUnits.QuadPart;
              if(!!(req&fs_metadata_flags::bfree)) ret.f_bfree=fffsi->ActualAvailableAllocationUnits.QuadPart;
              if(!!(req&fs_metadata_flags::bavail)) ret.f_bavail=fffsi->CallerAvailableAllocationUnits.QuadPart;
            }
            if(!!(req&fs_metadata_flags::fsid))
            {
              FILE_FS_OBJECTID_INFORMATION *ffoi=(FILE_FS_OBJECTID_INFORMATION *) buffer;
              ntstat=NtQueryVolumeInformationFile(h->native_handle(), &isb, ffoi, sizeof(buffer), FileFsObjectIdInformation);
              if(STATUS_PENDING==ntstat)
                  ntstat=NtWaitForSingleObject(h->native_handle(), FALSE, NULL);
              if(0/*STATUS_SUCCESS*/==ntstat)
              {
                // FAT32 doesn't support filing system id, so sink error
                memcpy(&ret.f_fsid, ffoi->ObjectId, sizeof(ret.f_fsid));
              }
            }
            if(!!(req&fs_metadata_flags::iosize))
            {
              FILE_FS_SECTOR_SIZE_INFORMATION *ffssi=(FILE_FS_SECTOR_SIZE_INFORMATION *) buffer;
              ntstat=NtQueryVolumeInformationFile(h->native_handle(), &isb, ffssi, sizeof(buffer), FileFsSectorSizeInformation);
              if(STATUS_PENDING==ntstat)
                  ntstat=NtWaitForSingleObject(h->native_handle(), FALSE, NULL);
              if(0/*STATUS_SUCCESS*/!=ntstat)
              {
                  // Windows XP and Vista don't support the FILE_FS_SECTOR_SIZE_INFORMATION
                  // API, so we'll just hardcode 512 bytes
                  ffssi->PhysicalBytesPerSectorForPerformance=512;
              }
              ret.f_iosize=ffssi->PhysicalBytesPerSectorForPerformance;
            }
            if(!!(req&fs_metadata_flags::mntfromname) || !!(req&fs_metadata_flags::mntonname))
            {
              // Irrespective we need the mount path before figuring out the mounted device
              ret.f_mntonname=MountPointFromHandle(h->native_handle());
              if(!!(req&fs_metadata_flags::mntfromname))
              {
                path::string_type volumename=VolumeNameFromMountPoint(ret.f_mntonname);
                ret.f_mntfromname.reserve(volumename.size());
                for(size_t n=0; n<volumename.size(); n++)
                  ret.f_mntfromname.push_back((char) volumename[n]);
              }
            }
            out->set_value(std::move(ret));
            return std::make_pair(true, h);
          }
          catch(...)
          {
            out->set_exception(current_exception());
            throw;
          }
        }
        // Called in unknown thread
        completion_returntype dolock(size_t id, future<> op, lock_req req)
        {
          handle_ptr h(op.get_handle());
          async_io_handle_windows *p=static_cast<async_io_handle_windows *>(h.get());
          assert(p);
          if(!p->lockfile)
            BOOST_AFIO_THROW(std::invalid_argument("This file handle was not opened with OSLockable."));
          return p->lockfile->lock(id, std::move(op), std::move(req));
        }

    public:
        async_file_io_dispatcher_windows(std::shared_ptr<thread_source> threadpool, file_flags flagsforce, file_flags flagsmask) : dispatcher(threadpool, flagsforce, flagsmask), pagesize(utils::page_sizes().front())
        {
        }

        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<>> dir(const std::vector<path_req> &reqs) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            for(auto &i: reqs)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::dir, reqs, async_op_flags::none, &async_file_io_dispatcher_windows::dodir);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<>> rmdir(const std::vector<path_req> &reqs) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            for(auto &i: reqs)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::rmdir, reqs, async_op_flags::none, &async_file_io_dispatcher_windows::dormdir);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<>> file(const std::vector<path_req> &reqs) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            for(auto &i: reqs)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::file, reqs, async_op_flags::none, &async_file_io_dispatcher_windows::dofile);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<>> rmfile(const std::vector<path_req> &reqs) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            for(auto &i: reqs)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::rmfile, reqs, async_op_flags::none, &async_file_io_dispatcher_windows::dormfile);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<>> symlink(const std::vector<path_req> &reqs, const std::vector<future<>> &targets) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            for(auto &i: reqs)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
            std::vector<future<>> ops(targets);
            ops.resize(reqs.size());
            for(size_t n=0; n<reqs.size(); n++)
            {
              if(!ops[n].valid())
                ops[n]=reqs[n].precondition;
            }
            return chain_async_ops((int) detail::OpType::symlink, ops, reqs, async_op_flags::none, &async_file_io_dispatcher_windows::dosymlink);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<>> rmsymlink(const std::vector<path_req> &reqs) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            for(auto &i: reqs)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::rmsymlink, reqs, async_op_flags::none, &async_file_io_dispatcher_windows::dormsymlink);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<>> sync(const std::vector<future<>> &ops) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            for(auto &i: ops)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::sync, ops, async_op_flags::none, &async_file_io_dispatcher_windows::dosync);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<>> zero(const std::vector<future<>> &ops, const std::vector<std::vector<std::pair<off_t, off_t>>> &ranges) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
          for(auto &i: ops)
          {
            if(!i.validate())
              BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
          }
#endif
          return chain_async_ops((int) detail::OpType::zero, ops, ranges, async_op_flags::none, &async_file_io_dispatcher_windows::dozero);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<>> close(const std::vector<future<>> &ops) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            for(auto &i: ops)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::close, ops, async_op_flags::none, &async_file_io_dispatcher_windows::doclose);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<>> read(const std::vector<detail::io_req_impl<false>> &reqs) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            for(auto &i: reqs)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::read, reqs, async_op_flags::none, &async_file_io_dispatcher_windows::doread);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<>> write(const std::vector<detail::io_req_impl<true>> &reqs) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            for(auto &i: reqs)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::write, reqs, async_op_flags::none, &async_file_io_dispatcher_windows::dowrite);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<>> truncate(const std::vector<future<>> &ops, const std::vector<off_t> &sizes) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            for(auto &i: ops)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
            if(ops.size()!=sizes.size())
                BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
#endif
            return chain_async_ops((int) detail::OpType::truncate, ops, sizes, async_op_flags::none, &async_file_io_dispatcher_windows::dotruncate);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<std::pair<std::vector<directory_entry>, bool>>> enumerate(const std::vector<enumerate_req> &reqs) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            for(auto &i: reqs)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::enumerate, reqs, async_op_flags::none, &async_file_io_dispatcher_windows::doenumerate);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<std::vector<std::pair<off_t, off_t>>>> extents(const std::vector<future<>> &ops) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            for(auto &i: ops)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::extents, ops, async_op_flags::none, &async_file_io_dispatcher_windows::doextents);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<statfs_t>> statfs(const std::vector<future<>> &ops, const std::vector<fs_metadata_flags> &reqs) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            for(auto &i: ops)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
            }
            if(ops.size()!=reqs.size())
                BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
#endif
            return chain_async_ops((int) detail::OpType::statfs, ops, reqs, async_op_flags::none, &async_file_io_dispatcher_windows::dostatfs);
        }
        BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<future<>> lock(const std::vector<lock_req> &reqs) override final
        {
#if BOOST_AFIO_VALIDATE_INPUTS
            for(auto &i: reqs)
            {
                if(!i.validate())
                    BOOST_AFIO_THROW(std::invalid_argument("Inputs are invalid."));
            }
#endif
            return chain_async_ops((int) detail::OpType::lock, reqs, async_op_flags::none, &async_file_io_dispatcher_windows::dolock);
        }
    };

    inline handle_ptr async_io_handle_windows::int_verifymyinode()
    {
        handle_ptr dirh(container());
        if(!dirh)
        {
          path_req req(path(true));
          dirh=parent()->int_get_handle_to_containing_dir(static_cast<async_file_io_dispatcher_windows *>(parent()), 0, req, &async_file_io_dispatcher_windows::dofile);
        }
        return dirh;
    }

    struct win_actual_lock_file : public actual_lock_file
    {
      HANDLE h;
      OVERLAPPED ol;
      static BOOST_CONSTEXPR_OR_CONST off_t magicbyte=(1ULL<<62)-1;
      static bool win32_lockmagicbyte(HANDLE h, DWORD flags)
      {
        OVERLAPPED ol={0};
        ol.Offset=(DWORD) (magicbyte & 0xffffffff);
        ol.OffsetHigh=(DWORD) ((magicbyte>>32) & 0xffffffff);
        DWORD len_low=1, len_high=0;
        return LockFileEx(h, LOCKFILE_FAIL_IMMEDIATELY|flags, 0, len_low, len_high, &ol)!=0;
      }
      static bool win32_unlockmagicbyte(HANDLE h)
      {
        OVERLAPPED ol={0};
        ol.Offset=(DWORD) (magicbyte & 0xffffffff);
        ol.OffsetHigh=(DWORD) ((magicbyte>>32) & 0xffffffff);
        DWORD len_low=1, len_high=0;
        return UnlockFileEx(h, 0, len_low, len_high, &ol)!=0;
      }
      win_actual_lock_file(BOOST_AFIO_V2_NAMESPACE::path p) : actual_lock_file(std::move(p)), h(nullptr)
      {
        memset(&ol, 0, sizeof(ol));
        bool done=false;
        do
        {
          NTSTATUS status=0;
          for(size_t n=0; n<10; n++)
          {
            // TODO FIXME: Lock file needs to copy exact security descriptor from its original
            std::tie(status, h)=ntcreatefile(handle_ptr(), lockfilepath, file_flags::create|file_flags::read_write|file_flags::temporary_file|file_flags::int_file_share_delete);
            // This may fail with STATUS_DELETE_PENDING, if so sleep and loop
            if(!status)
              break;
            else if(((NTSTATUS) 0xC0000056)/*STATUS_DELETE_PENDING*/==status)
              this_thread::sleep_for(chrono::milliseconds(100));
            else
              BOOST_AFIO_ERRHNT(status);
          }
          BOOST_AFIO_ERRHNT(status);
          // We can't use DeleteOnClose for Samba shares because he'll delete on close even if
          // other processes have a handle open. We hence read lock the same final byte as POSIX considers
          // it (i.e. 1<<62-1). If it fails then the other has a write lock and is about to delete.
          if(!(done=win32_lockmagicbyte(h, 0)))
            CloseHandle(h);
        } while(!done);
      }
      ~win_actual_lock_file()
      {
        // Lock POSIX magic byte for write access. Win32 doesn't permit conversion of shared to exclusive locks
        win32_unlockmagicbyte(h);
        if(win32_lockmagicbyte(h, LOCKFILE_EXCLUSIVE_LOCK))
        {
          // If this lock succeeded then I am potentially the last with this file open
          // so unlink myself, which will work with my open file handle as I was opened with FILE_SHARE_DELETE.
          // All further opens will now fail with STATUS_DELETE_PENDING
          path::string_type escapedpath(lockfilepath.filesystem_path().native());
          BOOST_AFIO_ERRHWIN(DeleteFile(escapedpath.c_str()));
        }
        BOOST_AFIO_ERRHWIN(CloseHandle(h));
      }
      dispatcher::completion_returntype lock(size_t id, future<> op, lock_req req, void *_thislockfile) override final
      {
        windows_nt_kernel::init();
        using namespace windows_nt_kernel;
        win_lock_file *thislockfile=(win_lock_file *) _thislockfile;
        auto completion_handler=[this, id, op, req](const error_code &ec)
        {
          handle_ptr h(op.get_handle());
          if(ec)
          {
            exception_ptr e;
            // boost::system::system_error makes no attempt to ask windows for what the error code means :(
            try
            {
              BOOST_AFIO_ERRGWINFN(ec.value(), [h]{return h->path();});
            }
            catch(...)
            {
              e=current_exception();
            }
            op.parent()->complete_async_op(id, h, e);
          }
          else
          {
            op.parent()->complete_async_op(id, h);
          }
        };
        // (1<<62)-1 byte used by POSIX for last use detection
        static BOOST_CONSTEXPR_OR_CONST off_t magicbyte=(1ULL<<62)-1;
        if(req.offset==magicbyte)
          BOOST_AFIO_THROW(std::invalid_argument("offset cannot be (1<<62)-1"));
        // If we straddle the magic byte then clamp to just under it
        if(req.offset<magicbyte && req.offset+req.length>magicbyte)
          req.length=std::min(req.offset+req.length, magicbyte)-req.offset;

        NTSTATUS ntstat=0;
        LARGE_INTEGER offset; offset.QuadPart=req.offset;
        LARGE_INTEGER length; length.QuadPart=req.length;
        if(req.type==lock_req::Type::unlock)
        {
          ntstat=NtUnlockFile(h, (PIO_STATUS_BLOCK) &ol, &offset, &length, 0);
          //printf("UL %u ntstat=%u\n", id, ntstat);
        }
        else
        {
          ntstat=NtLockFile(h, thislockfile->ev.native_handle(), nullptr, nullptr, (PIO_STATUS_BLOCK) &ol, &offset, &length, 0,
            false, req.type==lock_req::Type::write_lock);
          //printf("L %u ntstat=%u\n", id, ntstat);
        }
        if(STATUS_PENDING!=ntstat)
        {
          SetWin32LastErrorFromNtStatus(ntstat);
          error_code ec(GetLastError(), asio::error::get_system_category());
          completion_handler(ec);
        }
        else
          thislockfile->ev.async_wait(completion_handler);
        // Indicate we're not finished yet
        return std::make_pair(false, handle_ptr());
      }
    };
} // namespace
BOOST_AFIO_V2_NAMESPACE_END

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
