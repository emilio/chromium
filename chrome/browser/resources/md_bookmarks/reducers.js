// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Module of functions which produce a new page state in response
 * to an action. Reducers (in the same sense as Array.prototype.reduce) must be
 * pure functions: they must not modify existing state objects, or make any API
 * calls.
 */

cr.define('bookmarks', function() {
  var NodeState = {};

  /**
   * @param {NodeList} nodes
   * @param {string} id
   * @param {function(BookmarkNode):BookmarkNode} callback
   * @return {NodeList}
   */
  NodeState.modifyNode_ = function(nodes, id, callback) {
    var nodeModification = {};
    nodeModification[id] = callback(nodes[id]);
    return Object.assign({}, nodes, nodeModification);
  };

  /**
   * @param {NodeList} nodes
   * @param {Action} action
   * @return {NodeList}
   */
  NodeState.editBookmark = function(nodes, action) {
    // Do not allow folders to change URL (making them no longer folders).
    if (!nodes[action.id].url && action.changeInfo.url)
      delete action.changeInfo.url;

    return NodeState.modifyNode_(nodes, action.id, function(node) {
      return /** @type {BookmarkNode} */ (
          Object.assign({}, node, action.changeInfo));
    });
  };

  /**
   * @param {NodeList} nodes
   * @param {Action} action
   * @return {NodeList}
   */
  NodeState.removeBookmark = function(nodes, action) {
    return NodeState.modifyNode_(nodes, action.parentId, function(node) {
      var newChildren = node.children.slice();
      newChildren.splice(action.index, 1);
      return /** @type {BookmarkNode} */ (
          Object.assign({}, node, {children: newChildren}));
    });
  };

  /**
   * @param {NodeList} nodes
   * @param {Action} action
   * @return {NodeList}
   */
  NodeState.updateNodes = function(nodes, action) {
    switch (action.name) {
      case 'edit-bookmark':
        return NodeState.editBookmark(nodes, action);
      case 'remove-bookmark':
        return NodeState.removeBookmark(nodes, action);
      case 'refresh-nodes':
        return action.nodes;
      default:
        return nodes;
    }
  };

  var SelectedFolderState = {};

  /**
   * @param {NodeList} nodes
   * @param {string} ancestorId
   * @param {string} childId
   * @return {boolean}
   */
  SelectedFolderState.isAncestorOf = function(nodes, ancestorId, childId) {
    var currentId = childId;
    // Work upwards through the tree from child.
    while (currentId) {
      if (currentId == ancestorId)
        return true;
      currentId = nodes[currentId].parentId;
    }
    return false;
  };

  /**
   * @param {?string} selectedFolder
   * @param {Action} action
   * @param {NodeList} nodes
   * @return {?string}
   */
  SelectedFolderState.updateSelectedFolder = function(
      selectedFolder, action, nodes) {
    // TODO(tsergeant): It should not be possible to select a non-folder.
    switch (action.name) {
      case 'select-folder':
        return action.id;
      case 'change-folder-open':
        // When hiding the selected folder by closing its ancestor, select
        // that ancestor instead.
        if (!action.open && selectedFolder &&
            SelectedFolderState.isAncestorOf(
                nodes, action.id, selectedFolder)) {
          return action.id;
        }
        return selectedFolder;
      default:
        return selectedFolder;
    }
  };

  var ClosedFolderState = {};

  /**
   * @param {ClosedFolderState} state
   * @param {Action} action
   * @param {NodeList} nodes
   * @return {ClosedFolderState}
   */
  ClosedFolderState.openAncestorsOf = function(state, action, nodes) {
    var id = action.id;
    var modifications = {};
    var parentId = nodes[id].parentId;
    while (parentId) {
      if (state[parentId]) {
        modifications[parentId] = false;
      }
      parentId = nodes[parentId].parentId;
    }

    return Object.assign({}, state, modifications);
  };

  /**
   * @param {ClosedFolderState} state
   * @param {Action} action
   * @return {ClosedFolderState}
   */
  ClosedFolderState.changeFolderOpen = function(state, action) {
    var closed = !action.open;
    var modification = {};
    modification[action.id] = closed;

    return Object.assign({}, state, modification);
  };

  /**
   * @param {ClosedFolderState} state
   * @param {Action} action
   * @param {NodeList} nodes
   * @return {ClosedFolderState}
   */
  ClosedFolderState.updateClosedFolders = function(state, action, nodes) {
    switch (action.name) {
      case 'change-folder-open':
        return ClosedFolderState.changeFolderOpen(state, action);
      case 'select-folder':
        return ClosedFolderState.openAncestorsOf(state, action, nodes);
      default:
        return state;
    };
  };

  /**
   * Root reducer for the Bookmarks page. This is called by the store in
   * response to an action, and the return value is used to update the UI.
   * @param {!BookmarksPageState} state
   * @param {Action} action
   * @return {!BookmarksPageState}
   */
  function reduceAction(state, action) {
    return {
      nodes: NodeState.updateNodes(state.nodes, action),
      selectedFolder: SelectedFolderState.updateSelectedFolder(
          state.selectedFolder, action, state.nodes),
      closedFolders: ClosedFolderState.updateClosedFolders(
          state.closedFolders, action, state.nodes),
    };
  }

  return {
    reduceAction: reduceAction,
    ClosedFolderState: ClosedFolderState,
    NodeState: NodeState,
    SelectedFolderState: SelectedFolderState,
  };
});
