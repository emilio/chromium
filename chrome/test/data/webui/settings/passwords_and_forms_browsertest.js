// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Passwords and Forms tests. */

/** @const {string} Path to root from chrome/test/data/webui/settings/. */
var ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);

// Fake data generator.
GEN_INCLUDE([ROOT_PATH +
    'chrome/test/data/webui/settings/passwords_and_autofill_fake_data.js']);

/**
 * @constructor
 * @extends {PolymerTest}
 */
function PasswordsAndFormsBrowserTest() {}

PasswordsAndFormsBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/passwords_and_forms_page/' +
    'passwords_and_forms_page.html',

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
      '../fake_chrome_event.js',
      'fake_settings_private.js',
  ]),

  /** @override */
  setUp: function() {
    PolymerTest.prototype.setUp.call(this);

    // Test is run on an individual element that won't have a page language.
    this.accessibilityAuditConfig.auditRulesToIgnore.push('humanLangMissing');
  },
};

/** This test will validate that the section is loaded with data. */
TEST_F('PasswordsAndFormsBrowserTest', 'uiTests', function() {
  var passwordManager;
  var autofillManager;

  /**
   * Creates a new passwords and forms element.
   * @return {!Object}
   */
  function createPasswordsAndFormsElement(prefsElement) {
    var element = document.createElement('settings-passwords-and-forms-page');
    element.prefs = prefsElement.prefs;
    document.body.appendChild(element);
    element.$$('template[route-path="/passwords"]').if = true;
    element.$$('template[route-path="/autofill"]').if = true;
    Polymer.dom.flush();
    return element;
  }

  /**
   * @pram {boolean} autofill Whether autofill is enabled or not.
   * @param {boolean} passwords Whether passwords are enabled or not.
   * @return {!Promise<!Element>} The |prefs| element.
   */
  function createPrefs(autofill, passwords) {
    return new Promise(function(resolve) {
      CrSettingsPrefs.deferInitialization = true;
      var prefs = document.createElement('settings-prefs');
      prefs.initialize(new settings.FakeSettingsPrivate([
            {
              key: 'autofill.enabled',
              type: chrome.settingsPrivate.PrefType.BOOLEAN,
              value: autofill,
            },
            {
              key: 'credentials_enable_service',
              type: chrome.settingsPrivate.PrefType.BOOLEAN,
              value: passwords,
            },
            {
              key: 'credentials_enable_autosignin',
              type: chrome.settingsPrivate.PrefType.BOOLEAN,
              value: true,
            },
      ]));

      CrSettingsPrefs.initialized.then(function() {
        resolve(prefs);
      });
    });
  }

  /**
   * Cleans up prefs so tests can continue to run.
   * @param {!Element} prefs The prefs element.
   */
  function destroyPrefs(prefs) {
    CrSettingsPrefs.resetForTesting();
    CrSettingsPrefs.deferInitialization = false;
    prefs.resetForTesting();
  }

  /**
   * Creates PasswordManagerExpectations with the values expected after first
   * creating the element.
   * @return {!PasswordManagerExpectations}
   */
  function basePasswordExpectations() {
    var expected = new PasswordManagerExpectations();
    expected.requested.passwords = 1;
    expected.requested.exceptions = 1;
    expected.listening.passwords = 1;
    expected.listening.exceptions = 1;
    return expected;
  }

  /**
   * Creates AutofillManagerExpectations with the values expected after first
   * creating the element.
   * @return {!AutofillManagerExpectations}
   */
  function baseAutofillExpectations() {
    var expected = new AutofillManagerExpectations();
    expected.requested.addresses = 1;
    expected.requested.creditCards = 1;
    expected.listening.addresses = 1;
    expected.listening.creditCards = 1;
    return expected;
  }

  setup(function() {
    PolymerTest.clearBody();

    // Override the PasswordManagerImpl for testing.
    passwordManager = new TestPasswordManager();
    PasswordManagerImpl.instance_ = passwordManager;

    // Override the AutofillManagerImpl for testing.
    autofillManager = new TestAutofillManager();
    AutofillManagerImpl.instance_ = autofillManager;
  });

  suite('PasswordsAndForms', function() {
    test('baseLoadAndRemove', function() {
      return createPrefs(true, true).then(function(prefs) {
        var element = createPasswordsAndFormsElement(prefs);

        var passwordsExpectations = basePasswordExpectations();
        passwordManager.assertExpectations(passwordsExpectations);

        var autofillExpectations = baseAutofillExpectations();
        autofillManager.assertExpectations(autofillExpectations);

        element.remove();
        Polymer.dom.flush();

        passwordsExpectations.listening.passwords = 0;
        passwordsExpectations.listening.exceptions = 0;
        passwordManager.assertExpectations(passwordsExpectations);

        autofillExpectations.listening.addresses = 0;
        autofillExpectations.listening.creditCards = 0;
        autofillManager.assertExpectations(autofillExpectations);

        destroyPrefs(prefs);
      });
    });

    test('loadPasswordsAsync', function() {
      return createPrefs(true, true).then(function(prefs) {
        var element = createPasswordsAndFormsElement(prefs);

        var list = [
          FakeDataMaker.passwordEntry(),
          FakeDataMaker.passwordEntry()
        ];

        passwordManager.lastCallback.addSavedPasswordListChangedListener(list);
        Polymer.dom.flush();

        assertEquals(list, element.$$('#passwordSection').savedPasswords);

        // The callback is coming from the manager, so the element shouldn't
        // have additional calls to the manager after the base expectations.
        passwordManager.assertExpectations(basePasswordExpectations());
        autofillManager.assertExpectations(baseAutofillExpectations());

        destroyPrefs(prefs);
      });
    });

    test('loadExceptionsAsync', function() {
      return createPrefs(true, true).then(function(prefs) {
        var element = createPasswordsAndFormsElement(prefs);

        var list = [FakeDataMaker.exceptionEntry(),
        FakeDataMaker.exceptionEntry()];
        passwordManager.lastCallback.addExceptionListChangedListener(list);
        Polymer.dom.flush();

        assertEquals(list, element.$$('#passwordSection').passwordExceptions);

        // The callback is coming from the manager, so the element shouldn't
        // have additional calls to the manager after the base expectations.
        passwordManager.assertExpectations(basePasswordExpectations());
        autofillManager.assertExpectations(baseAutofillExpectations());

        destroyPrefs(prefs);
      });
    });

    test('loadAddressesAsync', function() {
      return createPrefs(true, true).then(function(prefs) {
        var element = createPasswordsAndFormsElement(prefs);

        var list = [FakeDataMaker.addressEntry(), FakeDataMaker.addressEntry()];
        autofillManager.lastCallback.addAddressListChangedListener(list);
        Polymer.dom.flush();

        assertEquals(list, element.$$('#autofillSection').addresses);

        // The callback is coming from the manager, so the element shouldn't
        // have additional calls to the manager after the base expectations.
        passwordManager.assertExpectations(basePasswordExpectations());
        autofillManager.assertExpectations(baseAutofillExpectations());

        destroyPrefs(prefs);
      });
    });

    test('loadCreditCardsAsync', function() {
      return createPrefs(true, true).then(function(prefs) {
        var element = createPasswordsAndFormsElement(prefs);

        var list = [FakeDataMaker.creditCardEntry(),
        FakeDataMaker.creditCardEntry()];
        autofillManager.lastCallback.addCreditCardListChangedListener(list);
        Polymer.dom.flush();

        assertEquals(list, element.$$('#autofillSection').creditCards);

        // The callback is coming from the manager, so the element shouldn't
        // have additional calls to the manager after the base expectations.
        passwordManager.assertExpectations(basePasswordExpectations());
        autofillManager.assertExpectations(baseAutofillExpectations());

        destroyPrefs(prefs);
      });
    });

    test('testActionabilityNope', function() {
      return createPrefs(false, false).then(function(prefs) {

        var element = createPasswordsAndFormsElement(prefs);

        assertFalse(element.$.autofillManagerButton.hasAttribute('actionable'));
        assertFalse(element.$.passwordManagerButton.hasAttribute('actionable'));

        destroyPrefs(prefs);
      });
    });

    test('testActionabilityYes', function() {
      return createPrefs(true, true).then(function(prefs) {
        var element = createPasswordsAndFormsElement(prefs);

        assertTrue(element.$.autofillManagerButton.hasAttribute('actionable'));
        assertTrue(element.$.passwordManagerButton.hasAttribute('actionable'));

        destroyPrefs(prefs);
      });
    });

    test('testAutofillExtensionIndicator', function() {
      return createPrefs(true, true).then(function(prefs) {
        var element = createPasswordsAndFormsElement(prefs);

        assertFalse(!!element.$$('#autofillExtensionIndicator'));
        element.set('prefs.autofill.enabled.extensionId', 'test-id');
        Polymer.dom.flush();

        assertTrue(!!element.$$('#autofillExtensionIndicator'));
        destroyPrefs(prefs);
      });
    });

    test('testPasswordsExtensionIndicator', function() {
      return createPrefs(true, true).then(function(prefs) {
        var element = createPasswordsAndFormsElement(prefs);

        assertFalse(!!element.$$('#passwordsExtensionIndicator'));
        element.set('prefs.credentials_enable_service.extensionId', 'test-id');
        Polymer.dom.flush();

        assertTrue(!!element.$$('#passwordsExtensionIndicator'));
        destroyPrefs(prefs);
      });
    });
  });

  mocha.run();
});
