// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements a wrapper for a guestview that manages its
// creation, attaching, and destruction.

var CreateEvent = require('guestViewEvents').CreateEvent;
var EventBindings = require('event_bindings');
var GuestViewInternal = getInternalApi ?
    getInternalApi('guestViewInternal') :
    require('binding').Binding.create('guestViewInternal').generate();
var GuestViewInternalNatives = requireNative('guest_view_internal');

// Events.
var ResizeEvent = CreateEvent('guestViewInternal.onResize');

// Error messages.
var ERROR_MSG_ALREADY_ATTACHED = 'The guest has already been attached.';
var ERROR_MSG_ALREADY_CREATED = 'The guest has already been created.';
var ERROR_MSG_INVALID_STATE = 'The guest is in an invalid state.';
var ERROR_MSG_NOT_ATTACHED = 'The guest is not attached.';
var ERROR_MSG_NOT_CREATED = 'The guest has not been created.';

// Properties.
var PROPERTY_ON_RESIZE = 'onresize';

// Contains and hides the internal implementation details of |GuestView|,
// including maintaining its state and enforcing the proper usage of its API
// fucntions.
function GuestViewImpl(guestView, viewType, guestInstanceId) {
  if (guestInstanceId) {
    this.id = guestInstanceId;
    this.state = GuestViewImpl.GuestState.GUEST_STATE_CREATED;
  } else {
    this.id = 0;
    this.state = GuestViewImpl.GuestState.GUEST_STATE_START;
  }
  this.actionQueue = [];
  this.contentWindow = null;
  this.guestView = guestView;
  this.pendingAction = null;
  this.viewType = viewType;
  this.internalInstanceId = 0;

  this.setupOnResize();
}

// Prevent GuestViewImpl inadvertently inheriting code from the global Object,
// allowing a pathway for executing unintended user code execution.
GuestViewImpl.prototype.__proto__ = null;

// Possible states.
GuestViewImpl.GuestState = {
  GUEST_STATE_START: 0,
  GUEST_STATE_CREATED: 1,
  GUEST_STATE_ATTACHED: 2
};

// Sets up the onResize property on the GuestView.
GuestViewImpl.prototype.setupOnResize = function() {
  $Object.defineProperty(this.guestView, PROPERTY_ON_RESIZE, {
    get: function() {
      return this[PROPERTY_ON_RESIZE];
    }.bind(this),
    set: function(value) {
      this[PROPERTY_ON_RESIZE] = value;
    }.bind(this),
    enumerable: true
  });

  this.callOnResize = function(e) {
    if (!this[PROPERTY_ON_RESIZE]) {
      return;
    }
    this[PROPERTY_ON_RESIZE](e);
  }.bind(this);
};

// Callback wrapper that is used to call the callback of the pending action (if
// one exists), and then performs the next action in the queue.
GuestViewImpl.prototype.handleCallback = function(callback) {
  if (callback) {
    callback();
  }
  this.pendingAction = null;
  this.performNextAction();
};

// Perform the next action in the queue, if one exists.
GuestViewImpl.prototype.performNextAction = function() {
  // Make sure that there is not already an action in progress, and that there
  // exists a queued action to perform.
  if (!this.pendingAction && this.actionQueue.length) {
    this.pendingAction = this.actionQueue.shift();
    this.pendingAction();
  }
};

// Check the current state to see if the proposed action is valid. Returns false
// if invalid.
GuestViewImpl.prototype.checkState = function(action) {
  // Create an error prefix based on the proposed action.
  var errorPrefix = 'Error calling ' + action + ': ';

  // Check that the current state is valid.
  if (!(this.state >= 0 && this.state <= 2)) {
    window.console.error(errorPrefix + ERROR_MSG_INVALID_STATE);
    return false;
  }

  // Map of possible errors for each action. For each action, the errors are
  // listed for states in the order: GUEST_STATE_START, GUEST_STATE_CREATED,
  // GUEST_STATE_ATTACHED.
  var errors = {
    'attach': [ERROR_MSG_NOT_CREATED, null, ERROR_MSG_ALREADY_ATTACHED],
    'create': [null, ERROR_MSG_ALREADY_CREATED, ERROR_MSG_ALREADY_CREATED],
    'destroy': [null, null, null],
    'detach': [ERROR_MSG_NOT_ATTACHED, ERROR_MSG_NOT_ATTACHED, null],
    'setSize': [ERROR_MSG_NOT_CREATED, null, null]
  };

  // Check that the proposed action is a real action.
  if (errors[action] == undefined) {
    window.console.error(errorPrefix + ERROR_MSG_INVALID_ACTION);
    return false;
  }

  // Report the error if the proposed action is found to be invalid for the
  // current state.
  var error;
  if (error = errors[action][this.state]) {
    window.console.error(errorPrefix + error);
    return false;
  }

  return true;
};

// Returns a wrapper function for |func| with a weak reference to |this|. This
// implementation of weakWrapper() requires a provided |viewInstanceId| since
// GuestViewImpl does not store this ID.
GuestViewImpl.prototype.weakWrapper = function(func, viewInstanceId) {
  return function() {
    var view = GuestViewInternalNatives.GetViewFromID(viewInstanceId);
    if (view && view.guest) {
      return $Function.apply(func,
                             privates(view.guest).internal,
                             $Array.slice(arguments));
    }
  };
};

// Internal implementation of attach().
GuestViewImpl.prototype.attachImpl$ = function(
    internalInstanceId, viewInstanceId, attachParams, callback) {
  // Check the current state.
  if (!this.checkState('attach')) {
    this.handleCallback(callback);
    return;
  }

  // Callback wrapper function to store the contentWindow from the attachGuest()
  // callback, handle potential attaching failure, register an automatic detach,
  // and advance the queue.
  var callbackWrapper = function(callback, contentWindow) {
    // Check if attaching failed.
    if (!contentWindow) {
      this.state = GuestViewImpl.GuestState.GUEST_STATE_CREATED;
      this.internalInstanceId = 0;
    } else {
      // Only update the contentWindow if attaching is successful.
      this.contentWindow = contentWindow;
    }

    this.handleCallback(callback);
  };

  attachParams['instanceId'] = viewInstanceId;
  GuestViewInternalNatives.AttachGuest(internalInstanceId,
                                       this.id,
                                       attachParams,
                                       callbackWrapper.bind(this, callback));

  this.internalInstanceId = internalInstanceId;
  this.state = GuestViewImpl.GuestState.GUEST_STATE_ATTACHED;

  // Detach automatically when the container is destroyed.
  GuestViewInternalNatives.RegisterDestructionCallback(
      internalInstanceId, this.weakWrapper(function() {
    if (this.state != GuestViewImpl.GuestState.GUEST_STATE_ATTACHED ||
        this.internalInstanceId != internalInstanceId) {
      return;
    }

    this.internalInstanceId = 0;
    this.state = GuestViewImpl.GuestState.GUEST_STATE_CREATED;
  }, viewInstanceId));
};

// Internal implementation of create().
GuestViewImpl.prototype.createImpl$ = function(createParams, callback) {
  // Check the current state.
  if (!this.checkState('create')) {
    this.handleCallback(callback);
    return;
  }

  // Callback wrapper function to store the guestInstanceId from the
  // createGuest() callback, handle potential creation failure, and advance the
  // queue.
  var callbackWrapper = function(callback, guestInfo) {
    this.id = guestInfo.id;
    this.contentWindow =
        GuestViewInternalNatives.GetContentWindow(guestInfo.contentWindowId);

    // Check if creation failed.
    if (this.id === 0) {
      this.state = GuestViewImpl.GuestState.GUEST_STATE_START;
      this.contentWindow = null;
    }

    ResizeEvent.addListener(this.callOnResize, {instanceId: this.id});
    this.handleCallback(callback);
  };

  this.sendCreateRequest(createParams, callbackWrapper.bind(this, callback));

  this.state = GuestViewImpl.GuestState.GUEST_STATE_CREATED;
};

GuestViewImpl.prototype.sendCreateRequest = function(
    createParams, boundCallback) {
  GuestViewInternal.createGuest(this.viewType, createParams, boundCallback);
};

// Internal implementation of destroy().
GuestViewImpl.prototype.destroyImpl = function(callback) {
  // Check the current state.
  if (!this.checkState('destroy')) {
    this.handleCallback(callback);
    return;
  }

  if (this.state == GuestViewImpl.GuestState.GUEST_STATE_START) {
    // destroy() does nothing in this case.
    this.handleCallback(callback);
    return;
  }

  // If this guest is attached, then detach it first.
  if (!!this.internalInstanceId) {
    GuestViewInternalNatives.DetachGuest(this.internalInstanceId);
  }

  GuestViewInternal.destroyGuest(this.id,
                                 this.handleCallback.bind(this, callback));

  // Reset the state of the destroyed guest;
  this.contentWindow = null;
  this.id = 0;
  this.internalInstanceId = 0;
  this.state = GuestViewImpl.GuestState.GUEST_STATE_START;
  if (ResizeEvent.hasListener(this.callOnResize)) {
    ResizeEvent.removeListener(this.callOnResize);
  }
};

// Internal implementation of detach().
GuestViewImpl.prototype.detachImpl = function(callback) {
  // Check the current state.
  if (!this.checkState('detach')) {
    this.handleCallback(callback);
    return;
  }

  GuestViewInternalNatives.DetachGuest(
      this.internalInstanceId,
      this.handleCallback.bind(this, callback));

  this.internalInstanceId = 0;
  this.state = GuestViewImpl.GuestState.GUEST_STATE_CREATED;
};

// Internal implementation of setSize().
GuestViewImpl.prototype.setSizeImpl = function(sizeParams, callback) {
  // Check the current state.
  if (!this.checkState('setSize')) {
    this.handleCallback(callback);
    return;
  }

  GuestViewInternal.setSize(this.id, sizeParams,
                            this.handleCallback.bind(this, callback));
};

// The exposed interface to a guestview. Exposes in its API the functions
// attach(), create(), destroy(), and getId(). All other implementation details
// are hidden.
function GuestView(viewType, guestInstanceId) {
  privates(this).internal = new GuestViewImpl(this, viewType, guestInstanceId);
}

// Attaches the guestview to the container with ID |internalInstanceId|.
GuestView.prototype.attach = function(
    internalInstanceId, viewInstanceId, attachParams, callback) {
  var internal = privates(this).internal;
  internal.actionQueue.push(internal.attachImpl$.bind(
      internal, internalInstanceId, viewInstanceId, attachParams, callback));
  internal.performNextAction();
};

// Creates the guestview.
GuestView.prototype.create = function(createParams, callback) {
  var internal = privates(this).internal;
  internal.actionQueue.push(internal.createImpl$.bind(
      internal, createParams, callback));
  internal.performNextAction();
};

// Destroys the guestview. Nothing can be done with the guestview after it has
// been destroyed.
GuestView.prototype.destroy = function(callback) {
  var internal = privates(this).internal;
  internal.actionQueue.push(internal.destroyImpl.bind(internal, callback));
  internal.performNextAction();
};

// Detaches the guestview from its container.
// Note: This is not currently used.
GuestView.prototype.detach = function(callback) {
  var internal = privates(this).internal;
  internal.actionQueue.push(internal.detachImpl.bind(internal, callback));
  internal.performNextAction();
};

// Adjusts the guestview's sizing parameters.
GuestView.prototype.setSize = function(sizeParams, callback) {
  var internal = privates(this).internal;
  internal.actionQueue.push(internal.setSizeImpl.bind(
      internal, sizeParams, callback));
  internal.performNextAction();
};

// Returns the contentWindow for this guestview.
GuestView.prototype.getContentWindow = function() {
  var internal = privates(this).internal;
  return internal.contentWindow;
};

// Returns the ID for this guestview.
GuestView.prototype.getId = function() {
  var internal = privates(this).internal;
  return internal.id;
};

// Exports
if (!apiBridge) {
  exports.$set('GuestView', GuestView);
  exports.$set('GuestViewImpl', GuestViewImpl);
  exports.$set('ResizeEvent', ResizeEvent);
}
