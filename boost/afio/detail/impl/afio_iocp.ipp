/* async_file_io
Provides a threadpool and asynchronous file i/o infrastructure based on Boost.ASIO, Boost.Iostreams and filesystem
(C) 2013-2014 Niall Douglas http://www.nedprod.com/
File Created: Mar 2013
*/

#ifdef WIN32
namespace boost { namespace afio { namespace detail {

    struct async_io_handle_windows : public async_io_handle
    {
        std::unique_ptr<boost::asio::windows::random_access_handle> h;
        void *myid;
        bool has_been_added, SyncOnClose;
        void *mapaddr;

        static HANDLE int_checkHandle(HANDLE h, const std::filesystem::path &path)
        {
            BOOST_AFIO_ERRHWINFN(INVALID_HANDLE_VALUE!=h, path);
            return h;
        }
        async_io_handle_windows(async_file_io_dispatcher_base *_parent, std::shared_ptr<async_io_handle> _dirh, const std::filesystem::path &path, file_flags flags) : async_io_handle(_parent, std::move(_dirh), path, flags), myid(nullptr), has_been_added(false), SyncOnClose(false), mapaddr(nullptr) { }
        inline async_io_handle_windows(async_file_io_dispatcher_base *_parent, std::shared_ptr<async_io_handle> _dirh, const std::filesystem::path &path, file_flags flags, bool _SyncOnClose, HANDLE _h);
        void int_close()
        {
            BOOST_AFIO_DEBUG_PRINT("D %p\n", this);
            if(has_been_added)
            {
                parent()->int_del_io_handle(myid);
                has_been_added=false;
            }
            if(mapaddr)
            {
                BOOST_AFIO_ERRHWINFN(UnmapViewOfFile(mapaddr), path());
                mapaddr=nullptr;
            }
            if(h)
            {
                // Windows doesn't provide an async fsync so do it synchronously
                if(SyncOnClose && write_count_since_fsync())
                    BOOST_AFIO_ERRHWINFN(FlushFileBuffers(myid), path());
                h->close();
                h.reset();
            }
            myid=nullptr;
        }
        virtual void close()
        {
            int_close();
        }
        virtual void *native_handle() const { return myid; }

        // You can't use shared_from_this() in a constructor so ...
        void do_add_io_handle_to_parent()
        {
            if(myid)
            {
                parent()->int_add_io_handle(myid, shared_from_this());
                has_been_added=true;
            }
        }
        ~async_io_handle_windows()
        {
            int_close();
        }
        virtual directory_entry direntry(metadata_flags wanted) const
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
            return directory_entry(path().leaf(), stat, wanted);
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
        virtual void *try_mapfile()
        {
            if(!mapaddr)
            {
                if(!(flags() & file_flags::Write) && !(flags() & file_flags::Append))
                {
                    HANDLE sectionh;
                    if(INVALID_HANDLE_VALUE!=(sectionh=CreateFileMapping(myid, NULL, PAGE_READONLY, 0, 0, nullptr)))
                    {
                        auto unsectionh=boost::afio::detail::Undoer([&sectionh]{ CloseHandle(sectionh); });
                        mapaddr=MapViewOfFile(sectionh, FILE_MAP_READ, 0, 0, 0);
                    }
                }
            }
            return mapaddr;
        }
    };
    inline async_io_handle_windows::async_io_handle_windows(async_file_io_dispatcher_base *_parent, std::shared_ptr<async_io_handle> _dirh, const std::filesystem::path &path, file_flags flags, bool _SyncOnClose, HANDLE _h) : async_io_handle(_parent, std::move(_dirh), path, flags), h(new boost::asio::windows::random_access_handle(_parent->p->pool->io_service(), int_checkHandle(_h, path))), myid(_h), has_been_added(false), SyncOnClose(_SyncOnClose), mapaddr(nullptr) { }

    class async_file_io_dispatcher_windows : public async_file_io_dispatcher_base
    {
        friend class directory_entry;
        friend void directory_entry::_int_fetch(metadata_flags wanted, std::shared_ptr<async_io_handle> dirh);

        size_t pagesize;
        // Called in unknown thread
        completion_returntype dodir(size_t id, async_io_op _, async_path_op_req req)
        {
            BOOL ret=0;
            req.flags=fileflags(req.flags)|file_flags::int_opening_dir|file_flags::Read;
            if(!(req.flags & file_flags::UniqueDirectoryHandle) && !!(req.flags & file_flags::Read) && !(req.flags & file_flags::Write))
            {
                // Return a copy of the one in the dir cache if available
                return std::make_pair(true, p->get_handle_to_dir(this, id, req, &async_file_io_dispatcher_windows::dofile));
            }
            else
            {
                // With the NT kernel, you create a directory by creating a file.
                return dofile(id, _, req);
            }
        }
        // Called in unknown thread
        completion_returntype dormdir(size_t id, async_io_op _, async_path_op_req req)
        {
            req.flags=fileflags(req.flags);
            BOOST_AFIO_ERRHWINFN(RemoveDirectory(req.path.c_str()), req.path);
            auto ret=std::make_shared<async_io_handle_windows>(this, std::shared_ptr<async_io_handle>(), req.path, req.flags);
            return std::make_pair(true, ret);
        }
        // Called in unknown thread
        completion_returntype dofile(size_t id, async_io_op, async_path_op_req req)
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
                if(!!(req.flags & file_flags::WillBeSequentiallyAccessed))
                    flags|=0x00000004/*FILE_SEQUENTIAL_ONLY*/;
                else if(!!(req.flags & file_flags::WillBeRandomlyAccessed))
                    flags|=0x00000800/*FILE_RANDOM_ACCESS*/;
            }
            if(!!(req.flags & file_flags::CreateOnlyIfNotExist)) creatdisp|=0x00000002/*FILE_CREATE*/;
            else if(!!(req.flags & file_flags::Create)) creatdisp|=0x00000003/*FILE_OPEN_IF*/;
            else if(!!(req.flags & file_flags::Truncate)) creatdisp|=0x00000005/*FILE_OVERWRITE_IF*/;
            else creatdisp|=0x00000001/*FILE_OPEN*/;
            if(!!(req.flags & file_flags::OSDirect)) flags|=0x00000008/*FILE_NO_INTERMEDIATE_BUFFERING*/;
            if(!!(req.flags & file_flags::AlwaysSync)) flags|=0x00000002/*FILE_WRITE_THROUGH*/;
            if(!!(req.flags & file_flags::FastDirectoryEnumeration))
                dirh=p->get_handle_to_containing_dir(this, id, req, &async_file_io_dispatcher_windows::dofile);

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
            auto ret=std::make_shared<async_io_handle_windows>(this, dirh, req.path, req.flags,
                (file_flags::SyncOnClose|file_flags::Write)==(req.flags & (file_flags::SyncOnClose|file_flags::Write|file_flags::AlwaysSync)),
                h);
            static_cast<async_io_handle_windows *>(ret.get())->do_add_io_handle_to_parent();
            if(!(req.flags & file_flags::int_opening_dir) && !(req.flags & file_flags::int_opening_link) && !!(req.flags & file_flags::OSMMap))
                ret->try_mapfile();
            return std::make_pair(true, ret);
        }
        // Called in unknown thread
        completion_returntype dormfile(size_t id, async_io_op _, async_path_op_req req)
        {
            req.flags=fileflags(req.flags);
            BOOST_AFIO_ERRHWINFN(DeleteFile(req.path.c_str()), req.path);
            auto ret=std::make_shared<async_io_handle_windows>(this, std::shared_ptr<async_io_handle>(), req.path, req.flags);
            return std::make_pair(true, ret);
        }
        // Called in unknown thread
        void boost_asio_symlink_completion_handler(size_t id, std::shared_ptr<async_io_handle> h, std::shared_ptr<std::unique_ptr<std::filesystem::path::value_type[]>> buffer, const boost::system::error_code &ec, size_t bytes_transferred)
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
        completion_returntype dosymlink(size_t id, async_io_op op, async_path_op_req req)
        {
            std::shared_ptr<async_io_handle> h(op.get());
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
                return dodir(id, op, req);
            }
            if(!!(h->flags()&file_flags::int_opening_dir))
            {
                // Create a directory junction
                windows_nt_kernel::init();
                using namespace windows_nt_kernel;
                using windows_nt_kernel::REPARSE_DATA_BUFFER;
                // First we need a new directory with write access
                req.flags=req.flags|file_flags::Write;
                completion_returntype ret=dodir(id, op, req);
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
                boost::asio::windows::overlapped_ptr ol(p->h->get_io_service(), boost::bind(&async_file_io_dispatcher_windows::boost_asio_symlink_completion_handler, this, id, ret.second, buffer, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
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
        completion_returntype dormsymlink(size_t id, async_io_op _, async_path_op_req req)
        {
            req.flags=fileflags(req.flags);
            BOOST_AFIO_ERRHWINFN(RemoveDirectory(req.path.c_str()), req.path);
            auto ret=std::make_shared<async_io_handle_windows>(this, std::shared_ptr<async_io_handle>(), req.path, req.flags);
            return std::make_pair(true, ret);
        }
        // Called in unknown thread
        completion_returntype dosync(size_t id, async_io_op op, async_io_op)
        {
            std::shared_ptr<async_io_handle> h(op.get());
            async_io_handle_windows *p=static_cast<async_io_handle_windows *>(h.get());
            off_t bytestobesynced=p->write_count_since_fsync();
            assert(p);
            if(bytestobesynced)
                BOOST_AFIO_ERRHWINFN(FlushFileBuffers(p->h->native_handle()), p->path());
            p->byteswrittenatlastfsync+=bytestobesynced;
            return std::make_pair(true, h);
        }
        // Called in unknown thread
        completion_returntype doclose(size_t id, async_io_op op, async_io_op)
        {
            std::shared_ptr<async_io_handle> h(op.get());
            async_io_handle_windows *p=static_cast<async_io_handle_windows *>(h.get());
            assert(p);
            if(!!(p->flags() & file_flags::int_opening_dir) && !(p->flags() & file_flags::UniqueDirectoryHandle) && !!(p->flags() & file_flags::Read) && !(p->flags() & file_flags::Write))
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
        void boost_asio_readwrite_completion_handler(bool is_write, size_t id, std::shared_ptr<async_io_handle> h, std::shared_ptr<std::pair<boost::afio::atomic<bool>, boost::afio::atomic<size_t>>> bytes_to_transfer, size_t bytes_this_chunk, const boost::system::error_code &ec, size_t bytes_transferred)
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
                size_t togo=(bytes_to_transfer->second-=bytes_this_chunk);
                if(!togo) // bytes_this_chunk may not equal bytes_transferred if final 4Kb chunk of direct file
                    complete_async_op(id, h);
                if(togo>((size_t)1<<(8*sizeof(size_t)-1)))
                    BOOST_AFIO_THROW_FATAL(std::runtime_error("IOCP returned more bytes than we asked for. This is probably memory corruption."));
                BOOST_AFIO_DEBUG_PRINT("H %u e=%u togo=%u bt=%u bc=%u\n", (unsigned) id, (unsigned) ec.value(), (unsigned) togo, (unsigned) bytes_transferred, (unsigned) bytes_this_chunk);
            }
            //std::cout << "id=" << id << " total=" << bytes_to_transfer->second << " this=" << bytes_transferred << std::endl;
        }
        template<bool iswrite> void doreadwrite(size_t id, std::shared_ptr<async_io_handle> h, detail::async_data_op_req_impl<iswrite> req, async_io_handle_windows *p)
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
                boost::asio::windows::overlapped_ptr ol(p->h->get_io_service(), boost::bind(&async_file_io_dispatcher_windows::boost_asio_readwrite_completion_handler, this, iswrite, id, h, bytes_to_transfer, amount, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
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
                    boost::asio::windows::overlapped_ptr ol(p->h->get_io_service(), boost::bind(&async_file_io_dispatcher_windows::boost_asio_readwrite_completion_handler, this, iswrite, id, h, bytes_to_transfer, boost::asio::buffer_size(b), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
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
        completion_returntype doread(size_t id, async_io_op op, detail::async_data_op_req_impl<false> req)
        {
            std::shared_ptr<async_io_handle> h(op.get());
            async_io_handle_windows *p=static_cast<async_io_handle_windows *>(h.get());
            assert(p);
            BOOST_AFIO_DEBUG_PRINT("R %u %p (%c) @ %u, b=%u\n", (unsigned) id, h.get(), p->path().native().back(), (unsigned) req.where, (unsigned) req.buffers.size());
#ifdef DEBUG_PRINTING
            BOOST_FOREACH(auto &b, req.buffers)
            {   BOOST_AFIO_DEBUG_PRINT("  R %u: %p %u\n", (unsigned) id, boost::asio::buffer_cast<const void *>(b), (unsigned) boost::asio::buffer_size(b)); }
#endif
            if(p->mapaddr)
            {
                void *addr=(void *)((char *) p->mapaddr + req.where);
                BOOST_FOREACH(auto &b, req.buffers)
                {
                    memcpy(boost::asio::buffer_cast<void *>(b), addr, boost::asio::buffer_size(b));
                    addr=(void *)((char *) addr + boost::asio::buffer_size(b));
                }
                return std::make_pair(true, h);
            }
            else
            {
                doreadwrite(id, h, req, p);
                // Indicate we're not finished yet
                return std::make_pair(false, h);
            }
        }
        // Called in unknown thread
        completion_returntype dowrite(size_t id, async_io_op op, detail::async_data_op_req_impl<true> req)
        {
            std::shared_ptr<async_io_handle> h(op.get());
            async_io_handle_windows *p=static_cast<async_io_handle_windows *>(h.get());
            assert(p);
            BOOST_AFIO_DEBUG_PRINT("W %u %p (%c) @ %u, b=%u\n", (unsigned) id, h.get(), p->path().native().back(), (unsigned) req.where, (unsigned) req.buffers.size());
#ifdef DEBUG_PRINTING
            BOOST_FOREACH(auto &b, req.buffers)
            {   BOOST_AFIO_DEBUG_PRINT("  W %u: %p %u\n", (unsigned) id, boost::asio::buffer_cast<const void *>(b), (unsigned) boost::asio::buffer_size(b)); }
#endif
            doreadwrite(id, h, req, p);
            // Indicate we're not finished yet
            return std::make_pair(false, h);
        }
        // Called in unknown thread
        completion_returntype dotruncate(size_t id, async_io_op op, off_t _newsize)
        {
            std::shared_ptr<async_io_handle> h(op.get());
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
        void boost_asio_enumerate_completion_handler(size_t id, async_io_op op, enumerate_state_t state, const boost::system::error_code &ec, size_t bytes_transferred)
        {
            using windows_nt_kernel::FILE_ID_FULL_DIR_INFORMATION;
            std::shared_ptr<async_io_handle> h(op.get());
            std::shared_ptr<promise<std::pair<std::vector<directory_entry>, bool>>> &ret=std::get<0>(*state);
            std::unique_ptr<FILE_ID_FULL_DIR_INFORMATION[]> &buffer=std::get<1>(*state);
            async_enumerate_op_req &req=std::get<2>(*state);
            if(ec && ERROR_MORE_DATA==ec.value() && bytes_transferred<(sizeof(FILE_ID_FULL_DIR_INFORMATION)+buffer.get()->FileNameLength))
            {
                // Bump maxitems by one and reschedule.
                req.maxitems++;
                buffer=std::unique_ptr<FILE_ID_FULL_DIR_INFORMATION[]>(new FILE_ID_FULL_DIR_INFORMATION[req.maxitems]);
                doenumerate(id, op, state);
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
                bool needmoremetadata=!!(req.metadata&~item.have_metadata);
                bool done=false;
                for(FILE_ID_FULL_DIR_INFORMATION *ffdi=buffer.get(); !done; ffdi=(FILE_ID_FULL_DIR_INFORMATION *)((size_t) ffdi + ffdi->NextEntryOffset))
                {
                    size_t length=ffdi->FileNameLength/sizeof(std::filesystem::path::value_type);
                    if(length<=2 && '.'==ffdi->FileName[0])
                        if(1==length || '.'==ffdi->FileName[1])
                        {
                            if(!ffdi->NextEntryOffset) done=true;
                            continue;
                        }
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
                if(needmoremetadata)
                {
                    BOOST_FOREACH(auto &i, _ret)
                    {
                        i.fetch_metadata(h, req.metadata);
                    }
                }
                ret->set_value(std::make_pair(std::move(_ret), !thisbatchdone));
                complete_async_op(id, h);
            }
        }
        // Called in unknown thread
        void doenumerate(size_t id, async_io_op op, enumerate_state_t state)
        {
            std::shared_ptr<async_io_handle> h(op.get());
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
            boost::asio::windows::overlapped_ptr ol(p->h->get_io_service(), boost::bind(&async_file_io_dispatcher_windows::boost_asio_enumerate_completion_handler, this, id, op, state, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
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
        completion_returntype doenumerate(size_t id, async_io_op op, async_enumerate_op_req req, std::shared_ptr<promise<std::pair<std::vector<directory_entry>, bool>>> ret)
        {
            windows_nt_kernel::init();
            using namespace windows_nt_kernel;

            // A bit messy this, but necessary
            enumerate_state_t state=std::make_shared<std::tuple<std::shared_ptr<promise<std::pair<std::vector<directory_entry>, bool>>>,
                std::unique_ptr<windows_nt_kernel::FILE_ID_FULL_DIR_INFORMATION[]>, async_enumerate_op_req>>(
                std::move(ret),
                std::unique_ptr<FILE_ID_FULL_DIR_INFORMATION[]>(new FILE_ID_FULL_DIR_INFORMATION[req.maxitems]),
                std::move(req)
                );
            doenumerate(id, std::move(op), std::move(state));

            // Indicate we're not finished yet
            return std::make_pair(false, std::shared_ptr<async_io_handle>());
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
            return chain_async_ops((int) detail::OpType::dir, reqs, async_op_flags::none, &async_file_io_dispatcher_windows::dodir);
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
            return chain_async_ops((int) detail::OpType::rmdir, reqs, async_op_flags::none, &async_file_io_dispatcher_windows::dormdir);
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
            return chain_async_ops((int) detail::OpType::file, reqs, async_op_flags::none, &async_file_io_dispatcher_windows::dofile);
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
            return chain_async_ops((int) detail::OpType::rmfile, reqs, async_op_flags::none, &async_file_io_dispatcher_windows::dormfile);
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
            return chain_async_ops((int) detail::OpType::symlink, reqs, async_op_flags::none, &async_file_io_dispatcher_windows::dosymlink);
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
            return chain_async_ops((int) detail::OpType::rmsymlink, reqs, async_op_flags::none, &async_file_io_dispatcher_windows::dormsymlink);
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
            return chain_async_ops((int) detail::OpType::sync, ops, async_op_flags::none, &async_file_io_dispatcher_windows::dosync);
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
            return chain_async_ops((int) detail::OpType::close, ops, async_op_flags::none, &async_file_io_dispatcher_windows::doclose);
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
            return chain_async_ops((int) detail::OpType::read, reqs, async_op_flags::none, &async_file_io_dispatcher_windows::doread);
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
            return chain_async_ops((int) detail::OpType::write, reqs, async_op_flags::none, &async_file_io_dispatcher_windows::dowrite);
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
            return chain_async_ops((int) detail::OpType::truncate, ops, sizes, async_op_flags::none, &async_file_io_dispatcher_windows::dotruncate);
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
            return chain_async_ops((int) detail::OpType::enumerate, reqs, async_op_flags::none, &async_file_io_dispatcher_windows::doenumerate);
        }
    };
} } } // namespace

#endif
