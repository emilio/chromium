// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/chrome_activity_item_source.h"

#import <MobileCoreServices/MobileCoreServices.h>

#include "base/logging.h"
#import "ios/chrome/browser/ui/activity_services/activity_type_util.h"
#import "ios/chrome/browser/ui/activity_services/appex_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - UIActivityTextSource

@interface UIActivityTextSource () {
  // The shared text.
  NSString* _text;
}

@end

@implementation UIActivityTextSource

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithText:(NSString*)text {
  DCHECK(text);
  self = [super init];
  if (self) {
    _text = [text copy];
  }
  return self;
}

#pragma mark - UIActivityItemSource

- (id)activityViewController:(UIActivityViewController*)activityViewController
         itemForActivityType:(NSString*)activityType {
  // The UIActivityTypeMail is excluded because it obtains the text through the
  // UIActivityURLSource's |...subjectForActivityType:| method.
  // The UIActivityTypeCopyToPasteboard and UIActivityTypeMessage are excluded
  // because the pasteboard and message should only contain the URL, to match
  // Safari's behavior.
  NSSet* excludedActivityTypes = [NSSet setWithArray:@[
    UIActivityTypeCopyToPasteboard, UIActivityTypeMail, UIActivityTypeMessage
  ]];
  if ([excludedActivityTypes containsObject:activityType]) {
    return nil;
  }
  return _text;
}

- (id)activityViewControllerPlaceholderItem:
    (UIActivityViewController*)activityViewController {
  return _text;
}

@end

#pragma mark - UIActivityImageSource

@interface UIActivityImageSource () {
  // The shared image.
  UIImage* _image;
}

@end

@implementation UIActivityImageSource

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithImage:(UIImage*)image {
  DCHECK(image);
  self = [super init];
  if (self) {
    _image = image;
  }
  return self;
}

#pragma mark - UIActivityItemSource

- (id)activityViewController:(UIActivityViewController*)activityViewController
         itemForActivityType:(NSString*)activityType {
  return _image;
}

- (id)activityViewControllerPlaceholderItem:
    (UIActivityViewController*)activityViewController {
  return _image;
}

@end

#pragma mark - UIActivityURLSource

@interface UIActivityURLSource () {
  NSString* _subject;
  NSURL* _url;
  ThumbnailGeneratorBlock _thumbnailGenerator;
}
@end

@implementation UIActivityURLSource

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithURL:(NSURL*)url
                    subject:(NSString*)subject
         thumbnailGenerator:(ThumbnailGeneratorBlock)thumbnailGenerator {
  DCHECK(url);
  DCHECK(subject);
  DCHECK(thumbnailGenerator);
  self = [super init];
  if (self) {
    _url = url;
    _subject = [subject copy];
    _thumbnailGenerator = thumbnailGenerator;
  }
  return self;
}

#pragma mark - UIActivityItemSource

- (id)activityViewControllerPlaceholderItem:
    (UIActivityViewController*)activityViewController {
  // Return the current URL as a placeholder
  return _url;
}

- (NSString*)activityViewController:
                 (UIActivityViewController*)activityViewController
             subjectForActivityType:(NSString*)activityType {
  return _subject;
}

- (id)activityViewController:(UIActivityViewController*)activityViewController
         itemForActivityType:(NSString*)activityType {
  NSNumber* versionNumber =
      activity_type_util::PasswordAppExActivityVersion(activityType);
  if (!versionNumber)
    return _url;

  // Constructs an NSExtensionItem object from the URL being "shared".
  NSDictionary* appExItems = @{
    activity_services::kPasswordAppExVersionNumberKey : versionNumber,
    activity_services::kPasswordAppExURLStringKey : [_url absoluteString]
  };
  NSItemProvider* itemProvider = [[NSItemProvider alloc]
        initWithItem:appExItems
      typeIdentifier:activity_services::kUTTypeAppExtensionFindLoginAction];
  NSExtensionItem* extensionItem = [[NSExtensionItem alloc] init];
  [extensionItem setAttachments:@[ itemProvider ]];
  return extensionItem;
}

- (NSString*)activityViewController:
                 (UIActivityViewController*)activityViewController
    dataTypeIdentifierForActivityType:(NSString*)activityType {
  // This UTI now satisfies both the Password Management App Extension and the
  // usual NSURL for Share extensions. If the following DCHECK fails, it is
  // probably due to missing or incorrect UTImportedTypeDeclarations in
  // Info.plist.
  NSString* findLoginType =
      activity_services::kUTTypeAppExtensionFindLoginAction;
  DCHECK(UTTypeConformsTo((__bridge CFStringRef)findLoginType, kUTTypeURL));
  DCHECK(UTTypeConformsTo((__bridge CFStringRef)findLoginType,
                          (__bridge CFStringRef)
                              @"org.appextension.chrome-password-action"));
  // This method may be called before or after the presentation of the
  // UIActivityViewController. When called before (i.e. user has not made a
  // selection of which AppEx to launch), |activityType| is nil. If and when
  // called after UIActivityViewController has been presented and dismissed
  // after user made a choice of which AppEx to run, this method may be called
  // with |activityType| equals to the bundle ID of the AppEx selected.
  // Default action is to return @"public.url" UTType.
  if (!activityType ||
      activity_type_util::PasswordAppExActivityVersion(activityType)) {
    return findLoginType;
  }
  return (NSString*)kUTTypeURL;
}

- (UIImage*)activityViewController:
                (UIActivityViewController*)activityViewController
     thumbnailImageForActivityType:(UIActivityType)activityType
                     suggestedSize:(CGSize)size {
  return _thumbnailGenerator(size);
}

@end
