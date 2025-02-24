// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/guest_view/guest_view_internal_custom_bindings.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "components/guest_view/common/guest_view_constants.h"
#include "components/guest_view/common/guest_view_messages.h"
#include "components/guest_view/renderer/guest_view_request.h"
#include "components/guest_view/renderer/iframe_guest_view_container.h"
#include "components/guest_view/renderer/iframe_guest_view_request.h"
#include "content/public/child/v8_value_converter.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/guest_view/extensions_guest_view_messages.h"
#include "extensions/renderer/guest_view/extensions_guest_view_container.h"
#include "extensions/renderer/script_context.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebRemoteFrame.h"
#include "third_party/WebKit/public/web/WebScopedUserGesture.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "v8/include/v8.h"

using content::V8ValueConverter;

namespace {

// A map from view instance ID to view object (stored via weak V8 reference).
// Views are registered into this map via
// GuestViewInternalCustomBindings::RegisterView(), and accessed via
// GuestViewInternalCustomBindings::GetViewFromID().
using ViewMap = std::map<int, v8::Global<v8::Object>*>;
static base::LazyInstance<ViewMap>::DestructorAtExit weak_view_map =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace extensions {

namespace {

content::RenderFrame* GetRenderFrame(v8::Handle<v8::Value> value) {
  v8::Local<v8::Context> context =
      v8::Local<v8::Object>::Cast(value)->CreationContext();
  if (context.IsEmpty())
    return nullptr;
  blink::WebLocalFrame* frame = blink::WebLocalFrame::frameForContext(context);
  if (!frame)
    return nullptr;
  return content::RenderFrame::FromWebFrame(frame);
}

class RenderFrameStatus : public content::RenderFrameObserver {
 public:
  explicit RenderFrameStatus(content::RenderFrame* render_frame)
      : content::RenderFrameObserver(render_frame) {}
  ~RenderFrameStatus() final {}

  bool is_ok() { return render_frame() != nullptr; }

  // RenderFrameObserver implementation.
  void OnDestruct() final {}
};

}  // namespace

GuestViewInternalCustomBindings::GuestViewInternalCustomBindings(
    ScriptContext* context)
    : ObjectBackedNativeHandler(context) {
  RouteFunction("AttachGuest",
                base::Bind(&GuestViewInternalCustomBindings::AttachGuest,
                           base::Unretained(this)));
  RouteFunction("DetachGuest",
                base::Bind(&GuestViewInternalCustomBindings::DetachGuest,
                           base::Unretained(this)));
  RouteFunction("AttachIframeGuest",
                base::Bind(&GuestViewInternalCustomBindings::AttachIframeGuest,
                           base::Unretained(this)));
  RouteFunction("DestroyContainer",
                base::Bind(&GuestViewInternalCustomBindings::DestroyContainer,
                           base::Unretained(this)));
  RouteFunction("GetContentWindow",
                base::Bind(&GuestViewInternalCustomBindings::GetContentWindow,
                           base::Unretained(this)));
  RouteFunction("GetViewFromID",
                base::Bind(&GuestViewInternalCustomBindings::GetViewFromID,
                           base::Unretained(this)));
  RouteFunction(
      "RegisterDestructionCallback",
      base::Bind(&GuestViewInternalCustomBindings::RegisterDestructionCallback,
                 base::Unretained(this)));
  RouteFunction(
      "RegisterElementResizeCallback",
      base::Bind(
          &GuestViewInternalCustomBindings::RegisterElementResizeCallback,
          base::Unretained(this)));
  RouteFunction("RegisterView",
                base::Bind(&GuestViewInternalCustomBindings::RegisterView,
                           base::Unretained(this)));
  RouteFunction(
      "RunWithGesture",
      base::Bind(&GuestViewInternalCustomBindings::RunWithGesture,
                 base::Unretained(this)));
}

GuestViewInternalCustomBindings::~GuestViewInternalCustomBindings() {}

// static
void GuestViewInternalCustomBindings::ResetMapEntry(
    const v8::WeakCallbackInfo<int>& data) {
  int* param = data.GetParameter();
  int view_instance_id = *param;
  delete param;
  ViewMap& view_map = weak_view_map.Get();
  auto entry = view_map.find(view_instance_id);
  if (entry == view_map.end())
    return;

  // V8 says we need to explicitly reset weak handles from their callbacks.
  // It is not implicit as one might expect.
  entry->second->Reset();
  delete entry->second;
  view_map.erase(entry);

  // Let the GuestViewManager know that a GuestView has been garbage collected.
  content::RenderThread::Get()->Send(
      new GuestViewHostMsg_ViewGarbageCollected(view_instance_id));
}

void GuestViewInternalCustomBindings::AttachGuest(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // Allow for an optional callback parameter.
  CHECK(args.Length() >= 3 && args.Length() <= 4);
  // Element Instance ID.
  CHECK(args[0]->IsInt32());
  // Guest Instance ID.
  CHECK(args[1]->IsInt32());
  // Attach Parameters.
  CHECK(args[2]->IsObject());
  // Optional Callback Function.
  CHECK(args.Length() < 4 || args[3]->IsFunction());

  int element_instance_id = args[0]->Int32Value();
  // An element instance ID uniquely identifies a GuestViewContainer.
  auto* guest_view_container =
      guest_view::GuestViewContainer::FromID(element_instance_id);

  // TODO(fsamuel): Should we be reporting an error if the element instance ID
  // is invalid?
  if (!guest_view_container)
    return;
  // Retain a weak pointer so we can easily test if the container goes away.
  auto weak_ptr = guest_view_container->GetWeakPtr();

  int guest_instance_id = args[1]->Int32Value();

  std::unique_ptr<base::DictionaryValue> params;
  {
    std::unique_ptr<V8ValueConverter> converter(V8ValueConverter::create());
    std::unique_ptr<base::Value> params_as_value(
        converter->FromV8Value(args[2], context()->v8_context()));
    params = base::DictionaryValue::From(std::move(params_as_value));
    CHECK(params);
  }
  // We should be careful that some malicious JS in the GuestView's embedder
  // hasn't destroyed |guest_view_container| during the enumeration of the
  // properties of the guest's object during extraction of |params| above
  // (see https://crbug.com/683523).
  if (!weak_ptr)
    return;

  // Add flag to |params| to indicate that the element size is specified in
  // logical units.
  params->SetBoolean(guest_view::kElementSizeIsLogical, true);

  std::unique_ptr<guest_view::GuestViewRequest> request(
      new guest_view::GuestViewAttachRequest(
          guest_view_container, guest_instance_id, std::move(params),
          args.Length() == 4 ? args[3].As<v8::Function>()
                             : v8::Local<v8::Function>(),
          args.GetIsolate()));
  guest_view_container->IssueRequest(std::move(request));

  args.GetReturnValue().Set(v8::Boolean::New(context()->isolate(), true));
}

void GuestViewInternalCustomBindings::DetachGuest(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // Allow for an optional callback parameter.
  CHECK(args.Length() >= 1 && args.Length() <= 2);
  // Element Instance ID.
  CHECK(args[0]->IsInt32());
  // Optional Callback Function.
  CHECK(args.Length() < 2 || args[1]->IsFunction());

  int element_instance_id = args[0]->Int32Value();
  // An element instance ID uniquely identifies a GuestViewContainer.
  auto* guest_view_container =
      guest_view::GuestViewContainer::FromID(element_instance_id);

  // TODO(fsamuel): Should we be reporting an error if the element instance ID
  // is invalid?
  if (!guest_view_container)
    return;

  std::unique_ptr<guest_view::GuestViewRequest> request(
      new guest_view::GuestViewDetachRequest(
          guest_view_container, args.Length() == 2 ? args[1].As<v8::Function>()
                                                   : v8::Local<v8::Function>(),
          args.GetIsolate()));
  guest_view_container->IssueRequest(std::move(request));

  args.GetReturnValue().Set(v8::Boolean::New(context()->isolate(), true));
}

void GuestViewInternalCustomBindings::AttachIframeGuest(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // Allow for an optional callback parameter.
  const int num_required_params = 4;
  CHECK(args.Length() >= num_required_params &&
        args.Length() <= (num_required_params + 1));
  // Element Instance ID.
  CHECK(args[0]->IsInt32());
  // Guest Instance ID.
  CHECK(args[1]->IsInt32());
  // Attach Parameters.
  CHECK(args[2]->IsObject());
  // <iframe>.contentWindow.
  CHECK(args[3]->IsObject());
  // Optional Callback Function.
  CHECK(args.Length() <= num_required_params ||
        args[num_required_params]->IsFunction());

  int element_instance_id = args[0]->Int32Value();
  int guest_instance_id = args[1]->Int32Value();

  // Get the WebLocalFrame before (possibly) executing any user-space JS while
  // getting the |params|. We track the status of the RenderFrame via an
  // observer in case it is deleted during user code execution.
  content::RenderFrame* render_frame = GetRenderFrame(args[3]);
  RenderFrameStatus render_frame_status(render_frame);

  std::unique_ptr<base::DictionaryValue> params;
  {
    std::unique_ptr<V8ValueConverter> converter(V8ValueConverter::create());
    std::unique_ptr<base::Value> params_as_value(
        converter->FromV8Value(args[2], context()->v8_context()));
    params = base::DictionaryValue::From(std::move(params_as_value));
    CHECK(params);
  }
  if (!render_frame_status.is_ok())
    return;

  blink::WebLocalFrame* frame = render_frame->GetWebFrame();
  // Parent must exist.
  blink::WebFrame* parent_frame = frame->parent();
  DCHECK(parent_frame);
  DCHECK(parent_frame->isWebLocalFrame());

  // Add flag to |params| to indicate that the element size is specified in
  // logical units.
  params->SetBoolean(guest_view::kElementSizeIsLogical, true);

  content::RenderFrame* embedder_parent_frame =
      content::RenderFrame::FromWebFrame(parent_frame);

  // Create a GuestViewContainer if it does not exist.
  // An element instance ID uniquely identifies an IframeGuestViewContainer
  // within a RenderView.
  auto* guest_view_container =
      guest_view::GuestViewContainer::FromID(element_instance_id);
  // This is the first time we hear about the |element_instance_id|.
  DCHECK(!guest_view_container);
  // The <webview> element's GC takes ownership of |guest_view_container|.
  guest_view_container =
      new guest_view::IframeGuestViewContainer(embedder_parent_frame);
  guest_view_container->SetElementInstanceID(element_instance_id);

  std::unique_ptr<guest_view::GuestViewRequest> request(
      new guest_view::GuestViewAttachIframeRequest(
          guest_view_container, render_frame->GetRoutingID(), guest_instance_id,
          std::move(params), args.Length() == (num_required_params + 1)
                                 ? args[num_required_params].As<v8::Function>()
                                 : v8::Local<v8::Function>(),
          args.GetIsolate()));
  guest_view_container->IssueRequest(std::move(request));

  args.GetReturnValue().Set(v8::Boolean::New(context()->isolate(), true));
}

void GuestViewInternalCustomBindings::DestroyContainer(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().SetNull();

  if (args.Length() != 1)
    return;

  // Element Instance ID.
  if (!args[0]->IsInt32())
    return;

  int element_instance_id = args[0]->Int32Value();
  auto* guest_view_container =
      guest_view::GuestViewContainer::FromID(element_instance_id);
  if (!guest_view_container)
    return;

  // Note: |guest_view_container| is deleted.
  // GuestViewContainer::DidDestroyElement() currently also destroys
  // a GuestViewContainer. That won't be necessary once GuestViewContainer
  // always runs w/o plugin.
  guest_view_container->Destroy(false /* embedder_frame_destroyed */);
}

void GuestViewInternalCustomBindings::GetContentWindow(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // Default to returning null.
  args.GetReturnValue().SetNull();

  if (args.Length() != 1)
    return;

  // The routing ID for the RenderView.
  if (!args[0]->IsInt32())
    return;

  int view_id = args[0]->Int32Value();
  if (view_id == MSG_ROUTING_NONE)
    return;

  content::RenderView* view = content::RenderView::FromRoutingID(view_id);
  if (!view)
    return;

  blink::WebFrame* frame = view->GetWebView()->mainFrame();
  // TODO(lazyboy,nasko): The WebLocalFrame branch is not used when running
  // on top of out-of-process iframes. Remove it once the code is converted.
  v8::Local<v8::Value> window;
  if (frame->isWebLocalFrame()) {
    window = frame->mainWorldScriptContext()->Global();
  } else {
    window =
        frame->toWebRemoteFrame()->deprecatedMainWorldScriptContext()->Global();
  }
  args.GetReturnValue().Set(window);
}

void GuestViewInternalCustomBindings::GetViewFromID(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // Default to returning null.
  args.GetReturnValue().SetNull();
  // There is one argument.
  CHECK(args.Length() == 1);
  // The view ID.
  CHECK(args[0]->IsInt32());
  int view_id = args[0]->Int32Value();

  ViewMap& view_map = weak_view_map.Get();
  auto map_entry = view_map.find(view_id);
  if (map_entry == view_map.end())
    return;

  auto return_object = v8::Handle<v8::Object>::New(args.GetIsolate(),
                                                   *map_entry->second);
  args.GetReturnValue().Set(return_object);
}

void GuestViewInternalCustomBindings::RegisterDestructionCallback(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // There are two parameters.
  CHECK(args.Length() == 2);
  // Element Instance ID.
  CHECK(args[0]->IsInt32());
  // Callback function.
  CHECK(args[1]->IsFunction());

  int element_instance_id = args[0]->Int32Value();
  // An element instance ID uniquely identifies a GuestViewContainer within a
  // RenderView.
  auto* guest_view_container =
      guest_view::GuestViewContainer::FromID(element_instance_id);
  if (!guest_view_container)
    return;

  guest_view_container->RegisterDestructionCallback(args[1].As<v8::Function>(),
                                                    args.GetIsolate());

  args.GetReturnValue().Set(v8::Boolean::New(context()->isolate(), true));
}

void GuestViewInternalCustomBindings::RegisterElementResizeCallback(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // There are two parameters.
  CHECK(args.Length() == 2);
  // Element Instance ID.
  CHECK(args[0]->IsInt32());
  // Callback function.
  CHECK(args[1]->IsFunction());

  int element_instance_id = args[0]->Int32Value();
  // An element instance ID uniquely identifies a ExtensionsGuestViewContainer
  // within a RenderView.
  auto* guest_view_container =
      guest_view::GuestViewContainer::FromID(element_instance_id);
  if (!guest_view_container)
    return;

  guest_view_container->RegisterElementResizeCallback(
      args[1].As<v8::Function>(), args.GetIsolate());

  args.GetReturnValue().Set(v8::Boolean::New(context()->isolate(), true));
}

void GuestViewInternalCustomBindings::RegisterView(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // There are three parameters.
  CHECK(args.Length() == 3);
  // View Instance ID.
  CHECK(args[0]->IsInt32());
  // View element.
  CHECK(args[1]->IsObject());
  // View type (e.g. "webview").
  CHECK(args[2]->IsString());

  // A reference to the view object is stored in |weak_view_map| using its view
  // ID as the key. The reference is made weak so that it will not extend the
  // lifetime of the object.
  int view_instance_id = args[0]->Int32Value();
  auto* object =
      new v8::Global<v8::Object>(args.GetIsolate(), args[1].As<v8::Object>());
  weak_view_map.Get().insert(std::make_pair(view_instance_id, object));

  // The |view_instance_id| is given to the SetWeak callback so that that view's
  // entry in |weak_view_map| can be cleared when the view object is garbage
  // collected.
  object->SetWeak(new int(view_instance_id),
                  &GuestViewInternalCustomBindings::ResetMapEntry,
                  v8::WeakCallbackType::kParameter);

  // Let the GuestViewManager know that a GuestView has been created.
  const std::string& view_type = *v8::String::Utf8Value(args[2]);
  content::RenderThread::Get()->Send(
      new GuestViewHostMsg_ViewCreated(view_instance_id, view_type));
}

void GuestViewInternalCustomBindings::RunWithGesture(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  // Gesture is required to request fullscreen.
  // TODO(devlin): All this needs to do is enter fullscreen. We should make this
  // EnterFullscreen() and do it directly rather than having a generic "run with
  // user gesture" function.
  blink::WebScopedUserGesture user_gesture(context()->web_frame());
  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsFunction());
  context()->SafeCallFunction(
      v8::Local<v8::Function>::Cast(args[0]), 0, nullptr);
}

}  // namespace extensions
