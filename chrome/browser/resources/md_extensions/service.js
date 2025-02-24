// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  /**
   * @constructor
   * @implements {extensions.ItemDelegate}
   * @implements {extensions.ToolbarDelegate}
   * @implements {extensions.PackDialogDelegate}
   * @implements {extensions.ErrorPageDelegate}
   */
  function Service() {}

  Service.prototype = {
    /** @private {boolean} */
    isDeleting_: false,

    /** @param {extensions.Manager} manager */
    managerReady: function(manager) {
      /** @private {extensions.Manager} */
      this.manager_ = manager;
      this.manager_.toolbar.setDelegate(this);
      this.manager_.set('itemDelegate', this);
      this.manager_.packDialog.set('delegate', this);
      this.manager_.errorPage.delegate = this;
      var keyboardShortcuts = this.manager_.keyboardShortcuts;
      keyboardShortcuts.addEventListener(
          'shortcut-updated',
          this.onExtensionCommandUpdated_.bind(this));
      keyboardShortcuts.addEventListener(
          'shortcut-capture-started',
          this.onShortcutCaptureChanged_.bind(this, true));
      keyboardShortcuts.addEventListener(
          'shortcut-capture-ended',
          this.onShortcutCaptureChanged_.bind(this, false));
      chrome.developerPrivate.onProfileStateChanged.addListener(
          this.onProfileStateChanged_.bind(this));
      chrome.developerPrivate.onItemStateChanged.addListener(
          this.onItemStateChanged_.bind(this));
      chrome.developerPrivate.getExtensionsInfo(
          {includeDisabled: true, includeTerminated: true},
          function(extensions) {
        /** @private {Array<chrome.developerPrivate.ExtensionInfo>} */
        this.extensions_ = extensions;
        for (let extension of extensions)
          this.manager_.addItem(extension);

        var id = new URLSearchParams(location.search).get('id');
        if (id) {
          var data = this.extensions_.find(function(e) {
            return e.id == id;
          });
          if (data)
            this.manager_.showItemDetails(data);
        }
      }.bind(this));
      chrome.developerPrivate.getProfileConfiguration(
          this.onProfileStateChanged_.bind(this));
    },

    /**
     * @param {chrome.developerPrivate.ProfileInfo} profileInfo
     * @private
     */
    onProfileStateChanged_: function(profileInfo) {
      this.manager_.set('inDevMode', profileInfo.inDeveloperMode);
    },

    /**
     * @param {chrome.developerPrivate.EventData} eventData
     * @private
     */
    onItemStateChanged_: function(eventData) {
      var currentIndex = this.extensions_.findIndex(function(extension) {
        return extension.id == eventData.item_id;
      });

      var EventType = chrome.developerPrivate.EventType;
      switch (eventData.event_type) {
        case EventType.VIEW_REGISTERED:
        case EventType.VIEW_UNREGISTERED:
        case EventType.INSTALLED:
        case EventType.LOADED:
        case EventType.UNLOADED:
        case EventType.ERROR_ADDED:
        case EventType.ERRORS_REMOVED:
        case EventType.PREFS_CHANGED:
          // |extensionInfo| can be undefined in the case of an extension
          // being unloaded right before uninstallation. There's nothing to do
          // here.
          if (!eventData.extensionInfo)
            break;

          if (currentIndex >= 0) {
            this.extensions_[currentIndex] = eventData.extensionInfo;
            this.manager_.updateItem(eventData.extensionInfo);
          } else {
            this.extensions_.push(eventData.extensionInfo);
            this.manager_.addItem(eventData.extensionInfo);
          }
          break;
        case EventType.UNINSTALLED:
          this.manager_.removeItem(this.extensions_[currentIndex]);
          this.extensions_.splice(currentIndex, 1);
          break;
        default:
          assertNotReached();
      }
    },

    /**
     * Opens a file browser dialog for the user to select a file (or directory).
     * @param {chrome.developerPrivate.SelectType} selectType
     * @param {chrome.developerPrivate.FileType} fileType
     * @return {Promise<string>} The promise to be resolved with the selected
     *     path.
     */
    chooseFilePath_: function(selectType, fileType) {
      return new Promise(function(resolve, reject) {
        chrome.developerPrivate.choosePath(
            selectType, fileType, function(path) {
          if (chrome.runtime.lastError &&
              chrome.runtime.lastError != 'File selection was canceled.') {
            reject(chrome.runtime.lastError);
          } else {
            resolve(path || '');
          }
        });
      });
    },

    /**
     * Updates an extension command.
     * @param {!CustomEvent} e
     * @private
     */
    onExtensionCommandUpdated_: function(e) {
      chrome.developerPrivate.updateExtensionCommand({
        extensionId: e.detail.item,
        commandName: e.detail.commandName,
        keybinding: e.detail.keybinding,
      });
    },

    /**
     * Called when shortcut capturing changes in order to suspend or re-enable
     * global shortcut handling. This is important so that the shortcuts aren't
     * processed normally as the user types them.
     * TODO(devlin): From very brief experimentation, it looks like preventing
     * the default handling on the event also does this. Investigate more in the
     * future.
     * @param {boolean} isCapturing
     * @param {!CustomEvent} e
     * @private
     */
    onShortcutCaptureChanged_: function(isCapturing, e) {
      chrome.developerPrivate.setShortcutHandlingSuspended(isCapturing);
    },

    /** @override */
    deleteItem: function(id) {
      if (this.isDeleting_)
        return;
      this.isDeleting_ = true;
      chrome.management.uninstall(id, {showConfirmDialog: true}, function() {
        // The "last error" was almost certainly the user canceling the dialog.
        // Do nothing. We only check it so we don't get noisy logs.
        /** @suppress {suspiciousCode} */
        chrome.runtime.lastError;
        this.isDeleting_ = false;
      }.bind(this));
    },

    /** @override */
    setItemEnabled: function(id, isEnabled) {
      chrome.management.setEnabled(id, isEnabled);
    },

    /** @override */
    setItemAllowedIncognito: function(id, isAllowedIncognito) {
      chrome.developerPrivate.updateExtensionConfiguration({
        extensionId: id,
        incognitoAccess: isAllowedIncognito,
      });
    },

    /** @override */
    setItemAllowedOnFileUrls: function(id, isAllowedOnFileUrls) {
      chrome.developerPrivate.updateExtensionConfiguration({
        extensionId: id,
        fileAccess: isAllowedOnFileUrls,
      });
    },

    /** @override */
    setItemAllowedOnAllSites: function(id, isAllowedOnAllSites) {
      chrome.developerPrivate.updateExtensionConfiguration({
        extensionId: id,
        runOnAllUrls: isAllowedOnAllSites,
      });
    },

    /** @override */
    setItemCollectsErrors: function(id, collectsErrors) {
      chrome.developerPrivate.updateExtensionConfiguration({
        extensionId: id,
        errorCollection: collectsErrors,
      });
    },

    /** @override */
    inspectItemView: function(id, view) {
      chrome.developerPrivate.openDevTools({
        extensionId: id,
        renderProcessId: view.renderProcessId,
        renderViewId: view.renderViewId,
        incognito: view.incognito,
      });
    },

    /** @override */
    reloadItem: function(id) {
      chrome.developerPrivate.reload(id, {failQuietly: false});
    },

    /** @override */
    repairItem: function(id) {
      chrome.developerPrivate.repairExtension(id);
    },

    /** @override */
    showItemOptionsPage: function(id) {
      var extension = this.extensions_.find(function(extension) {
        return extension.id == id;
      });
      assert(extension && extension.optionsPage);
      if (extension.optionsPage.openInTab)
        chrome.developerPrivate.showOptions(id);
      else
        this.manager_.optionsDialog.show(extension);
    },

    /** @override */
    setProfileInDevMode: function(inDevMode) {
      chrome.developerPrivate.updateProfileConfiguration(
          {inDeveloperMode: inDevMode});
    },

    /** @override */
    loadUnpacked: function() {
      chrome.developerPrivate.loadUnpacked({failQuietly: true});
    },

    /** @override */
    choosePackRootDirectory: function() {
      return this.chooseFilePath_(
          chrome.developerPrivate.SelectType.FOLDER,
          chrome.developerPrivate.FileType.LOAD);
    },

    /** @override */
    choosePrivateKeyPath: function() {
      return this.chooseFilePath_(
          chrome.developerPrivate.SelectType.FILE,
          chrome.developerPrivate.FileType.PEM);
    },

    /** @override */
    packExtension: function(rootPath, keyPath) {
      chrome.developerPrivate.packDirectory(rootPath, keyPath);
    },

    /** @override */
    updateAllExtensions: function() {
      chrome.developerPrivate.autoUpdate();
    },

    /** @override */
    deleteErrors: function(extensionId, errorIds, type) {
      chrome.developerPrivate.deleteExtensionErrors({
        extensionId: extensionId,
        errorIds: errorIds,
        type: type,
      });
    },

    /** @override */
    requestFileSource: function(args) {
      return new Promise(function(resolve, reject) {
        chrome.developerPrivate.requestFileSource(args, function(code) {
          resolve(code);
        });
      });
    },
  };

  cr.addSingletonGetter(Service);

  return {Service: Service};
});
