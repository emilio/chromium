// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/payment_request_util.h"

#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/payments/payment_request.h"
#include "ios/web/public/payments/payment_request.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace payment_request_util {

NSString* GetNameLabelFromAutofillProfile(
    const autofill::AutofillProfile& profile) {
  return base::SysUTF16ToNSString(
      profile.GetInfo(autofill::AutofillType(autofill::NAME_FULL),
                      GetApplicationContext()->GetApplicationLocale()));
}

NSString* GetAddressLabelFromAutofillProfile(
    const autofill::AutofillProfile& profile) {
  // Name and country are not included in the shipping address label.
  std::vector<autofill::ServerFieldType> label_fields;
  label_fields.push_back(autofill::COMPANY_NAME);
  label_fields.push_back(autofill::ADDRESS_HOME_LINE1);
  label_fields.push_back(autofill::ADDRESS_HOME_LINE2);
  label_fields.push_back(autofill::ADDRESS_HOME_DEPENDENT_LOCALITY);
  label_fields.push_back(autofill::ADDRESS_HOME_CITY);
  label_fields.push_back(autofill::ADDRESS_HOME_STATE);
  label_fields.push_back(autofill::ADDRESS_HOME_ZIP);
  label_fields.push_back(autofill::ADDRESS_HOME_SORTING_CODE);

  base::string16 label = profile.ConstructInferredLabel(
      label_fields, label_fields.size(),
      GetApplicationContext()->GetApplicationLocale());
  return !label.empty() ? base::SysUTF16ToNSString(label) : nil;
}

NSString* GetPhoneNumberLabelFromAutofillProfile(
    const autofill::AutofillProfile& profile) {
  base::string16 label = profile.GetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER);
  return !label.empty() ? base::SysUTF16ToNSString(label) : nil;
}

NSString* GetEmailLabelFromAutofillProfile(
    const autofill::AutofillProfile& profile) {
  base::string16 label = profile.GetRawInfo(autofill::EMAIL_ADDRESS);
  return !label.empty() ? base::SysUTF16ToNSString(label) : nil;
}

NSString* GetShippingSectionTitle(const PaymentRequest& payment_request) {
  switch (payment_request.payment_options().shipping_type) {
    case web::PaymentShippingType::SHIPPING:
      return l10n_util::GetNSString(IDS_PAYMENTS_SHIPPING_SUMMARY_LABEL);
    case web::PaymentShippingType::DELIVERY:
      return l10n_util::GetNSString(IDS_PAYMENTS_DELIVERY_SUMMARY_LABEL);
    case web::PaymentShippingType::PICKUP:
      return l10n_util::GetNSString(IDS_PAYMENTS_PICKUP_SUMMARY_LABEL);
    default:
      NOTREACHED();
      return @"";
  }
}

NSString* GetShippingAddressSelectorTitle(
    const PaymentRequest& payment_request) {
  switch (payment_request.payment_options().shipping_type) {
    case web::PaymentShippingType::SHIPPING:
      return l10n_util::GetNSString(IDS_PAYMENTS_SHIPPING_ADDRESS_LABEL);
    case web::PaymentShippingType::DELIVERY:
      return l10n_util::GetNSString(IDS_PAYMENTS_DELIVERY_ADDRESS_LABEL);
    case web::PaymentShippingType::PICKUP:
      return l10n_util::GetNSString(IDS_PAYMENTS_PICKUP_ADDRESS_LABEL);
    default:
      NOTREACHED();
      return @"";
  }
}

NSString* GetShippingAddressSelectorInfoMessage(
    const PaymentRequest& payment_request) {
  switch (payment_request.payment_options().shipping_type) {
    case web::PaymentShippingType::SHIPPING:
      return l10n_util::GetNSString(
          IDS_PAYMENTS_SELECT_SHIPPING_ADDRESS_FOR_SHIPPING_METHODS);
    case web::PaymentShippingType::DELIVERY:
      return l10n_util::GetNSString(
          IDS_PAYMENTS_SELECT_DELIVERY_ADDRESS_FOR_DELIVERY_METHODS);
    case web::PaymentShippingType::PICKUP:
      return l10n_util::GetNSString(
          IDS_PAYMENTS_SELECT_PICKUP_ADDRESS_FOR_PICKUP_METHODS);
    default:
      NOTREACHED();
      return @"";
  }
}

NSString* GetShippingAddressSelectorErrorMessage(
    const PaymentRequest& payment_request) {
  if (!payment_request.payment_details().error.empty())
    return base::SysUTF16ToNSString(payment_request.payment_details().error);

  switch (payment_request.payment_options().shipping_type) {
    case web::PaymentShippingType::SHIPPING:
      return l10n_util::GetNSString(IDS_PAYMENTS_UNSUPPORTED_SHIPPING_ADDRESS);
    case web::PaymentShippingType::DELIVERY:
      return l10n_util::GetNSString(IDS_PAYMENTS_UNSUPPORTED_DELIVERY_ADDRESS);
    case web::PaymentShippingType::PICKUP:
      return l10n_util::GetNSString(IDS_PAYMENTS_UNSUPPORTED_PICKUP_ADDRESS);
    default:
      NOTREACHED();
      return @"";
  }
}

NSString* GetShippingOptionSelectorTitle(
    const PaymentRequest& payment_request) {
  switch (payment_request.payment_options().shipping_type) {
    case web::PaymentShippingType::SHIPPING:
      return l10n_util::GetNSString(IDS_PAYMENTS_SHIPPING_OPTION_LABEL);
    case web::PaymentShippingType::DELIVERY:
      return l10n_util::GetNSString(IDS_PAYMENTS_DELIVERY_OPTION_LABEL);
    case web::PaymentShippingType::PICKUP:
      return l10n_util::GetNSString(IDS_PAYMENTS_PICKUP_OPTION_LABEL);
    default:
      NOTREACHED();
      return @"";
  }
}

NSString* GetShippingOptionSelectorErrorMessage(
    const PaymentRequest& payment_request) {
  if (!payment_request.payment_details().error.empty())
    return base::SysUTF16ToNSString(payment_request.payment_details().error);

  switch (payment_request.payment_options().shipping_type) {
    case web::PaymentShippingType::SHIPPING:
      return l10n_util::GetNSString(IDS_PAYMENTS_UNSUPPORTED_SHIPPING_OPTION);
    case web::PaymentShippingType::DELIVERY:
      return l10n_util::GetNSString(IDS_PAYMENTS_UNSUPPORTED_DELIVERY_OPTION);
    case web::PaymentShippingType::PICKUP:
      return l10n_util::GetNSString(IDS_PAYMENTS_UNSUPPORTED_PICKUP_OPTION);
    default:
      NOTREACHED();
      return @"";
  }
}

}  // namespace payment_request_util
