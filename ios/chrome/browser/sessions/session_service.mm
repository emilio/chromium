// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_service.h"

#import <UIKit/UIKit.h>

#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/mac/bind_objc_block.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_restrictions.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/sessions/session_window.h"
#import "ios/web/navigation/crw_session_certificate_policy_manager.h"
#import "ios/web/public/crw_navigation_item_storage.h"
#import "ios/web/public/crw_session_storage.h"
#include "ios/web/public/web_thread.h"

// When C++ exceptions are disabled, the C++ library defines |try| and
// |catch| so as to allow exception-expecting C++ code to build properly when
// language support for exceptions is not present.  These macros interfere
// with the use of |@try| and |@catch| in Objective-C files such as this one.
// Undefine these macros here, after everything has been #included, since
// there will be no C++ uses and only Objective-C uses from this point on.
#undef try
#undef catch

const NSTimeInterval kSaveDelay = 2.5;  // Value taken from Desktop Chrome.

@interface SessionWindowUnarchiver ()

// Register compatibility aliases to support loading serialised sessions
// informations when the serialised classes are renamed.
+ (void)registerCompatibilityAliases;

@end

@implementation SessionWindowUnarchiver

@synthesize browserState = _browserState;

- (instancetype)initForReadingWithData:(NSData*)data
                          browserState:(ios::ChromeBrowserState*)browserState {
  if (self = [super initForReadingWithData:data]) {
    _browserState = browserState;
  }
  return self;
}

- (instancetype)initForReadingWithData:(NSData*)data {
  return [self initForReadingWithData:data browserState:nullptr];
}

+ (void)initialize {
  [super initialize];
  [self registerCompatibilityAliases];
}

// When adding a new compatibility alias here, create a new crbug to track its
// removal and mark it with a release at least one year after the introduction
// of the alias.
+ (void)registerCompatibilityAliases {
  // TODO(crbug.com/661633): those aliases where introduced between M57 and
  // M58, so remove them after M67 has shipped to stable.
  [SessionWindowUnarchiver setClass:[CRWSessionCertificatePolicyManager class]
                       forClassName:@"SessionCertificatePolicyManager"];
  [SessionWindowUnarchiver setClass:[CRWSessionStorage class]
                       forClassName:@"SessionController"];
  [SessionWindowUnarchiver setClass:[CRWSessionStorage class]
                       forClassName:@"CRWSessionController"];
  [SessionWindowUnarchiver setClass:[CRWNavigationItemStorage class]
                       forClassName:@"SessionEntry"];
  [SessionWindowUnarchiver setClass:[CRWNavigationItemStorage class]
                       forClassName:@"CRWSessionEntry"];
  [SessionWindowUnarchiver setClass:[SessionWindowIOS class]
                       forClassName:@"SessionWindow"];

  // TODO(crbug.com/661633): this alias was introduced between M58 and M59, so
  // remove it after M68 has shipped to stable.
  [SessionWindowUnarchiver setClass:[CRWSessionStorage class]
                       forClassName:@"CRWNavigationManagerStorage"];
}

@end

@interface SessionServiceIOS () {
  // The SequencedTaskRunner on which File IO operations are performed.
  scoped_refptr<base::SequencedTaskRunner> _taskRunner;

  // Maps save directories to the pending SessionWindow for the delayed
  // save behavior.
  base::scoped_nsobject<NSMutableDictionary> _pendingWindows;
}

// Saves the session corresponding to |directory| on the background
// task runner |_taskRunner|.
- (void)performSaveToDirectoryInBackground:(NSString*)directory;
@end

@implementation SessionServiceIOS

+ (SessionServiceIOS*)sharedService {
  static SessionServiceIOS* singleton = nil;
  if (!singleton) {
    singleton = [[[self class] alloc] init];
  }
  return singleton;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _pendingWindows.reset([[NSMutableDictionary alloc] init]);
    auto* pool = web::WebThread::GetBlockingPool();
    _taskRunner = pool->GetSequencedTaskRunner(pool->GetSequenceToken());
  }
  return self;
}

// Returns the path of the session file.
- (NSString*)sessionFilePathForDirectory:(NSString*)directory {
  return [directory stringByAppendingPathComponent:@"session.plist"];
}

// Do the work of saving on a background thread. Assumes |window| is threadsafe.
- (void)performSaveToDirectoryInBackground:(NSString*)directory {
  DCHECK(directory);
  DCHECK([_pendingWindows objectForKey:directory] != nil);
  UIBackgroundTaskIdentifier identifier = [[UIApplication sharedApplication]
      beginBackgroundTaskWithExpirationHandler:^{
      }];
  DCHECK(identifier != UIBackgroundTaskInvalid);

  // Put the window into a local var so it can be retained for the block, yet
  // we can remove it from the dictionary to allow queuing another save.
  SessionWindowIOS* localWindow =
      [[_pendingWindows objectForKey:directory] retain];
  [_pendingWindows removeObjectForKey:directory];

  _taskRunner->PostTask(
      FROM_HERE, base::BindBlock(^{
        @try {
          [self performSaveWindow:localWindow toDirectory:directory];
        } @catch (NSException* e) {
          // Do nothing.
        }
        [localWindow release];
        [[UIApplication sharedApplication] endBackgroundTask:identifier];
      }));
}

// Saves a SessionWindowIOS in a given directory. In case the directory doesn't
// exists it will be automatically created.
- (void)performSaveWindow:(SessionWindowIOS*)window
              toDirectory:(NSString*)directory {
  base::ThreadRestrictions::AssertIOAllowed();
  NSFileManager* fileManager = [NSFileManager defaultManager];
  BOOL isDir;
  if (![fileManager fileExistsAtPath:directory isDirectory:&isDir]) {
    NSError* error = nil;
    BOOL result = [fileManager createDirectoryAtPath:directory
                         withIntermediateDirectories:YES
                                          attributes:nil
                                               error:&error];
    DCHECK(result);
    if (!result) {
      DLOG(ERROR) << "Error creating destination dir: "
                  << base::SysNSStringToUTF8([error description]);
      return;
    }
  } else {
    DCHECK(isDir);
    if (!isDir) {
      DLOG(ERROR) << "Destination Directory already exists and is a file";
      return;
    }
  }

  NSString* filename = [self sessionFilePathForDirectory:directory];
  if (filename) {
    BOOL result = [NSKeyedArchiver archiveRootObject:window toFile:filename];
    DCHECK(result);
    if (!result)
      DLOG(ERROR) << "Error writing session file to " << filename;
    // Encrypt the session file (mostly for Incognito, but can't hurt to
    // always do it).
    NSDictionary* attributeDict =
        [NSDictionary dictionaryWithObject:NSFileProtectionComplete
                                    forKey:NSFileProtectionKey];
    NSError* error = nil;
    BOOL success = [[NSFileManager defaultManager] setAttributes:attributeDict
                                                    ofItemAtPath:filename
                                                           error:&error];
    if (!success) {
      DLOG(ERROR) << "Error encrypting session file"
                  << base::SysNSStringToUTF8([error description]);
    }
  }
}

- (void)saveWindow:(SessionWindowIOS*)window
    forBrowserState:(ios::ChromeBrowserState*)browserState
        immediately:(BOOL)immediately {
  NSString* stashPath =
      base::SysUTF8ToNSString(browserState->GetStatePath().value());
  // If there's an existing session window for |stashPath|, clear it before it's
  // replaced.
  SessionWindowIOS* pendingSession = base::mac::ObjCCast<SessionWindowIOS>(
      [_pendingWindows objectForKey:stashPath]);
  [pendingSession clearSessions];
  // Set |window| as the pending save for |stashPath|.
  [_pendingWindows setObject:window forKey:stashPath];
  if (immediately) {
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
    [self performSaveToDirectoryInBackground:stashPath];
  } else if (!pendingSession) {
    // If there wasn't previously a delayed save pending for |stashPath|,
    // enqueue one now.
    [self performSelector:@selector(performSaveToDirectoryInBackground:)
               withObject:stashPath
               afterDelay:kSaveDelay];
  }
}

- (SessionWindowIOS*)loadWindowForBrowserState:
    (ios::ChromeBrowserState*)browserState {
  NSString* stashPath =
      base::SysUTF8ToNSString(browserState->GetStatePath().value());
  SessionWindowIOS* window =
      [self loadWindowFromPath:[self sessionFilePathForDirectory:stashPath]
               forBrowserState:browserState];
  return window;
}

- (SessionWindowIOS*)loadWindowFromPath:(NSString*)path
                        forBrowserState:(ios::ChromeBrowserState*)browserState {
  SessionWindowIOS* window = nil;
  @try {
    NSData* data = [NSData dataWithContentsOfFile:path];
    if (data) {
      base::scoped_nsobject<SessionWindowUnarchiver> unarchiver([
          [SessionWindowUnarchiver alloc] initForReadingWithData:data
                                                    browserState:browserState]);
      window = [[[unarchiver decodeObjectForKey:@"root"] retain] autorelease];
    }
  } @catch (NSException* exception) {
    DLOG(ERROR) << "Error loading session file.";
  }
  return window;
}

// Deletes the file containing the commands for the last session in the given
// browserState directory.
- (void)deleteLastSession:(NSString*)directory {
  NSString* sessionFile = [self sessionFilePathForDirectory:directory];
  _taskRunner->PostTask(
      FROM_HERE, base::BindBlock(^{
        base::ThreadRestrictions::AssertIOAllowed();
        NSFileManager* fileManager = [NSFileManager defaultManager];
        if (![fileManager fileExistsAtPath:sessionFile])
          return;
        if (![fileManager removeItemAtPath:sessionFile error:nil])
          CHECK(false) << "Unable to delete session file.";
      }));
}

@end
