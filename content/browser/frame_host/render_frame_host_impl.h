// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_IMPL_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/bad_message.h"
#include "content/browser/loader/global_routing_id.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/common/accessibility_mode_enums.h"
#include "content/common/ax_content_node_data.h"
#include "content/common/content_export.h"
#include "content/common/content_security_policy/content_security_policy.h"
#include "content/common/download/mhtml_save_status.h"
#include "content/common/frame.mojom.h"
#include "content/common/frame_message_enums.h"
#include "content/common/frame_replication_state.h"
#include "content/common/image_downloader/image_downloader.mojom.h"
#include "content/common/navigation_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/javascript_dialog_type.h"
#include "content/public/common/previews_state.h"
#include "media/mojo/interfaces/interface_factory.mojom.h"
#include "net/http/http_response_headers.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "third_party/WebKit/public/platform/WebFocusType.h"
#include "third_party/WebKit/public/platform/WebInsecureRequestPolicy.h"
#include "third_party/WebKit/public/web/WebTextDirection.h"
#include "third_party/WebKit/public/web/WebTreeScopeType.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/mojo/window_open_disposition.mojom.h"
#include "ui/base/page_transition_types.h"

class GURL;
struct AccessibilityHostMsg_EventParams;
struct AccessibilityHostMsg_FindInPageResultParams;
struct AccessibilityHostMsg_LocationChangeParams;
struct FrameHostMsg_DidCommitProvisionalLoad_Params;
struct FrameHostMsg_DidFailProvisionalLoadWithError_Params;
struct FrameHostMsg_OpenURL_Params;
struct FrameMsg_TextTrackSettings_Params;
#if defined(USE_EXTERNAL_POPUP_MENU)
struct FrameHostMsg_ShowPopup_Params;
#endif

namespace base {
class ListValue;
}

namespace blink {
namespace mojom {
class WebBluetoothService;
}
}

namespace gfx {
class Range;
}

namespace content {
class AssociatedInterfaceProviderImpl;
class FeaturePolicy;
class FrameTree;
class FrameTreeNode;
class MediaInterfaceProxy;
class NavigationHandleImpl;
class PermissionServiceContext;
class RenderFrameHostDelegate;
class RenderFrameProxyHost;
class RenderProcessHost;
class RenderViewHostImpl;
class RenderWidgetHostDelegate;
class RenderWidgetHostImpl;
class RenderWidgetHostView;
class RenderWidgetHostViewBase;
class ResourceRequestBody;
class StreamHandle;
class TimeoutMonitor;
class WebBluetoothServiceImpl;
struct ContentSecurityPolicyHeader;
struct ContextMenuParams;
struct FileChooserParams;
struct FrameOwnerProperties;
struct FileChooserParams;
struct ResourceResponse;

namespace mojom {
class CreateNewWindowParams;
}

class CONTENT_EXPORT RenderFrameHostImpl
    : public RenderFrameHost,
      NON_EXPORTED_BASE(public mojom::FrameHost),
      public BrowserAccessibilityDelegate,
      public SiteInstanceImpl::Observer,
      public NON_EXPORTED_BASE(
          service_manager::InterfaceFactory<media::mojom::InterfaceFactory>) {
 public:
  using AXTreeSnapshotCallback =
      base::Callback<void(
          const ui::AXTreeUpdate&)>;
  using SmartClipCallback = base::Callback<void(const base::string16& text,
                                                const base::string16& html)>;

  // An accessibility reset is only allowed to prevent very rare corner cases
  // or race conditions where the browser and renderer get out of sync. If
  // this happens more than this many times, kill the renderer.
  static const int kMaxAccessibilityResets = 5;

  static RenderFrameHostImpl* FromID(int process_id, int routing_id);
  static RenderFrameHostImpl* FromAXTreeID(
      ui::AXTreeIDRegistry::AXTreeID ax_tree_id);

  ~RenderFrameHostImpl() override;

  // RenderFrameHost
  int GetRoutingID() override;
  ui::AXTreeIDRegistry::AXTreeID GetAXTreeID() override;
  SiteInstanceImpl* GetSiteInstance() override;
  RenderProcessHost* GetProcess() override;
  RenderWidgetHostView* GetView() override;
  RenderFrameHostImpl* GetParent() override;
  int GetFrameTreeNodeId() override;
  const std::string& GetFrameName() override;
  bool IsCrossProcessSubframe() override;
  const GURL& GetLastCommittedURL() override;
  const url::Origin& GetLastCommittedOrigin() override;
  gfx::NativeView GetNativeView() override;
  void AddMessageToConsole(ConsoleMessageLevel level,
                           const std::string& message) override;
  void ExecuteJavaScript(const base::string16& javascript) override;
  void ExecuteJavaScript(const base::string16& javascript,
                         const JavaScriptResultCallback& callback) override;
  void ExecuteJavaScriptInIsolatedWorld(
      const base::string16& javascript,
      const JavaScriptResultCallback& callback,
      int world_id) override;
  void ExecuteJavaScriptForTests(const base::string16& javascript) override;
  void ExecuteJavaScriptForTests(
      const base::string16& javascript,
      const JavaScriptResultCallback& callback) override;
  void ExecuteJavaScriptWithUserGestureForTests(
      const base::string16& javascript) override;
  void ActivateFindInPageResultForAccessibility(int request_id) override;
  void InsertVisualStateCallback(const VisualStateCallback& callback) override;
  void CopyImageAt(int x, int y) override;
  void SaveImageAt(int x, int y) override;
  RenderViewHost* GetRenderViewHost() override;
  service_manager::InterfaceRegistry* GetInterfaceRegistry() override;
  service_manager::InterfaceProvider* GetRemoteInterfaces() override;
  AssociatedInterfaceProvider* GetRemoteAssociatedInterfaces() override;
  blink::WebPageVisibilityState GetVisibilityState() override;
  bool IsRenderFrameLive() override;
  int GetProxyCount() override;
  void FilesSelectedInChooser(const std::vector<FileChooserFileInfo>& files,
                              FileChooserParams::Mode permissions) override;
  bool HasSelection() override;
  void RequestTextSurroundingSelection(
      const TextSurroundingSelectionCallback& callback,
      int max_length) override;
  void AllowBindings(int binding_flags) override;
  int GetEnabledBindings() const override;

  // mojom::FrameHost
  void GetInterfaceProvider(
      service_manager::mojom::InterfaceProviderRequest interfaces) override;

  // IPC::Sender
  bool Send(IPC::Message* msg) override;

  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override;

  // BrowserAccessibilityDelegate
  void AccessibilityPerformAction(const ui::AXActionData& data) override;
  bool AccessibilityViewHasFocus() const override;
  gfx::Rect AccessibilityGetViewBounds() const override;
  gfx::Point AccessibilityOriginInScreen(
      const gfx::Rect& bounds) const override;
  void AccessibilityFatalError() override;
  gfx::AcceleratedWidget AccessibilityGetAcceleratedWidget() override;
  gfx::NativeViewAccessible AccessibilityGetNativeViewAccessible() override;

  // SiteInstanceImpl::Observer
  void RenderProcessGone(SiteInstanceImpl* site_instance) override;

  // Creates a RenderFrame in the renderer process.
  bool CreateRenderFrame(int proxy_routing_id,
                         int opener_routing_id,
                         int parent_routing_id,
                         int previous_sibling_routing_id);

  // Tracks whether the RenderFrame for this RenderFrameHost has been created in
  // the renderer process.  This is currently only used for subframes.
  // TODO(creis): Use this for main frames as well when RVH goes away.
  void SetRenderFrameCreated(bool created);

  // Called for renderer-created windows to resume requests from this frame,
  // after they are blocked in RenderWidgetHelper::CreateNewWindow.
  void Init();

  int routing_id() const { return routing_id_; }

  // Called when this frame has added a child. This is a continuation of an IPC
  // that was partially handled on the IO thread (to allocate |new_routing_id|),
  // and is forwarded here. The renderer has already been told to create a
  // RenderFrame with |new_routing_id|.
  void OnCreateChildFrame(int new_routing_id,
                          blink::WebTreeScopeType scope,
                          const std::string& frame_name,
                          const std::string& frame_unique_name,
                          blink::WebSandboxFlags sandbox_flags,
                          const FrameOwnerProperties& frame_owner_properties);

  // Called when this frame tries to open a new WebContents, e.g. via a script
  // call to window.open(). The renderer has already been told to create the
  // RenderView and RenderFrame with the specified route ids, which were
  // assigned on the IO thread.
  void OnCreateNewWindow(int32_t render_view_route_id,
                         int32_t main_frame_route_id,
                         int32_t main_frame_widget_route_id,
                         const mojom::CreateNewWindowParams& params,
                         SessionStorageNamespace* session_storage_namespace);

  RenderViewHostImpl* render_view_host() { return render_view_host_; }
  RenderFrameHostDelegate* delegate() { return delegate_; }
  FrameTreeNode* frame_tree_node() { return frame_tree_node_; }

  const GURL& last_committed_url() const { return last_committed_url_; }

  // Allows FrameTreeNode::SetCurrentURL to update this frame's last committed
  // URL.  Do not call this directly, since we rely on SetCurrentURL to track
  // whether a real load has committed or not.
  void set_last_committed_url(const GURL& url) {
    last_committed_url_ = url;
  }

  // The most recent non-net-error URL to commit in this frame.  In almost all
  // cases, use GetLastCommittedURL instead.
  const GURL& last_successful_url() { return last_successful_url_; }
  void set_last_successful_url(const GURL& url) {
    last_successful_url_ = url;
  }

  // Update this frame's last committed origin.
  void set_last_committed_origin(const url::Origin& origin) {
    last_committed_origin_ = origin;
  }

  // Returns the associated WebUI or null if none applies.
  WebUIImpl* web_ui() const { return web_ui_.get(); }

  // Returns the pending WebUI, or null if none applies.
  WebUIImpl* pending_web_ui() const {
    return should_reuse_web_ui_ ? web_ui_.get() : pending_web_ui_.get();
  }

  // Returns this RenderFrameHost's loading state. This method is only used by
  // FrameTreeNode. The proper way to check whether a frame is loading is to
  // call FrameTreeNode::IsLoading.
  bool is_loading() const { return is_loading_; }

  // Sets this RenderFrameHost loading state. This is only used in the case of
  // transfer navigations, where no DidStart/DidStopLoading notifications
  // should be sent during the transfer.
  // TODO(clamy): Remove this once PlzNavigate ships.
  void set_is_loading(bool is_loading) { is_loading_ = is_loading; }

  // Returns true if this is a top-level frame, or if this frame's RenderFrame
  // is in a different process from its parent frame. Local roots are
  // distinguished by owning a RenderWidgetHost, which manages input events
  // and painting for this frame and its contiguous local subtree in the
  // renderer process.
  bool is_local_root() const { return !!render_widget_host_; }

  // Returns the RenderWidgetHostImpl attached to this frame or the nearest
  // ancestor frame, which could potentially be the root. For most input
  // and rendering related purposes, GetView() should be preferred and
  // RenderWidgetHostViewBase methods used. GetRenderWidgetHost() will not
  // return a nullptr, whereas GetView() potentially will (for instance,
  // after a renderer crash).
  //
  // This method crashes if this RenderFrameHostImpl does not own a
  // a RenderWidgetHost and nor does any of its ancestors. That would
  // typically mean that the frame has been detached from the frame tree.
  RenderWidgetHostImpl* GetRenderWidgetHost();

  GlobalFrameRoutingId GetGlobalFrameRoutingId();

  // The unique ID of the latest NavigationEntry that this RenderFrameHost is
  // showing. This may change even when this frame hasn't committed a page,
  // such as for a new subframe navigation in a different frame.
  int nav_entry_id() const { return nav_entry_id_; }
  void set_nav_entry_id(int nav_entry_id) { nav_entry_id_ = nav_entry_id; }

  // A NavigationHandle for the pending navigation in this frame, if any. This
  // is cleared when the navigation commits.
  NavigationHandleImpl* navigation_handle() const {
    return navigation_handle_.get();
  }

  // Called when a new navigation starts in this RenderFrameHost. Ownership of
  // |navigation_handle| is transferred.
  // PlzNavigate: called when a navigation is ready to commit in this
  // RenderFrameHost.
  void SetNavigationHandle(
      std::unique_ptr<NavigationHandleImpl> navigation_handle);

  // Gives the ownership of |navigation_handle_| to the caller.
  // This happens during transfer navigations, where it should be transferred
  // from the RenderFrameHost that issued the initial request to the new
  // RenderFrameHost that will issue the transferring request.
  std::unique_ptr<NavigationHandleImpl> PassNavigationHandleOwnership();

  // Tells the renderer that this RenderFrame is being swapped out for one in a
  // different renderer process.  It should run its unload handler and move to
  // a blank document.  If |proxy| is not null, it should also create a
  // RenderFrameProxy to replace the RenderFrame and set it to |is_loading|
  // state. The renderer should preserve the RenderFrameProxy object until it
  // exits, in case we come back.  The renderer can exit if it has no other
  // active RenderFrames, but not until WasSwappedOut is called.
  void SwapOut(RenderFrameProxyHost* proxy, bool is_loading);

  // Whether an ongoing navigation is waiting for a BeforeUnload ACK from the
  // RenderFrame. Currently this only happens in cross-site navigations.
  // PlzNavigate: this happens in every browser-initiated navigation that is not
  // same-page.
  bool is_waiting_for_beforeunload_ack() const {
    return is_waiting_for_beforeunload_ack_;
  }

  // Whether the RFH is waiting for an unload ACK from the renderer.
  bool IsWaitingForUnloadACK() const;

  // Called when either the SwapOut request has been acknowledged or has timed
  // out.
  void OnSwappedOut();

  // This method returns true from the time this RenderFrameHost is created
  // until SwapOut is called, at which point it is pending deletion.
  bool is_active() { return !is_waiting_for_swapout_ack_; }

  // Sends the given navigation message. Use this rather than sending it
  // yourself since this does the internal bookkeeping described below. This
  // function takes ownership of the provided message pointer.
  //
  // If a cross-site request is in progress, we may be suspended while waiting
  // for the onbeforeunload handler, so this function might buffer the message
  // rather than sending it.
  void Navigate(const CommonNavigationParams& common_params,
                const StartNavigationParams& start_params,
                const RequestNavigationParams& request_params);

  // Navigates to an interstitial page represented by the provided data URL.
  void NavigateToInterstitialURL(const GURL& data_url);

  // Stop the load in progress.
  void Stop();

  // Returns whether navigation messages are currently suspended for this
  // RenderFrameHost. Only true during a cross-site navigation, while waiting
  // for the onbeforeunload handler.
  bool are_navigations_suspended() const { return navigations_suspended_; }

  // Suspends (or unsuspends) any navigation messages from being sent from this
  // RenderFrameHost. This is called when a pending RenderFrameHost is created
  // for a cross-site navigation, because we must suspend any navigations until
  // we hear back from the old renderer's onbeforeunload handler. Note that it
  // is important that only one navigation event happen after calling this
  // method with |suspend| equal to true. If |suspend| is false and there is a
  // suspended_nav_message_, this will send the message. This function should
  // only be called to toggle the state; callers should check
  // are_navigations_suspended() first. If |suspend| is false, the time that the
  // user decided the navigation should proceed should be passed as
  // |proceed_time|.
  void SetNavigationsSuspended(bool suspend,
                               const base::TimeTicks& proceed_time);

  // Clears any suspended navigation state after a cross-site navigation is
  // canceled or suspended. This is important if we later return to this
  // RenderFrameHost.
  void CancelSuspendedNavigations();

  // Runs the beforeunload handler for this frame. |for_navigation| indicates
  // whether this call is for the current frame during a cross-process
  // navigation. False means we're closing the entire tab. |is_reload|
  // indicates whether the navigation is a reload of the page or not. If
  // |for_navigation| is false, |is_reload| should be false as well.
  // PlzNavigate: this call happens on all browser-initiated navigations.
  void DispatchBeforeUnload(bool for_navigation, bool is_reload);

  // Simulate beforeunload ack on behalf of renderer if it's unrenresponsive.
  void SimulateBeforeUnloadAck();

  // Returns true if a call to DispatchBeforeUnload will actually send the
  // BeforeUnload IPC. This is the case if the current renderer is live and this
  // frame is the main frame.
  bool ShouldDispatchBeforeUnload();

  // Update the frame's opener in the renderer process in response to the
  // opener being modified (e.g., with window.open or being set to null) in
  // another renderer process.
  void UpdateOpener();

  // Set this frame as focused in the renderer process.  This supports
  // cross-process window.focus() calls.
  void SetFocusedFrame();

  // Continues sequential focus navigation in this frame. |source_proxy|
  // represents the frame that requested a focus change. It must be in the same
  // process as this or |nullptr|.
  void AdvanceFocus(blink::WebFocusType type,
                    RenderFrameProxyHost* source_proxy);

  // Deletes the current selection plus the specified number of characters
  // before and after the selection or caret.
  void ExtendSelectionAndDelete(size_t before, size_t after);

  // Deletes text before and after the current cursor position, excluding the
  // selection. The lengths are supplied in Java chars (UTF-16 Code Unit), not
  // in code points or in glyphs.
  void DeleteSurroundingText(size_t before, size_t after);

  // Deletes text before and after the current cursor position, excluding the
  // selection. The lengths are supplied in code points, not in Java chars
  // (UTF-16 Code Unit) or in glyphs. Do nothing if there are one or more
  // invalid surrogate pairs in the requested range.
  void DeleteSurroundingTextInCodePoints(int before, int after);

  // Notifies the RenderFrame that the JavaScript message that was shown was
  // closed by the user.
  void JavaScriptDialogClosed(IPC::Message* reply_msg,
                              bool success,
                              const base::string16& user_input,
                              bool is_before_unload_dialog,
                              bool dialog_was_suppressed);

  // Get the accessibility mode from the delegate and Send a message to the
  // renderer process to change the accessibility mode.
  void UpdateAccessibilityMode();

  // Samsung Galaxy Note-specific "smart clip" stylus text getter.
  void RequestSmartClipExtract(SmartClipCallback callback, gfx::Rect rect);

  // Request a one-time snapshot of the accessibility tree without changing
  // the accessibility mode.
  void RequestAXTreeSnapshot(AXTreeSnapshotCallback callback);

  // Resets the accessibility serializer in the renderer.
  void AccessibilityReset();

  // Turn on accessibility testing. The given callback will be run
  // every time an accessibility notification is received from the
  // renderer process, and the accessibility tree it sent can be
  // retrieved using GetAXTreeForTesting().
  void SetAccessibilityCallbackForTesting(
      const base::Callback<void(RenderFrameHostImpl*, ui::AXEvent, int)>&
          callback);

  // Called when the metadata about the accessibility tree for this frame
  // changes due to a browser-side change, as opposed to due to an IPC from
  // a renderer.
  void UpdateAXTreeData();

  // Set the AX tree ID of the embedder RFHI, if this is a browser plugin guest.
  void set_browser_plugin_embedder_ax_tree_id(
      ui::AXTreeIDRegistry::AXTreeID ax_tree_id) {
    browser_plugin_embedder_ax_tree_id_ = ax_tree_id;
  }

  // Send a message to the render process to change text track style settings.
  void SetTextTrackSettings(const FrameMsg_TextTrackSettings_Params& params);

  // Returns a snapshot of the accessibility tree received from the
  // renderer as of the last time an accessibility notification was
  // received.
  const ui::AXTree* GetAXTreeForTesting();

  // Access the BrowserAccessibilityManager if it already exists.
  BrowserAccessibilityManager* browser_accessibility_manager() const {
    return browser_accessibility_manager_.get();
  }

  // If accessibility is enabled, get the BrowserAccessibilityManager for
  // this frame, or create one if it doesn't exist yet, otherwise return
  // NULL.
  BrowserAccessibilityManager* GetOrCreateBrowserAccessibilityManager();

  void set_no_create_browser_accessibility_manager_for_testing(bool flag) {
    no_create_browser_accessibility_manager_for_testing_ = flag;
  }

#if defined(USE_EXTERNAL_POPUP_MENU)
#if defined(OS_MACOSX)
  // Select popup menu related methods (for external popup menus).
  void DidSelectPopupMenuItem(int selected_index);
  void DidCancelPopupMenu();
#else
  void DidSelectPopupMenuItems(const std::vector<int>& selected_indices);
  void DidCancelPopupMenu();
#endif
#endif

  // PlzNavigate: Indicates that a navigation is ready to commit and can be
  // handled by this RenderFrame.
  void CommitNavigation(ResourceResponse* response,
                        std::unique_ptr<StreamHandle> body,
                        const CommonNavigationParams& common_params,
                        const RequestNavigationParams& request_params,
                        bool is_view_source);

  // PlzNavigate
  // Indicates that a navigation failed and that this RenderFrame should display
  // an error page.
  void FailedNavigation(const CommonNavigationParams& common_params,
                        const BeginNavigationParams& begin_params,
                        const RequestNavigationParams& request_params,
                        bool has_stale_copy_in_cache,
                        int error_code);

  // Sets up the Mojo connection between this instance and its associated render
  // frame if it has not yet been set up.
  void SetUpMojoIfNeeded();

  // Tears down the browser-side state relating to the Mojo connection between
  // this instance and its associated render frame.
  void InvalidateMojoConnection();

  // Returns whether the frame is focused. A frame is considered focused when it
  // is the parent chain of the focused frame within the frame tree. In
  // addition, its associated RenderWidgetHost has to be focused.
  bool IsFocused();

  // Updates the pending WebUI of this RenderFrameHost based on the provided
  // |dest_url|, setting it to either none, a new instance or to reuse the
  // currently active one. Returns true if the pending WebUI was updated.
  // If this is a history navigation its NavigationEntry bindings should be
  // provided through |entry_bindings| to allow verifying that they are not
  // being set differently this time around. Otherwise |entry_bindings| should
  // be set to NavigationEntryImpl::kInvalidBindings so that no checks are done.
  bool UpdatePendingWebUI(const GURL& dest_url, int entry_bindings);

  // Updates the active WebUI with the pending one set by the last call to
  // UpdatePendingWebUI and then clears any pending data. If UpdatePendingWebUI
  // was not called the active WebUI will simply be cleared.
  void CommitPendingWebUI();

  // Destroys the pending WebUI and resets related data.
  void ClearPendingWebUI();

  // Destroys all WebUI instances and resets related data.
  void ClearAllWebUI();

  // Returns the Mojo ImageDownloader service.
  const content::mojom::ImageDownloaderPtr& GetMojoImageDownloader();

  // Resets the loading state. Following this call, the RenderFrameHost will be
  // in a non-loading state.
  void ResetLoadingState();

  // Returns the feature policy which should be enforced on this RenderFrame.
  FeaturePolicy* get_feature_policy() { return feature_policy_.get(); }

  // Clears any existing policy and constructs a new policy for this frame,
  // based on its parent frame.
  void ResetFeaturePolicy();

  // Tells the renderer that this RenderFrame will soon be swapped out, and thus
  // not to create any new modal dialogs until it happens.  This must be done
  // separately so that the ScopedPageLoadDeferrers of any current dialogs are
  // no longer on the stack when we attempt to swap it out.
  void SuppressFurtherDialogs();

  void SetHasReceivedUserGesture();

  void ClearFocusedElement();

  // Returns whether the given URL is allowed to commit in the current process.
  // This is a more conservative check than RenderProcessHost::FilterURL, since
  // it will be used to kill processes that commit unauthorized URLs.
  bool CanCommitURL(const GURL& url);

  // PlzNavigate: returns the PreviewsState of the last successful navigation
  // that made a network request. The PreviewsState is a bitmask of potentially
  // several Previews optimizations.
  PreviewsState last_navigation_previews_state() const {
    return last_navigation_previews_state_;
  }

  bool has_focused_editable_element() const {
    return has_focused_editable_element_;
  }

 protected:
  friend class RenderFrameHostFactory;

  // |flags| is a combination of CreateRenderFrameFlags.
  // TODO(nasko): Remove dependency on RenderViewHost here. RenderProcessHost
  // should be the abstraction needed here, but we need RenderViewHost to pass
  // into WebContentsObserver::FrameDetached for now.
  RenderFrameHostImpl(SiteInstance* site_instance,
                      RenderViewHostImpl* render_view_host,
                      RenderFrameHostDelegate* delegate,
                      RenderWidgetHostDelegate* rwh_delegate,
                      FrameTree* frame_tree,
                      FrameTreeNode* frame_tree_node,
                      int32_t routing_id,
                      int32_t widget_routing_id,
                      bool hidden,
                      bool renderer_initiated_creation);

 private:
  friend class TestRenderFrameHost;
  friend class TestRenderViewHost;

  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           CreateRenderViewAfterProcessKillAndClosedProxy);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           RestoreFileAccessForHistoryNavigation);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           RestoreSubframeFileAccessForHistoryNavigation);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           RenderViewInitAfterNewProxyAndProcessKill);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           UnloadPushStateOnCrossProcessNavigation);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest,
                           WebUIJavascriptDisallowedAfterSwapOut);
  FRIEND_TEST_ALL_PREFIXES(RenderFrameHostManagerTest, LastCommittedOrigin);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest, CrashSubframe);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           RenderViewHostIsNotReusedAfterDelayedSwapOutACK);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           RenderViewHostStaysActiveWithLateSwapoutACK);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           LoadEventForwardingWhilePendingDeletion);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           ContextMenuAfterCrossProcessNavigation);

  // IPC Message handlers.
  void OnDidAddMessageToConsole(int32_t level,
                                const base::string16& message,
                                int32_t line_no,
                                const base::string16& source_id);
  void OnDetach();
  void OnFrameFocused();
  void OnOpenURL(const FrameHostMsg_OpenURL_Params& params);
  void OnCancelInitialHistoryLoad();
  void OnDocumentOnLoadCompleted(
      FrameMsg_UILoadMetricsReportType::Value report_type,
      base::TimeTicks ui_timestamp);
  void OnDidStartProvisionalLoad(
      const GURL& url,
      const std::vector<GURL>& redirect_chain,
      const base::TimeTicks& navigation_start);
  void OnDidFailProvisionalLoadWithError(
      const FrameHostMsg_DidFailProvisionalLoadWithError_Params& params);
  void OnDidFailLoadWithError(
      const GURL& url,
      int error_code,
      const base::string16& error_description,
      bool was_ignored_by_handler);
  void OnDidCommitProvisionalLoad(const IPC::Message& msg);
  void OnUpdateState(const PageState& state);
  void OnBeforeUnloadACK(
      bool proceed,
      const base::TimeTicks& renderer_before_unload_start_time,
      const base::TimeTicks& renderer_before_unload_end_time);
  void OnSwapOutACK();
  void OnRenderProcessGone(int status, int error_code);
  void OnContextMenu(const ContextMenuParams& params);
  void OnJavaScriptExecuteResponse(int id, const base::ListValue& result);
  void OnVisualStateResponse(uint64_t id);
  void OnRunJavaScriptDialog(const base::string16& message,
                             const base::string16& default_prompt,
                             const GURL& frame_url,
                             JavaScriptDialogType dialog_type,
                             IPC::Message* reply_msg);
  void OnRunBeforeUnloadConfirm(const GURL& frame_url,
                                bool is_reload,
                                IPC::Message* reply_msg);
  void OnRunFileChooser(const FileChooserParams& params);
  void OnTextSurroundingSelectionResponse(const base::string16& content,
                                          uint32_t start_offset,
                                          uint32_t end_offset);
  void OnDidAccessInitialDocument();
  void OnDidChangeOpener(int32_t opener_routing_id);
  void OnDidChangeName(const std::string& name, const std::string& unique_name);
  void OnDidSetFeaturePolicyHeader(
      const ParsedFeaturePolicyHeader& parsed_header);
  void OnDidAddContentSecurityPolicy(
      const ContentSecurityPolicyHeader& header,
      const std::vector<ContentSecurityPolicy>& policy);
  void OnEnforceInsecureRequestPolicy(blink::WebInsecureRequestPolicy policy);
  void OnUpdateToUniqueOrigin(bool is_potentially_trustworthy_unique_origin);
  void OnDidChangeSandboxFlags(int32_t frame_routing_id,
                               blink::WebSandboxFlags flags);
  void OnDidChangeFrameOwnerProperties(int32_t frame_routing_id,
                                       const FrameOwnerProperties& properties);
  void OnUpdateTitle(const base::string16& title,
                     blink::WebTextDirection title_direction);
  void OnUpdateEncoding(const std::string& encoding);
  void OnBeginNavigation(const CommonNavigationParams& common_params,
                         const BeginNavigationParams& begin_params);
  void OnDispatchLoad();
  void OnAccessibilityEvents(
      const std::vector<AccessibilityHostMsg_EventParams>& params,
      int reset_token,
      int ack_token);
  void OnAccessibilityLocationChanges(
      const std::vector<AccessibilityHostMsg_LocationChangeParams>& params);
  void OnAccessibilityFindInPageResult(
      const AccessibilityHostMsg_FindInPageResultParams& params);
  void OnAccessibilityChildFrameHitTestResult(const gfx::Point& point,
                                              int hit_obj_id);
  void OnAccessibilitySnapshotResponse(
      int callback_id,
      const AXContentTreeUpdate& snapshot);
  void OnSmartClipDataExtracted(uint32_t id,
                                base::string16 text,
                                base::string16 html);
  void OnToggleFullscreen(bool enter_fullscreen);
  void OnDidStartLoading(bool to_different_document);
  void OnDidStopLoading();
  void OnDidChangeLoadProgress(double load_progress);
  void OnSerializeAsMHTMLResponse(
      int job_id,
      MhtmlSaveStatus save_status,
      const std::set<std::string>& digests_of_uris_of_serialized_resources,
      base::TimeDelta renderer_main_thread_time);
  void OnSelectionChanged(const base::string16& text,
                          uint32_t offset,
                          const gfx::Range& range);
  void OnFocusedNodeChanged(bool is_editable_element,
                            const gfx::Rect& bounds_in_frame_widget);
  void OnSetHasReceivedUserGesture();

#if defined(USE_EXTERNAL_POPUP_MENU)
  void OnShowPopup(const FrameHostMsg_ShowPopup_Params& params);
  void OnHidePopup();
#endif
  void OnShowCreatedWindow(int pending_widget_routing_id,
                           WindowOpenDisposition disposition,
                           const gfx::Rect& initial_rect,
                           bool user_gesture);

  // Registers Mojo interfaces that this frame host makes available.
  void RegisterMojoInterfaces();

  // Resets any waiting state of this RenderFrameHost that is no longer
  // relevant.
  void ResetWaitingState();

  // Returns whether the given origin is allowed to commit in the current
  // RenderFrameHost. The |url| is used to ensure it matches the origin in cases
  // where it is applicable. This is a more conservative check than
  // RenderProcessHost::FilterURL, since it will be used to kill processes that
  // commit unauthorized origins.
  bool CanCommitOrigin(const url::Origin& origin, const GURL& url);

  // Asserts that the given RenderFrameHostImpl is part of the same browser
  // context (and crashes if not), then returns whether the given frame is
  // part of the same site instance.
  bool IsSameSiteInstance(RenderFrameHostImpl* other_render_frame_host);

  // Returns whether the current RenderProcessHost has read access to all the
  // files reported in |state|.
  bool CanAccessFilesOfPageState(const PageState& state);

  // Grants the current RenderProcessHost read access to any file listed in
  // |validated_state|.  It is important that the PageState has been validated
  // upon receipt from the renderer process to prevent it from forging access to
  // files without the user's consent.
  void GrantFileAccessFromPageState(const PageState& validated_state);

  // Grants the current RenderProcessHost read access to any file listed in
  // |body|.  It is important that the ResourceRequestBody has been validated
  // upon receipt from the renderer process to prevent it from forging access to
  // files without the user's consent.
  void GrantFileAccessFromResourceRequestBody(
      const ResourceRequestBodyImpl& body);

  void UpdatePermissionsForNavigation(
      const CommonNavigationParams& common_params,
      const RequestNavigationParams& request_params);

  // Returns true if the ExecuteJavaScript() API can be used on this host.
  bool CanExecuteJavaScript();

  // Map a routing ID from a frame in the same frame tree to a globally
  // unique AXTreeID.
  ui::AXTreeIDRegistry::AXTreeID RoutingIDToAXTreeID(int routing_id);

  // Map a browser plugin instance ID to the AXTreeID of the plugin's
  // main frame.
  ui::AXTreeIDRegistry::AXTreeID BrowserPluginInstanceIDToAXTreeID(
      int routing_id);

  // Convert the content-layer-specific AXContentNodeData to a general-purpose
  // AXNodeData structure.
  void AXContentNodeDataToAXNodeData(const AXContentNodeData& src,
                                     ui::AXNodeData* dst);

  // Convert the content-layer-specific AXContentTreeData to a general-purpose
  // AXTreeData structure.
  void AXContentTreeDataToAXTreeData(ui::AXTreeData* dst);

  // Returns the RenderWidgetHostView used for accessibility. For subframes,
  // this function will return the platform view on the main frame; for main
  // frames, it will return the current frame's view.
  RenderWidgetHostViewBase* GetViewForAccessibility();

  // Sends a navigate message to the RenderFrame and notifies DevTools about
  // navigation happening. Should be used instead of sending the message
  // directly.
  void SendNavigateMessage(const CommonNavigationParams& common_params,
                           const StartNavigationParams& start_params,
                           const RequestNavigationParams& request_params);

  // Returns the child FrameTreeNode if |child_frame_routing_id| is an
  // immediate child of this FrameTreeNode.  |child_frame_routing_id| is
  // considered untrusted, so the renderer process is killed if it refers to a
  // FrameTreeNode that is not a child of this node.
  FrameTreeNode* FindAndVerifyChild(int32_t child_frame_routing_id,
                                    bad_message::BadMessageReason reason);

  // Creates Web Bluetooth Service owned by the frame. Returns a raw pointer
  // to it.
  WebBluetoothServiceImpl* CreateWebBluetoothService(
      mojo::InterfaceRequest<blink::mojom::WebBluetoothService> request);

  // Deletes the Web Bluetooth Service owned by the frame.
  void DeleteWebBluetoothService(
      WebBluetoothServiceImpl* web_bluetooth_service);

  // service_manager::InterfaceFactory<media::mojom::InterfaceFactory>
  void Create(const service_manager::Identity& remote_identity,
              media::mojom::InterfaceFactoryRequest request) override;

  // Callback for connection error on the media::mojom::InterfaceFactory client.
  void OnMediaInterfaceFactoryConnectionError();

  // Allows tests to disable the swapout event timer to simulate bugs that
  // happen before it fires (to avoid flakiness).
  void DisableSwapOutTimerForTesting();

  void OnRendererConnect(const service_manager::ServiceInfo& local_info,
                         const service_manager::ServiceInfo& remote_info);

  void SendJavaScriptDialogReply(IPC::Message* reply_msg,
                                 bool success,
                                 const base::string16& user_input);

  // Returns ownership of the NavigationHandle associated with a navigation that
  // just committed.
  std::unique_ptr<NavigationHandleImpl> TakeNavigationHandleForCommit(
      const FrameHostMsg_DidCommitProvisionalLoad_Params& params);

  // For now, RenderFrameHosts indirectly keep RenderViewHosts alive via a
  // refcount that calls Shutdown when it reaches zero.  This allows each
  // RenderFrameHostManager to just care about RenderFrameHosts, while ensuring
  // we have a RenderViewHost for each RenderFrameHost.
  // TODO(creis): RenderViewHost will eventually go away and be replaced with
  // some form of page context.
  RenderViewHostImpl* const render_view_host_;

  RenderFrameHostDelegate* const delegate_;

  // The SiteInstance associated with this RenderFrameHost. All content drawn
  // in this RenderFrameHost is part of this SiteInstance. Cannot change over
  // time.
  const scoped_refptr<SiteInstanceImpl> site_instance_;

  // The renderer process this RenderFrameHost is associated with. It is
  // equivalent to the result of site_instance_->GetProcess(), but that
  // method has the side effect of creating the process if it doesn't exist.
  // Cache a pointer to avoid unnecessary process creation.
  RenderProcessHost* const process_;

  // Reference to the whole frame tree that this RenderFrameHost belongs to.
  // Allows this RenderFrameHost to add and remove nodes in response to
  // messages from the renderer requesting DOM manipulation.
  FrameTree* const frame_tree_;

  // The FrameTreeNode which this RenderFrameHostImpl is hosted in.
  FrameTreeNode* const frame_tree_node_;

  // The active parent RenderFrameHost for this frame, if it is a subframe.
  // Null for the main frame.  This is cached because the parent FrameTreeNode
  // may change its current RenderFrameHost while this child is pending
  // deletion, and GetParent() should never return a different value.
  RenderFrameHostImpl* parent_;

  // Track this frame's last committed URL.
  GURL last_committed_url_;

  // Track this frame's last committed origin.
  url::Origin last_committed_origin_;

  // The most recent non-error URL to commit in this frame.  Remove this in
  // favor of GetLastCommittedURL() once PlzNavigate is enabled or cross-process
  // transfers work for net errors.  See https://crbug.com/588314.
  GURL last_successful_url_;

  // The mapping of pending JavaScript calls created by
  // ExecuteJavaScript and their corresponding callbacks.
  std::map<int, JavaScriptResultCallback> javascript_callbacks_;
  std::map<uint64_t, VisualStateCallback> visual_state_callbacks_;

  // RenderFrameHosts that need management of the rendering and input events
  // for their frame subtrees require RenderWidgetHosts. This typically
  // means frames that are rendered in different processes from their parent
  // frames.
  // TODO(kenrb): Later this will also be used on the top-level frame, when
  // RenderFrameHost owns its RenderViewHost.
  RenderWidgetHostImpl* render_widget_host_;

  int routing_id_;

  // Boolean indicating whether this RenderFrameHost is being actively used or
  // is waiting for FrameHostMsg_SwapOut_ACK and thus pending deletion.
  bool is_waiting_for_swapout_ack_;

  // Tracks whether the RenderFrame for this RenderFrameHost has been created in
  // the renderer process.  Currently only used for subframes.
  // TODO(creis): Use this for main frames as well when RVH goes away.
  bool render_frame_created_;

  // Whether we should buffer outgoing Navigate messages rather than sending
  // them. This will be true when a RenderFrameHost is created for a cross-site
  // request, until we hear back from the onbeforeunload handler of the old
  // RenderFrameHost.
  bool navigations_suspended_;

  // Holds the parameters for a suspended navigation. This can only happen while
  // this RFH is the pending RenderFrameHost of a RenderFrameHostManager. There
  // will only ever be one suspended navigation, because RenderFrameHostManager
  // will destroy the pending RenderFrameHost and create a new one if a second
  // navigation occurs.
  // PlzNavigate: unused as navigations are never suspended.
  std::unique_ptr<NavigationParams> suspended_nav_params_;

  // When the last BeforeUnload message was sent.
  base::TimeTicks send_before_unload_start_time_;

  // Set to true when there is a pending FrameMsg_BeforeUnload message.  This
  // ensures we don't spam the renderer with multiple beforeunload requests.
  // When either this value or IsWaitingForUnloadACK is true, the value of
  // unload_ack_is_for_cross_site_transition_ indicates whether this is for a
  // cross-site transition or a tab close attempt.
  // TODO(clamy): Remove this boolean and add one more state to the state
  // machine.
  bool is_waiting_for_beforeunload_ack_;

  // Valid only when is_waiting_for_beforeunload_ack_ or
  // IsWaitingForUnloadACK is true.  This tells us if the unload request
  // is for closing the entire tab ( = false), or only this RenderFrameHost in
  // the case of a navigation ( = true). Currently only cross-site navigations
  // require a beforeUnload/unload ACK.
  // PlzNavigate: all navigations require a beforeUnload ACK.
  bool unload_ack_is_for_navigation_;

  // Indicates whether this RenderFrameHost is in the process of loading a
  // document or not.
  bool is_loading_;

  // PlzNavigate
  // Used to track whether a commit is expected in this frame. Only used in
  // tests.
  bool pending_commit_;

  // The unique ID of the latest NavigationEntry that this RenderFrameHost is
  // showing. This may change even when this frame hasn't committed a page,
  // such as for a new subframe navigation in a different frame.  Tracking this
  // allows us to send things like title and state updates to the latest
  // relevant NavigationEntry.
  int nav_entry_id_;

  // Used to swap out or shut down this RFH when the unload event is taking too
  // long to execute, depending on the number of active frames in the
  // SiteInstance.  May be null in tests.
  std::unique_ptr<TimeoutMonitor> swapout_event_monitor_timeout_;

  std::unique_ptr<service_manager::InterfaceRegistry> interface_registry_;
  std::unique_ptr<service_manager::InterfaceProvider> remote_interfaces_;

  service_manager::ServiceInfo browser_info_;
  service_manager::ServiceInfo renderer_info_;

  int on_connect_handler_id_ = 0;

  std::list<std::unique_ptr<WebBluetoothServiceImpl>> web_bluetooth_services_;

  // The object managing the accessibility tree for this frame.
  std::unique_ptr<BrowserAccessibilityManager> browser_accessibility_manager_;

  // This is nonzero if we sent an accessibility reset to the renderer and
  // we're waiting for an IPC containing this reset token (sequentially
  // assigned) and a complete replacement accessibility tree.
  int accessibility_reset_token_;

  // A count of the number of times we needed to reset accessibility, so
  // we don't keep trying to reset forever.
  int accessibility_reset_count_;

  // The last AXContentTreeData for this frame received from the RenderFrame.
  AXContentTreeData ax_content_tree_data_;

  // The AX tree ID of the embedder, if this is a browser plugin guest.
  ui::AXTreeIDRegistry::AXTreeID browser_plugin_embedder_ax_tree_id_;

  // The mapping from callback id to corresponding callback for pending
  // accessibility tree snapshot calls created by RequestAXTreeSnapshot.
  std::map<int, AXTreeSnapshotCallback> ax_tree_snapshot_callbacks_;

  // Samsung Galaxy Note-specific "smart clip" stylus text getter.
  std::map<uint32_t, SmartClipCallback> smart_clip_callbacks_;

  // Callback when an event is received, for testing.
  base::Callback<void(RenderFrameHostImpl*, ui::AXEvent, int)>
      accessibility_testing_callback_;
  // The most recently received accessibility tree - for testing only.
  std::unique_ptr<ui::AXTree> ax_tree_for_testing_;
  // Flag to not create a BrowserAccessibilityManager, for testing. If one
  // already exists it will still be used.
  bool no_create_browser_accessibility_manager_for_testing_;

  // PlzNavigate: Owns the stream used in navigations to store the body of the
  // response once it has started.
  std::unique_ptr<StreamHandle> stream_handle_;

  // Context shared for each mojom::PermissionService instance created for this
  // RFH.
  std::unique_ptr<PermissionServiceContext> permission_service_context_;

  // Holder of Mojo connection with ImageDownloader service in RenderFrame.
  content::mojom::ImageDownloaderPtr mojo_image_downloader_;

  // Tracks a navigation happening in this frame. Note that while there can be
  // two navigations in the same FrameTreeNode, there can only be one
  // navigation per RenderFrameHost.
  // PlzNavigate: before the navigation is ready to be committed, the
  // NavigationHandle for it is owned by the NavigationRequest.
  std::unique_ptr<NavigationHandleImpl> navigation_handle_;

  // The associated WebUIImpl and its type. They will be set if the current
  // document is from WebUI source. Otherwise they will be null and
  // WebUI::kNoWebUI, respectively.
  std::unique_ptr<WebUIImpl> web_ui_;
  WebUI::TypeID web_ui_type_;

  // The pending WebUIImpl and its type. These values will be used exclusively
  // for same-site navigations to keep a transition of a WebUI in a pending
  // state until the navigation commits.
  std::unique_ptr<WebUIImpl> pending_web_ui_;
  WebUI::TypeID pending_web_ui_type_;

  // If true the associated WebUI should be reused when CommitPendingWebUI is
  // called (no pending instance should be set).
  bool should_reuse_web_ui_;

  // If true, then the RenderFrame has selected text.
  bool has_selection_;

  // PlzNavigate: The Previews state of the last navigation. This is used during
  // history navigation of subframes to ensure that subframes navigate with the
  // same Previews status as the top-level frame.
  PreviewsState last_navigation_previews_state_;

  mojo::Binding<mojom::FrameHost> frame_host_binding_;
  mojom::FramePtr frame_;
  mojom::FrameBindingsControlAssociatedPtr frame_bindings_control_;

  // If this is true then this object was created in response to a renderer
  // initiated request. Init() will be called, and until then navigation
  // requests should be queued.
  bool waiting_for_init_;

  // If true then this frame's document has a focused element which is editable.
  bool has_focused_editable_element_;

  typedef std::pair<CommonNavigationParams, BeginNavigationParams>
      PendingNavigation;
  std::unique_ptr<PendingNavigation> pendinging_navigate_;

  // Callback for responding when
  // |FrameHostMsg_TextSurroundingSelectionResponse| message comes.
  TextSurroundingSelectionCallback text_surrounding_selection_callback_;

  // Hosts media::mojom::InterfaceFactory for the RenderFrame and forwards
  // media::mojom::InterfaceFactory calls to the remote "media" service.
  std::unique_ptr<MediaInterfaceProxy> media_interface_proxy_;

  std::vector<std::unique_ptr<service_manager::InterfaceRegistry>>
      media_registries_;

  std::unique_ptr<AssociatedInterfaceProviderImpl>
      remote_associated_interfaces_;

  // A bitwise OR of bindings types that have been enabled for this RenderFrame.
  // See BindingsPolicy for details.
  int enabled_bindings_ = 0;

  // Tracks the feature policy which has been set on this frame.
  std::unique_ptr<FeaturePolicy> feature_policy_;

  // NOTE: This must be the last member.
  base::WeakPtrFactory<RenderFrameHostImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_HOST_IMPL_H_
