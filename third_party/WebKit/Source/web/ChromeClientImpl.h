/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ChromeClientImpl_h
#define ChromeClientImpl_h

#include "core/page/ChromeClient.h"
#include "core/page/WindowFeatures.h"
#include "public/web/WebNavigationPolicy.h"
#include "web/WebExport.h"
#include <memory>

namespace blink {

class PagePopup;
class PagePopupClient;
class WebViewImpl;
struct WebCursorInfo;

// Handles window-level notifications from core on behalf of a WebView.
class WEB_EXPORT ChromeClientImpl final : public ChromeClient {
 public:
  static ChromeClientImpl* create(WebViewImpl*);
  ~ChromeClientImpl() override;

  void* webView() const override;

  // ChromeClient methods:
  void chromeDestroyed() override;
  void setWindowRect(const IntRect&, LocalFrame&) override;
  IntRect rootWindowRect() override;
  IntRect pageRect() override;
  void focus() override;
  bool canTakeFocus(WebFocusType) override;
  void takeFocus(WebFocusType) override;
  void focusedNodeChanged(Node* fromNode, Node* toNode) override;
  void beginLifecycleUpdates() override;
  bool hadFormInteraction() const override;
  void startDragging(LocalFrame*,
                     const WebDragData&,
                     WebDragOperationsMask,
                     const WebImage& dragImage,
                     const WebPoint& dragImageOffset) override;
  bool acceptsLoadDrops() const override;
  Page* createWindow(LocalFrame*,
                     const FrameLoadRequest&,
                     const WindowFeatures&,
                     NavigationPolicy) override;
  void show(NavigationPolicy) override;
  void didOverscroll(const FloatSize& overscrollDelta,
                     const FloatSize& accumulatedOverscroll,
                     const FloatPoint& positionInViewport,
                     const FloatSize& velocityInViewport) override;
  void setToolbarsVisible(bool) override;
  bool toolbarsVisible() override;
  void setStatusbarVisible(bool) override;
  bool statusbarVisible() override;
  void setScrollbarsVisible(bool) override;
  bool scrollbarsVisible() override;
  void setMenubarVisible(bool) override;
  bool menubarVisible() override;
  void setResizable(bool) override;
  bool shouldReportDetailedMessageForSource(LocalFrame&,
                                            const String&) override;
  void addMessageToConsole(LocalFrame*,
                           MessageSource,
                           MessageLevel,
                           const String& message,
                           unsigned lineNumber,
                           const String& sourceID,
                           const String& stackTrace) override;
  bool canOpenBeforeUnloadConfirmPanel() override;
  bool openBeforeUnloadConfirmPanelDelegate(LocalFrame*,
                                            bool isReload) override;
  void closeWindowSoon() override;
  bool openJavaScriptAlertDelegate(LocalFrame*, const String&) override;
  bool openJavaScriptConfirmDelegate(LocalFrame*, const String&) override;
  bool openJavaScriptPromptDelegate(LocalFrame*,
                                    const String& message,
                                    const String& defaultValue,
                                    String& result) override;
  void setStatusbarText(const String& message) override;
  bool tabsToLinks() override;
  void invalidateRect(const IntRect&) override;
  void scheduleAnimation(FrameViewBase*) override;
  IntRect viewportToScreen(const IntRect&, const FrameViewBase*) const override;
  float windowToViewportScalar(const float) const override;
  WebScreenInfo screenInfo() const override;
  WTF::Optional<IntRect> visibleContentRectForPainting() const override;
  void contentsSizeChanged(LocalFrame*, const IntSize&) const override;
  void pageScaleFactorChanged() const override;
  float clampPageScaleFactorToLimits(float scale) const override;
  void mainFrameScrollOffsetChanged() const override;
  void layoutUpdated(LocalFrame*) const override;
  void showMouseOverURL(const HitTestResult&) override;
  void setToolTip(LocalFrame&, const String&, TextDirection) override;
  void dispatchViewportPropertiesDidChange(
      const ViewportDescription&) const override;
  void printDelegate(LocalFrame*) override;
  void annotatedRegionsChanged() override;
  ColorChooser* openColorChooser(LocalFrame*,
                                 ColorChooserClient*,
                                 const Color&) override;
  DateTimeChooser* openDateTimeChooser(
      DateTimeChooserClient*,
      const DateTimeChooserParameters&) override;
  void openFileChooser(LocalFrame*, PassRefPtr<FileChooser>) override;
  void enumerateChosenDirectory(FileChooser*) override;
  void setCursor(const Cursor&, LocalFrame*) override;
  Cursor lastSetCursorForTesting() const override;
  // The client keeps track of which touch/mousewheel event types have handlers,
  // and if they do, whether the handlers are passive and/or blocking. This
  // allows the client to know which optimizations can be used for the
  // associated event classes.
  void setEventListenerProperties(LocalFrame*,
                                  WebEventListenerClass,
                                  WebEventListenerProperties) override;
  WebEventListenerProperties eventListenerProperties(
      LocalFrame*,
      WebEventListenerClass) const override;
  void updateTouchRectsForSubframeIfNecessary(LocalFrame*);
  // Informs client about the existence of handlers for scroll events so
  // appropriate scroll optimizations can be chosen.
  void setHasScrollEventHandlers(LocalFrame*, bool hasEventHandlers) override;
  void setTouchAction(LocalFrame*, TouchAction) override;

  void attachRootGraphicsLayer(GraphicsLayer*, LocalFrame* localRoot) override;

  void attachRootLayer(WebLayer*, LocalFrame* localRoot) override;

  void attachCompositorAnimationTimeline(CompositorAnimationTimeline*,
                                         LocalFrame*) override;
  void detachCompositorAnimationTimeline(CompositorAnimationTimeline*,
                                         LocalFrame*) override;

  void enterFullscreen(LocalFrame&) override;
  void exitFullscreen(LocalFrame&) override;
  void fullscreenElementChanged(Element*, Element*) override;

  void clearCompositedSelection(LocalFrame*) override;
  void updateCompositedSelection(LocalFrame*,
                                 const CompositedSelection&) override;

  // ChromeClient methods:
  void postAccessibilityNotification(AXObject*,
                                     AXObjectCache::AXNotification) override;
  String acceptLanguages() override;

  // ChromeClientImpl:
  void setCursorForPlugin(const WebCursorInfo&, LocalFrame*);
  void setNewWindowNavigationPolicy(WebNavigationPolicy);
  void setCursorOverridden(bool);

  bool hasOpenedPopup() const override;
  PopupMenu* openPopupMenu(LocalFrame&, HTMLSelectElement&) override;
  PagePopup* openPagePopup(PagePopupClient*);
  void closePagePopup(PagePopup*);
  DOMWindow* pagePopupWindowForTesting() const override;

  bool shouldOpenModalDialogDuringPageDismissal(
      LocalFrame&,
      DialogType,
      const String& dialogMessage,
      Document::PageDismissalType) const override;

  bool requestPointerLock(LocalFrame*) override;
  void requestPointerUnlock(LocalFrame*) override;

  // AutofillClient pass throughs:
  void didAssociateFormControlsAfterLoad(LocalFrame*) override;
  void handleKeyboardEventOnTextField(HTMLInputElement&,
                                      KeyboardEvent&) override;
  void didChangeValueInTextField(HTMLFormControlElement&) override;
  void didEndEditingOnTextField(HTMLInputElement&) override;
  void openTextDataListChooser(HTMLInputElement&) override;
  void textFieldDataListChanged(HTMLInputElement&) override;
  void ajaxSucceeded(LocalFrame*) override;

  void showVirtualKeyboardOnElementFocus(LocalFrame&) override;

  void registerViewportLayers() const override;

  void showUnhandledTapUIIfNeeded(IntPoint, Node*, bool) override;
  void onMouseDown(Node*) override;
  void didUpdateBrowserControls() const override;

  CompositorWorkerProxyClient* createCompositorWorkerProxyClient(
      LocalFrame*) override;
  AnimationWorkletProxyClient* createAnimationWorkletProxyClient(
      LocalFrame*) override;

  FloatSize elasticOverscroll() const override;

  void didObserveNonGetFetchFromScript() const override;

  std::unique_ptr<WebFrameScheduler> createFrameScheduler(
      BlameContext*) override;

  double lastFrameTimeMonotonic() const override;

  void notifyPopupOpeningObservers() const;

  void installSupplements(LocalFrame&) override;

  WebLayerTreeView* getWebLayerTreeView(LocalFrame*) override;

 private:
  explicit ChromeClientImpl(WebViewImpl*);

  bool isChromeClientImpl() const override { return true; }
  void registerPopupOpeningObserver(PopupOpeningObserver*) override;
  void unregisterPopupOpeningObserver(PopupOpeningObserver*) override;

  void setCursor(const WebCursorInfo&, LocalFrame*);

  WebViewImpl* m_webView;  // Weak pointer.
  WindowFeatures m_windowFeatures;
  Vector<PopupOpeningObserver*> m_popupOpeningObservers;
  Cursor m_lastSetMouseCursorForTesting;
  bool m_cursorOverridden;
  bool m_didRequestNonEmptyToolTip;
};

DEFINE_TYPE_CASTS(ChromeClientImpl,
                  ChromeClient,
                  client,
                  client->isChromeClientImpl(),
                  client.isChromeClientImpl());

}  // namespace blink

#endif
