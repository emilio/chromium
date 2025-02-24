// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_X509_CERTIFICATE_H_
#define NET_CERT_X509_CERTIFICATE_H_

#include <stddef.h>
#include <string.h>

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/cert/cert_type.h"
#include "net/cert/x509_cert_types.h"

#if defined(OS_WIN)
#include <windows.h>
#include "crypto/wincrypt_shim.h"
#elif defined(OS_MACOSX)
#include <CoreFoundation/CFArray.h>
#include <Security/SecBase.h>
#elif defined(USE_OPENSSL_CERTS)
// Forward declaration; real one in <x509.h>
typedef struct x509_st X509;
typedef struct x509_store_st X509_STORE;
#elif defined(USE_NSS_CERTS)
// Forward declaration; real one in <cert.h>
struct CERTCertificateStr;
#endif

namespace base {
class Pickle;
class PickleIterator;
}

namespace net {

class X509Certificate;

typedef std::vector<scoped_refptr<X509Certificate> > CertificateList;

// X509Certificate represents a X.509 certificate, which is comprised a
// particular identity or end-entity certificate, such as an SSL server
// identity or an SSL client certificate, and zero or more intermediate
// certificates that may be used to build a path to a root certificate.
class NET_EXPORT X509Certificate
    : public base::RefCountedThreadSafe<X509Certificate> {
 public:
  // An OSCertHandle is a handle to a certificate object in the underlying
  // crypto library. We assume that OSCertHandle is a pointer type on all
  // platforms and that NULL represents an invalid OSCertHandle.
#if defined(OS_WIN)
  typedef PCCERT_CONTEXT OSCertHandle;
#elif defined(OS_MACOSX)
  typedef SecCertificateRef OSCertHandle;
#elif defined(USE_OPENSSL_CERTS)
  typedef X509* OSCertHandle;
#elif defined(USE_NSS_CERTS)
  typedef struct CERTCertificateStr* OSCertHandle;
#else
  // TODO(ericroman): not implemented
  typedef void* OSCertHandle;
#endif

  typedef std::vector<OSCertHandle> OSCertHandles;

  enum PublicKeyType {
    kPublicKeyTypeUnknown,
    kPublicKeyTypeRSA,
    kPublicKeyTypeDSA,
    kPublicKeyTypeECDSA,
    kPublicKeyTypeDH,
    kPublicKeyTypeECDH
  };

  enum Format {
    // The data contains a single DER-encoded certificate, or a PEM-encoded
    // DER certificate with the PEM encoding block name of "CERTIFICATE".
    // Any subsequent blocks will be ignored.
    FORMAT_SINGLE_CERTIFICATE = 1 << 0,

    // The data contains a sequence of one or more PEM-encoded, DER
    // certificates, with the PEM encoding block name of "CERTIFICATE".
    // All PEM blocks will be parsed, until the first error is encountered.
    FORMAT_PEM_CERT_SEQUENCE = 1 << 1,

    // The data contains a PKCS#7 SignedData structure, whose certificates
    // member is to be used to initialize the certificate and intermediates.
    // The data may further be encoded using PEM, specifying block names of
    // either "PKCS7" or "CERTIFICATE".
    FORMAT_PKCS7 = 1 << 2,

    // Automatically detect the format.
    FORMAT_AUTO = FORMAT_SINGLE_CERTIFICATE | FORMAT_PEM_CERT_SEQUENCE |
                  FORMAT_PKCS7,
  };

  // PickleType is intended for deserializing certificates that were pickled
  // by previous releases as part of a net::HttpResponseInfo.
  // When serializing certificates to a new base::Pickle,
  // PICKLETYPE_CERTIFICATE_CHAIN_V3 is always used.
  enum PickleType {
    // When reading a certificate from a Pickle, the Pickle only contains a
    // single certificate.
    PICKLETYPE_SINGLE_CERTIFICATE,

    // When reading a certificate from a Pickle, the Pickle contains the
    // the certificate plus any certificates that were stored in
    // |intermediate_ca_certificates_| at the time it was serialized.
    // The count of certificates is stored as a size_t, which is either 32
    // or 64 bits.
    PICKLETYPE_CERTIFICATE_CHAIN_V2,

    // The Pickle contains the certificate and any certificates that were
    // stored in |intermediate_ca_certs_| at the time it was serialized.
    // The format is [int count], [data - this certificate],
    // [data - intermediate1], ... [data - intermediateN].
    // All certificates are stored in DER form.
    PICKLETYPE_CERTIFICATE_CHAIN_V3,
  };

  // Create an X509Certificate from a handle to the certificate object in the
  // underlying crypto library.
  static scoped_refptr<X509Certificate> CreateFromHandle(
      OSCertHandle cert_handle,
      const OSCertHandles& intermediates);

  // Create an X509Certificate from a chain of DER encoded certificates. The
  // first certificate in the chain is the end-entity certificate to which a
  // handle is returned. The other certificates in the chain are intermediate
  // certificates.
  static scoped_refptr<X509Certificate> CreateFromDERCertChain(
      const std::vector<base::StringPiece>& der_certs);

  // Create an X509Certificate from the DER-encoded representation.
  // Returns NULL on failure.
  static scoped_refptr<X509Certificate> CreateFromBytes(const char* data,
                                                        size_t length);

#if defined(USE_NSS_CERTS)
  // The default nickname of the certificate, based on the certificate type
  // passed in.
  std::string GetDefaultNickname(CertType type) const;
#endif

  // Create an X509Certificate from the representation stored in the given
  // pickle.  The data for this object is found relative to the given
  // pickle_iter, which should be passed to the pickle's various Read* methods.
  // Returns NULL on failure.
  static scoped_refptr<X509Certificate> CreateFromPickle(
      base::PickleIterator* pickle_iter,
      PickleType type);

  // Parses all of the certificates possible from |data|. |format| is a
  // bit-wise OR of Format, indicating the possible formats the
  // certificates may have been serialized as. If an error occurs, an empty
  // collection will be returned.
  static CertificateList CreateCertificateListFromBytes(const char* data,
                                                        size_t length,
                                                        int format);

  // Appends a representation of this object to the given pickle.
  void Persist(base::Pickle* pickle);

  // The serial number, DER encoded, possibly including a leading 00 byte.
  const std::string& serial_number() const { return serial_number_; }

  // The subject of the certificate.  For HTTPS server certificates, this
  // represents the web server.  The common name of the subject should match
  // the host name of the web server.
  const CertPrincipal& subject() const { return subject_; }

  // The issuer of the certificate.
  const CertPrincipal& issuer() const { return issuer_; }

  // Time period during which the certificate is valid.  More precisely, this
  // certificate is invalid before the |valid_start| date and invalid after
  // the |valid_expiry| date.
  // If we were unable to parse either date from the certificate (or if the cert
  // lacks either date), the date will be null (i.e., is_null() will be true).
  const base::Time& valid_start() const { return valid_start_; }
  const base::Time& valid_expiry() const { return valid_expiry_; }

  // Gets the DNS names in the certificate.  Pursuant to RFC 2818, Section 3.1
  // Server Identity, if the certificate has a subjectAltName extension of
  // type dNSName, this method gets the DNS names in that extension.
  // Otherwise, it gets the common name in the subject field.
  void GetDNSNames(std::vector<std::string>* dns_names) const;

  // Gets the subjectAltName extension field from the certificate, if any.
  // For future extension; currently this only returns those name types that
  // are required for HTTP certificate name verification - see VerifyHostname.
  // Unrequired parameters may be passed as NULL.
  void GetSubjectAltName(std::vector<std::string>* dns_names,
                         std::vector<std::string>* ip_addrs) const;

  // Convenience method that returns whether this certificate has expired as of
  // now.
  bool HasExpired() const;

  // Returns true if this object and |other| represent the same certificate.
  bool Equals(const X509Certificate* other) const;

  // Returns intermediate certificates added via AddIntermediateCertificate().
  // Ownership follows the "get" rule: it is the caller's responsibility to
  // retain the elements of the result.
  const OSCertHandles& GetIntermediateCertificates() const {
    return intermediate_ca_certs_;
  }

#if defined(OS_MACOSX)
  // Does this certificate's usage allow SSL client authentication?
  bool SupportsSSLClientAuth() const;

  // Returns a new CFMutableArrayRef containing this certificate and its
  // intermediate certificates in the form expected by Security.framework
  // and Keychain Services, or NULL on failure.
  // The first item in the array will be this certificate, followed by its
  // intermediates, if any.
  CFMutableArrayRef CreateOSCertChainForCert() const;
#endif

  // Do any of the given issuer names appear in this cert's chain of trust?
  // |valid_issuers| is a list of DER-encoded X.509 DistinguishedNames.
  bool IsIssuedByEncoded(const std::vector<std::string>& valid_issuers);

#if defined(OS_WIN)
  // Returns a new PCCERT_CONTEXT containing this certificate and its
  // intermediate certificates, or NULL on failure. The returned
  // PCCERT_CONTEXT *MUST NOT* be stored in an X509Certificate, as this will
  // cause os_cert_handle() to return incorrect results. This function is only
  // necessary if the CERT_CONTEXT.hCertStore member will be accessed or
  // enumerated, which is generally true for any CryptoAPI functions involving
  // certificate chains, including validation or certificate display.
  //
  // Remarks:
  // Depending on the CryptoAPI function, Windows may need to access the
  // HCERTSTORE that the passed-in PCCERT_CONTEXT belongs to, such as to
  // locate additional intermediates. However, all certificate handles are added
  // to a NULL HCERTSTORE, allowing the system to manage the resources. As a
  // result, intermediates for |cert_handle_| cannot be located simply via
  // |cert_handle_->hCertStore|, as it refers to a magic value indicating
  // "only this certificate".
  //
  // To avoid this problems, a new in-memory HCERTSTORE is created containing
  // just this certificate and its intermediates. The handle to the version of
  // the current certificate in the new HCERTSTORE is then returned, with the
  // PCCERT_CONTEXT's HCERTSTORE set to be automatically freed when the returned
  // certificate handle is freed.
  //
  // This function is only needed when the HCERTSTORE of the os_cert_handle()
  // will be accessed, which is generally only during certificate validation
  // or display. While the returned PCCERT_CONTEXT and its HCERTSTORE can
  // safely be used on multiple threads if no further modifications happen, it
  // is generally preferable for each thread that needs such a context to
  // obtain its own, rather than risk thread-safety issues by sharing.
  //
  // Because of how X509Certificate caching is implemented, attempting to
  // create an X509Certificate from the returned PCCERT_CONTEXT may result in
  // the original handle (and thus the originall HCERTSTORE) being returned by
  // os_cert_handle(). For this reason, the returned PCCERT_CONTEXT *MUST NOT*
  // be stored in an X509Certificate.
  PCCERT_CONTEXT CreateOSCertChainForCert() const;
#endif

#if defined(USE_OPENSSL_CERTS)
  // Returns a handle to a global, in-memory certificate store. We
  // use it for test code, e.g. importing the test server's certificate.
  static X509_STORE* cert_store();
#endif

  // Verifies that |hostname| matches this certificate.
  // Does not verify that the certificate is valid, only that the certificate
  // matches this host.
  // If |allow_common_name_fallback| is set to true, and iff no SANs are
  // present of type dNSName or iPAddress, then fallback to using the
  // certificate's commonName field in the Subject.
  bool VerifyNameMatch(const std::string& hostname,
                       bool allow_common_name_fallback) const;

  // Obtains the DER encoded certificate data for |cert_handle|. On success,
  // returns true and writes the DER encoded certificate to |*der_encoded|.
  static bool GetDEREncoded(OSCertHandle cert_handle,
                            std::string* der_encoded);

  // Returns the PEM encoded data from a DER encoded certificate. If the return
  // value is true, then the PEM encoded certificate is written to
  // |pem_encoded|.
  static bool GetPEMEncodedFromDER(const std::string& der_encoded,
                                   std::string* pem_encoded);

  // Returns the PEM encoded data from an OSCertHandle. If the return value is
  // true, then the PEM encoded certificate is written to |pem_encoded|.
  static bool GetPEMEncoded(OSCertHandle cert_handle,
                            std::string* pem_encoded);

  // Encodes the entire certificate chain (this certificate and any
  // intermediate certificates stored in |intermediate_ca_certs_|) as a series
  // of PEM encoded strings. Returns true if all certificates were encoded,
  // storig the result in |*pem_encoded|, with this certificate stored as
  // the first element.
  bool GetPEMEncodedChain(std::vector<std::string>* pem_encoded) const;

  // Sets |*size_bits| to be the length of the public key in bits, and sets
  // |*type| to one of the |PublicKeyType| values. In case of
  // |kPublicKeyTypeUnknown|, |*size_bits| will be set to 0.
  static void GetPublicKeyInfo(OSCertHandle cert_handle,
                               size_t* size_bits,
                               PublicKeyType* type);

  // Returns the OSCertHandle of this object. Because of caching, this may
  // differ from the OSCertHandle originally supplied during initialization.
  // Note: On Windows, CryptoAPI may return unexpected results if this handle
  // is used across multiple threads. For more details, see
  // CreateOSCertChainForCert().
  OSCertHandle os_cert_handle() const { return cert_handle_; }

  // Returns true if two OSCertHandles refer to identical certificates.
  static bool IsSameOSCert(OSCertHandle a, OSCertHandle b);

  // Creates an OS certificate handle from the DER-encoded representation.
  // Returns NULL on failure.
  static OSCertHandle CreateOSCertHandleFromBytes(const char* data,
                                                  size_t length);

#if defined(USE_NSS_CERTS)
  // Creates an OS certificate handle from the DER-encoded representation.
  // Returns NULL on failure.  Sets the default nickname if |nickname| is
  // non-NULL.
  static OSCertHandle CreateOSCertHandleFromBytesWithNickname(
      const char* data,
      size_t length,
      const char* nickname);
#endif

  // Creates all possible OS certificate handles from |data| encoded in a
  // specific |format|. Returns an empty collection on failure.
  static OSCertHandles CreateOSCertHandlesFromBytes(const char* data,
                                                    size_t length,
                                                    Format format);

  // Duplicates (or adds a reference to) an OS certificate handle.
  static OSCertHandle DupOSCertHandle(OSCertHandle cert_handle);

  // Frees (or releases a reference to) an OS certificate handle.
  static void FreeOSCertHandle(OSCertHandle cert_handle);

  // Calculates the SHA-256 fingerprint of the certificate.  Returns an empty
  // (all zero) fingerprint on failure.
  static SHA256HashValue CalculateFingerprint256(OSCertHandle cert_handle);

  // Calculates the SHA-256 fingerprint of the intermediate CA certificates.
  // Returns an empty (all zero) fingerprint on failure.
  static SHA256HashValue CalculateCAFingerprint256(
      const OSCertHandles& intermediates);

  // Calculates the SHA-256 fingerprint for the complete chain, including the
  // leaf certificate and all intermediate CA certificates. Returns an empty
  // (all zero) fingerprint on failure.
  static SHA256HashValue CalculateChainFingerprint256(
      OSCertHandle leaf,
      const OSCertHandles& intermediates);

  // Returns true if the certificate is self-signed.
  static bool IsSelfSigned(OSCertHandle cert_handle);

 private:
  friend class base::RefCountedThreadSafe<X509Certificate>;
  friend class TestRootCerts;  // For unit tests

  FRIEND_TEST_ALL_PREFIXES(X509CertificateNameVerifyTest, VerifyHostname);
  FRIEND_TEST_ALL_PREFIXES(X509CertificateTest, SerialNumbers);

  // Construct an X509Certificate from a handle to the certificate object
  // in the underlying crypto library.
  X509Certificate(OSCertHandle cert_handle,
                  const OSCertHandles& intermediates);

  ~X509Certificate();

  // Common object initialization code.  Called by the constructors only.
  void Initialize();

#if defined(USE_OPENSSL_CERTS)
  // Resets the store returned by cert_store() to default state. Used by
  // TestRootCerts to undo modifications.
  static void ResetCertStore();
#endif

  // Verifies that |hostname| matches one of the certificate names or IP
  // addresses supplied, based on TLS name matching rules - specifically,
  // following http://tools.ietf.org/html/rfc6125.
  // |cert_common_name| is the Subject CN, e.g. from X509Certificate::subject().
  // The members of |cert_san_dns_names| and |cert_san_ipaddrs| must be filled
  // from the dNSName and iPAddress components of the subject alternative name
  // extension, if present. Note these IP addresses are NOT ascii-encoded:
  // they must be 4 or 16 bytes of network-ordered data, for IPv4 and IPv6
  // addresses, respectively.
  // If |allow_common_name_fallback| is true, then the |cert_common_name| will
  // be used if the |cert_san_dns_names| and |cert_san_ip_addrs| parameters are
  // empty.
  static bool VerifyHostname(const std::string& hostname,
                             const std::string& cert_common_name,
                             const std::vector<std::string>& cert_san_dns_names,
                             const std::vector<std::string>& cert_san_ip_addrs,
                             bool allow_common_name_fallback);

  // Reads a single certificate from |pickle_iter| and returns a
  // platform-specific certificate handle. The format of the certificate
  // stored in |pickle_iter| is not guaranteed to be the same across different
  // underlying cryptographic libraries, nor acceptable to CreateFromBytes().
  // Returns an invalid handle, NULL, on failure.
  // NOTE: This should not be used for any new code. It is provided for
  // migration purposes and should eventually be removed.
  static OSCertHandle ReadOSCertHandleFromPickle(
      base::PickleIterator* pickle_iter);

  // Writes a single certificate to |pickle| in DER form. Returns false on
  // failure.
  static bool WriteOSCertHandleToPickle(OSCertHandle handle,
                                        base::Pickle* pickle);

  // The subject of the certificate.
  CertPrincipal subject_;

  // The issuer of the certificate.
  CertPrincipal issuer_;

  // This certificate is not valid before |valid_start_|
  base::Time valid_start_;

  // This certificate is not valid after |valid_expiry_|
  base::Time valid_expiry_;

  // The serial number of this certificate, DER encoded.
  std::string serial_number_;

  // A handle to the certificate object in the underlying crypto library.
  OSCertHandle cert_handle_;

  // Untrusted intermediate certificates associated with this certificate
  // that may be needed for chain building.
  OSCertHandles intermediate_ca_certs_;

  DISALLOW_COPY_AND_ASSIGN(X509Certificate);
};

}  // namespace net

#endif  // NET_CERT_X509_CERTIFICATE_H_
