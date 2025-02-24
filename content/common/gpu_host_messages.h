// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard here, but see below
// for a much smaller-than-usual include guard section.

#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/common/establish_channel_params.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/common/gpu_command_buffer_traits.h"
#include "gpu/ipc/common/gpu_param_traits.h"
#include "gpu/ipc/common/memory_stats.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_start.h"
#include "media/media_features.h"
#include "ui/events/ipc/latency_info_param_traits.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"
#include "url/gurl.h"
#include "url/ipc/url_param_traits.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START GpuMsgStart

IPC_ENUM_TRAITS_VALIDATE(gpu::GpuPreferences::VpxDecodeVendors,
                         ((value >= gpu::GpuPreferences::VPX_VENDOR_NONE) &&
                          (value <= gpu::GpuPreferences::VPX_VENDOR_ALL)))

IPC_STRUCT_BEGIN(GpuMsg_CreateGpuMemoryBuffer_Params)
  IPC_STRUCT_MEMBER(gfx::GpuMemoryBufferId, id)
  IPC_STRUCT_MEMBER(gfx::Size, size)
  IPC_STRUCT_MEMBER(gfx::BufferFormat, format)
  IPC_STRUCT_MEMBER(gfx::BufferUsage, usage)
  IPC_STRUCT_MEMBER(int32_t, client_id)
  IPC_STRUCT_MEMBER(gpu::SurfaceHandle, surface_handle)
IPC_STRUCT_END()

IPC_STRUCT_TRAITS_BEGIN(gpu::GpuPreferences)
  IPC_STRUCT_TRAITS_MEMBER(single_process)
  IPC_STRUCT_TRAITS_MEMBER(in_process_gpu)
  IPC_STRUCT_TRAITS_MEMBER(ui_prioritize_in_gpu_process)
  IPC_STRUCT_TRAITS_MEMBER(disable_accelerated_video_decode)
#if defined(OS_CHROMEOS)
  IPC_STRUCT_TRAITS_MEMBER(disable_vaapi_accelerated_video_encode)
#endif
#if BUILDFLAG(ENABLE_WEBRTC)
  IPC_STRUCT_TRAITS_MEMBER(disable_web_rtc_hw_encoding)
#endif
#if defined(OS_WIN)
  IPC_STRUCT_TRAITS_MEMBER(enable_accelerated_vpx_decode)
  IPC_STRUCT_TRAITS_MEMBER(enable_low_latency_dxva)
  IPC_STRUCT_TRAITS_MEMBER(enable_zero_copy_dxgi_video)
  IPC_STRUCT_TRAITS_MEMBER(enable_nv12_dxgi_video)
#endif
  IPC_STRUCT_TRAITS_MEMBER(compile_shader_always_succeeds)
  IPC_STRUCT_TRAITS_MEMBER(disable_gl_error_limit)
  IPC_STRUCT_TRAITS_MEMBER(disable_glsl_translator)
  IPC_STRUCT_TRAITS_MEMBER(disable_gpu_driver_bug_workarounds)
  IPC_STRUCT_TRAITS_MEMBER(disable_shader_name_hashing)
  IPC_STRUCT_TRAITS_MEMBER(enable_gpu_command_logging)
  IPC_STRUCT_TRAITS_MEMBER(enable_gpu_debugging)
  IPC_STRUCT_TRAITS_MEMBER(enable_gpu_service_logging_gpu)
  IPC_STRUCT_TRAITS_MEMBER(enable_gpu_driver_debug_logging)
  IPC_STRUCT_TRAITS_MEMBER(disable_gpu_program_cache)
  IPC_STRUCT_TRAITS_MEMBER(enforce_gl_minimums)
  IPC_STRUCT_TRAITS_MEMBER(force_gpu_mem_available)
  IPC_STRUCT_TRAITS_MEMBER(gpu_program_cache_size)
  IPC_STRUCT_TRAITS_MEMBER(disable_gpu_shader_disk_cache)
  IPC_STRUCT_TRAITS_MEMBER(enable_threaded_texture_mailboxes)
  IPC_STRUCT_TRAITS_MEMBER(gl_shader_interm_output)
  IPC_STRUCT_TRAITS_MEMBER(emulate_shader_precision)
  IPC_STRUCT_TRAITS_MEMBER(enable_gpu_service_logging)
  IPC_STRUCT_TRAITS_MEMBER(enable_gpu_service_tracing)
  IPC_STRUCT_TRAITS_MEMBER(enable_es3_apis)
  IPC_STRUCT_TRAITS_MEMBER(use_passthrough_cmd_decoder)
IPC_STRUCT_TRAITS_END()

//------------------------------------------------------------------------------
// GPU Messages
// These are messages from the browser to the GPU process.

// Tells the GPU process to shutdown itself.
IPC_MESSAGE_CONTROL0(GpuMsg_Finalize)

// Tells the GPU process to create a new gpu memory buffer.
IPC_MESSAGE_CONTROL1(GpuMsg_CreateGpuMemoryBuffer,
                     GpuMsg_CreateGpuMemoryBuffer_Params)

// Tells the GPU process to destroy buffer.
IPC_MESSAGE_CONTROL3(GpuMsg_DestroyGpuMemoryBuffer,
                     gfx::GpuMemoryBufferId, /* id */
                     int32_t,                /* client_id */
                     gpu::SyncToken /* sync_token */)

// Tells the GPU process to create a context for collecting graphics card
// information.
IPC_MESSAGE_CONTROL0(GpuMsg_CollectGraphicsInfo)

// Tells the GPU process that the browser has seen a GPU switch.
IPC_MESSAGE_CONTROL0(GpuMsg_GpuSwitched)

//------------------------------------------------------------------------------
// GPU Host Messages
// These are messages to the browser.

// Response from GPU to a GputMsg_Initialize message.
IPC_MESSAGE_CONTROL3(GpuHostMsg_Initialized,
                     bool /* result */,
                     ::gpu::GPUInfo /* gpu_info */,
                     ::gpu::GpuFeatureInfo /* gpu_feature_info */)

// Response from GPU to a GpuMsg_CreateGpuMemoryBuffer message.
IPC_MESSAGE_CONTROL1(GpuHostMsg_GpuMemoryBufferCreated,
                     gfx::GpuMemoryBufferHandle /* handle */)

// Response from GPU to a GpuMsg_CollectGraphicsInfo.
IPC_MESSAGE_CONTROL1(GpuHostMsg_GraphicsInfoCollected,
                     gpu::GPUInfo /* GPU logging stats */)

// Message from GPU to add a GPU log message to the about:gpu page.
IPC_MESSAGE_CONTROL3(GpuHostMsg_OnLogMessage,
                     int /*severity*/,
                     std::string /* header */,
                     std::string /* message */)

// Sent by the GPU process to indicate that a fields trial has been activated.
IPC_MESSAGE_CONTROL1(GpuHostMsg_FieldTrialActivated, std::string /* name */)
