/* Test that functions do not need a /proc mount in the current mount namespace
File Created: September 2026


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

#include "../test_kernel_decl.hpp"

#include <llfio/v2.0/storage_profile.hpp>

#include <sys/capability.h>
#include <sys/mount.h>

#ifdef __linux__

namespace
{
  namespace llfio = LLFIO_V2_NAMESPACE;

  bool try_setup()
  {
    // Initialize proc_base
    llfio::get_proc_base().value();

    // noexcept because if one of these operations fail, the process is going to be in a *weird* state.
    // No point in trying to recover from that.
    static constexpr auto do_unshare = []() noexcept
    {
      const uid_t outer_uid{getuid()};
      const gid_t outer_gid{getgid()};
      if(::unshare(CLONE_NEWUSER | CLONE_NEWNS) < 0)
      {
        if(errno == EPERM)
        {
          return false;
        }
        llfio::posix_error().throw_exception();
      }
      const uid_t inner_uid{getuid()};
      const gid_t inner_gid{getgid()};
      // Map UIDs
      {
        std::string map_range = std::to_string(inner_uid) + " " + std::to_string(outer_uid) + " 1\n";
        auto uid_file = llfio::file({}, "/proc/self/uid_map", llfio::file_handle::mode::write,
                                    llfio::file_handle::creation::open_existing)
                        .value();
        // Not seekable so we must use plain ::write
        if(::write(uid_file.native_handle().fd, map_range.data(), map_range.size()) < 0)
        {
          llfio::posix_error().throw_exception();
        }
      }
      {
        std::string_view deny = "deny\n";
        auto set_groups_file = llfio::file({}, "/proc/self/setgroups", llfio::file_handle::mode::write,
                                           llfio::file_handle::creation::open_existing)
                               .value();
        if(::write(set_groups_file.native_handle().fd, deny.data(), deny.size()) < 0)
        {
          llfio::posix_error().throw_exception();
        }
      }
      {
        std::string map_range = std::to_string(inner_gid) + " " + std::to_string(outer_gid) + " 1\n";
        auto gid_file = llfio::file({}, "/proc/self/gid_map", llfio::file_handle::mode::write,
                                    llfio::file_handle::creation::open_existing)
                        .value();
        if(::write(gid_file.native_handle().fd, map_range.data(), map_range.size()) < 0)
        {
          llfio::posix_error().throw_exception();
        }
      }
      // Disable mount event propagation outside of the new namespace
      if(::mount(nullptr, "/", nullptr, MS_REC | MS_SLAVE, nullptr) == -1)
      {
        llfio::posix_error().throw_exception();
      }
      return true;
    };
    [[maybe_unused]] static bool can_unshare{do_unshare()};
    return can_unshare;
  }

  llfio::result<llfio::file_handle> get_current_mount_namespace() noexcept
  {
    llfio::path_handle proc_base{llfio::get_proc_base().value(), llfio::path_handle::flag::none};
    auto release_proc_base = llfio::make_scope_exit(
    [&]() noexcept
    {
      // We don't actually own this handle so release it before it goes out of scope and closes the underlying fd.
      proc_base.release();
    });
    auto result = llfio::file(proc_base, "thread-self/ns/mnt");
    return result;
  }

  class scoped_mount_namespace_join
  {
    struct token
    {
      explicit token() = default;
    };

  public:
    explicit scoped_mount_namespace_join(token, llfio::file_handle &&to_return)
        : to_return{std::move(to_return)}
    {
    }

    scoped_mount_namespace_join(const scoped_mount_namespace_join &) = delete;
    scoped_mount_namespace_join(scoped_mount_namespace_join &&) = default;
    scoped_mount_namespace_join &operator=(const scoped_mount_namespace_join &) = delete;
    scoped_mount_namespace_join &operator=(scoped_mount_namespace_join &&) = delete;

    static llfio::result<scoped_mount_namespace_join> join(const llfio::file_handle &to_join) noexcept
    {
      OUTCOME_TRY(llfio::file_handle current_mount_namespace, get_current_mount_namespace());
      if(::setns(to_join.native_handle().fd, CLONE_NEWNS) < 0)
      {
        return llfio::posix_error();
      }
      llfio::result<scoped_mount_namespace_join> result{token{}, std::move(current_mount_namespace)};
      return result;
    }

    ~scoped_mount_namespace_join()
    {
      if(!to_return.is_valid())
      {
        return;
      }
      if(::setns(to_return.native_handle().fd, CLONE_NEWNS) < 0)
      {
        llfio::posix_error().throw_exception();
      }
    }

  private:
    llfio::file_handle to_return;
  };

  class isolated_mount_namespace
  {
  public:
    explicit isolated_mount_namespace()
    {
      mount_point = llfio::temp_directory().value();

      llfio::file_handle current_mount_namespace = get_current_mount_namespace().value();
      if(::unshare(CLONE_NEWNS) < 0)
      {
        llfio::posix_error().throw_exception();
      }
      auto return_to_original_mount_namespace = llfio::make_scope_exit(
      [&]() noexcept
      {
        if(::setns(current_mount_namespace.native_handle().fd, CLONE_NEWNS) < 0)
        {
          // Should not fail unless the system is out of memory, in which case we might as well terminate.
          llfio::posix_error().throw_exception();
        }
      });
      mount_namespace_handle = get_current_mount_namespace().value();
      // Disable mount event propagation in or out of the new namespace
      if(::mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE, nullptr) == -1)
      {
        llfio::posix_error().throw_exception();
      }
      auto mount_point_path = mount_point.current_path().value();
      // Mount a tmpfs on the temporary directory
      if(::mount("tmpfs", mount_point_path.c_str(), "tmpfs", MS_NODEV | MS_NOSUID | MS_NOEXEC, "size=16M") < 0)
      {
        llfio::posix_error().throw_exception();
      }
      // Stack the current root on top of the new tmpfs mount point and move both to "/"
      if(::syscall(SYS_pivot_root, mount_point_path.c_str(), mount_point_path.c_str()) < 0)
      {
        llfio::posix_error().throw_exception();
      }
      // Unmount the old root
      if(::umount2("/", MNT_DETACH) < 0)
      {
        llfio::posix_error().throw_exception();
      }
      if(::chdir("/") < 0)
      {
        llfio::posix_error().throw_exception();
      }
    }

    ~isolated_mount_namespace()
    {
      std::ignore = mount_namespace_handle.close();
      std::ignore = mount_point.unlink();
    }

    llfio::result<scoped_mount_namespace_join> join() noexcept
    {
      return scoped_mount_namespace_join::join(mount_namespace_handle);
    }

  private:
    llfio::directory_handle mount_point;
    llfio::file_handle mount_namespace_handle;
  };

  void TestProcBaseSetProcBase()
  {
    auto proc_path = llfio::directory({}, "/proc");
    BOOST_REQUIRE(proc_path);
    int fd_number{proc_path.value().native_handle().fd};

    auto set_result = llfio::set_proc_base(std::move(proc_path).value());
    BOOST_CHECK(set_result);
    if(set_result.value().fd != fd_number)
    {
      std::cerr << "WARNING: proc_base was already set. May need to run test earlier in the process." << std::endl;
    }
  }

  void TestProcBaseUtilsPageSizes()
  {
    if(!try_setup())
    {
      std::cerr << "WARNING: Failed to unshare user namespace, assuming unprivileged_userns_clone is disabled on this "
                   "system, skipping test"
                << std::endl;
      return;
    }
    isolated_mount_namespace mount_namespace;
    auto mount_namespace_guard = mount_namespace.join().value();

    BOOST_CHECK(!llfio::utils::page_sizes().empty());
  }

  void TestProcBaseUtilsDropFilesystemCache()
  {
    if(!try_setup())
    {
      std::cerr << "WARNING: Failed to unshare user namespace, assuming unprivileged_userns_clone is disabled on this "
                   "system, skipping test"
                << std::endl;
      return;
    }
    isolated_mount_namespace mount_namespace;
    auto mount_namespace_guard = mount_namespace.join().value();

    auto drop_cache_result = llfio::utils::drop_filesystem_cache();
    if(drop_cache_result.has_error() && drop_cache_result.error() == std::errc::permission_denied)
    {
      std::cerr << "WARNING: Not permitted to drop caches, skipping test" << std::endl;
      return;
    }
    BOOST_CHECK(drop_cache_result);
  }

  void TestProcBaseUtilsCurrentProcessMemoryUsage()
  {
    if(!try_setup())
    {
      std::cerr << "WARNING: Failed to unshare user namespace, assuming unprivileged_userns_clone is disabled on this "
                   "system, skipping test"
                << std::endl;
      return;
    }
    isolated_mount_namespace mount_namespace;
    auto mount_namespace_guard = mount_namespace.join().value();

    auto usage = llfio::utils::current_process_memory_usage(llfio::utils::process_memory_usage::want::all);
    BOOST_REQUIRE(usage);
    BOOST_CHECK(usage.value().private_paged_in > 0);
    BOOST_CHECK(usage.value().private_committed > 0);
    BOOST_CHECK(usage.value().system_physical_memory_total > 0);
  }

  void TestProcBaseUtilsCurrentProcessCpuUsage()
  {
    if(!try_setup())
    {
      std::cerr << "WARNING: Failed to unshare user namespace, assuming unprivileged_userns_clone is disabled on this "
                   "system, skipping test"
                << std::endl;
      return;
    }
    isolated_mount_namespace mount_namespace;
    auto mount_namespace_guard = mount_namespace.join().value();

    auto usage = llfio::utils::current_process_cpu_usage();
    BOOST_REQUIRE(usage);
    BOOST_CHECK(usage.value().system_ns_in_kernel_mode > 0);
    BOOST_CHECK(usage.value().process_ns_in_kernel_mode > 0);
  }

  void TestProcBaseStorageProfileSystemCpu()
  {
    if(!try_setup())
    {
      std::cerr << "WARNING: Failed to unshare user namespace, assuming unprivileged_userns_clone is disabled on this "
                   "system, skipping test"
                << std::endl;
      return;
    }
    isolated_mount_namespace mount_namespace;
    auto mount_namespace_guard = mount_namespace.join().value();

    llfio::storage_profile::storage_profile profile{};
    llfio::file_handle unused_handle{};
    BOOST_CHECK(llfio::storage_profile::system::cpu(profile, unused_handle));
    BOOST_CHECK(profile.cpu_physical_cores.value > 0);
  }

  void TestProcBaseMemoryAccounting()
  {
    if(!try_setup())
    {
      std::cerr << "WARNING: Failed to unshare user namespace, assuming unprivileged_userns_clone is disabled on this "
                   "system, skipping test"
                << std::endl;
      return;
    }
    isolated_mount_namespace mount_namespace;
    auto mount_namespace_guard = mount_namespace.join().value();

    BOOST_CHECK(llfio::map_handle::memory_accounting() != llfio::map_handle::memory_accounting_kind::unknown);
  }

  void TestProcBaseFsCurrentPath()
  {
    if(!try_setup())
    {
      std::cerr << "WARNING: Failed to unshare user namespace, assuming unprivileged_userns_clone is disabled on this "
                   "system, skipping test"
                << std::endl;
      return;
    }
    isolated_mount_namespace mount_namespace;
    auto mount_namespace_guard = mount_namespace.join().value();

    llfio::path_view expected_path = "/TestProcBaseCurrentPath";
    auto dirh = llfio::directory({}, expected_path, llfio::directory_handle::mode::write,
                                 llfio::directory_handle::creation::only_if_not_exist);
    BOOST_REQUIRE(dirh);

    auto actual_path = dirh.value().current_path();
    BOOST_REQUIRE(actual_path);
    BOOST_CHECK(expected_path.compare(actual_path.value()) == 0);
  }

  void TestProcBaseProcessCurrentPath()
  {
    if(!try_setup())
    {
      std::cerr << "WARNING: Failed to unshare user namespace, assuming unprivileged_userns_clone is disabled on this "
                   "system, skipping test"
                << std::endl;
      return;
    }
    isolated_mount_namespace mount_namespace;
    auto mount_namespace_guard = mount_namespace.join().value();

    BOOST_CHECK(llfio::process_handle::current().current_path());
  }

  void TestProcBaseStatfs()
  {
    if(!try_setup())
    {
      std::cerr << "WARNING: Failed to unshare user namespace, assuming unprivileged_userns_clone is disabled on this "
                   "system, skipping test"
                << std::endl;
      return;
    }
    isolated_mount_namespace mount_namespace;
    auto mount_namespace_guard = mount_namespace.join().value();

    auto root_handle = llfio::path("/");
    BOOST_REQUIRE(root_handle);

    llfio::statfs_t statfs{};
    BOOST_REQUIRE(statfs.fill(root_handle.value(), llfio::statfs_t::want::all));
    BOOST_CHECK(statfs.f_fstypename == "tmpfs");
  }

}  // namespace

KERNELTEST_TEST_KERNEL(integration, llfio, proc_base, set_proc_base, "Tests that set_proc_base() works",
                       TestProcBaseSetProcBase())

KERNELTEST_TEST_KERNEL(integration, llfio, proc_base, util_page_sizes,
                       "Tests that util::page_sizes() works without /proc mounted in the current mount namespace",
                       TestProcBaseUtilsPageSizes())

KERNELTEST_TEST_KERNEL(
integration, llfio, proc_base, util_drop_filesystem_cache,
"Tests that util::drop_filesystem_cache() works without /proc mounted in the current mount namespace",
TestProcBaseUtilsDropFilesystemCache())

KERNELTEST_TEST_KERNEL(
integration, llfio, proc_base, util_current_process_memory_usage,
"Tests that util::current_process_memory_usage() works without /proc mounted in the current mount namespace",
TestProcBaseUtilsCurrentProcessMemoryUsage())

KERNELTEST_TEST_KERNEL(
integration, llfio, proc_base, util_current_process_cpu_usage,
"Tests that util::current_process_cpu_usage() works without /proc mounted in the current mount namespace",
TestProcBaseUtilsCurrentProcessCpuUsage())

KERNELTEST_TEST_KERNEL(
integration, llfio, proc_base, storage_profile_system_cpu,
"Tests that storage_profile::system::cpu() works without /proc mounted in the current mount namespace",
TestProcBaseStorageProfileSystemCpu())

KERNELTEST_TEST_KERNEL(
integration, llfio, proc_base, memory_accounting,
"Tests that map_handle::memory_accounting() works without /proc mounted in the current mount namespace",
TestProcBaseMemoryAccounting())

KERNELTEST_TEST_KERNEL(integration, llfio, proc_base, handle_current_path,
                       "Tests that handle::current_path() works without /proc mounted in the current mount namespace",
                       TestProcBaseFsCurrentPath())

KERNELTEST_TEST_KERNEL(
integration, llfio, proc_base, process_handle_current_path,
"Tests that process_handle::current_path() works without /proc mounted in the current mount namespace",
TestProcBaseProcessCurrentPath())

KERNELTEST_TEST_KERNEL(integration, llfio, proc_base, statfs,
                       "Tests that statfs_t::fill works without /proc mounted in the current mount namespace",
                       TestProcBaseStatfs())

#endif
