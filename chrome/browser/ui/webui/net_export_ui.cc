// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/net_export_ui.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/scoped_observer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/net_export_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/url_constants.h"
#include "components/grit/components_resources.h"
#include "components/net_log/chrome_net_log.h"
#include "components/net_log/net_export_ui_constants.h"
#include "components/net_log/net_log_file_writer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "extensions/features/features.h"
#include "net/log/net_log_capture_mode.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/shell_dialogs/select_file_dialog.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/intent_helper.h"
#endif

using content::BrowserThread;
using content::WebContents;
using content::WebUIMessageHandler;

namespace {

class ProxyScriptFetcherContextGetter : public net::URLRequestContextGetter {
 public:
  explicit ProxyScriptFetcherContextGetter(IOThread* io_thread)
      : io_thread_(io_thread) {}

  net::URLRequestContext* GetURLRequestContext() override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(io_thread_->globals()->proxy_script_fetcher_context.get());
    return io_thread_->globals()->proxy_script_fetcher_context.get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetNetworkTaskRunner()
      const override {
    return BrowserThread::GetTaskRunnerForThread(BrowserThread::IO);
  }

 protected:
  ~ProxyScriptFetcherContextGetter() override {}

 private:
  IOThread* const io_thread_;  // Owned by BrowserProcess.
};

// May only be accessed on the UI thread
base::LazyInstance<base::FilePath>::Leaky
    last_save_dir = LAZY_INSTANCE_INITIALIZER;

content::WebUIDataSource* CreateNetExportHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUINetExportHost);

  source->SetJsonPath("strings.js");
  source->AddResourcePath(net_log::kNetExportUIJS, IDR_NET_LOG_NET_EXPORT_JS);
  source->SetDefaultResource(IDR_NET_LOG_NET_EXPORT_HTML);
  return source;
}

void SetIfNotNull(base::DictionaryValue* dict,
                  const base::StringPiece& path,
                  std::unique_ptr<base::Value> in_value) {
  if (in_value) {
    dict->Set(path, std::move(in_value));
  }
}

// This class receives javascript messages from the renderer.
// Note that the WebUI infrastructure runs on the UI thread, therefore all of
// this class's public methods are expected to run on the UI thread.
class NetExportMessageHandler
    : public WebUIMessageHandler,
      public base::SupportsWeakPtr<NetExportMessageHandler>,
      public ui::SelectFileDialog::Listener,
      public net_log::NetLogFileWriter::StateObserver {
 public:
  NetExportMessageHandler();
  ~NetExportMessageHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Messages
  void OnEnableNotifyUIWithState(const base::ListValue* list);
  void OnStartNetLog(const base::ListValue* list);
  void OnStopNetLog(const base::ListValue* list);
  void OnSendNetLog(const base::ListValue* list);

  // ui::SelectFileDialog::Listener implementation.
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void FileSelectionCanceled(void* params) override;

  // net_log::NetLogFileWriter::StateObserver implementation.
  void OnNewState(const base::DictionaryValue& state) override;

 private:
  using URLRequestContextGetterList =
      std::vector<scoped_refptr<net::URLRequestContextGetter>>;

  // Send NetLog data via email.
  static void SendEmail(const base::FilePath& file_to_send);

  // chrome://net-export can be used on both mobile and desktop platforms.
  // On mobile a user cannot pick where their NetLog file is saved to.
  // Instead, everything is saved on the user's temp directory. Thus the
  // mobile user has the UI available to send their NetLog file as an
  // email while the desktop user, who gets to choose their NetLog file's
  // location, does not. Furthermore, since every time a user starts logging
  // to a new NetLog file on mobile platforms it overwrites the previous
  // NetLog file, a warning message appears on the Start Logging button
  // that informs the user of this. This does not exist on the desktop
  // UI.
  static bool UsingMobileUI();

  // Calls NetExportView.onExportNetLogInfoChanged JavaScript function in the
  // renderer, passing in |file_writer_state|.
  void NotifyUIWithState(std::unique_ptr<base::DictionaryValue> state);

  // Opens the SelectFileDialog UI with the default path to save a
  // NetLog file.
  void ShowSelectFileDialog(const base::FilePath& default_path);

  // Returns a list of context getters used to retrieve ongoing events when
  // logging starts so that net log entries can be added for those events.
  URLRequestContextGetterList GetURLRequestContexts() const;

  // Cache of g_browser_process->net_log()->net_log_file_writer(). This
  // is owned by ChromeNetLog which is owned by BrowserProcessImpl.
  net_log::NetLogFileWriter* file_writer_;

  ScopedObserver<net_log::NetLogFileWriter,
                 net_log::NetLogFileWriter::StateObserver>
      state_observer_manager_;

  // The capture mode the user chose in the UI when logging started is cached
  // here and is read after a file path is chosen in the save dialog.
  // Its value is only valid while the save dialog is open on the desktop UI.
  net::NetLogCaptureMode capture_mode_;

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  base::WeakPtrFactory<NetExportMessageHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetExportMessageHandler);
};

NetExportMessageHandler::NetExportMessageHandler()
    : file_writer_(g_browser_process->net_log()->net_log_file_writer()),
      state_observer_manager_(this),
      weak_ptr_factory_(this) {
  file_writer_->Initialize(
      BrowserThread::GetTaskRunnerForThread(BrowserThread::FILE_USER_BLOCKING),
      BrowserThread::GetTaskRunnerForThread(BrowserThread::IO));
}

NetExportMessageHandler::~NetExportMessageHandler() {
  // There may be a pending file dialog, it needs to be told that the user
  // has gone away so that it doesn't try to call back.
  if (select_file_dialog_)
    select_file_dialog_->ListenerDestroyed();

  file_writer_->StopNetLog(nullptr, nullptr);
}

void NetExportMessageHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  web_ui()->RegisterMessageCallback(
      net_log::kEnableNotifyUIWithStateHandler,
      base::Bind(&NetExportMessageHandler::OnEnableNotifyUIWithState,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      net_log::kStartNetLogHandler,
      base::Bind(&NetExportMessageHandler::OnStartNetLog,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      net_log::kStopNetLogHandler,
      base::Bind(&NetExportMessageHandler::OnStopNetLog,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      net_log::kSendNetLogHandler,
      base::Bind(&NetExportMessageHandler::OnSendNetLog,
                 base::Unretained(this)));
}

// The net-export UI is not notified of state changes until this function runs.
// After this function, NotifyUIWithState() will be called on all |file_writer_|
// state changes.
void NetExportMessageHandler::OnEnableNotifyUIWithState(
    const base::ListValue* list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!state_observer_manager_.IsObservingSources()) {
    state_observer_manager_.Add(file_writer_);
  }
  NotifyUIWithState(file_writer_->GetState());
}

void NetExportMessageHandler::OnStartNetLog(const base::ListValue* list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string capture_mode_string;
  bool result = list->GetString(0, &capture_mode_string);
  DCHECK(result);

  capture_mode_ =
      net_log::NetLogFileWriter::CaptureModeFromString(capture_mode_string);

  if (UsingMobileUI()) {
    file_writer_->StartNetLog(base::FilePath(), capture_mode_,
                              GetURLRequestContexts());
  } else {
    base::FilePath initial_dir = last_save_dir.Pointer()->empty() ?
        DownloadPrefs::FromBrowserContext(
            web_ui()->GetWebContents()->GetBrowserContext())->DownloadPath() :
        *last_save_dir.Pointer();
    base::FilePath initial_path =
        initial_dir.Append(FILE_PATH_LITERAL("chrome-net-export-log.json"));
    ShowSelectFileDialog(initial_path);
  }
}

void NetExportMessageHandler::OnStopNetLog(const base::ListValue* list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::unique_ptr<base::DictionaryValue> ui_thread_polled_data(
      new base::DictionaryValue());

  Profile* profile = Profile::FromWebUI(web_ui());
  SetIfNotNull(ui_thread_polled_data.get(), "dataReductionProxyInfo",
               chrome_browser_net::GetDataReductionProxyInfo(profile));
  SetIfNotNull(ui_thread_polled_data.get(), "historicNetworkStats",
               chrome_browser_net::GetHistoricNetworkStats(profile));
  SetIfNotNull(ui_thread_polled_data.get(), "prerenderInfo",
               chrome_browser_net::GetPrerenderInfo(profile));
  SetIfNotNull(ui_thread_polled_data.get(), "sessionNetworkStats",
               chrome_browser_net::GetSessionNetworkStats(profile));
  SetIfNotNull(ui_thread_polled_data.get(), "extensionInfo",
               chrome_browser_net::GetExtensionInfo(profile));
#if defined(OS_WIN)
  SetIfNotNull(ui_thread_polled_data.get(), "serviceProviders",
               chrome_browser_net::GetWindowsServiceProviders());
#endif

  file_writer_->StopNetLog(std::move(ui_thread_polled_data),
                           Profile::FromWebUI(web_ui())->GetRequestContext());
}

void NetExportMessageHandler::OnSendNetLog(const base::ListValue* list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  file_writer_->GetFilePathToCompletedLog(
      base::Bind(&NetExportMessageHandler::SendEmail));
}

void NetExportMessageHandler::FileSelected(const base::FilePath& path,
                                           int index,
                                           void* params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(select_file_dialog_);
  select_file_dialog_ = nullptr;
  *last_save_dir.Pointer() = path.DirName();

  file_writer_->StartNetLog(path, capture_mode_, GetURLRequestContexts());
}

void NetExportMessageHandler::FileSelectionCanceled(void* params) {
  DCHECK(select_file_dialog_);
  select_file_dialog_ = nullptr;
}

void NetExportMessageHandler::OnNewState(const base::DictionaryValue& state) {
  NotifyUIWithState(state.CreateDeepCopy());
}

// static
void NetExportMessageHandler::SendEmail(const base::FilePath& file_to_send) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if defined(OS_ANDROID)
  if (file_to_send.empty())
    return;
  std::string email;
  std::string subject = "net_internals_log";
  std::string title = "Issue number: ";
  std::string body =
      "Please add some informative text about the network issues.";
  base::FilePath::StringType file_to_attach(file_to_send.value());
  chrome::android::SendEmail(
      base::UTF8ToUTF16(email), base::UTF8ToUTF16(subject),
      base::UTF8ToUTF16(body), base::UTF8ToUTF16(title),
      base::UTF8ToUTF16(file_to_attach));
#endif
}

// static
bool NetExportMessageHandler::UsingMobileUI() {
#if defined(OS_ANDROID) || defined(OS_IOS)
  return true;
#else
  return false;
#endif
}

void NetExportMessageHandler::NotifyUIWithState(
    std::unique_ptr<base::DictionaryValue> state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(web_ui());
  state->SetBoolean("useMobileUI", UsingMobileUI());
  web_ui()->CallJavascriptFunctionUnsafe(net_log::kOnExportNetLogInfoChanged,
                                         *state);
}

void NetExportMessageHandler::ShowSelectFileDialog(
    const base::FilePath& default_path) {
  // User may have clicked more than once before the save dialog appears.
  // This prevents creating more than one save dialog.
  if (select_file_dialog_)
    return;

  WebContents* webcontents = web_ui()->GetWebContents();

  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, new ChromeSelectFilePolicy(webcontents));
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions = {{FILE_PATH_LITERAL("json")}};
  gfx::NativeWindow owning_window = webcontents->GetTopLevelNativeWindow();
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE, base::string16(), default_path,
      &file_type_info, 0, base::FilePath::StringType(), owning_window, nullptr);
}

NetExportMessageHandler::URLRequestContextGetterList
NetExportMessageHandler::GetURLRequestContexts() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  URLRequestContextGetterList context_getters;

  Profile* profile = Profile::FromWebUI(web_ui());

  context_getters.push_back(profile->GetRequestContext());
  context_getters.push_back(
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetMediaURLRequestContext());
#if BUILDFLAG(ENABLE_EXTENSIONS)
  context_getters.push_back(profile->GetRequestContextForExtensions());
#endif
  context_getters.push_back(
      g_browser_process->io_thread()->system_url_request_context_getter());
  context_getters.push_back(
      new ProxyScriptFetcherContextGetter(g_browser_process->io_thread()));

  return context_getters;
}

}  // namespace

NetExportUI::NetExportUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(base::MakeUnique<NetExportMessageHandler>());

  // Set up the chrome://net-export/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateNetExportHTMLSource());
}
