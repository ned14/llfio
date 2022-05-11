/* A TLS socket source based on OpenSSL
(C) 2021-2022 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
File Created: Dec 2021


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

#include "../../../tls_socket_handle.hpp"

#define LLFIO_OPENSSL_ENABLE_DEBUG_PRINTING 0

#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#if LLFIO_OPENSSL_ENABLE_DEBUG_PRINTING
#include <iostream>
#endif

#ifdef _WIN32
#include <cryptuiapi.h>
#pragma comment(lib, "cryptui.lib")
#endif

LLFIO_V2_NAMESPACE_BEGIN

namespace detail
{
  /* Put ChaCha20 in front to aid lower end devices. Keep the list under fifty for embedded
  devices, so we disable CAMELLIA, ARIA, SHA, kRSA.

  We disable SHA for MAC and RSA for key exchange, which may cause problems for older clients?

  The non-authenticating ciphers are enabled in this list, disabled in the list below.

  This is the list my OpenSSL v1.1 offers in HELLO for this spec (52):

   TLS_AES_256_GCM_SHA384
   TLS_CHACHA20_POLY1305_SHA256
   TLS_AES_128_GCM_SHA256
   ECDHE-ECDSA-CHACHA20-POLY1305
   ECDHE-RSA-CHACHA20-POLY1305
   DHE-RSA-CHACHA20-POLY1305
   RSA-PSK-CHACHA20-POLY1305
   DHE-PSK-CHACHA20-POLY1305
   ECDHE-PSK-CHACHA20-POLY1305
   PSK-CHACHA20-POLY1305
   ECDHE-ECDSA-AES256-GCM-SHA384
   ECDHE-RSA-AES256-GCM-SHA384
   DHE-DSS-AES256-GCM-SHA384
   DHE-RSA-AES256-GCM-SHA384
   ECDHE-ECDSA-AES256-CCM8
   ECDHE-ECDSA-AES256-CCM
   DHE-RSA-AES256-CCM8
   DHE-RSA-AES256-CCM
   ADH-AES256-GCM-SHA384
   ECDHE-ECDSA-AES128-GCM-SHA256
   ECDHE-RSA-AES128-GCM-SHA256
   DHE-DSS-AES128-GCM-SHA256
   DHE-RSA-AES128-GCM-SHA256
   ECDHE-ECDSA-AES128-CCM8
   ECDHE-ECDSA-AES128-CCM
   DHE-RSA-AES128-CCM8
   DHE-RSA-AES128-CCM
   ADH-AES128-GCM-SHA256
   ECDHE-ECDSA-AES256-SHA384
   ECDHE-RSA-AES256-SHA384
   DHE-RSA-AES256-SHA256
   DHE-DSS-AES256-SHA256
   ADH-AES256-SHA256
   ECDHE-ECDSA-AES128-SHA256
   ECDHE-RSA-AES128-SHA256
   DHE-RSA-AES128-SHA256
   DHE-DSS-AES128-SHA256
   ADH-AES128-SHA256
   RSA-PSK-AES256-GCM-SHA384
   DHE-PSK-AES256-GCM-SHA384
   DHE-PSK-AES256-CCM8
   DHE-PSK-AES256-CCM
   PSK-AES256-GCM-SHA384
   PSK-AES256-CCM8
   PSK-AES256-CCM
   RSA-PSK-AES128-GCM-SHA256
   DHE-PSK-AES128-GCM-SHA256
   DHE-PSK-AES128-CCM8
   DHE-PSK-AES128-CCM
   PSK-AES128-GCM-SHA256
   PSK-AES128-CCM8
   PSK-AES128-CCM
*/
  static const char *openssl_unverified_cipher_list = "CHACHA20:HIGH:aNULL:!EXPORT:!LOW:!eNULL:!SSLv2:!SSLv3:!TLSv1.0:!CAMELLIA:!ARIA:!SHA:!kRSA:@SECLEVEL=0";
  // static const char *openssl_unverified_cipher_list = "CHACHA20:HIGH:!aNULL:!EXPORT:!LOW:!eNULL:!SSLv2:!SSLv3:!TLSv1.0:!CAMELLIA:!ARIA:!SHA:!kRSA";
  /* This is the list my OpenSSL v1.1 offers in HELLO for this spec (48):

   TLS_AES_256_GCM_SHA384
   TLS_CHACHA20_POLY1305_SHA256
   TLS_AES_128_GCM_SHA256
   ECDHE-ECDSA-CHACHA20-POLY1305
   ECDHE-RSA-CHACHA20-POLY1305
   DHE-RSA-CHACHA20-POLY1305
   RSA-PSK-CHACHA20-POLY1305
   DHE-PSK-CHACHA20-POLY1305
   ECDHE-PSK-CHACHA20-POLY1305
   PSK-CHACHA20-POLY1305
   ECDHE-ECDSA-AES256-GCM-SHA384
   ECDHE-RSA-AES256-GCM-SHA384
   DHE-DSS-AES256-GCM-SHA384
   DHE-RSA-AES256-GCM-SHA384
   ECDHE-ECDSA-AES256-CCM8
   ECDHE-ECDSA-AES256-CCM
   DHE-RSA-AES256-CCM8
   DHE-RSA-AES256-CCM
   ECDHE-ECDSA-AES128-GCM-SHA256
   ECDHE-RSA-AES128-GCM-SHA256
   DHE-DSS-AES128-GCM-SHA256
   DHE-RSA-AES128-GCM-SHA256
   ECDHE-ECDSA-AES128-CCM8
   ECDHE-ECDSA-AES128-CCM
   DHE-RSA-AES128-CCM8
   DHE-RSA-AES128-CCM
   ECDHE-ECDSA-AES256-SHA384
   ECDHE-RSA-AES256-SHA384
   DHE-RSA-AES256-SHA256
   DHE-DSS-AES256-SHA256
   ECDHE-ECDSA-AES128-SHA256
   ECDHE-RSA-AES128-SHA256
   DHE-RSA-AES128-SHA256
   DHE-DSS-AES128-SHA256
   RSA-PSK-AES256-GCM-SHA384
   DHE-PSK-AES256-GCM-SHA384
   DHE-PSK-AES256-CCM8
   DHE-PSK-AES256-CCM
   PSK-AES256-GCM-SHA384
   PSK-AES256-CCM8
   PSK-AES256-CCM
   RSA-PSK-AES128-GCM-SHA256
   DHE-PSK-AES128-GCM-SHA256
   DHE-PSK-AES128-CCM8
   DHE-PSK-AES128-CCM
   PSK-AES128-GCM-SHA256
   PSK-AES128-CCM8
   PSK-AES128-CCM
*/
  static const char *openssl_verified_cipher_list = "CHACHA20:HIGH:!aNULL:!EXPORT:!LOW:!eNULL:!SSLv2:!SSLv3:!TLSv1.0:!CAMELLIA:!ARIA:!SHA:!kRSA";
  static int openssl_library_code = ERR_get_next_error_library();
#define LLFIO_OPENSSL_SET_RESULT_ERROR(which) ERR_PUT_error(detail::openssl_library_code, 1, (which), nullptr, 0)
#if LLFIO_OPENSSL_ENABLE_DEBUG_PRINTING
  static std::mutex openssl_printing_lock;
#endif
}  // namespace detail

#if LLFIO_EXPERIMENTAL_STATUS_CODE
namespace detail
{
  struct openssl_error_domain final : public OUTCOME_V2_NAMESPACE::experimental::status_code_domain
  {
    using value_type = unsigned long;

    constexpr openssl_error_domain()
        : OUTCOME_V2_NAMESPACE::experimental::status_code_domain("{32158b31-4370-0ba7-1752-7b56c62610d2}")
    {
    }

    virtual string_ref name() const noexcept override { return string_ref("openssl"); }
    virtual payload_info_t payload_info() const noexcept override
    {
      return {sizeof(value_type), sizeof(status_code_domain *) + sizeof(value_type),
              (alignof(value_type) > alignof(status_code_domain *)) ? alignof(value_type) : alignof(status_code_domain *)};
    }
    static inline constexpr const openssl_error_domain &get();

    virtual bool _do_failure(const OUTCOME_V2_NAMESPACE::experimental::status_code<void> &code) const noexcept override
    {
      auto &c = static_cast<const OUTCOME_V2_NAMESPACE::experimental::status_code<openssl_error_domain> &>(code);
      return c.value() != 0;
    }
    virtual bool _do_equivalent(const OUTCOME_V2_NAMESPACE::experimental::status_code<void> &code1,
                                const OUTCOME_V2_NAMESPACE::experimental::status_code<void> &code2) const noexcept override
    {
      assert(code1.domain() == *this);                                                                                     // NOLINT
      const auto &c1 = static_cast<const OUTCOME_V2_NAMESPACE::experimental::status_code<openssl_error_domain> &>(code1);  // NOLINT
      if(code2.domain() == *this)
      {
        const auto &c2 = static_cast<const OUTCOME_V2_NAMESPACE::experimental::status_code<openssl_error_domain> &>(code2);  // NOLINT
        return c1.value() == c2.value();
      }
      return false;
    }
    virtual OUTCOME_V2_NAMESPACE::experimental::generic_code
    _generic_code(const OUTCOME_V2_NAMESPACE::experimental::status_code<void> &code) const noexcept override
    {
      auto &c = static_cast<const OUTCOME_V2_NAMESPACE::experimental::status_code<openssl_error_domain> &>(code);
      auto mapped = ERR_GET_REASON(c.value());
      switch(mapped)
      {
      case ERR_R_MALLOC_FAILURE:
        return errc::not_enough_memory;
      case ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED:
      case ERR_R_PASSED_NULL_PARAMETER:
      case ERR_R_PASSED_INVALID_ARGUMENT:
        return errc::invalid_argument;
      case ERR_R_INTERNAL_ERROR:
        return errc::state_not_recoverable;
      case ERR_R_DISABLED:
        return errc::operation_not_supported;
      case ERR_R_INIT_FAIL:
        return errc::device_or_resource_busy;
      case ERR_R_OPERATION_FAIL:
        return errc::io_error;
      }
      return errc::unknown;
    }
    virtual string_ref _do_message(const OUTCOME_V2_NAMESPACE::experimental::status_code<void> &code) const noexcept override
    {
      auto &c = static_cast<const OUTCOME_V2_NAMESPACE::experimental::status_code<openssl_error_domain> &>(code);
      if(c.value() == 0)
      {
        return string_ref("not an error");
      }
      auto *buffer = (char *) malloc(120);
      if(buffer == nullptr)
      {
        return string_ref("not enough memory to allocate this message");
      }
      ERR_error_string_n(c.value(), buffer, 120);
      return atomic_refcounted_string_ref(buffer);
    }
    SYSTEM_ERROR2_NORETURN virtual void _do_throw_exception(const OUTCOME_V2_NAMESPACE::experimental::status_code<void> &code) const override
    {
      auto &c = static_cast<const OUTCOME_V2_NAMESPACE::experimental::status_code<openssl_error_domain> &>(code);
      throw OUTCOME_V2_NAMESPACE::experimental::status_error<openssl_error_domain>(c);
    }
  };
  constexpr openssl_error_domain openssl_error_domain_inst;
  inline constexpr const openssl_error_domain &openssl_error_domain::get() { return openssl_error_domain_inst; }
  using openssl_code = OUTCOME_V2_NAMESPACE::experimental::status_code<openssl_error_domain>;

  struct x509_error_domain final : public OUTCOME_V2_NAMESPACE::experimental::status_code_domain
  {
    using value_type = int;

    constexpr x509_error_domain()
        : OUTCOME_V2_NAMESPACE::experimental::status_code_domain("{bea47e79-6787-6009-7ac7-ba8616575312}")
    {
    }

    virtual string_ref name() const noexcept override { return string_ref("x509"); }
    virtual payload_info_t payload_info() const noexcept override
    {
      return {sizeof(value_type), sizeof(status_code_domain *) + sizeof(value_type),
              (alignof(value_type) > alignof(status_code_domain *)) ? alignof(value_type) : alignof(status_code_domain *)};
    }
    static inline constexpr const x509_error_domain &get();

    virtual bool _do_failure(const OUTCOME_V2_NAMESPACE::experimental::status_code<void> &code) const noexcept override
    {
      auto &c = static_cast<const OUTCOME_V2_NAMESPACE::experimental::status_code<x509_error_domain> &>(code);
      return c.value() != 0;
    }
    virtual bool _do_equivalent(const OUTCOME_V2_NAMESPACE::experimental::status_code<void> &code1,
                                const OUTCOME_V2_NAMESPACE::experimental::status_code<void> &code2) const noexcept override
    {
      assert(code1.domain() == *this);                                                                                  // NOLINT
      const auto &c1 = static_cast<const OUTCOME_V2_NAMESPACE::experimental::status_code<x509_error_domain> &>(code1);  // NOLINT
      if(code2.domain() == *this)
      {
        const auto &c2 = static_cast<const OUTCOME_V2_NAMESPACE::experimental::status_code<x509_error_domain> &>(code2);  // NOLINT
        return c1.value() == c2.value();
      }
      return false;
    }
    virtual OUTCOME_V2_NAMESPACE::experimental::generic_code
    _generic_code(const OUTCOME_V2_NAMESPACE::experimental::status_code<void> &) const noexcept override
    {
      return errc::unknown;
    }
    virtual string_ref _do_message(const OUTCOME_V2_NAMESPACE::experimental::status_code<void> &code) const noexcept override
    {
      auto &c = static_cast<const OUTCOME_V2_NAMESPACE::experimental::status_code<x509_error_domain> &>(code);
      if(c.value() == 0)
      {
        return string_ref("not an error");
      }
      return string_ref(X509_verify_cert_error_string(c.value()));
    }
    SYSTEM_ERROR2_NORETURN virtual void _do_throw_exception(const OUTCOME_V2_NAMESPACE::experimental::status_code<void> &code) const override
    {
      auto &c = static_cast<const OUTCOME_V2_NAMESPACE::experimental::status_code<x509_error_domain> &>(code);
      throw OUTCOME_V2_NAMESPACE::experimental::status_error<x509_error_domain>(c);
    }
  };
  constexpr x509_error_domain x509_error_domain_inst;
  inline constexpr const x509_error_domain &x509_error_domain::get() { return x509_error_domain_inst; }
  using x509_code = OUTCOME_V2_NAMESPACE::experimental::status_code<x509_error_domain>;
}  // namespace detail
template <class T> inline result<void> openssl_error(T *inst, unsigned long errcode = ERR_get_error())
{
  if(ERR_GET_LIB(errcode) == detail::openssl_library_code)
  {
    if(inst == nullptr)
    {
      abort();
    }
    auto ret = (ERR_GET_REASON(errcode) == 2) ? std::move(inst->_write_error) : std::move(inst->_read_error);
#if LLFIO_OPENSSL_ENABLE_DEBUG_PRINTING
    std::lock_guard<std::mutex> g(detail::openssl_printing_lock);
    std::cerr << "OpenSSL underlying error: " << ret.error().message().c_str() << std::endl;
#endif
    return ret;
  }
  detail::openssl_code ret(errcode);
#if LLFIO_OPENSSL_ENABLE_DEBUG_PRINTING
  std::lock_guard<std::mutex> g(detail::openssl_printing_lock);
  std::cerr << "OpenSSL error: " << ret.message().c_str() << std::endl;
#endif
  assert(ret.failure());
  return ret;
}
inline result<void> openssl_error(std::nullptr_t, unsigned long errcode = ERR_get_error())
{
  if(ERR_GET_LIB(errcode) == detail::openssl_library_code)
  {
    abort();
  }
  detail::openssl_code ret(errcode);
#if LLFIO_OPENSSL_ENABLE_DEBUG_PRINTING
  std::lock_guard<std::mutex> g(detail::openssl_printing_lock);
  std::cerr << "OpenSSL error: " << ret.message().c_str() << std::endl;
#endif
  assert(ret.failure());
  return ret;
}
inline result<void> x509_error(int errcode)
{
  detail::x509_code ret(errcode);
#if LLFIO_OPENSSL_ENABLE_DEBUG_PRINTING
  std::lock_guard<std::mutex> g(detail::openssl_printing_lock);
  std::cerr << "X509 error: " << ret.message().c_str() << std::endl;
#endif
  assert(ret.failure());
  return ret;
}
#else
namespace detail
{
  struct openssl_error_category final : public std::error_category
  {
    virtual const char *name() const noexcept override { return "openssl"; }
    virtual std::error_condition default_error_condition(int code) const noexcept override
    {
      auto mapped = ERR_GET_REASON((unsigned long) code);
      switch(mapped)
      {
      case ERR_R_MALLOC_FAILURE:
        return errc::not_enough_memory;
      case ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED:
      case ERR_R_PASSED_NULL_PARAMETER:
      case ERR_R_PASSED_INVALID_ARGUMENT:
        return errc::invalid_argument;
      case ERR_R_INTERNAL_ERROR:
        return errc::state_not_recoverable;
      case ERR_R_DISABLED:
        return errc::operation_not_supported;
      case ERR_R_INIT_FAIL:
        return errc::device_or_resource_busy;
      case ERR_R_OPERATION_FAIL:
        return errc::io_error;
      }
      return {code, *this};
    }
    virtual std::string message(int code) const override
    {
      if(code == 0)
        return "not an error";
      char buffer[1024] = "";
      ERR_error_string_n((unsigned long) code, buffer, sizeof(buffer));
      return {buffer};
    }
  };
  struct x509_error_category final : public std::error_category
  {
    virtual const char *name() const noexcept override { return "x509"; }
    virtual std::error_condition default_error_condition(int code) const noexcept override { return {code, *this}; }
    virtual std::string message(int code) const override
    {
      if(code == 0)
        return "not an error";
      return X509_verify_cert_error_string(code);
    }
  };
}  // namespace detail
template <class T> inline result<void> openssl_error(T *inst, unsigned long errcode = ERR_get_error())
{
  if(ERR_GET_LIB(errcode) == detail::openssl_library_code)
  {
    if(inst == nullptr)
    {
      abort();
    }
    return (ERR_GET_REASON(errcode) == 2) ? std::move(inst->_write_error) : std::move(inst->_read_error);
  }
  static detail::openssl_error_category cat;
  error_info ret(std::error_code((int) errcode, cat));
#if LLFIO_OPENSSL_ENABLE_DEBUG_PRINTING
  std::lock_guard<std::mutex> g(detail::openssl_printing_lock);
  std::cout << "ERROR: " << ret.message() << std::endl;
#endif
  return ret;
}
inline result<void> openssl_error(std::nullptr_t, unsigned long errcode = ERR_get_error())
{
  if(ERR_GET_LIB(errcode) == detail::openssl_library_code)
  {
    abort();
  }
  static detail::openssl_error_category cat;
  return error_info(std::error_code((int) errcode, cat));
}
inline result<void> x509_error(int errcode)
{
  static detail::x509_error_category cat;
  return error_info(std::error_code((int) errcode, cat));
}
#endif

namespace detail
{
  static struct openssl_custom_bio_method_t
  {
    BIO_METHOD *method;
    openssl_custom_bio_method_t()
    {
      method = BIO_meth_new(BIO_get_new_index() | BIO_TYPE_SOURCE_SINK, "llfio");
      if(method == nullptr)
      {
        abort();
      }
      BIO_meth_set_write_ex(method, _bwrite);
      BIO_meth_set_read_ex(method, _bread);
      // BIO_meth_set_puts(method, );
      // BIO_meth_set_gets(method, );
      BIO_meth_set_ctrl(method, _ctrl);
      // BIO_meth_set_create(method, _create);
      // BIO_meth_set_destroy(method, _destroy);
      // BIO_meth_set_callback_ctrl(method, _callback_ctrl);
    }
    ~openssl_custom_bio_method_t()
    {
      BIO_meth_free(method);
      method = nullptr;
    }

    static inline int _bwrite(BIO *bio, const char *buffer, size_t bytes, size_t *written);
    static inline int _bread(BIO *bio, char *buffer, size_t bytes, size_t *read);
    static long _ctrl(BIO * /*unused*/, int cmd, long /*unused*/, void * /*unused*/)
    {
      switch(cmd)
      {
      case BIO_CTRL_FLUSH:
        return 1;
      case BIO_CTRL_WPENDING:
      case BIO_CTRL_PUSH:
      case BIO_CTRL_POP:
        return 0;
      }
      return 0;
    }
    // static int _create(BIO */*unused*/) { }
    // static int _destroy(BIO * /*unused*/) {}
    // static long _callback_ctrl(BIO *bio, int cmd, BIO_info_cb *cb) {}
  } openssl_custom_bio;

  static struct openssl_default_ctxs_t
  {
    SSL_CTX *unverified{nullptr}, *verified{nullptr};
    X509_STORE *certstore{nullptr};
    ~openssl_default_ctxs_t()
    {
      if(certstore != nullptr)
      {
        X509_STORE_free(certstore);
        certstore = nullptr;
      }
      if(verified != nullptr)
      {
        SSL_CTX_free(verified);
        verified = nullptr;
      }
      if(unverified != nullptr)
      {
        SSL_CTX_free(unverified);
        unverified = nullptr;
      }
    }
    result<void> init()
    {
      if(verified == nullptr)
      {
        static QUICKCPPLIB_NAMESPACE::configurable_spinlock::spinlock<unsigned> lock;
        QUICKCPPLIB_NAMESPACE::configurable_spinlock::lock_guard<QUICKCPPLIB_NAMESPACE::configurable_spinlock::spinlock<unsigned>> g(lock);
        if(verified == nullptr)
        {
#ifdef _WIN32
          // Create an OpenSSL certificate store made up of the certs from the Windows certificate store
          HCERTSTORE winstore = CertOpenSystemStoreW(NULL, L"ROOT");
          if(!winstore)
          {
            return win32_error();
          }
          auto unwinstore = make_scope_exit([&]() noexcept { CertCloseStore(winstore, 0); });
          certstore = X509_STORE_new();
          if(!certstore)
          {
            return openssl_error(nullptr);
          }
          PCCERT_CONTEXT context = nullptr;
          auto uncontext = make_scope_exit(
          [&]() noexcept
          {
            if(context != nullptr)
            {
              CertFreeCertificateContext(context);
            }
          });
          while((context = CertEnumCertificatesInStore(winstore, context)) != nullptr)
          {
            const unsigned char *in = (const unsigned char *) context->pbCertEncoded;
            X509 *x509 = d2i_X509(nullptr, &in, context->cbCertEncoded);
            if(!x509)
            {
              return openssl_error(nullptr);
            }
            auto unx509 = make_scope_exit([&]() noexcept { X509_free(x509); });
            //{
            //  X509_NAME_print_ex_fp(stdout, X509_get_issuer_name(x509), 3, 0);
            //  printf("\n");
            //}
            if(X509_STORE_add_cert(certstore, x509) <= 0)
            {
              return openssl_error(nullptr);
            }
          }
#endif
          auto make_ctx = [certstore = certstore](bool verify_peer) -> result<SSL_CTX *>
          {
            SSL_CTX *_ctx = SSL_CTX_new(TLS_method());
            if(_ctx == nullptr)
            {
              return openssl_error(nullptr).as_failure();
            }
            if(!verify_peer)
            {
              if(!SSL_CTX_set_cipher_list(_ctx, openssl_unverified_cipher_list))
              {
                return openssl_error(nullptr).as_failure();
              }
              SSL_CTX_set_verify(_ctx, SSL_VERIFY_NONE, nullptr);
            }
            else
            {
              if(certstore != nullptr)
              {
                SSL_CTX_set1_cert_store(_ctx, certstore);
              }
              if(!SSL_CTX_set_cipher_list(_ctx, openssl_verified_cipher_list))
              {
                return openssl_error(nullptr).as_failure();
              }
              SSL_CTX_set_verify(_ctx, SSL_VERIFY_PEER, nullptr);
              SSL_CTX_set_verify_depth(_ctx, 4);
              if(SSL_CTX_set_default_verify_paths(_ctx) <= 0)
              {
                return openssl_error(nullptr).as_failure();
              }
            }
            SSL_CTX_set_options(_ctx, SSL_OP_PRIORITIZE_CHACHA);
            SSL_CTX_set_min_proto_version(_ctx, TLS1_2_VERSION);
            SSL_CTX_set_ecdh_auto(_ctx, 1);
            SSL_CTX_set_dh_auto(_ctx, 1);
            return _ctx;
          };
          OUTCOME_TRY(unverified, make_ctx(false));
          OUTCOME_TRY(verified, make_ctx(true));
        }
      }
      return success();
    }
  } openssl_default_ctxs;
}  // namespace detail

class openssl_socket_handle final : public tls_socket_handle
{
  static constexpr size_t BUFFERS_COUNT = 2;
  template <class T> friend inline result<void> openssl_error(T *, unsigned long errcode);

  BIO *_ssl_bio{nullptr};
  BIO *_self_bio{nullptr};

  optional<filesystem::path> _authentication_certificates_path;
  std::string _connect_hostname_port;

  /* We use a registered buffer from the underlying transport for reads only, but not writes.
  The reason why not is that from my best reading of the implementation source code,
  OpenSSL doesn't seem to allow fixed size write buffers (as according to the documentation
  for BIO_set_mem_buf), plus OpenSSL seems to treat failure to allocate memory as an abort
  situation rather than a retry situation, so there seems to me that is no way of backpressuring
  a fixed size buffer in OpenSSL.

  Furthermore, the documentation for BUF_MEM suggests that the buffer must be a single
  contiguous region of memory, so we can't use a list of multiple registered
  buffers either. If your TLS implementation were better designed, it should be possible to
  use registered buffers for both reads and writes, and that then reduces CPU cache loading
  on a very busy server.

  Be aware that due to this design flaw in OpenSSL, it is possible to buffer writes to
  infinity i.e. it never returns a partial write, it always accepts fully every write. We
  therefore have to emulate backpressure below.
  */
  std::mutex _lock;
  std::unique_lock<std::mutex> _lock_holder{_lock, std::defer_lock};
  uint16_t _read_buffer_source_idx{0}, _read_buffer_sink_idx{0};
  bool _write_socket_full{false};
  uint8_t _still_connecting{0};
  deadline _read_deadline, _write_deadline;
  result<void> _read_error{success()}, _write_error{success()};
  std::chrono::steady_clock::time_point _read_deadline_began_steady, _write_deadline_began_steady;
  byte_socket_handle::registered_buffer_type _read_buffers[BUFFERS_COUNT]{};
  byte_socket_handle::buffer_type _read_buffers_valid[BUFFERS_COUNT]{};

  // Front of the queue
  std::pair<byte_socket_handle::registered_buffer_type *, byte_socket_handle::buffer_type *> _toread_source() noexcept
  {
    return {&_read_buffers[_read_buffer_source_idx % BUFFERS_COUNT], &_read_buffers_valid[_read_buffer_source_idx % BUFFERS_COUNT]};
  }
  bool _toread_source_empty() const noexcept { return _read_buffers_valid[_read_buffer_source_idx % BUFFERS_COUNT].empty(); }
  // Back of the queue. Can return "full"
  std::pair<byte_socket_handle::registered_buffer_type *, byte_socket_handle::buffer_type *> _toread_sink() noexcept
  {
    const auto idx = _read_buffer_sink_idx % BUFFERS_COUNT;
    if(idx == (_read_buffer_source_idx % BUFFERS_COUNT) &&
       (_read_buffers_valid[idx].data() + _read_buffers_valid[idx].size()) == (_read_buffers[idx]->data() + _read_buffers[idx]->size()))
    {
      return {nullptr, nullptr};  // full
    }
    return {&_read_buffers[idx], &_read_buffers_valid[idx]};
  }

#undef LLFIO_OPENSSL_DISPATCH
#define LLFIO_OPENSSL_DISPATCH(functp, functt, ...)                                                                                                            \
  ((_v.behaviour & native_handle_type::disposition::is_pointer) ? (reinterpret_cast<byte_socket_handle *>(_v.ptr)->functp) __VA_ARGS__ :                       \
                                                                  (tls_socket_handle::functt) __VA_ARGS__)

protected:
  virtual size_t _do_max_buffers() const noexcept override { return 1; /* There is no atomicity of scatter gather i/o at all! */ }
  virtual result<registered_buffer_type> _do_allocate_registered_buffer(size_t &bytes) noexcept override
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    return LLFIO_OPENSSL_DISPATCH(allocate_registered_buffer, _do_allocate_registered_buffer, (bytes));
  }
  virtual io_result<buffers_type> _do_read(io_request<buffers_type> reqs, deadline d) noexcept override
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    if(d && !this->is_nonblocking())
    {
      return errc::not_supported;
    }
    _lock_holder.lock();
    auto unlock = make_scope_exit(
    [this]() noexcept
    {
      if(_lock_holder.owns_lock())
      {
        _lock_holder.unlock();
      }
    });
    if(!(_v.behaviour & native_handle_type::disposition::_is_connected) || _still_connecting > 0)
    {
      return errc::not_connected;
    }
    LLFIO_DEADLINE_TO_SLEEP_INIT(d);
    if(d)
    {
      _read_deadline_began_steady = began_steady;
      _read_deadline = d;
    }
    else
    {
      _read_deadline = {};
    }
    for(size_t n = 0; n < reqs.buffers.size(); n++)
    {
      size_t read = 0;
      // OpenSSL early outs if buf is ever null
      byte dummy{}, *buf = reqs.buffers[n].empty() ? &dummy : reqs.buffers[n].data();
      auto res = BIO_read_ex(_ssl_bio, buf, reqs.buffers[n].size(), &read);
      if(res <= 0)
      {
        auto errcode = ERR_get_error();
        if(n > 0 || errcode == 0)
        {
          // Sink the error, return what we've already read.
          reqs.buffers = {reqs.buffers.data(), n};
          break;
        }
        if(BIO_should_retry(_ssl_bio))
        {
          return errc::operation_would_block;
        }
        return openssl_error(this, errcode).as_failure();
      }
      if(read < reqs.buffers[n].size())
      {
        reqs.buffers[n] = {reqs.buffers[n].data(), read};
        if(n == 0 && read == 0)
        {
          reqs.buffers = {reqs.buffers.data(), n};
          return std::move(reqs.buffers);
        }
        reqs.buffers = {reqs.buffers.data(), n + 1};
        break;
      }
    }
    return std::move(reqs.buffers);
  }
  virtual io_result<const_buffers_type> _do_write(io_request<const_buffers_type> reqs, deadline d) noexcept override
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    if(d && !this->is_nonblocking())
    {
      return errc::not_supported;
    }
    _lock_holder.lock();
    auto unlock = make_scope_exit(
    [this]() noexcept
    {
      if(_lock_holder.owns_lock())
      {
        _lock_holder.unlock();
      }
    });
    if(!(_v.behaviour & native_handle_type::disposition::_is_connected) || _still_connecting > 0)
    {
      return errc::not_connected;
    }
    LLFIO_DEADLINE_TO_SLEEP_INIT(d);
    if(d)
    {
      _write_deadline_began_steady = began_steady;
      _write_deadline = d;
    }
    else
    {
      _write_deadline = {};
    }
    // OpenSSL will accept new writes forever, so we need to emulate write backpressure
    if(_write_socket_full)
    {
      // Write nothing new to OpenSSL, should cause _bwrite() to get called which will check
      // if the socket's write buffers have drained
      size_t written = 0;
      auto res = BIO_write_ex(_ssl_bio, nullptr, 0, &written);
      if(res <= 0)
      {
        auto errcode = ERR_get_error();
        if(BIO_should_retry(_ssl_bio))
        {
          return errc::operation_would_block;
        }
        return openssl_error(this, errcode).as_failure();
      }
      if(_write_socket_full)
      {
        // Return no buffers written
        reqs.buffers = {reqs.buffers.data(), size_t(0)};
        return std::move(reqs.buffers);
      }
    }
    for(size_t n = 0; n < reqs.buffers.size(); n++)
    {
      size_t written = 0;
      auto res = BIO_write_ex(_ssl_bio, reqs.buffers[n].data(), reqs.buffers[n].size(), &written);
      if(res <= 0)
      {
        auto errcode = ERR_get_error();
        if(n > 0 || errcode == 0)
        {
          // Sink the error, return what we've already written.
          reqs.buffers = {reqs.buffers.data(), n};
          break;
        }
        if(BIO_should_retry(_ssl_bio))
        {
          return errc::operation_would_block;
        }
        return openssl_error(this, errcode).as_failure();
      }
      if(written < reqs.buffers[n].size())
      {
        reqs.buffers[n] = {reqs.buffers[n].data(), written};
        if(n == 0 && written == 0)
        {
          reqs.buffers = {reqs.buffers.data(), n};
          return std::move(reqs.buffers);
        }
        reqs.buffers = {reqs.buffers.data(), n + 1};
        break;
      }
    }
    if(this->are_writes_durable())
    {
      deadline nd;
      LLFIO_DEADLINE_TO_PARTIAL_DEADLINE(nd, d);
      OUTCOME_TRY(_flush_towrite(nd));
    }
    return std::move(reqs.buffers);
  }

  virtual result<void> _do_connect(const ip::address &addr, deadline d) noexcept override
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    if(d && !this->is_nonblocking())
    {
      return errc::not_supported;
    }
    _lock_holder.lock();
    auto unlock = make_scope_exit(
    [this]() noexcept
    {
      if(_lock_holder.owns_lock())
      {
        _lock_holder.unlock();
      }
    });
    if(_ssl_bio == nullptr)
    {
      OUTCOME_TRY(_init(true, _authentication_certificates_path));
    }
    if(!(_v.behaviour & native_handle_type::disposition::_is_connected) || _still_connecting > 0)
    {
      LLFIO_DEADLINE_TO_SLEEP_INIT(d);
      OUTCOME_TRY(LLFIO_OPENSSL_DISPATCH(connect, _do_connect, (addr, d)));
      for(;;)
      {
        deadline nd;
        LLFIO_DEADLINE_TO_PARTIAL_DEADLINE(nd, d);
        if(nd)
        {
          _read_deadline_began_steady = began_steady;
          _write_deadline_began_steady = began_steady;
          _read_deadline = d;
          _write_deadline = d;
        }
        else
        {
          _read_deadline = {};
          _write_deadline = {};
        }
        if(_still_connecting < 1)
        {
          auto res = BIO_do_connect(_ssl_bio);
          _still_connecting = 1;
          if(res != 1)
          {
            if(BIO_should_retry(_ssl_bio))
            {
              return errc::operation_in_progress;
            }
            return openssl_error(this).as_failure();
          }
        }
        {
          auto res = BIO_do_handshake(_ssl_bio);
          if(res != 1)
          {
            if(BIO_should_retry(_ssl_bio))
            {
              return errc::operation_in_progress;
            }
            return openssl_error(this).as_failure();
          }
        }
        break;
      }
      if(!_connect_hostname_port.empty())
      {
        SSL *ssl{nullptr};
        BIO_get_ssl(_ssl_bio, &ssl);
        if(ssl == nullptr)
        {
          return openssl_error(this).as_failure();
        }
        X509 *cert = SSL_get_peer_certificate(ssl);
        if(cert != nullptr)
        {
          X509_free(cert);
        }
        else
        {
          return openssl_error(this).as_failure();
        }
        auto res = SSL_get_verify_result(ssl);
        if(X509_V_OK != res)
        {
          return x509_error(res).as_failure();
        }
      }
      _v.behaviour |= native_handle_type::disposition::_is_connected;
      _still_connecting = 0;
    }
    return success();
  }

public:
  explicit openssl_socket_handle(byte_socket_handle sock)
      : tls_socket_handle(std::move(sock))
  {
  }
  explicit openssl_socket_handle(byte_socket_handle *sock)
      : tls_socket_handle(handle(), nullptr)
  {
    this->_v.ptr = sock;
    this->_v.behaviour = (sock->native_handle().behaviour & ~(native_handle_type::disposition::kernel_handle)) | native_handle_type::disposition::is_pointer;
  }

  result<void> _init(bool is_client, const optional<filesystem::path> &certpath) noexcept
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    const bool verify_peer =
    (is_client && (!certpath.has_value() || (certpath.has_value() && !certpath->empty()))) || (!is_client && (!certpath.has_value() || !certpath->empty()));
    OUTCOME_TRY(detail::openssl_default_ctxs.init());
    assert(_ssl_bio == nullptr);
    _ssl_bio = BIO_new_ssl(verify_peer ? detail::openssl_default_ctxs.verified : detail::openssl_default_ctxs.unverified, is_client);
    if(_ssl_bio == nullptr)
    {
      return openssl_error(this).as_failure();
    }
    if(certpath.has_value() && !certpath->empty())
    {
      SSL *ssl{nullptr};
      BIO_get_ssl(_ssl_bio, &ssl);
      if(ssl == nullptr)
      {
        return openssl_error(this).as_failure();
      }
      if(SSL_use_certificate_file(ssl, certpath->string().c_str(), SSL_FILETYPE_PEM) <= 0)
      {
        return openssl_error(this).as_failure();
      }
      if(SSL_use_PrivateKey_file(ssl, certpath->string().c_str(), SSL_FILETYPE_PEM) <= 0)
      {
        return openssl_error(this).as_failure();
      }
    }
    _self_bio = BIO_new(detail::openssl_custom_bio.method);
    if(_self_bio == nullptr)
    {
      return openssl_error(this).as_failure();
    }
    BIO_set_data(_self_bio, this);
    BIO_push(_ssl_bio, _self_bio);
    return success();
  }

  virtual result<void> shutdown(shutdown_kind kind) noexcept override
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    if(kind == shutdown_write || kind == shutdown_both)
    {
      _lock_holder.lock();
      auto unlock = make_scope_exit(
      [this]() noexcept
      {
        if(_lock_holder.owns_lock())
        {
          _lock_holder.unlock();
        }
      });
      SSL *ssl{nullptr};
      BIO_get_ssl(_ssl_bio, &ssl);
      if(ssl == nullptr)
      {
        return openssl_error(this).as_failure();
      }
      for(;;)
      {
        auto res = SSL_shutdown(ssl);
        if(res == 1)
        {
          break;
        }
        if(res < 0)
        {
          auto e = SSL_get_error(ssl, res);
          if(e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE)
          {
            return errc::operation_in_progress;
          }
          if(e == SSL_ERROR_SSL)
          {
            // Already shut down?
            break;
          }
          return openssl_error(this, e).as_failure();
        }
        // Shutdown is in progress
        if(this->is_nonblocking())
        {
          return errc::operation_in_progress;
        }
        OUTCOME_TRY(_flush_towrite({}));
      }
      OUTCOME_TRY(LLFIO_OPENSSL_DISPATCH(shutdown, shutdown, (kind)));
    }
    return success();
  }

  virtual ~openssl_socket_handle() override
  {
    if(_v)
    {
      auto r = openssl_socket_handle::close();
      if(!r)
      {
        // std::cout << r.error().message() << std::endl;
        LLFIO_LOG_FATAL(_v.fd, "openssl_socket_handle::~openssl_socket_handle() close failed");
        abort();
      }
    }
  }
  virtual result<void> close() noexcept override
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    if(_v)
    {
      _lock_holder.lock();
      auto unlock = make_scope_exit(
      [this]() noexcept
      {
        if(_lock_holder.owns_lock())
        {
          _lock_holder.unlock();
        }
      });
      OUTCOME_TRY(_flush_towrite({}));
      if(are_safety_barriers_issued() && is_writable())
      {
        _lock_holder.unlock();
        auto r = shutdown(shutdown_write);
        _lock_holder.lock();
        if(r)
        {
          byte buffer[4096];
          for(;;)
          {
            _lock_holder.unlock();
            OUTCOME_TRY(auto readed, read(0, {{buffer}}));
            _lock_holder.lock();
            if(readed == 0)
            {
              break;
            }
          }
        }
        else if(r.error() != errc::not_connected)
        {
          OUTCOME_TRY(std::move(r));
        }
      }
      if(_ssl_bio != nullptr)
      {
        BIO_free_all(_ssl_bio);  // also frees _self_bio
        _ssl_bio = nullptr;
      }
      if(_v.behaviour & native_handle_type::disposition::is_pointer)
      {
        tls_socket_handle::release();
      }
      else
      {
        if(!!(_v.behaviour & native_handle_type::disposition::_is_connected) && _still_connecting == 0)
        {
          _lock_holder.unlock();
          auto r = tls_socket_handle::close();
          if(!r)
          {
            char msg[1024];
#ifdef _MSC_VER
            sprintf_s(msg, "WARNING: openssl_socket_handle::close() underlying close() fails with %s", r.error().message().c_str());
#else
            sprintf(msg, "WARNING: openssl_socket_handle::close() underlying close() fails with %s", r.error().message().c_str());
#endif
            LLFIO_LOG_WARN(this, msg);
          }
          _lock_holder.lock();
        }
      }
      for(size_t n = 0; n < BUFFERS_COUNT; n++)
      {
        _read_buffers_valid[n] = {};
        _read_buffers[n].reset();
      }
    }
    return success();
  }

  virtual std::string algorithms_description() const override
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    if(_ssl_bio == nullptr)
    {
      if(!detail::openssl_default_ctxs.init())
      {
        return {};
      }
      auto ctx = (!_authentication_certificates_path || !_authentication_certificates_path->empty()) ? detail::openssl_default_ctxs.verified :
                                                                                                       detail::openssl_default_ctxs.unverified;
      auto *ciphers = SSL_CTX_get_ciphers(ctx);
      if(ciphers != nullptr)
      {
        std::string ret;
        for(int n = 0; n < sk_SSL_CIPHER_num(ciphers); n++)
        {
          if(n > 0)
          {
            ret.push_back(',');
          }
          ret.append(SSL_CIPHER_get_name(sk_SSL_CIPHER_value(ciphers, n)));
        }
        return ret;
      }
    }
    else
    {
      SSL *ssl{nullptr};
      BIO_get_ssl(_ssl_bio, &ssl);
      if(!(_v.behaviour & native_handle_type::disposition::_is_connected))
      {
        auto *ciphers = SSL_get1_supported_ciphers(ssl);
        if(ciphers != nullptr)
        {
          auto unciphers = make_scope_exit([&]() noexcept { sk_SSL_CIPHER_free(ciphers); });
          std::string ret;
          for(int n = 0; n < sk_SSL_CIPHER_num(ciphers); n++)
          {
            if(n > 0)
            {
              ret.push_back(',');
            }
            ret.append(SSL_CIPHER_get_name(sk_SSL_CIPHER_value(ciphers, n)));
          }
          return ret;
        }
      }
      else
      {
        auto *cipher = SSL_get_current_cipher(ssl);
        if(cipher != nullptr)
        {
          return SSL_CIPHER_get_name(cipher);
        }
      }
    }
    return {};
  }

  virtual result<void> set_registered_buffer_chunk_size(size_t bytes) noexcept override
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    _lock_holder.lock();
    auto unlock = make_scope_exit(
    [this]() noexcept
    {
      if(_lock_holder.owns_lock())
      {
        _lock_holder.unlock();
      }
    });
    if(!_toread_source_empty())
    {
      return errc::device_or_resource_busy;
    }
    if(!_read_buffers[0] || _read_buffers[0]->size() != bytes)
    {
      for(size_t n = 0; n < BUFFERS_COUNT; n++)
      {
        auto _bytes = bytes;
        OUTCOME_TRY(_read_buffers[n], LLFIO_OPENSSL_DISPATCH(allocate_registered_buffer, allocate_registered_buffer, (_bytes)));
        _read_buffers_valid[n] = {_read_buffers[n]->data(), 0};
      }
    }
    return success();
  }

  virtual result<void> set_algorithms(tls_algorithm set) noexcept override
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    // OpenSSL v1.1 doesn't implement FIPS 840-2, so fail if that is set.
    if(set & tls_algorithm::FIPS_140_2)
    {
      return errc::not_supported;
    }
    return success();
  }

  virtual result<void> set_authentication_certificates_path(path_view identifier) noexcept override
  {
    try
    {
      LLFIO_LOG_FUNCTION_CALL(this);
      _authentication_certificates_path = identifier.path();
      return success();
    }
    catch(...)
    {
      return error_from_exception();
    }
  }

  virtual result<string_view> set_connect_hostname(string_view host, uint16_t port) noexcept override
  {
    try
    {
      LLFIO_LOG_FUNCTION_CALL(this);
      _connect_hostname_port.assign(host.data(), host.size());
      _connect_hostname_port.push_back(':');
      _connect_hostname_port.append(std::to_string(port));
      if(_ctx == nullptr)
      {
        OUTCOME_TRY(_init(true, _authentication_certificates_path));
      }
      auto res = BIO_set_conn_hostname(_ssl_bio, _connect_hostname_port.c_str());
      /* if(res != 1)
      {
        return openssl_error(this).as_failure();
      }*/
      SSL *ssl{nullptr};
      BIO_get_ssl(_ssl_bio, &ssl);
      if(ssl == nullptr)
      {
        return openssl_error(this).as_failure();
      }
      std::string hostname(host);
      res = SSL_set_tlsext_host_name(ssl, hostname.c_str());
      if(res != 1)
      {
        return openssl_error(this).as_failure();
      }
      return string_view(_connect_hostname_port).substr(host.size() + 1);
    }
    catch(...)
    {
      return error_from_exception();
    }
  }

  // Read the underlying socket. Lock should be held!
  int _bread(BIO *bio, char *buffer, size_t bytes, size_t *read)
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    auto ret = [=
#if __cplusplus >= 202000L || _HAS_CXX20
                ,
                this
#endif
    ]() mutable
    {
      assert(_lock_holder.owns_lock());
      *read = 0;
      BIO_clear_retry_flags(bio);
      auto copy_out = [&]
      {
        while(!_toread_source_empty() && bytes > 0)
        {
          auto s = _toread_source();
          auto tocopy = std::min(s.second->size(), bytes);
          memcpy(buffer, (byte *) s.second->data(), tocopy);
          buffer += tocopy;
          bytes -= tocopy;
          *read += tocopy;
          *s.second = {s.second->data() + tocopy, s.second->size() - tocopy};
          if(s.second->data() == (*s.first)->data() + (*s.first)->size())
          {
            *s.second = {(*s.first)->data(), 0};
            _read_buffer_source_idx++;
          }
        }
      };
      copy_out();
      // Only do a speculative buffer refill if underlying socket is nonblocking
      if(!this->is_nonblocking() && *read > 0)
      {
        return 1;
      }
      // Fill more of the buffer
      auto s = _toread_sink();
      // Are we full?
      if(s.second == nullptr)
      {
        if(*read == 0)
        {
          BIO_set_retry_read(bio);
          return 0;
        }
        return 1;
      }
      auto remaining = (size_t) (((*s.first)->data() + (*s.first)->size()) - (s.second->data() + s.second->size()));
      byte_socket_handle::buffer_type b{s.second->data() + s.second->size(), remaining};
      auto &began_steady = _read_deadline_began_steady;
      deadline nd;
      if(this->is_nonblocking())
      {
        if(*read == 0)
        {
          LLFIO_DEADLINE_TO_PARTIAL_DEADLINE(nd, _read_deadline);
        }
        else
        {
          // Don't block if we have data to return
          nd = std::chrono::seconds(0);
        }
      }
      _lock_holder.unlock();
      assert(!requires_aligned_io());
      assert(_v.is_valid());
      auto r = LLFIO_OPENSSL_DISPATCH(read, _do_read, (*s.first, {{&b, 1}, 0}, nd));
      _lock_holder.lock();
      if(!r)
      {
        // Return an error if we never read any data, otherwise sink the error
        if(*read > 0)
        {
          return 1;
        }
        _read_error = std::move(r).as_failure();
        LLFIO_OPENSSL_SET_RESULT_ERROR(1);
        return 0;
      }
      *s.second = {s.second->data(), s.second->size() + b.size()};
      if(b.size() == remaining)
      {
        _read_buffer_sink_idx++;
      }
      copy_out();
      if(*read > 0)
      {
        return 1;
      }
      BIO_set_retry_read(bio);
      return 0;
    }();
#if LLFIO_OPENSSL_ENABLE_DEBUG_PRINTING
    std::lock_guard<std::mutex> g(detail::openssl_printing_lock);
    auto s = _toread_source();
    std::cout << this << " _bread(" << (void *) buffer << ", " << bytes << ") returns " << ret << " with " << *read << " bytes read and " << s.second->size()
              << " remaining in source buffer." << std::endl;
#endif
    return ret;
  }

  // Write the underlying socket. Lock should be held!
  int _bwrite(BIO *bio, const char *buffer, size_t bytes, size_t *written)
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    auto ret = [=
#if __cplusplus >= 202000L || _HAS_CXX20
                ,
                this
#endif
    ]() mutable
    {
      assert(_lock_holder.owns_lock());
      *written = 0;
      BIO_clear_retry_flags(bio);
      auto &began_steady = _write_deadline_began_steady;
      deadline nd;
      if(this->is_nonblocking())
      {
        LLFIO_DEADLINE_TO_PARTIAL_DEADLINE(nd, _write_deadline);
      }
      _lock_holder.unlock();
      assert(!requires_aligned_io());
      assert(_v.is_valid());
      const_buffer_type b((const byte *) buffer, bytes);
      auto r = LLFIO_OPENSSL_DISPATCH(write, _do_write, ({{&b, 1}, 0}, nd));
      _lock_holder.lock();
      if(!r)
      {
        _write_error = std::move(r).as_failure();
        LLFIO_OPENSSL_SET_RESULT_ERROR(2);
        return 0;
      }
      _write_socket_full = (b.size() == 0);
      buffer += b.size();
      bytes -= b.size();
      *written += b.size();
      return 1;
    }();
#if LLFIO_OPENSSL_ENABLE_DEBUG_PRINTING
    std::lock_guard<std::mutex> g(detail::openssl_printing_lock);
    std::cout << this << "_bwrite(" << (void *) buffer << ", " << bytes << ") returns " << ret << " with " << *written << " bytes written." << std::endl;
#endif
    return ret;
  }

  // Flush the towrite buffer, which may need to pump both reads and writes. Lock must be held!
  result<void> _flush_towrite(deadline d) noexcept
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    LLFIO_DEADLINE_TO_SLEEP_INIT(d);
    assert(_lock_holder.owns_lock());
    if(_ssl_bio != nullptr)
    {
      auto res = BIO_flush(_ssl_bio);
      if(res <= 0 && !BIO_should_retry(_ssl_bio))
      {
        return openssl_error(this).as_failure();
      }
    }
    return success();
  }
};

namespace detail
{
  inline int openssl_custom_bio_method_t::_bwrite(BIO *bio, const char *buffer, size_t bytes, size_t *written)
  {
    auto *h = (openssl_socket_handle *) BIO_get_data(bio);
    return h->_bwrite(bio, buffer, bytes, written);
  }
  inline int openssl_custom_bio_method_t::_bread(BIO *bio, char *buffer, size_t bytes, size_t *read)
  {
    auto *h = (openssl_socket_handle *) BIO_get_data(bio);
    return h->_bread(bio, buffer, bytes, read);
  }
}  // namespace detail

/************************************************************************************************************/

class listening_openssl_socket_handle final : public listening_tls_socket_handle
{
  size_t _registered_buffer_chunk_size{4096};
  optional<filesystem::path> _authentication_certificates_path;

#undef LLFIO_OPENSSL_DISPATCH
  /* *this has a vptr whose functions point into this class, so what
  we need is to bind the function implementation listening_byte_socket_handle
  using its vptr and call it.
  */
#define LLFIO_OPENSSL_DISPATCH(functp, functt, ...)                                                                                                            \
  ((_v.behaviour & native_handle_type::disposition::is_pointer) ? (reinterpret_cast<listening_byte_socket_handle *>(_v.ptr)->functp) __VA_ARGS__ :             \
                                                                  (listening_byte_socket_handle::functt) __VA_ARGS__)

protected:
  virtual result<buffers_type> _do_read(io_request<buffers_type> req, deadline d) noexcept override
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    if(d && !this->is_nonblocking())
    {
      return errc::not_supported;
    }
    listening_byte_socket_handle::buffer_type b;
    OUTCOME_TRY(auto &&read, _underlying_read<listening_byte_socket_handle>({b}, d));
    assert(this->is_nonblocking() == b.first.is_nonblocking());
    auto *p = new(std::nothrow) openssl_socket_handle(std::move(read.connected_socket().first));
    if(p == nullptr)
    {
      return errc::not_enough_memory;
    }
    assert(this->is_nonblocking() == p->is_nonblocking());
    req.buffers.connected_socket() = {tls_socket_handle_ptr(p), read.connected_socket().second};
    OUTCOME_TRY(p->set_registered_buffer_chunk_size(_registered_buffer_chunk_size));
    OUTCOME_TRY(p->_init(false, _authentication_certificates_path));
    return {std::move(req.buffers)};
  }

  virtual io_result<buffers_type> _do_multiplexer_read(io_request<buffers_type> req, deadline d) noexcept override
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    if(d && !this->is_nonblocking())
    {
      return errc::not_supported;
    }
    listening_byte_socket_handle::buffer_type b;
    OUTCOME_TRY(auto &&read, _underlying_read<listening_byte_socket_handle>({b}, d));
    assert(this->is_nonblocking() == b.first.is_nonblocking());
    auto *p = new(std::nothrow) openssl_socket_handle(std::move(read.connected_socket().first));
    if(p == nullptr)
    {
      return errc::not_enough_memory;
    }
    assert(this->is_nonblocking() == p->is_nonblocking());
    req.buffers.connected_socket() = {tls_socket_handle_ptr(p), read.connected_socket().second};
    OUTCOME_TRY(p->set_registered_buffer_chunk_size(_registered_buffer_chunk_size));
    OUTCOME_TRY(p->_init(false, _authentication_certificates_path));
    return {std::move(req.buffers)};
  }

public:
  explicit listening_openssl_socket_handle(listening_byte_socket_handle &&sock)
      : listening_tls_socket_handle(std::move(sock))
  {
  }
  explicit listening_openssl_socket_handle(listening_byte_socket_handle *sock)
      : listening_tls_socket_handle(handle(), nullptr)
  {
    this->_v.ptr = sock;
    this->_v.behaviour = (sock->native_handle().behaviour & ~(native_handle_type::disposition::kernel_handle)) | native_handle_type::disposition::is_pointer;
  }

  virtual ~listening_openssl_socket_handle() override
  {
    if(_v)
    {
      auto r = listening_openssl_socket_handle::close();
      if(!r)
      {
        // std::cout << r.error().message() << std::endl;
        LLFIO_LOG_FATAL(_v.fd, "listening_openssl_socket_handle::~listening_openssl_socket_handle() close failed");
        abort();
      }
    }
  }
  virtual result<void> close() noexcept override
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    if(_v.behaviour & native_handle_type::disposition::is_pointer)
    {
      listening_tls_socket_handle::release();
      return success();
    }
    else
    {
      return listening_tls_socket_handle::close();
    }
  }

  virtual result<void> set_multiplexer(byte_io_multiplexer *c = this_thread::multiplexer()) noexcept override
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    return LLFIO_OPENSSL_DISPATCH(set_multiplexer, set_multiplexer, (c));
  }

  virtual result<ip::address> local_endpoint() const noexcept override
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    return LLFIO_OPENSSL_DISPATCH(local_endpoint, local_endpoint, ());
  }

  virtual result<void> bind(const ip::address &addr, creation _creation = creation::only_if_not_exist, int backlog = -1) noexcept override
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    return LLFIO_OPENSSL_DISPATCH(bind, bind, (addr, _creation, backlog));
  }

  virtual std::string algorithms_description() const override
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    if(!detail::openssl_default_ctxs.init())
    {
      return {};
    }
    auto ctx = (!_authentication_certificates_path || !_authentication_certificates_path->empty()) ? detail::openssl_default_ctxs.verified :
                                                                                                     detail::openssl_default_ctxs.unverified;
    auto *ciphers = SSL_CTX_get_ciphers(ctx);
    if(ciphers != nullptr)
    {
      std::string ret;
      for(int n = 0; n < sk_SSL_CIPHER_num(ciphers); n++)
      {
        if(n > 0)
        {
          ret.push_back(',');
        }
        ret.append(SSL_CIPHER_get_name(sk_SSL_CIPHER_value(ciphers, n)));
      }
      return ret;
    }
    return {};
  }

  virtual result<void> set_registered_buffer_chunk_size(size_t bytes) noexcept override
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    _registered_buffer_chunk_size = bytes;
    return success();
  }

  virtual result<void> set_algorithms(tls_algorithm set) noexcept override
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    // OpenSSL v1.1 doesn't implement FIPS 840-2, so fail if that is set.
    if(set & tls_algorithm::FIPS_140_2)
    {
      return errc::not_supported;
    }
    return success();
  }

  virtual result<void> set_authentication_certificates_path(path_view identifier) noexcept override
  {
    try
    {
      LLFIO_LOG_FUNCTION_CALL(this);
      _authentication_certificates_path = identifier.path();
      return success();
    }
    catch(...)
    {
      return error_from_exception();
    }
  }
};


/************************************************************************************************************/

static tls_socket_source_implementation_information openssl_socket_source_registration_info;
static struct openssl_socket_source_registration_t
{
  struct _tls_socket_source final : tls_socket_source
  {
    constexpr _tls_socket_source()
        : tls_socket_source(openssl_socket_source_registration_info)
    {
    }

    virtual result<tls_socket_handle_ptr> connecting_socket(ip::family family, byte_socket_handle::mode _mode, byte_socket_handle::caching _caching,
                                                            byte_socket_handle::flag flags) noexcept override
    {
      OUTCOME_TRY(auto &&sock, byte_socket_handle::byte_socket(family, _mode, _caching, flags));
      auto *p = new(std::nothrow) openssl_socket_handle(std::move(sock));
      if(p == nullptr)
      {
        return errc::not_enough_memory;
      }
      tls_socket_handle_ptr ret(p);
      OUTCOME_TRY(p->set_registered_buffer_chunk_size(4096));
      return {std::move(ret)};
    }

    virtual result<listening_tls_socket_handle_ptr> listening_socket(ip::family family, byte_socket_handle::mode _mode, byte_socket_handle::caching _caching,
                                                                     byte_socket_handle::flag flags) noexcept override
    {
      OUTCOME_TRY(auto &&sock, listening_byte_socket_handle::listening_byte_socket(family, _mode, _caching, flags));
      auto *p = new(std::nothrow) listening_openssl_socket_handle(std::move(sock));
      if(p == nullptr)
      {
        return errc::not_enough_memory;
      }
      listening_tls_socket_handle_ptr ret(p);
      return {std::move(ret)};
    }

    virtual result<tls_socket_handle_ptr> wrap(byte_socket_handle *transport) noexcept override
    {
      auto *p = new(std::nothrow) openssl_socket_handle(transport);
      if(p == nullptr)
      {
        return errc::not_enough_memory;
      }
      tls_socket_handle_ptr ret(p);
      OUTCOME_TRY(p->set_registered_buffer_chunk_size(4096));
      return {std::move(ret)};
    }

    virtual result<listening_tls_socket_handle_ptr> wrap(listening_byte_socket_handle *listening) noexcept override
    {
      auto *p = new(std::nothrow) listening_openssl_socket_handle(listening);
      if(p == nullptr)
      {
        return errc::not_enough_memory;
      }
      listening_tls_socket_handle_ptr ret(p);
      return {std::move(ret)};
    }
  };

  static result<tls_socket_source_ptr> _instantiate() noexcept
  {
    auto *p = new(std::nothrow) _tls_socket_source;
    if(p == nullptr)
    {
      return errc::not_enough_memory;
    }
    return tls_socket_source_ptr(p);
  }
  static result<tls_socket_source_ptr> _instantiate_with(byte_io_multiplexer * /*unused*/) noexcept { return errc::function_not_supported; }

  openssl_socket_source_registration_t()
  {
    auto &info = openssl_socket_source_registration_info;
    info.name = "openssl";
    info.version.major = (OPENSSL_VERSION_NUMBER >> 28) & 0xf;
    info.version.minor = (OPENSSL_VERSION_NUMBER >> 20) & 0xff;
    info.version.patch = (OPENSSL_VERSION_NUMBER >> 12) & 0xff;
    info.postfix = "nonmultiplexable";
    info.features = tls_socket_source_implementation_features::kernel_sockets | tls_socket_source_implementation_features::supports_wrap;
#ifndef _WIN32
    info.features |= tls_socket_source_implementation_features::system_implementation;
#endif
    info.instantiate = _instantiate;
    info.instantiate_with = _instantiate_with;
    tls_socket_source_registry::register_source(info).value();
  }
  ~openssl_socket_source_registration_t() { tls_socket_source_registry::unregister_source(openssl_socket_source_registration_info); }
} openssl_socket_source_registration;

LLFIO_V2_NAMESPACE_END
