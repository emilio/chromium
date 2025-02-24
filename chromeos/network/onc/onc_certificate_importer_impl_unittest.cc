// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_certificate_importer_impl.h"

#include <cert.h>
#include <certdb.h>
#include <keyhi.h>
#include <pk11pub.h>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chromeos/network/onc/onc_test_utils.h"
#include "components/onc/onc_constants.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/base/hash_value.h"
#include "net/cert/cert_type.h"
#include "net/cert/nss_cert_database_chromeos.h"
#include "net/cert/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace onc {

namespace {

#if defined(USE_NSS_CERTS)
// In NSS 3.13, CERTDB_VALID_PEER was renamed CERTDB_TERMINAL_RECORD. So we use
// the new name of the macro.
#if !defined(CERTDB_TERMINAL_RECORD)
#define CERTDB_TERMINAL_RECORD CERTDB_VALID_PEER
#endif

net::CertType GetCertType(net::X509Certificate::OSCertHandle cert) {
  CERTCertTrust trust = {0};
  CERT_GetCertTrust(cert, &trust);

  unsigned all_flags = trust.sslFlags | trust.emailFlags |
      trust.objectSigningFlags;

  if (cert->nickname && (all_flags & CERTDB_USER))
    return net::USER_CERT;
  if ((all_flags & CERTDB_VALID_CA) || CERT_IsCACert(cert, NULL))
    return net::CA_CERT;
  // TODO(mattm): http://crbug.com/128633.
  if (trust.sslFlags & CERTDB_TERMINAL_RECORD)
    return net::SERVER_CERT;
  return net::OTHER_CERT;
}
#else
net::CertType GetCertType(net::X509Certificate::OSCertHandle cert) {
  NOTIMPLEMENTED();
  return net::OTHER_CERT;
}
#endif  // USE_NSS_CERTS

}  // namespace

class ONCCertificateImporterImplTest : public testing::Test {
 public:
  ONCCertificateImporterImplTest() {}
  ~ONCCertificateImporterImplTest() override {}

  void SetUp() override {
    ASSERT_TRUE(public_nssdb_.is_open());
    ASSERT_TRUE(private_nssdb_.is_open());

    task_runner_ = new base::TestSimpleTaskRunner();
    thread_task_runner_handle_.reset(
        new base::ThreadTaskRunnerHandle(task_runner_));

    test_nssdb_.reset(new net::NSSCertDatabaseChromeOS(
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(public_nssdb_.slot())),
        crypto::ScopedPK11Slot(PK11_ReferenceSlot(private_nssdb_.slot()))));

    // Test db should be empty at start of test.
    EXPECT_TRUE(ListCertsInPublicSlot().empty());
    EXPECT_TRUE(ListCertsInPrivateSlot().empty());
  }

  void TearDown() override {
    thread_task_runner_handle_.reset();
    task_runner_ = NULL;
  }

 protected:
  void OnImportCompleted(bool expected_success,
                         bool success,
                         const net::CertificateList& onc_trusted_certificates) {
    EXPECT_EQ(expected_success, success);
    web_trust_certificates_ = onc_trusted_certificates;
  }

  void AddCertificatesFromFile(const std::string& filename,
                               bool expected_success) {
    std::unique_ptr<base::DictionaryValue> onc =
        test_utils::ReadTestDictionary(filename);
    std::unique_ptr<base::Value> certificates_value;
    base::ListValue* certificates = NULL;
    onc->RemoveWithoutPathExpansion(::onc::toplevel_config::kCertificates,
                                    &certificates_value);
    certificates_value.release()->GetAsList(&certificates);
    onc_certificates_.reset(certificates);

    web_trust_certificates_.clear();
    CertificateImporterImpl importer(task_runner_, test_nssdb_.get());
    importer.ImportCertificates(
        *certificates,
        ::onc::ONC_SOURCE_USER_IMPORT,  // allow web trust
        base::Bind(&ONCCertificateImporterImplTest::OnImportCompleted,
                   base::Unretained(this),
                   expected_success));

    task_runner_->RunUntilIdle();

    public_list_ = ListCertsInPublicSlot();
    private_list_ = ListCertsInPrivateSlot();
  }

  void AddCertificateFromFile(const std::string& filename,
                              net::CertType expected_type,
                              std::string* guid) {
    std::string guid_temporary;
    if (!guid)
      guid = &guid_temporary;

    AddCertificatesFromFile(filename, true);

    if (expected_type == net::SERVER_CERT || expected_type == net::CA_CERT) {
      ASSERT_EQ(1u, public_list_.size());
      EXPECT_EQ(expected_type, GetCertType(public_list_[0]->os_cert_handle()));
      EXPECT_TRUE(private_list_.empty());
    } else {  // net::USER_CERT
      EXPECT_TRUE(public_list_.empty());
      ASSERT_EQ(1u, private_list_.size());
      EXPECT_EQ(expected_type, GetCertType(private_list_[0]->os_cert_handle()));
    }

    base::DictionaryValue* certificate = NULL;
    onc_certificates_->GetDictionary(0, &certificate);
    certificate->GetStringWithoutPathExpansion(::onc::certificate::kGUID, guid);
  }

  // Certificates and the NSSCertDatabase depend on these test DBs. Destroy them
  // last.
  crypto::ScopedTestNSSDB public_nssdb_;
  crypto::ScopedTestNSSDB private_nssdb_;

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  std::unique_ptr<base::ThreadTaskRunnerHandle> thread_task_runner_handle_;
  std::unique_ptr<net::NSSCertDatabaseChromeOS> test_nssdb_;
  std::unique_ptr<base::ListValue> onc_certificates_;
  // List of certs in the nssdb's public slot.
  net::CertificateList public_list_;
  // List of certs in the nssdb's "private" slot.
  net::CertificateList private_list_;
  net::CertificateList web_trust_certificates_;

 private:
  net::CertificateList ListCertsInPublicSlot() {
    return ListCertsInSlot(public_nssdb_.slot());
  }

  net::CertificateList ListCertsInPrivateSlot() {
    return ListCertsInSlot(private_nssdb_.slot());
  }

  net::CertificateList ListCertsInSlot(PK11SlotInfo* slot) {
    net::CertificateList result;
    CERTCertList* cert_list = PK11_ListCertsInSlot(slot);
    for (CERTCertListNode* node = CERT_LIST_HEAD(cert_list);
         !CERT_LIST_END(node, cert_list);
         node = CERT_LIST_NEXT(node)) {
      result.push_back(net::X509Certificate::CreateFromHandle(
          node->cert, net::X509Certificate::OSCertHandles()));
    }
    CERT_DestroyCertList(cert_list);

    std::sort(result.begin(), result.end(),
              [](const scoped_refptr<net::X509Certificate>& lhs,
                 const scoped_refptr<net::X509Certificate>& rhs) {
                return net::SHA256HashValueLessThan()(
                    net::X509Certificate::CalculateFingerprint256(
                        lhs->os_cert_handle()),
                    net::X509Certificate::CalculateFingerprint256(
                        rhs->os_cert_handle()));
              });
    return result;
  }
};

TEST_F(ONCCertificateImporterImplTest, MultipleCertificates) {
  AddCertificatesFromFile("managed_toplevel2.onc", true);
  EXPECT_EQ(onc_certificates_->GetSize(), public_list_.size());
  EXPECT_TRUE(private_list_.empty());
  EXPECT_EQ(2ul, public_list_.size());
}

TEST_F(ONCCertificateImporterImplTest, MultipleCertificatesWithFailures) {
  AddCertificatesFromFile("toplevel_partially_invalid.onc", false);
  EXPECT_EQ(3ul, onc_certificates_->GetSize());
  EXPECT_EQ(1ul, private_list_.size());
  EXPECT_TRUE(public_list_.empty());
}

TEST_F(ONCCertificateImporterImplTest, AddClientCertificate) {
  std::string guid;
  AddCertificateFromFile("certificate-client.onc", net::USER_CERT, &guid);
  EXPECT_TRUE(web_trust_certificates_.empty());
  EXPECT_EQ(1ul, private_list_.size());
  EXPECT_TRUE(public_list_.empty());

  SECKEYPrivateKeyList* privkey_list =
      PK11_ListPrivKeysInSlot(private_nssdb_.slot(), NULL, NULL);
  EXPECT_TRUE(privkey_list);
  if (privkey_list) {
    SECKEYPrivateKeyListNode* node = PRIVKEY_LIST_HEAD(privkey_list);
    int count = 0;
    while (!PRIVKEY_LIST_END(node, privkey_list)) {
      char* name = PK11_GetPrivateKeyNickname(node->key);
      EXPECT_STREQ(guid.c_str(), name);
      PORT_Free(name);
      count++;
      node = PRIVKEY_LIST_NEXT(node);
    }
    EXPECT_EQ(1, count);
    SECKEY_DestroyPrivateKeyList(privkey_list);
  }

  SECKEYPublicKeyList* pubkey_list =
      PK11_ListPublicKeysInSlot(private_nssdb_.slot(), NULL);
  EXPECT_TRUE(pubkey_list);
  if (pubkey_list) {
    SECKEYPublicKeyListNode* node = PUBKEY_LIST_HEAD(pubkey_list);
    int count = 0;
    while (!PUBKEY_LIST_END(node, pubkey_list)) {
      count++;
      node = PUBKEY_LIST_NEXT(node);
    }
    EXPECT_EQ(1, count);
    SECKEY_DestroyPublicKeyList(pubkey_list);
  }
}

TEST_F(ONCCertificateImporterImplTest, AddServerCertificateWithWebTrust) {
  AddCertificateFromFile("certificate-server.onc", net::SERVER_CERT, NULL);

  SECKEYPrivateKeyList* privkey_list =
      PK11_ListPrivKeysInSlot(private_nssdb_.slot(), NULL, NULL);
  EXPECT_FALSE(privkey_list);

  SECKEYPublicKeyList* pubkey_list =
      PK11_ListPublicKeysInSlot(private_nssdb_.slot(), NULL);
  EXPECT_FALSE(pubkey_list);

  ASSERT_EQ(1u, web_trust_certificates_.size());
  ASSERT_EQ(1u, public_list_.size());
  EXPECT_TRUE(private_list_.empty());
  EXPECT_TRUE(CERT_CompareCerts(public_list_[0]->os_cert_handle(),
                                web_trust_certificates_[0]->os_cert_handle()));
}

TEST_F(ONCCertificateImporterImplTest, AddWebAuthorityCertificateWithWebTrust) {
  AddCertificateFromFile("certificate-web-authority.onc", net::CA_CERT, NULL);

  SECKEYPrivateKeyList* privkey_list =
      PK11_ListPrivKeysInSlot(private_nssdb_.slot(), NULL, NULL);
  EXPECT_FALSE(privkey_list);

  SECKEYPublicKeyList* pubkey_list =
      PK11_ListPublicKeysInSlot(private_nssdb_.slot(), NULL);
  EXPECT_FALSE(pubkey_list);

  ASSERT_EQ(1u, web_trust_certificates_.size());
  ASSERT_EQ(1u, public_list_.size());
  EXPECT_TRUE(private_list_.empty());
  EXPECT_TRUE(CERT_CompareCerts(public_list_[0]->os_cert_handle(),
                                web_trust_certificates_[0]->os_cert_handle()));
}

TEST_F(ONCCertificateImporterImplTest, AddAuthorityCertificateWithoutWebTrust) {
  AddCertificateFromFile("certificate-authority.onc", net::CA_CERT, NULL);
  EXPECT_TRUE(web_trust_certificates_.empty());

  SECKEYPrivateKeyList* privkey_list =
      PK11_ListPrivKeysInSlot(private_nssdb_.slot(), NULL, NULL);
  EXPECT_FALSE(privkey_list);

  SECKEYPublicKeyList* pubkey_list =
      PK11_ListPublicKeysInSlot(private_nssdb_.slot(), NULL);
  EXPECT_FALSE(pubkey_list);
}

struct CertParam {
  CertParam(net::CertType certificate_type,
            const char* original_filename,
            const char* update_filename)
      : cert_type(certificate_type),
        original_file(original_filename),
        update_file(update_filename) {}

  net::CertType cert_type;
  const char* original_file;
  const char* update_file;
};

class ONCCertificateImporterImplTestWithParam :
      public ONCCertificateImporterImplTest,
      public testing::WithParamInterface<CertParam> {
};

TEST_P(ONCCertificateImporterImplTestWithParam, UpdateCertificate) {
  // First we import a certificate.
  {
    SCOPED_TRACE("Import original certificate");
    AddCertificateFromFile(GetParam().original_file, GetParam().cert_type,
                           NULL);
  }

  // Now we import the same certificate with a different GUID. In case of a
  // client cert, the cert should be retrievable via the new GUID.
  {
    SCOPED_TRACE("Import updated certificate");
    AddCertificateFromFile(GetParam().update_file, GetParam().cert_type, NULL);
  }
}

TEST_P(ONCCertificateImporterImplTestWithParam, ReimportCertificate) {
  // Verify that reimporting a client certificate works.
  for (int i = 0; i < 2; ++i) {
    SCOPED_TRACE("Import certificate, iteration " + base::IntToString(i));
    AddCertificateFromFile(GetParam().original_file, GetParam().cert_type,
                           NULL);
  }
}

INSTANTIATE_TEST_CASE_P(
    ONCCertificateImporterImplTestWithParam,
    ONCCertificateImporterImplTestWithParam,
    ::testing::Values(
        CertParam(net::USER_CERT,
                  "certificate-client.onc",
                  "certificate-client-update.onc"),
        CertParam(net::SERVER_CERT,
                  "certificate-server.onc",
                  "certificate-server-update.onc"),
        CertParam(net::CA_CERT,
                  "certificate-web-authority.onc",
                  "certificate-web-authority-update.onc")));

}  // namespace onc
}  // namespace chromeos
