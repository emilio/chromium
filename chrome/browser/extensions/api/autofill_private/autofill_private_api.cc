// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autofill_private/autofill_private_api.h"

#include <stddef.h>
#include <utility>

#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/autofill_private/autofill_util.h"
#include "chrome/common/extensions/api/autofill_private.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_registry.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui_component.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/localization.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_private = extensions::api::autofill_private;
namespace addressinput = i18n::addressinput;

namespace {

static const char kSettingsOrigin[] = "Chrome settings";
static const char kErrorDataUnavailable[] = "Autofill data unavailable.";

// Fills |components| with the address UI components that should be used to
// input an address for |country_code| when UI BCP 47 language code is
// |ui_language_code|.
void PopulateAddressComponents(
    const std::string& country_code,
    const std::string& ui_language_code,
    autofill_private::AddressComponents* address_components) {
  DCHECK(address_components);

  i18n::addressinput::Localization localization;
  localization.SetGetter(l10n_util::GetStringUTF8);
  std::string best_address_language_code;
  std::vector<addressinput::AddressUiComponent> components =
      i18n::addressinput::BuildComponents(
          country_code,
          localization,
          ui_language_code,
          &best_address_language_code);
  if (components.empty()) {
    static const char kDefaultCountryCode[] = "US";
    components = i18n::addressinput::BuildComponents(
        kDefaultCountryCode,
        localization,
        ui_language_code,
        &best_address_language_code);
  }
  address_components->language_code = best_address_language_code;
  DCHECK(!components.empty());

  autofill_private::AddressComponentRow* row = nullptr;
  for (size_t i = 0; i < components.size(); ++i) {
    if (!row ||
        components[i - 1].length_hint ==
            addressinput::AddressUiComponent::HINT_LONG ||
        components[i].length_hint ==
            addressinput::AddressUiComponent::HINT_LONG) {
      address_components->components.push_back(
          autofill_private::AddressComponentRow());
      row = &address_components->components.back();
    }

    autofill_private::AddressComponent component;
    component.field_name = components[i].name;

    switch (components[i].field) {
      case i18n::addressinput::COUNTRY:
        component.field =
            autofill_private::AddressField::ADDRESS_FIELD_COUNTRY_CODE;
        break;
      case i18n::addressinput::ADMIN_AREA:
        component.field =
            autofill_private::AddressField::ADDRESS_FIELD_ADDRESS_LEVEL_1;
        break;
      case i18n::addressinput::LOCALITY:
        component.field =
            autofill_private::AddressField::ADDRESS_FIELD_ADDRESS_LEVEL_2;
        break;
      case i18n::addressinput::DEPENDENT_LOCALITY:
        component.field =
            autofill_private::AddressField::ADDRESS_FIELD_ADDRESS_LEVEL_3;
        break;
      case i18n::addressinput::SORTING_CODE:
        component.field =
            autofill_private::AddressField::ADDRESS_FIELD_SORTING_CODE;
        break;
      case i18n::addressinput::POSTAL_CODE:
        component.field =
            autofill_private::AddressField::ADDRESS_FIELD_POSTAL_CODE;
        break;
      case i18n::addressinput::STREET_ADDRESS:
        component.field =
            autofill_private::AddressField::ADDRESS_FIELD_ADDRESS_LINES;
        break;
      case i18n::addressinput::ORGANIZATION:
        component.field =
            autofill_private::AddressField::ADDRESS_FIELD_COMPANY_NAME;
        break;
      case i18n::addressinput::RECIPIENT:
        component.field =
            autofill_private::AddressField::ADDRESS_FIELD_FULL_NAME;
        break;
    }

    switch (components[i].length_hint) {
      case addressinput::AddressUiComponent::HINT_LONG:
        component.is_long_field = true;
        break;
      case addressinput::AddressUiComponent::HINT_SHORT:
        component.is_long_field = false;
        break;
    }

    row->row.push_back(std::move(component));
  }
}

// Searches the |list| for the value at |index|.  If this value is present in
// any of the rest of the list, then the item (at |index|) is removed. The
// comparison of phone number values is done on normalized versions of the phone
// number values.
void RemoveDuplicatePhoneNumberAtIndex(
    size_t index, const std::string& country_code, base::ListValue* list) {
  base::string16 new_value;
  if (!list->GetString(index, &new_value)) {
    NOTREACHED() << "List should have a value at index " << index;
    return;
  }

  bool is_duplicate = false;
  std::string app_locale = g_browser_process->GetApplicationLocale();
  for (size_t i = 0; i < list->GetSize() && !is_duplicate; ++i) {
    if (i == index)
      continue;

    base::string16 existing_value;
    if (!list->GetString(i, &existing_value)) {
      NOTREACHED() << "List should have a value at index " << i;
      continue;
    }
    is_duplicate = autofill::i18n::PhoneNumbersMatch(
        new_value, existing_value, country_code, app_locale);
  }

  if (is_duplicate)
    list->Remove(index, nullptr);
}

}  // namespace

namespace extensions {

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateSaveAddressFunction

AutofillPrivateSaveAddressFunction::AutofillPrivateSaveAddressFunction()
    : chrome_details_(this) {}

AutofillPrivateSaveAddressFunction::~AutofillPrivateSaveAddressFunction() {}

ExtensionFunction::ResponseAction AutofillPrivateSaveAddressFunction::Run() {
  std::unique_ptr<api::autofill_private::SaveAddress::Params> parameters =
      api::autofill_private::SaveAddress::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
      chrome_details_.GetProfile());
  if (!personal_data || !personal_data->IsDataLoaded())
    return RespondNow(Error(kErrorDataUnavailable));

  api::autofill_private::AddressEntry* address = &parameters->address;

  std::string guid = address->guid ? *address->guid : "";
  autofill::AutofillProfile profile(guid, kSettingsOrigin);

  // Strings from JavaScript use UTF-8 encoding. This container is used as an
  // intermediate container for functions which require UTF-16 strings.
  std::vector<base::string16> string16Container;

  if (address->full_names) {
    std::string full_name;
    if (!address->full_names->empty())
      full_name = address->full_names->at(0);
    profile.SetInfo(autofill::AutofillType(autofill::NAME_FULL),
                    base::UTF8ToUTF16(full_name),
                    g_browser_process->GetApplicationLocale());
  }

  if (address->company_name) {
    profile.SetRawInfo(
        autofill::COMPANY_NAME,
        base::UTF8ToUTF16(*address->company_name));
  }

  if (address->address_lines) {
    profile.SetRawInfo(
        autofill::ADDRESS_HOME_STREET_ADDRESS,
        base::UTF8ToUTF16(*address->address_lines));
  }

  if (address->address_level1) {
    profile.SetRawInfo(
        autofill::ADDRESS_HOME_STATE,
        base::UTF8ToUTF16(*address->address_level1));
  }

  if (address->address_level2) {
    profile.SetRawInfo(
        autofill::ADDRESS_HOME_CITY,
        base::UTF8ToUTF16(*address->address_level2));
  }

  if (address->address_level3) {
    profile.SetRawInfo(
        autofill::ADDRESS_HOME_DEPENDENT_LOCALITY,
        base::UTF8ToUTF16(*address->address_level3));
  }

  if (address->postal_code) {
    profile.SetRawInfo(
        autofill::ADDRESS_HOME_ZIP,
        base::UTF8ToUTF16(*address->postal_code));
  }

  if (address->sorting_code) {
    profile.SetRawInfo(
        autofill::ADDRESS_HOME_SORTING_CODE,
        base::UTF8ToUTF16(*address->sorting_code));
  }

  if (address->country_code) {
    profile.SetRawInfo(
        autofill::ADDRESS_HOME_COUNTRY,
        base::UTF8ToUTF16(*address->country_code));
  }

  if (address->phone_numbers) {
    std::string phone;
    if (!address->phone_numbers->empty())
      phone = address->phone_numbers->at(0);
    profile.SetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER,
                       base::UTF8ToUTF16(phone));
  }

  if (address->email_addresses) {
    std::string email;
    if (!address->email_addresses->empty())
      email = address->email_addresses->at(0);
    profile.SetRawInfo(autofill::EMAIL_ADDRESS, base::UTF8ToUTF16(email));
  }

  if (address->language_code)
    profile.set_language_code(*address->language_code);

  if (!base::IsValidGUID(profile.guid())) {
    profile.set_guid(base::GenerateGUID());
    personal_data->AddProfile(profile);
  } else {
    personal_data->UpdateProfile(profile);
  }

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetCountryListFunction

AutofillPrivateGetCountryListFunction::AutofillPrivateGetCountryListFunction()
    : chrome_details_(this) {}

AutofillPrivateGetCountryListFunction::
    ~AutofillPrivateGetCountryListFunction() {}

ExtensionFunction::ResponseAction AutofillPrivateGetCountryListFunction::Run() {
  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
          chrome_details_.GetProfile());

  // Return an empty list if data is not loaded.
  if (!(personal_data && personal_data->IsDataLoaded())) {
    autofill_util::CountryEntryList empty_list;
    return RespondNow(ArgumentList(
        api::autofill_private::GetCountryList::Results::Create(empty_list)));
  }

  autofill_util::CountryEntryList country_list =
      autofill_util::GenerateCountryList(*personal_data);

  return RespondNow(ArgumentList(
      api::autofill_private::GetCountryList::Results::Create(country_list)));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetAddressComponentsFunction

AutofillPrivateGetAddressComponentsFunction::
    ~AutofillPrivateGetAddressComponentsFunction() {}

ExtensionFunction::ResponseAction
    AutofillPrivateGetAddressComponentsFunction::Run() {
  std::unique_ptr<api::autofill_private::GetAddressComponents::Params>
      parameters =
          api::autofill_private::GetAddressComponents::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  autofill_private::AddressComponents components;
  PopulateAddressComponents(
      parameters->country_code,
      g_browser_process->GetApplicationLocale(),
      &components);

  return RespondNow(OneArgument(components.ToValue()));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetAddressListFunction

AutofillPrivateGetAddressListFunction::AutofillPrivateGetAddressListFunction()
    : chrome_details_(this) {}

AutofillPrivateGetAddressListFunction::
    ~AutofillPrivateGetAddressListFunction() {}

ExtensionFunction::ResponseAction AutofillPrivateGetAddressListFunction::Run() {
  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
          chrome_details_.GetProfile());

  DCHECK(personal_data && personal_data->IsDataLoaded());

  autofill_util::AddressEntryList address_list =
      autofill_util::GenerateAddressList(*personal_data);

  return RespondNow(ArgumentList(
      api::autofill_private::GetAddressList::Results::Create(address_list)));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateSaveCreditCardFunction

AutofillPrivateSaveCreditCardFunction::AutofillPrivateSaveCreditCardFunction()
    : chrome_details_(this) {}

AutofillPrivateSaveCreditCardFunction::
    ~AutofillPrivateSaveCreditCardFunction() {}

ExtensionFunction::ResponseAction AutofillPrivateSaveCreditCardFunction::Run() {
  std::unique_ptr<api::autofill_private::SaveCreditCard::Params> parameters =
      api::autofill_private::SaveCreditCard::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
      chrome_details_.GetProfile());
  if (!personal_data || !personal_data->IsDataLoaded())
    return RespondNow(Error(kErrorDataUnavailable));

  api::autofill_private::CreditCardEntry* card = &parameters->card;

  std::string guid = card->guid ? *card->guid : "";
  autofill::CreditCard credit_card(guid, kSettingsOrigin);

  if (card->name) {
    credit_card.SetRawInfo(autofill::CREDIT_CARD_NAME_FULL,
                           base::UTF8ToUTF16(*card->name));
  }

  if (card->card_number) {
    credit_card.SetRawInfo(
        autofill::CREDIT_CARD_NUMBER,
        base::UTF8ToUTF16(*card->card_number));
  }

  if (card->expiration_month) {
    credit_card.SetRawInfo(
        autofill::CREDIT_CARD_EXP_MONTH,
        base::UTF8ToUTF16(*card->expiration_month));
  }

  if (card->expiration_year) {
    credit_card.SetRawInfo(
        autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR,
        base::UTF8ToUTF16(*card->expiration_year));
  }

  if (!base::IsValidGUID(credit_card.guid())) {
    credit_card.set_guid(base::GenerateGUID());
    personal_data->AddCreditCard(credit_card);
  } else {
    personal_data->UpdateCreditCard(credit_card);
  }

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateRemoveEntryFunction

AutofillPrivateRemoveEntryFunction::AutofillPrivateRemoveEntryFunction()
    : chrome_details_(this) {}

AutofillPrivateRemoveEntryFunction::~AutofillPrivateRemoveEntryFunction() {}

ExtensionFunction::ResponseAction AutofillPrivateRemoveEntryFunction::Run() {
  std::unique_ptr<api::autofill_private::RemoveEntry::Params> parameters =
      api::autofill_private::RemoveEntry::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
      chrome_details_.GetProfile());
  if (!personal_data || !personal_data->IsDataLoaded())
    return RespondNow(Error(kErrorDataUnavailable));

  personal_data->RemoveByGUID(parameters->guid);

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateValidatePhoneNumbersFunction

AutofillPrivateValidatePhoneNumbersFunction::
    ~AutofillPrivateValidatePhoneNumbersFunction() {}

ExtensionFunction::ResponseAction
    AutofillPrivateValidatePhoneNumbersFunction::Run() {
  std::unique_ptr<api::autofill_private::ValidatePhoneNumbers::Params>
      parameters =
          api::autofill_private::ValidatePhoneNumbers::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  api::autofill_private::ValidatePhoneParams* params = &parameters->params;

  // Extract the phone numbers into a ListValue.
  std::unique_ptr<base::ListValue> phone_numbers(new base::ListValue);
  phone_numbers->AppendStrings(params->phone_numbers);

  RemoveDuplicatePhoneNumberAtIndex(params->index_of_new_number,
                                    params->country_code, phone_numbers.get());

  return RespondNow(OneArgument(std::move(phone_numbers)));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateMaskCreditCardFunction

AutofillPrivateMaskCreditCardFunction::AutofillPrivateMaskCreditCardFunction()
    : chrome_details_(this) {}

AutofillPrivateMaskCreditCardFunction::
    ~AutofillPrivateMaskCreditCardFunction() {}

ExtensionFunction::ResponseAction AutofillPrivateMaskCreditCardFunction::Run() {
  std::unique_ptr<api::autofill_private::MaskCreditCard::Params> parameters =
      api::autofill_private::MaskCreditCard::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
      chrome_details_.GetProfile());
  if (!personal_data || !personal_data->IsDataLoaded())
    return RespondNow(Error(kErrorDataUnavailable));

  personal_data->ResetFullServerCard(parameters->guid);

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetCreditCardListFunction

AutofillPrivateGetCreditCardListFunction::
    AutofillPrivateGetCreditCardListFunction()
    : chrome_details_(this) {}

AutofillPrivateGetCreditCardListFunction::
    ~AutofillPrivateGetCreditCardListFunction() {}

ExtensionFunction::ResponseAction
AutofillPrivateGetCreditCardListFunction::Run() {
  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
          chrome_details_.GetProfile());

  DCHECK(personal_data && personal_data->IsDataLoaded());

  autofill_util::CreditCardEntryList credit_card_list =
      autofill_util::GenerateCreditCardList(*personal_data);

  return RespondNow(
      ArgumentList(api::autofill_private::GetCreditCardList::Results::Create(
          credit_card_list)));
}

}  // namespace extensions
