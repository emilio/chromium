// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_AGGREGATOR_H_
#define CC_SURFACES_SURFACE_AGGREGATOR_H_

#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "cc/quads/draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/resources/transferable_resource.h"
#include "cc/surfaces/surface_id.h"
#include "cc/surfaces/surfaces_export.h"
#include "ui/gfx/color_space.h"

namespace cc {

class CompositorFrame;
class ResourceProvider;
class Surface;
class SurfaceDrawQuad;
class SurfaceManager;

class CC_SURFACES_EXPORT SurfaceAggregator {
 public:
  using SurfaceIndexMap = std::unordered_map<SurfaceId, int, SurfaceIdHash>;

  SurfaceAggregator(SurfaceManager* manager,
                    ResourceProvider* provider,
                    bool aggregate_only_damaged);
  ~SurfaceAggregator();

  CompositorFrame Aggregate(const SurfaceId& surface_id);
  void ReleaseResources(const SurfaceId& surface_id);
  SurfaceIndexMap& previous_contained_surfaces() {
    return previous_contained_surfaces_;
  }
  void SetFullDamageForSurface(const SurfaceId& surface_id);
  void set_output_is_secure(bool secure) { output_is_secure_ = secure; }

  // Set the color spaces for the created RenderPasses, which is propagated
  // to the output surface.
  void SetOutputColorSpace(const gfx::ColorSpace& blending_color_space,
                           const gfx::ColorSpace& output_color_space);

 private:
  struct ClipData {
    ClipData() : is_clipped(false) {}
    ClipData(bool is_clipped, const gfx::Rect& rect)
        : is_clipped(is_clipped), rect(rect) {}

    bool is_clipped;
    gfx::Rect rect;
  };

  struct PrewalkResult {
    PrewalkResult();
    ~PrewalkResult();
    // This is the set of Surfaces that were referenced by another Surface, but
    // not included in a SurfaceDrawQuad.
    std::set<SurfaceId> undrawn_surfaces;
    bool may_contain_video = false;
  };

  struct RenderPassInfo {
    // This is the id the pass is mapped to.
    int id;
    // This is true if the pass was used in the last aggregated frame.
    bool in_use = true;
  };

  ClipData CalculateClipRect(const ClipData& surface_clip,
                             const ClipData& quad_clip,
                             const gfx::Transform& target_transform);

  int RemapPassId(int surface_local_pass_id, const SurfaceId& surface_id);

  void HandleSurfaceQuad(const SurfaceDrawQuad* surface_quad,
                         const gfx::Transform& target_transform,
                         const ClipData& clip_rect,
                         RenderPass* dest_pass,
                         bool ignore_undamaged,
                         gfx::Rect* damage_rect_in_quad_space,
                         bool* damage_rect_in_quad_space_valid);

  SharedQuadState* CopySharedQuadState(const SharedQuadState* source_sqs,
                                       const gfx::Transform& target_transform,
                                       const ClipData& clip_rect,
                                       RenderPass* dest_render_pass);
  void CopyQuadsToPass(
      const QuadList& source_quad_list,
      const SharedQuadStateList& source_shared_quad_state_list,
      const std::unordered_map<ResourceId, ResourceId>& resource_to_child_map,
      const gfx::Transform& target_transform,
      const ClipData& clip_rect,
      RenderPass* dest_pass,
      const SurfaceId& surface_id);
  gfx::Rect PrewalkTree(const SurfaceId& surface_id,
                        bool in_moved_pixel_surface,
                        int parent_pass,
                        PrewalkResult* result);
  void CopyUndrawnSurfaces(PrewalkResult* prewalk);
  void CopyPasses(const CompositorFrame& frame, Surface* surface);
  void AddColorConversionPass();

  // Remove Surfaces that were referenced before but aren't currently
  // referenced from the ResourceProvider.
  // Also notifies SurfaceAggregatorClient of newly added and removed
  // child surfaces.
  void ProcessAddedAndRemovedSurfaces();

  void PropagateCopyRequestPasses();

  int ChildIdForSurface(Surface* surface);
  gfx::Rect DamageRectForSurface(const Surface* surface,
                                 const RenderPass& source,
                                 const gfx::Rect& full_rect) const;

  SurfaceManager* manager_;
  ResourceProvider* provider_;

  // Every Surface has its own RenderPass ID namespace. This structure maps
  // each source (SurfaceId, RenderPass id) to a unified ID namespace that's
  // used in the aggregated frame. An entry is removed from the map if it's not
  // used for one output frame.
  using RenderPassIdAllocatorMap =
      std::map<std::pair<SurfaceId, int>, RenderPassInfo>;
  RenderPassIdAllocatorMap render_pass_allocator_map_;
  int next_render_pass_id_;
  const bool aggregate_only_damaged_;
  bool output_is_secure_;

  // The color space for the root render pass. If this is different from
  // |blending_color_space_|, then a final render pass to convert between
  // the two will be added.
  gfx::ColorSpace output_color_space_;
  // The color space in which blending is done, used for all non-root render
  // passes.
  gfx::ColorSpace blending_color_space_;
  // The id for the final color conversion render pass.
  int color_conversion_render_pass_id_ = 0;

  using SurfaceToResourceChildIdMap =
      std::unordered_map<SurfaceId, int, SurfaceIdHash>;
  SurfaceToResourceChildIdMap surface_id_to_resource_child_id_;

  // The following state is only valid for the duration of one Aggregate call
  // and is only stored on the class to avoid having to pass through every
  // function call.

  // This is the set of surfaces referenced in the aggregation so far, used to
  // detect cycles.
  using SurfaceSet = std::set<SurfaceId>;
  SurfaceSet referenced_surfaces_;

  // For each Surface used in the last aggregation, gives the frame_index at
  // that time.
  SurfaceIndexMap previous_contained_surfaces_;
  SurfaceIndexMap contained_surfaces_;

  // After surface validation, every Surface in this set is valid.
  std::unordered_set<SurfaceId, SurfaceIdHash> valid_surfaces_;

  // This is the pass list for the aggregated frame.
  RenderPassList* dest_pass_list_;

  // This is the set of aggregated pass ids that are affected by filters that
  // move pixels.
  std::unordered_set<int> moved_pixel_passes_;

  // This is the set of aggregated pass ids that are drawn by copy requests, so
  // should not have their damage rects clipped to the root damage rect.
  std::unordered_set<int> copy_request_passes_;

  // This maps each aggregated pass id to the set of (aggregated) pass ids
  // that its RenderPassDrawQuads depend on
  std::unordered_map<int, std::unordered_set<int>> render_pass_dependencies_;

  // The root damage rect of the currently-aggregating frame.
  gfx::Rect root_damage_rect_;

  // True if the frame that's currently being aggregated has copy requests.
  // This is valid during Aggregate after PrewalkTree is called.
  bool has_copy_requests_;

  // Resource list for the aggregated frame.
  TransferableResourceArray* dest_resource_list_;

  base::WeakPtrFactory<SurfaceAggregator> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceAggregator);
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_AGGREGATOR_H_
