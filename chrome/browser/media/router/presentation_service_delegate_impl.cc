// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/presentation_service_delegate_impl.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/containers/small_map.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "chrome/browser/media/router/browser_presentation_connection_proxy.h"
#include "chrome/browser/media/router/create_presentation_connection_request.h"
#include "chrome/browser/media/router/media_route.h"
#include "chrome/browser/media/router/media_router.h"
#include "chrome/browser/media/router/media_router_dialog_controller.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/media_sink.h"
#include "chrome/browser/media/router/media_source_helper.h"
#include "chrome/browser/media/router/offscreen_presentation_manager.h"
#include "chrome/browser/media/router/offscreen_presentation_manager_factory.h"
#include "chrome/browser/media/router/presentation_media_sinks_observer.h"
#include "chrome/browser/media/router/route_message.h"
#include "chrome/browser/media/router/route_message_observer.h"
#include "chrome/browser/media/router/route_request_result.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/presentation_screen_availability_listener.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/presentation_session.h"
#include "url/gurl.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#endif

DEFINE_WEB_CONTENTS_USER_DATA_KEY(
    media_router::PresentationServiceDelegateImpl);

using content::RenderFrameHost;

namespace media_router {

namespace {

using DelegateObserver = content::PresentationServiceDelegate::Observer;

// Returns the unique identifier for the supplied RenderFrameHost.
RenderFrameHostId GetRenderFrameHostId(RenderFrameHost* render_frame_host) {
  int render_process_id = render_frame_host->GetProcess()->GetID();
  int render_frame_id = render_frame_host->GetRoutingID();
  return RenderFrameHostId(render_process_id, render_frame_id);
}

// Gets the last committed URL for the render frame specified by
// |render_frame_host_id|.
url::Origin GetLastCommittedURLForFrame(
    RenderFrameHostId render_frame_host_id) {
  RenderFrameHost* render_frame_host = RenderFrameHost::FromID(
      render_frame_host_id.first, render_frame_host_id.second);
  DCHECK(render_frame_host);
  return render_frame_host->GetLastCommittedOrigin();
}

// Observes messages originating from the MediaSink connected to a MediaRoute
// that represents a presentation. Converts the messages into
// content::PresentationSessionMessages and dispatches them via the provided
// PresentationSessionMessageCallback.
class PresentationSessionMessagesObserver : public RouteMessageObserver {
 public:
  // |message_cb|: The callback to invoke whenever messages are received.
  // |route_id|: ID of MediaRoute to listen for messages.
  PresentationSessionMessagesObserver(
      MediaRouter* router,
      const MediaRoute::Id& route_id,
      const content::PresentationConnectionMessageCallback& message_cb)
      : RouteMessageObserver(router, route_id), message_cb_(message_cb) {
    DCHECK(!message_cb_.is_null());
  }

  ~PresentationSessionMessagesObserver() final {}

  void OnMessagesReceived(const std::vector<RouteMessage>& messages) final {
    DVLOG(2) << __func__ << ", number of messages : " << messages.size();
    // TODO(mfoltz): Remove RouteMessage and replace with move-only
    // PresentationConnectionMessage.
    std::vector<content::PresentationConnectionMessage> presentation_messages;
    for (const RouteMessage& message : messages) {
      if (message.type == RouteMessage::TEXT && message.text) {
        presentation_messages.emplace_back(message.text.value());
      } else if (message.type == RouteMessage::BINARY && message.binary) {
        presentation_messages.emplace_back(message.binary.value());
      } else {
        NOTREACHED() << "Unknown route message type";
      }
    }
    message_cb_.Run(std::move(presentation_messages));
  }

 private:
  const content::PresentationConnectionMessageCallback message_cb_;

  DISALLOW_COPY_AND_ASSIGN(PresentationSessionMessagesObserver);
};

}  // namespace

// Used by PresentationServiceDelegateImpl to manage
// listeners and default presentation info in a render frame.
// Its lifetime:
//  * Create an instance with |render_frame_host_id_| if no instance with the
//    same |render_frame_host_id_| exists in:
//      PresentationFrameManager::OnPresentationSessionStarted
//      PresentationFrameManager::OnDefaultPresentationSessionStarted
//      PresentationFrameManager::SetScreenAvailabilityListener
//  * Destroy the instance in:
//      PresentationFrameManager::Reset
class PresentationFrame {
 public:
  PresentationFrame(const RenderFrameHostId& render_frame_host_id,
                    content::WebContents* web_contents,
                    MediaRouter* router);
  ~PresentationFrame();

  // Mirror corresponding APIs in PresentationServiceDelegateImpl.
  bool SetScreenAvailabilityListener(
      content::PresentationScreenAvailabilityListener* listener);
  bool RemoveScreenAvailabilityListener(
      content::PresentationScreenAvailabilityListener* listener);
  bool HasScreenAvailabilityListenerForTest(
      const MediaSource::Id& source_id) const;
  std::string GetDefaultPresentationId() const;
  void ListenForConnectionStateChange(
      const content::PresentationSessionInfo& connection,
      const content::PresentationConnectionStateChangedCallback&
          state_changed_cb);
  void ListenForSessionMessages(
      const content::PresentationSessionInfo& session,
      const content::PresentationConnectionMessageCallback& message_cb);

  void Reset();
  void RemoveConnection(const std::string& presentation_id,
                        const MediaRoute::Id& route_id);

  const MediaRoute::Id GetRouteId(const std::string& presentation_id) const;

  void OnPresentationSessionStarted(
      const content::PresentationSessionInfo& session,
      const MediaRoute& route);
  void OnPresentationServiceDelegateDestroyed() const;

  bool IsOffscreenPresentation(const std::string& presentation_id) const;
  void ConnectToPresentation(
      const content::PresentationSessionInfo& session,
      content::PresentationConnectionPtr controller_connection_ptr,
      content::PresentationConnectionRequest receiver_connection_request);

 private:
  MediaSource GetMediaSourceFromListener(
      content::PresentationScreenAvailabilityListener* listener) const;
  base::SmallMap<std::map<std::string, MediaRoute>> presentation_id_to_route_;
  base::SmallMap<
      std::map<std::string, std::unique_ptr<PresentationMediaSinksObserver>>>
      url_to_sinks_observer_;
  std::unordered_map<
      MediaRoute::Id,
      std::unique_ptr<PresentationConnectionStateSubscription>>
      connection_state_subscriptions_;
  std::unordered_map<
      MediaRoute::Id,
      std::unique_ptr<PresentationSessionMessagesObserver>>
      session_messages_observers_;
  std::unordered_map<MediaRoute::Id,
                     std::unique_ptr<BrowserPresentationConnectionProxy>>
      browser_connection_proxies_;

  RenderFrameHostId render_frame_host_id_;

  // References to the owning WebContents, and the corresponding MediaRouter.
  content::WebContents* web_contents_;
  MediaRouter* router_;
};

PresentationFrame::PresentationFrame(
    const RenderFrameHostId& render_frame_host_id,
    content::WebContents* web_contents,
    MediaRouter* router)
    : render_frame_host_id_(render_frame_host_id),
      web_contents_(web_contents),
      router_(router) {
  DCHECK(web_contents_);
  DCHECK(router_);
}

PresentationFrame::~PresentationFrame() {
}

void PresentationFrame::OnPresentationSessionStarted(
    const content::PresentationSessionInfo& session,
    const MediaRoute& route) {
  presentation_id_to_route_.insert(
      std::make_pair(session.presentation_id, route));
}

const MediaRoute::Id PresentationFrame::GetRouteId(
    const std::string& presentation_id) const {
  auto it = presentation_id_to_route_.find(presentation_id);
  return it != presentation_id_to_route_.end() ? it->second.media_route_id()
                                               : "";
}

bool PresentationFrame::SetScreenAvailabilityListener(
    content::PresentationScreenAvailabilityListener* listener) {
  MediaSource source(GetMediaSourceFromListener(listener));
  auto& sinks_observer = url_to_sinks_observer_[source.id()];
  if (sinks_observer && sinks_observer->listener() == listener)
    return false;

  sinks_observer.reset(new PresentationMediaSinksObserver(
      router_, listener, source,
      GetLastCommittedURLForFrame(render_frame_host_id_)));

  if (!sinks_observer->Init()) {
    url_to_sinks_observer_.erase(source.id());
    listener->OnScreenAvailabilityNotSupported();
    return false;
  }

  return true;
}

bool PresentationFrame::RemoveScreenAvailabilityListener(
    content::PresentationScreenAvailabilityListener* listener) {
  MediaSource source(GetMediaSourceFromListener(listener));
  auto sinks_observer_it = url_to_sinks_observer_.find(source.id());
  if (sinks_observer_it != url_to_sinks_observer_.end() &&
      sinks_observer_it->second->listener() == listener) {
    url_to_sinks_observer_.erase(sinks_observer_it);
    return true;
  }
  return false;
}

bool PresentationFrame::HasScreenAvailabilityListenerForTest(
    const MediaSource::Id& source_id) const {
  return url_to_sinks_observer_.find(source_id) != url_to_sinks_observer_.end();
}

void PresentationFrame::Reset() {
  auto* offscreen_presentation_manager =
      OffscreenPresentationManagerFactory::GetOrCreateForWebContents(
          web_contents_);

  for (const auto& pid_route : presentation_id_to_route_) {
    if (IsOffscreenPresentation(pid_route.first)) {
      offscreen_presentation_manager->UnregisterOffscreenPresentationController(
          pid_route.first, render_frame_host_id_);
    }
    router_->DetachRoute(pid_route.second.media_route_id());
  }

  presentation_id_to_route_.clear();
  url_to_sinks_observer_.clear();
  connection_state_subscriptions_.clear();
  session_messages_observers_.clear();
  browser_connection_proxies_.clear();
}

void PresentationFrame::RemoveConnection(const std::string& presentation_id,
                                         const MediaRoute::Id& route_id) {
  // Remove the presentation id mapping so a later call to Reset is a no-op.
  presentation_id_to_route_.erase(presentation_id);

  // We no longer need to observe route messages.
  session_messages_observers_.erase(route_id);

  browser_connection_proxies_.erase(route_id);
  // We keep the PresentationConnectionStateChangedCallback registered with MR
  // so the MRP can tell us when terminate() completed.
}

void PresentationFrame::ListenForConnectionStateChange(
    const content::PresentationSessionInfo& connection,
    const content::PresentationConnectionStateChangedCallback&
        state_changed_cb) {
  auto it = presentation_id_to_route_.find(connection.presentation_id);
  if (it == presentation_id_to_route_.end()) {
    DLOG(ERROR) << __func__ << "route id not found for presentation: "
                << connection.presentation_id;
    return;
  }

  const MediaRoute::Id& route_id = it->second.media_route_id();
  if (connection_state_subscriptions_.find(route_id) !=
      connection_state_subscriptions_.end()) {
    DLOG(ERROR) << __func__
                << "Already listening connection state change for route: "
                << route_id;
    return;
  }

  connection_state_subscriptions_.insert(std::make_pair(
      route_id, router_->AddPresentationConnectionStateChangedCallback(
                    route_id, state_changed_cb)));
}

void PresentationFrame::ListenForSessionMessages(
    const content::PresentationSessionInfo& session,
    const content::PresentationConnectionMessageCallback& message_cb) {
  auto it = presentation_id_to_route_.find(session.presentation_id);
  if (it == presentation_id_to_route_.end()) {
    DVLOG(2) << "ListenForSessionMessages: no route for "
             << session.presentation_id;
    return;
  }

  if (it->second.is_offscreen_presentation()) {
    DVLOG(2) << "ListenForSessionMessages: do not listen for offscreen "
             << "presentation [id]: " << session.presentation_id;
    return;
  }

  const MediaRoute::Id& route_id = it->second.media_route_id();
  if (session_messages_observers_.find(route_id) !=
      session_messages_observers_.end()) {
    DLOG(ERROR) << __func__
                << "Already listening for session messages for route: "
                << route_id;
    return;
  }

  session_messages_observers_.insert(std::make_pair(
      route_id, base::MakeUnique<PresentationSessionMessagesObserver>(
                    router_, route_id, message_cb)));
}

MediaSource PresentationFrame::GetMediaSourceFromListener(
    content::PresentationScreenAvailabilityListener* listener) const {
  // If the default presentation URL is empty then fall back to tab mirroring.
  return listener->GetAvailabilityUrl().is_empty()
             ? MediaSourceForTab(SessionTabHelper::IdForTab(web_contents_))
             : MediaSourceForPresentationUrl(listener->GetAvailabilityUrl());
}

bool PresentationFrame::IsOffscreenPresentation(
    const std::string& presentation_id) const {
  const auto it = presentation_id_to_route_.find(presentation_id);
  if (it == presentation_id_to_route_.end()) {
    DLOG(WARNING) << "No route for [presentation_id]: " << presentation_id;
    return false;
  }

  return it->second.is_offscreen_presentation();
}

void PresentationFrame::ConnectToPresentation(
    const content::PresentationSessionInfo& session,
    content::PresentationConnectionPtr controller_connection_ptr,
    content::PresentationConnectionRequest receiver_connection_request) {
  const auto pid_route_it =
      presentation_id_to_route_.find(session.presentation_id);

  if (pid_route_it == presentation_id_to_route_.end()) {
    DLOG(WARNING) << "No route for [presentation_id]: "
                  << session.presentation_id;
    return;
  }

  if (IsOffscreenPresentation(session.presentation_id)) {
    auto* const offscreen_presentation_manager =
        OffscreenPresentationManagerFactory::GetOrCreateForWebContents(
            web_contents_);
    offscreen_presentation_manager->RegisterOffscreenPresentationController(
        session.presentation_id, session.presentation_url,
        render_frame_host_id_, std::move(controller_connection_ptr),
        std::move(receiver_connection_request));
  } else {
    DVLOG(2)
        << "Creating BrowserPresentationConnectionProxy for [presentation_id]: "
        << session.presentation_id;
    MediaRoute::Id route_id = pid_route_it->second.media_route_id();
    auto* proxy = new BrowserPresentationConnectionProxy(
        router_, route_id, std::move(receiver_connection_request),
        std::move(controller_connection_ptr));

    browser_connection_proxies_.insert(
        std::make_pair(route_id, base::WrapUnique(proxy)));
  }
}

// Used by PresentationServiceDelegateImpl to manage PresentationFrames.
class PresentationFrameManager {
 public:
  PresentationFrameManager(content::WebContents* web_contents,
                           MediaRouter* router);
  ~PresentationFrameManager();

  // Mirror corresponding APIs in PresentationServiceDelegateImpl.
  bool SetScreenAvailabilityListener(
      const RenderFrameHostId& render_frame_host_id,
      content::PresentationScreenAvailabilityListener* listener);
  bool RemoveScreenAvailabilityListener(
      const RenderFrameHostId& render_frame_host_id,
      content::PresentationScreenAvailabilityListener* listener);
  void ListenForConnectionStateChange(
      const RenderFrameHostId& render_frame_host_id,
      const content::PresentationSessionInfo& connection,
      const content::PresentationConnectionStateChangedCallback&
          state_changed_cb);
  void ListenForSessionMessages(
      const RenderFrameHostId& render_frame_host_id,
      const content::PresentationSessionInfo& session,
      const content::PresentationConnectionMessageCallback& message_cb);

  // Sets or clears the default presentation request and callback for the given
  // frame. Also sets / clears the default presentation requests for the owning
  // tab WebContents.
  void SetDefaultPresentationUrls(
      const RenderFrameHostId& render_frame_host_id,
      const std::vector<GURL>& default_presentation_urls,
      const content::PresentationSessionStartedCallback& callback);
  void AddDelegateObserver(const RenderFrameHostId& render_frame_host_id,
                           DelegateObserver* observer);
  void RemoveDelegateObserver(const RenderFrameHostId& render_frame_host_id);
  void AddDefaultPresentationRequestObserver(
      PresentationServiceDelegateImpl::DefaultPresentationRequestObserver*
          observer);
  void RemoveDefaultPresentationRequestObserver(
      PresentationServiceDelegateImpl::DefaultPresentationRequestObserver*
          observer);
  void Reset(const RenderFrameHostId& render_frame_host_id);
  void RemoveConnection(const RenderFrameHostId& render_frame_host_id,
                        const MediaRoute::Id& route_id,
                        const std::string& presentation_id);
  bool HasScreenAvailabilityListenerForTest(
      const RenderFrameHostId& render_frame_host_id,
      const MediaSource::Id& source_id) const;
  void SetMediaRouterForTest(MediaRouter* router);

  void OnPresentationSessionStarted(
      const RenderFrameHostId& render_frame_host_id,
      const content::PresentationSessionInfo& session,
      const MediaRoute& route);
  void OnDefaultPresentationSessionStarted(
      const PresentationRequest& request,
      const content::PresentationSessionInfo& session,
      const MediaRoute& route);

  void ConnectToPresentation(
      const RenderFrameHostId& render_frame_host_id,
      const content::PresentationSessionInfo& session,
      content::PresentationConnectionPtr controller_connection_ptr,
      content::PresentationConnectionRequest receiver_connection_request);

  const MediaRoute::Id GetRouteId(const RenderFrameHostId& render_frame_host_id,
                                  const std::string& presentation_id) const;

  const PresentationRequest* default_presentation_request() const {
    return default_presentation_request_.get();
  }

 private:
  PresentationFrame* GetOrAddPresentationFrame(
      const RenderFrameHostId& render_frame_host_id);

  // Sets the default presentation request for the owning WebContents and
  // notifies observers of changes.
  void SetDefaultPresentationRequest(
      const PresentationRequest& default_presentation_request);

  // Clears the default presentation request for the owning WebContents and
  // notifies observers of changes. Also resets
  // |default_presentation_started_callback_|.
  void ClearDefaultPresentationRequest();

  bool IsMainFrame(const RenderFrameHostId& render_frame_host_id) const;

  // Maps a frame identifier to a PresentationFrame object for frames
  // that are using presentation API.
  std::unordered_map<RenderFrameHostId, std::unique_ptr<PresentationFrame>,
                     RenderFrameHostIdHasher>
      presentation_frames_;

  // Default presentation request for the owning tab WebContents.
  std::unique_ptr<PresentationRequest> default_presentation_request_;

  // Callback to invoke when default presentation has started.
  content::PresentationSessionStartedCallback
      default_presentation_started_callback_;

  // References to the observers listening for changes to this tab WebContent's
  // default presentation.
  base::ObserverList<
      PresentationServiceDelegateImpl::DefaultPresentationRequestObserver>
      default_presentation_request_observers_;

  // References to the owning WebContents, and the corresponding MediaRouter.
  MediaRouter* router_;
  content::WebContents* web_contents_;
};

PresentationFrameManager::PresentationFrameManager(
    content::WebContents* web_contents,
    MediaRouter* router)
    : router_(router), web_contents_(web_contents) {
  DCHECK(web_contents_);
  DCHECK(router_);
}

PresentationFrameManager::~PresentationFrameManager() {}

void PresentationFrameManager::OnPresentationSessionStarted(
    const RenderFrameHostId& render_frame_host_id,
    const content::PresentationSessionInfo& session,
    const MediaRoute& route) {
  auto* presentation_frame = GetOrAddPresentationFrame(render_frame_host_id);
  presentation_frame->OnPresentationSessionStarted(session, route);
}

void PresentationFrameManager::OnDefaultPresentationSessionStarted(
    const PresentationRequest& request,
    const content::PresentationSessionInfo& session,
    const MediaRoute& route) {
  OnPresentationSessionStarted(request.render_frame_host_id(), session, route);

  if (default_presentation_request_ &&
      default_presentation_request_->Equals(request)) {
    default_presentation_started_callback_.Run(session);
  }
}

void PresentationFrameManager::ConnectToPresentation(
    const RenderFrameHostId& render_frame_host_id,
    const content::PresentationSessionInfo& session,
    content::PresentationConnectionPtr controller_connection_ptr,
    content::PresentationConnectionRequest receiver_connection_request) {
  auto* presentation_frame = GetOrAddPresentationFrame(render_frame_host_id);
  presentation_frame->ConnectToPresentation(
      session, std::move(controller_connection_ptr),
      std::move(receiver_connection_request));
}

const MediaRoute::Id PresentationFrameManager::GetRouteId(
    const RenderFrameHostId& render_frame_host_id,
    const std::string& presentation_id) const {
  const auto it = presentation_frames_.find(render_frame_host_id);
  return it != presentation_frames_.end()
      ? it->second->GetRouteId(presentation_id) : MediaRoute::Id();
}

bool PresentationFrameManager::SetScreenAvailabilityListener(
    const RenderFrameHostId& render_frame_host_id,
    content::PresentationScreenAvailabilityListener* listener) {
  DCHECK(listener);
  auto* presentation_frame = GetOrAddPresentationFrame(render_frame_host_id);
  return presentation_frame->SetScreenAvailabilityListener(listener);
}

bool PresentationFrameManager::RemoveScreenAvailabilityListener(
    const RenderFrameHostId& render_frame_host_id,
    content::PresentationScreenAvailabilityListener* listener) {
  DCHECK(listener);
  const auto it = presentation_frames_.find(render_frame_host_id);
  return it != presentation_frames_.end() &&
         it->second->RemoveScreenAvailabilityListener(listener);
}

bool PresentationFrameManager::HasScreenAvailabilityListenerForTest(
    const RenderFrameHostId& render_frame_host_id,
    const MediaSource::Id& source_id) const {
  const auto it = presentation_frames_.find(render_frame_host_id);
  return it != presentation_frames_.end() &&
         it->second->HasScreenAvailabilityListenerForTest(source_id);
}

void PresentationFrameManager::ListenForConnectionStateChange(
    const RenderFrameHostId& render_frame_host_id,
    const content::PresentationSessionInfo& connection,
    const content::PresentationConnectionStateChangedCallback&
        state_changed_cb) {
  const auto it = presentation_frames_.find(render_frame_host_id);
  if (it != presentation_frames_.end())
    it->second->ListenForConnectionStateChange(connection, state_changed_cb);
}

void PresentationFrameManager::ListenForSessionMessages(
    const RenderFrameHostId& render_frame_host_id,
    const content::PresentationSessionInfo& session,
    const content::PresentationConnectionMessageCallback& message_cb) {
  const auto it = presentation_frames_.find(render_frame_host_id);
  if (it == presentation_frames_.end()) {
    DVLOG(2) << "ListenForSessionMessages: PresentationFrame does not exist "
             << "for: (" << render_frame_host_id.first << ", "
             << render_frame_host_id.second << ")";
    return;
  }
  it->second->ListenForSessionMessages(session, message_cb);
}

void PresentationFrameManager::SetDefaultPresentationUrls(
    const RenderFrameHostId& render_frame_host_id,
    const std::vector<GURL>& default_presentation_urls,
    const content::PresentationSessionStartedCallback& callback) {
  if (!IsMainFrame(render_frame_host_id))
    return;

  if (default_presentation_urls.empty()) {
    ClearDefaultPresentationRequest();
  } else {
    DCHECK(!callback.is_null());
    const auto& frame_origin =
        GetLastCommittedURLForFrame(render_frame_host_id);
    PresentationRequest request(render_frame_host_id, default_presentation_urls,
                                frame_origin);
    default_presentation_started_callback_ = callback;
    SetDefaultPresentationRequest(request);
  }
}

void PresentationFrameManager::AddDefaultPresentationRequestObserver(
    PresentationServiceDelegateImpl::DefaultPresentationRequestObserver*
        observer) {
  default_presentation_request_observers_.AddObserver(observer);
}

void PresentationFrameManager::RemoveDefaultPresentationRequestObserver(
    PresentationServiceDelegateImpl::DefaultPresentationRequestObserver*
        observer) {
  default_presentation_request_observers_.RemoveObserver(observer);
}

void PresentationFrameManager::Reset(
    const RenderFrameHostId& render_frame_host_id) {
  const auto it = presentation_frames_.find(render_frame_host_id);
  if (it != presentation_frames_.end()) {
    it->second->Reset();
    presentation_frames_.erase(it);
  }

  if (default_presentation_request_ &&
      render_frame_host_id ==
          default_presentation_request_->render_frame_host_id()) {
    ClearDefaultPresentationRequest();
  }
}

void PresentationFrameManager::RemoveConnection(
    const RenderFrameHostId& render_frame_host_id,
    const MediaRoute::Id& route_id,
    const std::string& presentation_id) {
  const auto it = presentation_frames_.find(render_frame_host_id);
  if (it != presentation_frames_.end())
    it->second->RemoveConnection(route_id, presentation_id);
}

PresentationFrame* PresentationFrameManager::GetOrAddPresentationFrame(
    const RenderFrameHostId& render_frame_host_id) {
  std::unique_ptr<PresentationFrame>& presentation_frame =
      presentation_frames_[render_frame_host_id];
  if (!presentation_frame) {
    presentation_frame.reset(new PresentationFrame(
        render_frame_host_id, web_contents_, router_));
  }
  return presentation_frame.get();
}

void PresentationFrameManager::ClearDefaultPresentationRequest() {
  default_presentation_started_callback_.Reset();
  if (!default_presentation_request_)
    return;

  default_presentation_request_.reset();
  for (auto& observer : default_presentation_request_observers_)
    observer.OnDefaultPresentationRemoved();
}

bool PresentationFrameManager::IsMainFrame(
    const RenderFrameHostId& render_frame_host_id) const {
  RenderFrameHost* main_frame = web_contents_->GetMainFrame();
  return main_frame && GetRenderFrameHostId(main_frame) == render_frame_host_id;
}

void PresentationFrameManager::SetDefaultPresentationRequest(
    const PresentationRequest& default_presentation_request) {
  if (default_presentation_request_ &&
      default_presentation_request_->Equals(default_presentation_request))
    return;

  default_presentation_request_.reset(
      new PresentationRequest(default_presentation_request));
  for (auto& observer : default_presentation_request_observers_)
    observer.OnDefaultPresentationChanged(*default_presentation_request_);
}

void PresentationFrameManager::SetMediaRouterForTest(MediaRouter* router) {
  router_ = router;
}

PresentationServiceDelegateImpl*
PresentationServiceDelegateImpl::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  // CreateForWebContents does nothing if the delegate instance already exists.
  PresentationServiceDelegateImpl::CreateForWebContents(web_contents);
  return PresentationServiceDelegateImpl::FromWebContents(web_contents);
}

PresentationServiceDelegateImpl::PresentationServiceDelegateImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      router_(MediaRouterFactory::GetApiForBrowserContext(
          web_contents_->GetBrowserContext())),
      frame_manager_(new PresentationFrameManager(web_contents, router_)),
      weak_factory_(this) {
  DCHECK(web_contents_);
  DCHECK(router_);
}

PresentationServiceDelegateImpl::~PresentationServiceDelegateImpl() {
}

void PresentationServiceDelegateImpl::AddObserver(int render_process_id,
                                                  int render_frame_id,
                                                  DelegateObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(render_process_id, render_frame_id, observer);
}

void PresentationServiceDelegateImpl::RemoveObserver(int render_process_id,
                                                     int render_frame_id) {
  observers_.RemoveObserver(render_process_id, render_frame_id);
}

bool PresentationServiceDelegateImpl::AddScreenAvailabilityListener(
    int render_process_id,
    int render_frame_id,
    content::PresentationScreenAvailabilityListener* listener) {
  DCHECK(listener);
  return frame_manager_->SetScreenAvailabilityListener(
      RenderFrameHostId(render_process_id, render_frame_id), listener);
}

void PresentationServiceDelegateImpl::RemoveScreenAvailabilityListener(
    int render_process_id,
    int render_frame_id,
    content::PresentationScreenAvailabilityListener* listener) {
  DCHECK(listener);
  frame_manager_->RemoveScreenAvailabilityListener(
      RenderFrameHostId(render_process_id, render_frame_id), listener);
}

void PresentationServiceDelegateImpl::Reset(int render_process_id,
                                            int render_frame_id) {
  RenderFrameHostId render_frame_host_id(render_process_id, render_frame_id);
  frame_manager_->Reset(render_frame_host_id);
}

void PresentationServiceDelegateImpl::SetDefaultPresentationUrls(
    int render_process_id,
    int render_frame_id,
    const std::vector<GURL>& default_presentation_urls,
    const content::PresentationSessionStartedCallback& callback) {
  RenderFrameHostId render_frame_host_id(render_process_id, render_frame_id);
  frame_manager_->SetDefaultPresentationUrls(
      render_frame_host_id, default_presentation_urls, callback);
}

void PresentationServiceDelegateImpl::OnJoinRouteResponse(
    int render_process_id,
    int render_frame_id,
    const GURL& presentation_url,
    const std::string& presentation_id,
    const content::PresentationSessionStartedCallback& success_cb,
    const content::PresentationSessionErrorCallback& error_cb,
    const RouteRequestResult& result) {
  if (!result.route()) {
    error_cb.Run(content::PresentationError(
        content::PRESENTATION_ERROR_NO_PRESENTATION_FOUND, result.error()));
  } else {
    DVLOG(1) << "OnJoinRouteResponse: "
             << "route_id: " << result.route()->media_route_id()
             << ", presentation URL: " << presentation_url
             << ", presentation ID: " << presentation_id;
    DCHECK_EQ(presentation_id, result.presentation_id());
    content::PresentationSessionInfo session(presentation_url,
                                             result.presentation_id());
    frame_manager_->OnPresentationSessionStarted(
        RenderFrameHostId(render_process_id, render_frame_id), session,
        *result.route());
    success_cb.Run(session);
  }
}

void PresentationServiceDelegateImpl::OnStartSessionSucceeded(
    int render_process_id,
    int render_frame_id,
    const content::PresentationSessionStartedCallback& success_cb,
    const content::PresentationSessionInfo& new_session,
    const MediaRoute& route) {
  DVLOG(1) << "OnStartSessionSucceeded: "
           << "route_id: " << route.media_route_id()
           << ", presentation URL: " << new_session.presentation_url
           << ", presentation ID: " << new_session.presentation_id;
  frame_manager_->OnPresentationSessionStarted(
      RenderFrameHostId(render_process_id, render_frame_id), new_session,
      route);
  success_cb.Run(new_session);
}

void PresentationServiceDelegateImpl::StartSession(
    int render_process_id,
    int render_frame_id,
    const std::vector<GURL>& presentation_urls,
    const content::PresentationSessionStartedCallback& success_cb,
    const content::PresentationSessionErrorCallback& error_cb) {
  if (presentation_urls.empty()) {
    error_cb.Run(content::PresentationError(content::PRESENTATION_ERROR_UNKNOWN,
                                            "Invalid presentation arguments."));
    return;
  }

  // TODO(crbug.com/670848): Improve handling of invalid URLs in
  // PresentationService::start().
  if (presentation_urls.empty() ||
      std::find_if_not(presentation_urls.begin(), presentation_urls.end(),
                       IsValidPresentationUrl) != presentation_urls.end()) {
    error_cb.Run(content::PresentationError(
        content::PRESENTATION_ERROR_NO_PRESENTATION_FOUND,
        "Invalid presentation URL."));
    return;
  }

  RenderFrameHostId render_frame_host_id(render_process_id, render_frame_id);
  std::unique_ptr<CreatePresentationConnectionRequest> request(
      new CreatePresentationConnectionRequest(
          render_frame_host_id, presentation_urls,
          GetLastCommittedURLForFrame(render_frame_host_id),
          base::Bind(&PresentationServiceDelegateImpl::OnStartSessionSucceeded,
                     weak_factory_.GetWeakPtr(), render_process_id,
                     render_frame_id, success_cb),
          error_cb));
  MediaRouterDialogController* controller =
      MediaRouterDialogController::GetOrCreateForWebContents(web_contents_);
  if (!controller->ShowMediaRouterDialogForPresentation(std::move(request))) {
    LOG(ERROR) << "Media router dialog already exists. Ignoring StartSession.";
    error_cb.Run(content::PresentationError(content::PRESENTATION_ERROR_UNKNOWN,
                                            "Unable to create dialog."));
    return;
  }
}

void PresentationServiceDelegateImpl::JoinSession(
    int render_process_id,
    int render_frame_id,
    const std::vector<GURL>& presentation_urls,
    const std::string& presentation_id,
    const content::PresentationSessionStartedCallback& success_cb,
    const content::PresentationSessionErrorCallback& error_cb) {
  DVLOG(2) << "PresentationServiceDelegateImpl::JoinSession";
  if (presentation_urls.empty()) {
    error_cb.Run(content::PresentationError(
        content::PRESENTATION_ERROR_NO_PRESENTATION_FOUND,
        "Invalid presentation arguments."));
    return;
  }

  const url::Origin& origin = GetLastCommittedURLForFrame(
      RenderFrameHostId(render_process_id, render_frame_id));

#if !defined(OS_ANDROID)
  if (IsAutoJoinPresentationId(presentation_id) &&
      ShouldCancelAutoJoinForOrigin(origin)) {
    error_cb.Run(content::PresentationError(
        content::PRESENTATION_ERROR_SESSION_REQUEST_CANCELLED,
        "Auto-join request cancelled by user preferences."));
    return;
  }
#endif  // !defined(OS_ANDROID)

  // TODO(crbug.com/627655): Handle multiple URLs.
  const GURL& presentation_url = presentation_urls[0];
  bool incognito = web_contents_->GetBrowserContext()->IsOffTheRecord();
  std::vector<MediaRouteResponseCallback> route_response_callbacks;
  route_response_callbacks.push_back(
      base::Bind(&PresentationServiceDelegateImpl::OnJoinRouteResponse,
                 weak_factory_.GetWeakPtr(), render_process_id, render_frame_id,
                 presentation_url, presentation_id, success_cb, error_cb));
  router_->JoinRoute(MediaSourceForPresentationUrl(presentation_url).id(),
                     presentation_id, origin, web_contents_,
                     route_response_callbacks, base::TimeDelta(), incognito);
}

void PresentationServiceDelegateImpl::CloseConnection(
    int render_process_id,
    int render_frame_id,
    const std::string& presentation_id) {
  const RenderFrameHostId rfh_id(render_process_id, render_frame_id);
  const MediaRoute::Id& route_id =
      frame_manager_->GetRouteId(rfh_id, presentation_id);
  if (route_id.empty()) {
    DVLOG(1) << "No active route for: " << presentation_id;
    return;
  }

  router_->DetachRoute(route_id);
  frame_manager_->RemoveConnection(rfh_id, presentation_id, route_id);
  // TODO(mfoltz): close() should always succeed so there is no need to keep the
  // state_changed_cb around - remove it and fire the ChangeEvent on the
  // PresentationConnection in Blink.
}

void PresentationServiceDelegateImpl::Terminate(
    int render_process_id,
    int render_frame_id,
    const std::string& presentation_id) {
  const RenderFrameHostId rfh_id(render_process_id, render_frame_id);
  const MediaRoute::Id& route_id =
      frame_manager_->GetRouteId(rfh_id, presentation_id);
  if (route_id.empty()) {
    DVLOG(1) << "No active route for: " << presentation_id;
    return;
  }
  router_->TerminateRoute(route_id);
  frame_manager_->RemoveConnection(rfh_id, presentation_id, route_id);
}

void PresentationServiceDelegateImpl::ListenForConnectionMessages(
    int render_process_id,
    int render_frame_id,
    const content::PresentationSessionInfo& session,
    const content::PresentationConnectionMessageCallback& message_cb) {
  frame_manager_->ListenForSessionMessages(
      RenderFrameHostId(render_process_id, render_frame_id), session,
      message_cb);
}

void PresentationServiceDelegateImpl::SendMessage(
    int render_process_id,
    int render_frame_id,
    const content::PresentationSessionInfo& session,
    content::PresentationConnectionMessage message,
    const SendMessageCallback& send_message_cb) {
  const MediaRoute::Id& route_id = frame_manager_->GetRouteId(
      RenderFrameHostId(render_process_id, render_frame_id),
      session.presentation_id);
  if (route_id.empty()) {
    DVLOG(1) << "No active route for  " << session.presentation_id;
    send_message_cb.Run(false);
    return;
  }

  if (message.is_binary()) {
    router_->SendRouteBinaryMessage(
        route_id,
        base::MakeUnique<std::vector<uint8_t>>(std::move(message.data.value())),
        send_message_cb);
  } else {
    router_->SendRouteMessage(route_id, message.message.value(),
                              send_message_cb);
  }
}

void PresentationServiceDelegateImpl::ListenForConnectionStateChange(
    int render_process_id,
    int render_frame_id,
    const content::PresentationSessionInfo& connection,
    const content::PresentationConnectionStateChangedCallback&
        state_changed_cb) {
  frame_manager_->ListenForConnectionStateChange(
      RenderFrameHostId(render_process_id, render_frame_id), connection,
      state_changed_cb);
}

void PresentationServiceDelegateImpl::ConnectToPresentation(
    int render_process_id,
    int render_frame_id,
    const content::PresentationSessionInfo& session,
    content::PresentationConnectionPtr controller_connection_ptr,
    content::PresentationConnectionRequest receiver_connection_request) {
  RenderFrameHostId render_frame_host_id(render_process_id, render_frame_id);
  frame_manager_->ConnectToPresentation(render_frame_host_id, session,
                                        std::move(controller_connection_ptr),
                                        std::move(receiver_connection_request));
}

void PresentationServiceDelegateImpl::OnRouteResponse(
    const PresentationRequest& presentation_request,
    const RouteRequestResult& result) {
  if (!result.route() ||
      !base::ContainsValue(presentation_request.presentation_urls(),
                           result.presentation_url())) {
    return;
  }

  content::PresentationSessionInfo session_info(result.presentation_url(),
                                                result.presentation_id());
  frame_manager_->OnDefaultPresentationSessionStarted(
      presentation_request, session_info, *result.route());
}

void PresentationServiceDelegateImpl::AddDefaultPresentationRequestObserver(
    DefaultPresentationRequestObserver* observer) {
  frame_manager_->AddDefaultPresentationRequestObserver(observer);
}

void PresentationServiceDelegateImpl::RemoveDefaultPresentationRequestObserver(
    DefaultPresentationRequestObserver* observer) {
  frame_manager_->RemoveDefaultPresentationRequestObserver(observer);
}

PresentationRequest
PresentationServiceDelegateImpl::GetDefaultPresentationRequest() const {
  DCHECK(HasDefaultPresentationRequest());
  return *frame_manager_->default_presentation_request();
}

bool PresentationServiceDelegateImpl::HasDefaultPresentationRequest() const {
  return frame_manager_->default_presentation_request() != nullptr;
}

base::WeakPtr<PresentationServiceDelegateImpl>
PresentationServiceDelegateImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void PresentationServiceDelegateImpl::SetMediaRouterForTest(
    MediaRouter* router) {
  router_ = router;
  frame_manager_->SetMediaRouterForTest(router);
}

bool PresentationServiceDelegateImpl::HasScreenAvailabilityListenerForTest(
    int render_process_id,
    int render_frame_id,
    const MediaSource::Id& source_id) const {
  RenderFrameHostId render_frame_host_id(render_process_id, render_frame_id);
  return frame_manager_->HasScreenAvailabilityListenerForTest(
      render_frame_host_id, source_id);
}

#if !defined(OS_ANDROID)
bool PresentationServiceDelegateImpl::ShouldCancelAutoJoinForOrigin(
    const url::Origin& origin) const {
  const base::ListValue* origins =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext())
          ->GetPrefs()
          ->GetList(prefs::kMediaRouterTabMirroringSources);
  return origins &&
         origins->Find(base::Value(origin.Serialize())) != origins->end();
}
#endif  // !defined(OS_ANDROID)

}  // namespace media_router
