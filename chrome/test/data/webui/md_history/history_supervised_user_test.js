// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('history-list supervised-user', function() {
  var app;
  var historyList;
  var toolbar;
  var TEST_HISTORY_RESULTS;

  suiteSetup(function() {
    TEST_HISTORY_RESULTS =
        [createHistoryEntry('2016-03-15', 'https://www.google.com')];
  });

  setup(function() {
    app = replaceApp();
    historyList = app.$.history;
    toolbar = app.$.toolbar;
    app.historyResult(createHistoryInfo(), TEST_HISTORY_RESULTS);
  });

  test('checkboxes disabled for supervised user', function() {
    return PolymerTest.flushTasks().then(function() {
      var items =
          Polymer.dom(historyList.root).querySelectorAll('history-item');

      MockInteractions.tap(items[0].$['checkbox']);

      assertFalse(items[0].selected);
    });
  });

  test('deletion disabled for supervised user', function() {
    // Make sure that removeVisits is not being called.
    registerMessageCallback('removeVisits', this, function (toBeRemoved) {
      assertNotReached();
    });

    historyList.historyData_[0].selected = true;
    toolbar.onDeleteTap_();
  });

  test('remove history menu button disabled', function() {
    var listContainer = app.$['history'];
    listContainer.$.sharedMenu.get();
    assertTrue(listContainer.$$('#menuRemoveButton').hidden);
  });
});
