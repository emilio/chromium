// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_TABLE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_TABLE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/metadata_batch.h"
#include "components/webdata/common/web_database_table.h"

class WebDatabase;

namespace base {
class Time;
}

namespace autofill {

class AutofillChange;
class AutofillEntry;
class AutofillProfile;
class AutofillTableEncryptor;
class AutofillTableTest;
class CreditCard;

struct FormFieldData;

// This class manages the various Autofill tables within the SQLite database
// passed to the constructor. It expects the following schemas:
//
// Note: The database stores time in seconds, UTC.
//
// autofill             This table contains autocomplete history data (not
//                      structured information).
//
//   name               The name of the input as specified in the html.
//   value              The literal contents of the text field.
//   value_lower        The contents of the text field made lower_case.
//   date_created       The date on which the user first entered the string
//                      |value| into a field of name |name|.
//   date_last_used     The date on which the user last entered the string
//                      |value| into a field of name |name|.
//   count              How many times the user has entered the string |value|
//                      in a field of name |name|.
//
// autofill_profiles    This table contains Autofill profile data added by the
//                      user with the Autofill dialog.  Most of the columns are
//                      standard entries in a contact information form.
//
//   guid               A guid string to uniquely identify the profile.
//                      Added in version 31.
//   company_name
//   street_address     The combined lines of the street address.
//                      Added in version 54.
//   dependent_locality
//                      A sub-classification beneath the city, e.g. an
//                      inner-city district or suburb.  Added in version 54.
//   city
//   state
//   zipcode
//   sorting_code       Similar to the zipcode column, but used for businesses
//                      or organizations that might not be geographically
//                      contiguous.  The canonical example is CEDEX in France.
//                      Added in version 54.
//   country_code
//   use_count          The number of times this profile has been used to fill
//                      a form. Added in version 61.
//   use_date           The date this profile was last used to fill a form,
//                      in time_t. Added in version 61.
//   date_modified      The date on which this profile was last modified, in
//                      time_t. Added in version 30.
//   origin             The domain of origin for this profile.
//                      Added in version 50.
//   language_code      The BCP 47 language code used to format the address for
//                      display. For example, a JP address with "ja" language
//                      code starts with the postal code, but a JP address with
//                      "ja-latn" language code starts with the recipient name.
//                      Added in version 56.
//
// autofill_profile_names
//                      This table contains the multi-valued name fields
//                      associated with a profile.
//
//   guid               The guid string that identifies the profile to which
//                      the name belongs.
//   first_name
//   middle_name
//   last_name
//   full_name
//
// autofill_profile_emails
//                      This table contains the multi-valued email fields
//                      associated with a profile.
//
//   guid               The guid string that identifies the profile to which
//                      the email belongs.
//   email
//
// autofill_profile_phones
//                      This table contains the multi-valued phone fields
//                      associated with a profile.
//
//   guid               The guid string that identifies the profile to which the
//                      phone number belongs.
//   number
//
// autofill_profiles_trash
//                      This table contains guids of "trashed" autofill
//                      profiles.  When a profile is removed its guid is added
//                      to this table so that Sync can perform deferred removal.
//
//   guid               The guid string that identifies the trashed profile.
//
// credit_cards         This table contains credit card data added by the user
//                      with the Autofill dialog.  Most of the columns are
//                      standard entries in a credit card form.
//
//   guid               A guid string to uniquely identify the credit card.
//                      Added in version 31.
//   name_on_card
//   expiration_month
//   expiration_year
//   card_number_encrypted
//                      Stores encrypted credit card number.
//   use_count          The number of times this card has been used to fill
//                      a form. Added in version 61.
//   use_date           The date this card was last used to fill a form,
//                      in time_t. Added in version 61.
//   date_modified      The date on which this entry was last modified, in
//                      time_t. Added in version 30.
//   origin             The domain of origin for this profile.
//                      Added in version 50.
//   billing_address_id The guid string that identifies the local profile which
//                      is the billing address for this card. Can be null in the
//                      database, but always returned as an empty string in
//                      CreditCard. Added in version 66.
//
// masked_credit_cards
//                      This table contains "masked" credit card information
//                      about credit cards stored on the server. It consists
//                      of a short description and an ID, but not full payment
//                      information. Writing to this table is only done by sync.
//                      When a server card is unmasked, it will stay here and
//                      will additionally be added in unmasked_credit_cards.
//
//   id                 String assigned by the server to identify this card.
//                      This is opaque to the client.
//   status             Server's status of this card.
//                      TODO(brettw) define constants for this.
//   name_on_card
//   type               Type of the credit card. This is one of the
//                      kSyncCardType* strings.
//   last_four          Last four digits of the card number. For de-duping
//                      with locally stored cards and generating descriptions.
//   exp_month          Expiration month: 1-12
//   exp_year           Four-digit year: 2017
//
// unmasked_credit_cards
//                      When a masked credit credit card is unmasked and the
//                      full number is downloaded, it will be stored here.
//
//   id                 Server ID. This can be joined with the id in the
//                      masked_credit_cards table to get the rest of the data.
//   card_number_encrypted
//                      Full card number, encrypted.
//   use_count          DEPRECATED in version 65. See server_card_metadata.
//   use_date           DEPRECATED in version 65. See server_card_metadata.
//                      TODO(crbug.com/682326): Remove deprecated columns.
//   unmask_date        The date this card was unmasked in units of
//                      Time::ToInternalValue. Added in version 64.
//
// server_card_metadata
//                      Metadata (currently, usage data) about server credit
//                      cards. This will be synced.
//
//   id                 The server ID, which matches an ID from the
//                      masked_credit_cards table.
//   use_count          The number of times this card has been used to fill
//                      a form.
//   use_date           The date this card was last used to fill a form,
//                      in internal t.
//   billing_address_id The string that identifies the profile which is the
//                      billing address for this card. Can be null in the
//                      database, but always returned as an empty string in
//                      CreditCard. Added in version 71.
//
// server_addresses     This table contains Autofill address data synced from
//                      the wallet server. It's basically the same as the
//                      autofill_profiles table but locally immutable.
//
//   id                 String assigned by the server to identify this address.
//                      This is opaque to the client.
//   recipient_name     Added in v63.
//   company_name
//   street_address     The combined lines of the street address.
//   address_1          Also known as "administrative area". This is normally
//                      the state or province in most countries.
//   address_2          Also known as "locality". In the US this is the city.
//   address_3          A sub-classification beneath the city, e.g. an
//                      inner-city district or suburb. Also known as
//                      "dependent_locality".
//   address_4          Used in certain countries. Also known as
//                      "sub_dependent_locality".
//   postal_code
//   sorting_code       Similar to the zipcode column, but used for businesses
//                      or organizations that might not be geographically
//                      contiguous. The canonical example is CEDEX in France.
//   country_code
//   language_code      The BCP 47 language code used to format the address for
//                      display. For example, a JP address with "ja" language
//                      code starts with the postal code, but a JP address with
//                      "ja-latn" language code starts with the recipient name.
//   phone_number       Phone number. This is a string and has no formatting
//                      constraints. Added in version 64.
//
// server_address_metadata
//                      Metadata (currently, usage data) about server addresses.
//                      This will be synced.
//
//   id                 The server ID, which matches an ID from the
//                      server_addresses table.
//   use_count          The number of times this address has been used to fill
//                      a form.
//   use_date           The date this address was last used to fill a form,
//                      in internal t.
//   has_converted      Whether this server address has been converted to a
//                      local autofill profile.
//
// autofill_sync_metadata
//                      Sync-specific metadata for autofill records.
//
//   storage_key        A string that uniquely identifies the metadata record
//                      as well as the corresponding autofill record.
//   value              The serialized EntityMetadata record.
//
// autofill_model_type_state
//                      Single row table that contains the sync ModelTypeState
//                      for the autofill model type.
//
//   value              The serialized ModelTypeState record.

class AutofillTable : public WebDatabaseTable {
 public:
  AutofillTable();
  ~AutofillTable() override;

  // Retrieves the AutofillTable* owned by |db|.
  static AutofillTable* FromWebDatabase(WebDatabase* db);

  // WebDatabaseTable:
  WebDatabaseTable::TypeKey GetTypeKey() const override;
  bool CreateTablesIfNecessary() override;
  bool IsSyncable() override;
  bool MigrateToVersion(int version, bool* update_compatible_version) override;

  // Records the form elements in |elements| in the database in the
  // autofill table.  A list of all added and updated autofill entries
  // is returned in the changes out parameter.
  bool AddFormFieldValues(const std::vector<FormFieldData>& elements,
                          std::vector<AutofillChange>* changes);

  // Records a single form element in the database in the autofill table. A list
  // of all added and updated autofill entries is returned in the changes out
  // parameter.
  bool AddFormFieldValue(const FormFieldData& element,
                         std::vector<AutofillChange>* changes);

  // Retrieves a vector of all values which have been recorded in the autofill
  // table as the value in a form element with name |name| and which start with
  // |prefix|.  The comparison of the prefix is case insensitive.
  bool GetFormValuesForElementName(const base::string16& name,
                                   const base::string16& prefix,
                                   std::vector<base::string16>* values,
                                   int limit);

  // Removes rows from the autofill table if they were created on or after
  // |delete_begin| and last used strictly before |delete_end|.  For rows where
  // the time range [date_created, date_last_used] overlaps with [delete_begin,
  // delete_end), but is not entirely contained within the latter range, updates
  // the rows so that their resulting time range [new_date_created,
  // new_date_last_used] lies entirely outside of [delete_begin, delete_end),
  // updating the count accordingly.  A list of all changed keys and whether
  // each was updater or removed is returned in the changes out parameter.
  bool RemoveFormElementsAddedBetween(const base::Time& delete_begin,
                                      const base::Time& delete_end,
                                      std::vector<AutofillChange>* changes);

  // Removes rows from the autofill table if they were last accessed strictly
  // before |AutofillEntry::ExpirationTime()|.
  bool RemoveExpiredFormElements(std::vector<AutofillChange>* changes);

  // Removes the row from the autofill table for the given |name| |value| pair.
  virtual bool RemoveFormElement(const base::string16& name,
                                 const base::string16& value);

  // Returns the number of unique values such that for all autofill entries with
  // that value, the interval between creation date and last usage is entirely
  // contained between [|begin|, |end|).
  virtual int GetCountOfValuesContainedBetween(const base::Time& begin,
                                               const base::Time& end);

  // Retrieves all of the entries in the autofill table.
  virtual bool GetAllAutofillEntries(std::vector<AutofillEntry>* entries);

  // Retrieves a single entry from the autofill table.
  virtual bool GetAutofillTimestamps(const base::string16& name,
                                     const base::string16& value,
                                     base::Time* date_created,
                                     base::Time* date_last_used);

  // Replaces existing autofill entries with the entries supplied in
  // the argument.  If the entry does not already exist, it will be
  // added.
  virtual bool UpdateAutofillEntries(const std::vector<AutofillEntry>& entries);

  // Records a single Autofill profile in the autofill_profiles table.
  virtual bool AddAutofillProfile(const AutofillProfile& profile);

  // Updates the database values for the specified profile.  Mulit-value aware.
  virtual bool UpdateAutofillProfile(const AutofillProfile& profile);

  // Removes a row from the autofill_profiles table.  |guid| is the identifier
  // of the profile to remove.
  virtual bool RemoveAutofillProfile(const std::string& guid);

  // Retrieves a profile with guid |guid|.
  std::unique_ptr<AutofillProfile> GetAutofillProfile(const std::string& guid);

  // Retrieves local/server profiles in the database.
  virtual bool GetAutofillProfiles(
      std::vector<std::unique_ptr<AutofillProfile>>* profiles);
  virtual bool GetServerProfiles(
      std::vector<std::unique_ptr<AutofillProfile>>* profiles) const;

  // Sets the server profiles. All old profiles are deleted and replaced with
  // the given ones.
  void SetServerProfiles(const std::vector<AutofillProfile>& profiles);

  // Records a single credit card in the credit_cards table.
  bool AddCreditCard(const CreditCard& credit_card);

  // Updates the database values for the specified credit card.
  bool UpdateCreditCard(const CreditCard& credit_card);

  // Removes a row from the credit_cards table.  |guid| is the identifier of the
  // credit card to remove.
  bool RemoveCreditCard(const std::string& guid);

  // Retrieves a credit card with guid |guid|.
  std::unique_ptr<CreditCard> GetCreditCard(const std::string& guid);

  // Retrieves the local/server credit cards in the database.
  virtual bool GetCreditCards(
      std::vector<std::unique_ptr<CreditCard>>* credit_cards);
  virtual bool GetServerCreditCards(
      std::vector<std::unique_ptr<CreditCard>>* credit_cards) const;

  // Replaces all server credit cards with the given vector. Unmasked cards
  // present in the new list will be preserved (even if the input is MASKED).
  void SetServerCreditCards(const std::vector<CreditCard>& credit_cards);

  // Cards synced from the server may be "masked" (only last 4 digits
  // available) or "unmasked" (everything is available). These functions set
  // that state.
  bool UnmaskServerCreditCard(const CreditCard& masked,
                              const base::string16& full_number);
  bool MaskServerCreditCard(const std::string& id);

  bool UpdateServerCardMetadata(const CreditCard& credit_card);
  bool UpdateServerAddressMetadata(const AutofillProfile& profile);

  // Deletes all data from the server card and profile tables. Returns true if
  // any data was deleted, false if not (so false means "commit not needed"
  // rather than "error").
  bool ClearAllServerData();

  // Removes rows from autofill_profiles and credit_cards if they were created
  // on or after |delete_begin| and strictly before |delete_end|.  Returns the
  // list of deleted profile guids in |profile_guids|.  Return value is true if
  // all rows were successfully removed.  Returns false on database error.  In
  // that case, the output vector state is undefined, and may be partially
  // filled.
  bool RemoveAutofillDataModifiedBetween(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      std::vector<std::string>* profile_guids,
      std::vector<std::string>* credit_card_guids);

  // Removes origin URLs from the autofill_profiles and credit_cards tables if
  // they were written on or after |delete_begin| and strictly before
  // |delete_end|.  Returns the list of modified profiles in |profiles|.  Return
  // value is true if all rows were successfully updated.  Returns false on
  // database error.  In that case, the output vector state is undefined, and
  // may be partially filled.
  bool RemoveOriginURLsModifiedBetween(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      std::vector<std::unique_ptr<AutofillProfile>>* profiles);

  // Retrieves all profiles in the database that have been deleted since last
  // "empty" of the trash.
  bool GetAutofillProfilesInTrash(std::vector<std::string>* guids);

  // Empties the Autofill profiles "trash can".
  bool EmptyAutofillProfilesTrash();

  // Retrieves all profiles in the database that have been deleted since last
  // "empty" of the trash.
  bool AddAutofillGUIDToTrash(const std::string& guid);

  // Clear all profiles.
  bool ClearAutofillProfiles();

  // Read all the stored metadata for |model_type| and fill |metadata_batch|
  // with it.
  bool GetAllSyncMetadata(syncer::ModelType model_type,
                          syncer::MetadataBatch* metadata_batch);

  // Update the metadata row for |model_type|, keyed by |storage_key|, to
  // contain the contents of |metadata|.
  bool UpdateSyncMetadata(syncer::ModelType model_type,
                          const std::string& storage_key,
                          const sync_pb::EntityMetadata& metadata);

  // Remove the metadata row of type |model_type| keyed by |storage|key|.
  bool ClearSyncMetadata(syncer::ModelType model_type,
                         const std::string& storage_key);

  // Update the stored sync state for the |model_type|.
  bool UpdateModelTypeState(syncer::ModelType model_type,
                            const sync_pb::ModelTypeState& model_type_state);

  // Clear the stored sync state for |model_type|.
  bool ClearModelTypeState(syncer::ModelType model_type);

  // Table migration functions. NB: These do not and should not rely on other
  // functions in this class. The implementation of a function such as
  // GetCreditCard may change over time, but MigrateToVersionXX should never
  // change.
  bool MigrateToVersion54AddI18nFieldsAndRemoveDeprecatedFields();
  bool MigrateToVersion55MergeAutofillDatesTable();
  bool MigrateToVersion56AddProfileLanguageCodeForFormatting();
  bool MigrateToVersion57AddFullNameField();
  bool MigrateToVersion60AddServerCards();
  bool MigrateToVersion61AddUsageStats();
  bool MigrateToVersion62AddUsageStatsForUnmaskedCards();
  bool MigrateToVersion63AddServerRecipientName();
  bool MigrateToVersion64AddUnmaskDate();
  bool MigrateToVersion65AddServerMetadataTables();
  bool MigrateToVersion66AddCardBillingAddress();
  bool MigrateToVersion67AddMaskedCardBillingAddress();
  bool MigrateToVersion70AddSyncMetadata();
  bool MigrateToVersion71AddHasConvertedAndBillingAddressIdMetadata();

  // Max data length saved in the table, AKA the maximum length allowed for
  // form data.
  // Copied to components/autofill/ios/browser/resources/autofill_controller.js.
  static const size_t kMaxDataLength;

 private:
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, Autofill);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, Autofill_AddChanges);
  FRIEND_TEST_ALL_PREFIXES(
      AutofillTableTest,
      Autofill_GetCountOfValuesContainedBetween);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, Autofill_RemoveBetweenChanges);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, Autofill_UpdateDontReplace);
  FRIEND_TEST_ALL_PREFIXES(
      AutofillTableTest,
      Autofill_RemoveFormElementsAddedBetween_UsedOnlyBefore);
  FRIEND_TEST_ALL_PREFIXES(
      AutofillTableTest,
      Autofill_RemoveFormElementsAddedBetween_UsedOnlyAfter);
  FRIEND_TEST_ALL_PREFIXES(
      AutofillTableTest,
      Autofill_RemoveFormElementsAddedBetween_UsedOnlyDuring);
  FRIEND_TEST_ALL_PREFIXES(
      AutofillTableTest,
      Autofill_RemoveFormElementsAddedBetween_UsedBeforeAndDuring);
  FRIEND_TEST_ALL_PREFIXES(
      AutofillTableTest,
      Autofill_RemoveFormElementsAddedBetween_UsedDuringAndAfter);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, Autofill_AddFormFieldValues);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, AutofillProfile);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, UpdateAutofillProfile);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, AutofillProfileTrash);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, AutofillProfileTrashInteraction);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest,
                           RemoveAutofillDataModifiedBetween);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, CreditCard);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest, UpdateCreditCard);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest,
                           Autofill_GetAllAutofillEntries_OneResult);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest,
                           Autofill_GetAllAutofillEntries_TwoDistinct);
  FRIEND_TEST_ALL_PREFIXES(AutofillTableTest,
                           Autofill_GetAllAutofillEntries_TwoSame);

  // Methods for adding autofill entries at a specified time.  For
  // testing only.
  bool AddFormFieldValuesTime(
      const std::vector<FormFieldData>& elements,
      std::vector<AutofillChange>* changes,
      base::Time time);
  bool AddFormFieldValueTime(const FormFieldData& element,
                             std::vector<AutofillChange>* changes,
                             base::Time time);

  bool GetAllSyncEntityMetadata(syncer::ModelType model_type,
                                syncer::EntityMetadataMap* metadata_records);

  bool GetModelTypeState(syncer::ModelType model_type,
                         sync_pb::ModelTypeState* state);

  // Insert a single AutofillEntry into the autofill table.
  bool InsertAutofillEntry(const AutofillEntry& entry);

  // Checks if the trash is empty.
  bool IsAutofillProfilesTrashEmpty();

  // Checks if the guid is in the trash.
  bool IsAutofillGUIDInTrash(const std::string& guid);

  bool InitMainTable();
  bool InitCreditCardsTable();
  bool InitDatesTable();
  bool InitProfilesTable();
  bool InitProfileNamesTable();
  bool InitProfileEmailsTable();
  bool InitProfilePhonesTable();
  bool InitProfileTrashTable();
  bool InitMaskedCreditCardsTable();
  bool InitUnmaskedCreditCardsTable();
  bool InitServerCardMetadataTable();
  bool InitServerAddressesTable();
  bool InitServerAddressMetadataTable();
  bool InitAutofillSyncMetadataTable();
  bool InitModelTypeStateTable();

  std::unique_ptr<AutofillTableEncryptor> autofill_table_encryptor_;

  DISALLOW_COPY_AND_ASSIGN(AutofillTable);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_TABLE_H_
