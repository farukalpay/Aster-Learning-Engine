// Author: Faruk Alpay
// Do not remove this notice.

#include "frame_presenter.hpp"

#define DESKTOP_WINDOW_EXPOSE_NATIVE_COCOA
#include <DESKTOP_WINDOW/desktop_window3.h>
#include <DESKTOP_WINDOW/desktop_window3native.h>

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <vector>

namespace {

struct PendingState {
  bool vsync = true;
};

struct NativeFrameState {
  id<MTLTexture> texture = nil;
  id<MTLCommandQueue> queue = nil;
  int width = 0;
  int height = 0;
};

std::map<DESKTOP_WINDOWwindow *, PendingState> &pendingStates() {
  static std::map<DESKTOP_WINDOWwindow *, PendingState> states;
  return states;
}

NativeFrameState &nativeFrameState() {
  static NativeFrameState state;
  return state;
}

class MetalFramePresenter {
public:
  explicit MetalFramePresenter(DESKTOP_WINDOWwindow *window) {
    @autoreleasepool {
      view_ = desktop_windowGetCocoaView(window);
      if (view_ == nil) {
        return;
      }

      device_ = MTLCreateSystemDefaultDevice();
      if (device_ == nil) {
        return;
      }
      [device_ retain];

      queue_ = [device_ newCommandQueue];
      if (queue_ == nil) {
        return;
      }

      CAMetalLayer *metal_layer = [CAMetalLayer layer];
      metal_layer.device = device_;
      metal_layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
      metal_layer.framebufferOnly = YES;
      metal_layer.opaque = YES;

      [view_ setWantsLayer:YES];
      [view_ setLayer:metal_layer];
      layer_ = metal_layer;
      [layer_ retain];

      NSString *source =
          @"#include <metal_stdlib>\n"
           "using namespace metal;\n"
           "struct VSOut { float4 position [[position]]; float2 uv; };\n"
           "vertex VSOut vs_main(uint id [[vertex_id]]) {\n"
           "  float2 pos[3] = { float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0) };\n"
           "  float2 uv[3] = { float2(0.0, 1.0), float2(2.0, 1.0), float2(0.0, -1.0) };\n"
           "  VSOut out; out.position = float4(pos[id], 0.0, 1.0); out.uv = uv[id]; return out;\n"
           "}\n"
           "fragment float4 fs_main(VSOut in [[stage_in]],\n"
           "                        texture2d<float> scene [[texture(0)]],\n"
           "                        texture2d<float> overlay [[texture(1)]],\n"
           "                        constant uint &mode [[buffer(0)]]) {\n"
           "  constexpr sampler s(address::clamp_to_edge, filter::nearest);\n"
           "  float4 base = ((mode & 1u) != 0u) ? scene.sample(s, in.uv) : float4(0.0, 0.0, 0.0, "
           "1.0);\n"
           "  float4 over = ((mode & 2u) != 0u) ? overlay.sample(s, in.uv) : float4(0.0);\n"
           "  return float4(base.rgb * (1.0 - over.a) + over.rgb, 1.0);\n"
           "}\n";

      NSError *error = nil;
      library_ = [device_ newLibraryWithSource:source options:nil error:&error];
      if (library_ == nil) {
        return;
      }

      id<MTLFunction> vertex = [library_ newFunctionWithName:@"vs_main"];
      id<MTLFunction> fragment = [library_ newFunctionWithName:@"fs_main"];
      MTLRenderPipelineDescriptor *pipeline_desc = [[MTLRenderPipelineDescriptor alloc] init];
      pipeline_desc.vertexFunction = vertex;
      pipeline_desc.fragmentFunction = fragment;
      pipeline_desc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
      pipeline_ = [device_ newRenderPipelineStateWithDescriptor:pipeline_desc error:&error];
      [pipeline_desc release];
      [vertex release];
      [fragment release];
    }
  }

  ~MetalFramePresenter() {
    @autoreleasepool {
      [texture_ release];
      [pipeline_ release];
      [library_ release];
      [queue_ release];
      [layer_ release];
      [device_ release];
    }
  }

  void setVsync(const bool enabled) {
    vsync_ = enabled;
    @autoreleasepool {
      if (layer_ != nil && [layer_ respondsToSelector:@selector(setDisplaySyncEnabled:)]) {
        layer_.displaySyncEnabled = enabled ? YES : NO;
      }
    }
  }

  void present(const aster::SoftwareFrameBuffer &framebuffer) {
    NativeFrameState &native_state = nativeFrameState();
    const bool has_native_frame =
        native_state.texture != nil && native_state.width > 0 && native_state.height > 0;
    if ((!has_native_frame && framebuffer.empty()) || device_ == nil || queue_ == nil ||
        layer_ == nil || pipeline_ == nil) {
      return;
    }

    @autoreleasepool {
      const std::uint32_t width = static_cast<std::uint32_t>(
          has_native_frame ? native_state.width : std::max(framebuffer.width(), 1));
      const std::uint32_t height = static_cast<std::uint32_t>(
          has_native_frame ? native_state.height : std::max(framebuffer.height(), 1));
      const CGFloat scale = [[view_ window] backingScaleFactor] > 0.0
                                ? [[view_ window] backingScaleFactor]
                                : [[NSScreen mainScreen] backingScaleFactor];
      layer_.contentsScale = scale;
      layer_.drawableSize = CGSizeMake(width, height);
      if ([layer_ respondsToSelector:@selector(setDisplaySyncEnabled:)]) {
        layer_.displaySyncEnabled = vsync_ ? YES : NO;
      }

      if (texture_ == nil || texture_width_ != width || texture_height_ != height) {
        [texture_ release];
        MTLTextureDescriptor *texture_desc =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                               width:width
                                                              height:height
                                                           mipmapped:NO];
        texture_desc.usage = MTLTextureUsageShaderRead;
        texture_desc.storageMode = MTLStorageModeShared;
        texture_ = [device_ newTextureWithDescriptor:texture_desc];
        texture_width_ = width;
        texture_height_ = height;
      }
      if (texture_ == nil) {
        return;
      }

      const bool has_overlay = !framebuffer.empty() &&
                               static_cast<std::uint32_t>(framebuffer.width()) == width &&
                               static_cast<std::uint32_t>(framebuffer.height()) == height;
      if (has_overlay) {
        const std::span<const std::uint8_t> rgba = framebuffer.rgba8();
        MTLRegion region = MTLRegionMake2D(0, 0, width, height);
        [texture_ replaceRegion:region
                    mipmapLevel:0
                      withBytes:rgba.data()
                    bytesPerRow:static_cast<NSUInteger>(width) * 4u];
      }

      id<CAMetalDrawable> drawable = [layer_ nextDrawable];
      if (drawable == nil) {
        return;
      }

      const bool use_native_scene = has_native_frame &&
                                    static_cast<std::uint32_t>(native_state.width) == width &&
                                    static_cast<std::uint32_t>(native_state.height) == height;
      id<MTLCommandQueue> present_queue =
          use_native_scene && native_state.queue != nil ? native_state.queue : queue_;
      id<MTLCommandBuffer> command_buffer = [present_queue commandBuffer];
      MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
      pass.colorAttachments[0].texture = drawable.texture;
      pass.colorAttachments[0].loadAction = MTLLoadActionClear;
      pass.colorAttachments[0].storeAction = MTLStoreActionStore;
      pass.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);

      id<MTLRenderCommandEncoder> encoder =
          [command_buffer renderCommandEncoderWithDescriptor:pass];
      [encoder setRenderPipelineState:pipeline_];
      const std::uint32_t mode = (use_native_scene ? 1u : 0u) | (has_overlay ? 2u : 0u);
      [encoder setFragmentTexture:use_native_scene ? native_state.texture : texture_ atIndex:0];
      [encoder setFragmentTexture:texture_ atIndex:1];
      [encoder setFragmentBytes:&mode length:sizeof(mode) atIndex:0];
      [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
      [encoder endEncoding];

      [command_buffer presentDrawable:drawable];
      [command_buffer commit];
    }
  }

private:
  NSView *view_ = nil;
  CAMetalLayer *layer_ = nil;
  id<MTLDevice> device_ = nil;
  id<MTLCommandQueue> queue_ = nil;
  id<MTLLibrary> library_ = nil;
  id<MTLRenderPipelineState> pipeline_ = nil;
  id<MTLTexture> texture_ = nil;
  std::uint32_t texture_width_ = 0;
  std::uint32_t texture_height_ = 0;
  bool vsync_ = true;
};

std::map<DESKTOP_WINDOWwindow *, std::unique_ptr<MetalFramePresenter>> &presenters() {
  static std::map<DESKTOP_WINDOWwindow *, std::unique_ptr<MetalFramePresenter>> instances;
  return instances;
}

MetalFramePresenter &presenterFor(DESKTOP_WINDOWwindow *window) {
  auto &all = presenters();
  auto it = all.find(window);
  if (it == all.end()) {
    auto presenter = std::make_unique<MetalFramePresenter>(window);
    const auto pending = pendingStates().find(window);
    if (pending != pendingStates().end()) {
      presenter->setVsync(pending->second.vsync);
    }
    it = all.emplace(window, std::move(presenter)).first;
  }
  return *it->second;
}

} // namespace

namespace aster {

void setFramePresentationVsync(DESKTOP_WINDOWwindow *window, const bool enabled) {
  if (window == nullptr) {
    return;
  }
  pendingStates()[window].vsync = enabled;
  const auto it = presenters().find(window);
  if (it != presenters().end()) {
    it->second->setVsync(enabled);
  }
}

void publishNativeFrameTexture(void *texture, void *queue, const int width, const int height) {
  NativeFrameState &state = nativeFrameState();
  id<MTLTexture> metal_texture = (id<MTLTexture>)texture;
  id<MTLCommandQueue> metal_queue = (id<MTLCommandQueue>)queue;
  if (metal_texture != state.texture) {
    [metal_texture retain];
    [state.texture release];
    state.texture = metal_texture;
  }
  if (metal_queue != state.queue) {
    [metal_queue retain];
    [state.queue release];
    state.queue = metal_queue;
  }
  state.width = width;
  state.height = height;
}

bool resolveNativeFrameTexture(SoftwareFrameBuffer &framebuffer) {
  NativeFrameState &state = nativeFrameState();
  if (state.texture == nil || state.width <= 0 || state.height <= 0) {
    return false;
  }

  @autoreleasepool {
    const std::size_t pixel_count =
        static_cast<std::size_t>(state.width) * static_cast<std::size_t>(state.height);
    std::vector<std::uint8_t> rgba(pixel_count * 4u);
    id<MTLDevice> device = [state.texture device];
    id<MTLCommandQueue> queue = state.queue;
    bool owns_queue = false;
    if (queue == nil && device != nil) {
      queue = [device newCommandQueue];
      owns_queue = true;
    }
    if (device == nil || queue == nil) {
      if (owns_queue) {
        [queue release];
      }
      return false;
    }

    MTLTextureDescriptor *readback_desc = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                     width:static_cast<NSUInteger>(state.width)
                                    height:static_cast<NSUInteger>(state.height)
                                 mipmapped:NO];
    readback_desc.storageMode = MTLStorageModeShared;
    id<MTLTexture> readback_texture = [device newTextureWithDescriptor:readback_desc];
    if (readback_texture == nil) {
      if (owns_queue) {
        [queue release];
      }
      return false;
    }

    id<MTLCommandBuffer> command_buffer = [queue commandBuffer];
    id<MTLBlitCommandEncoder> blit = [command_buffer blitCommandEncoder];
    [blit copyFromTexture:state.texture
              sourceSlice:0
              sourceLevel:0
             sourceOrigin:MTLOriginMake(0, 0, 0)
               sourceSize:MTLSizeMake(static_cast<NSUInteger>(state.width),
                                      static_cast<NSUInteger>(state.height), 1)
                toTexture:readback_texture
         destinationSlice:0
         destinationLevel:0
        destinationOrigin:MTLOriginMake(0, 0, 0)];
    [blit endEncoding];
    [command_buffer commit];
    [command_buffer waitUntilCompleted];
    [readback_texture getBytes:rgba.data()
                   bytesPerRow:static_cast<NSUInteger>(state.width) * 4u
                    fromRegion:MTLRegionMake2D(0, 0, static_cast<NSUInteger>(state.width),
                                               static_cast<NSUInteger>(state.height))
                   mipmapLevel:0];
    [readback_texture release];
    if (owns_queue) {
      [queue release];
    }

    const bool has_overlay = !framebuffer.empty() && framebuffer.width() == state.width &&
                             framebuffer.height() == state.height;
    if (has_overlay) {
      const std::span<const std::uint8_t> overlay = framebuffer.rgba8();
      for (std::size_t pixel = 0; pixel < pixel_count; ++pixel) {
        const std::size_t base = pixel * 4u;
        const float alpha = static_cast<float>(overlay[base + 3u]) / 255.0f;
        if (alpha <= 0.0f) {
          rgba[base + 3u] = 255u;
          continue;
        }
        const float keep = 1.0f - alpha;
        rgba[base + 0u] = static_cast<std::uint8_t>(std::clamp(
            static_cast<float>(overlay[base + 0u]) + static_cast<float>(rgba[base + 0u]) * keep,
            0.0f, 255.0f));
        rgba[base + 1u] = static_cast<std::uint8_t>(std::clamp(
            static_cast<float>(overlay[base + 1u]) + static_cast<float>(rgba[base + 1u]) * keep,
            0.0f, 255.0f));
        rgba[base + 2u] = static_cast<std::uint8_t>(std::clamp(
            static_cast<float>(overlay[base + 2u]) + static_cast<float>(rgba[base + 2u]) * keep,
            0.0f, 255.0f));
        rgba[base + 3u] = 255u;
      }
    }

    framebuffer.replaceRgba8(state.width, state.height, rgba);
    return true;
  }
}

void presentFrameBuffer(DESKTOP_WINDOWwindow *window, const SoftwareFrameBuffer &framebuffer) {
  if (window == nullptr) {
    return;
  }
  presenterFor(window).present(framebuffer);
}

void releaseFramePresenter(DESKTOP_WINDOWwindow *window) {
  presenters().erase(window);
  pendingStates().erase(window);
  NativeFrameState &state = nativeFrameState();
  [state.texture release];
  [state.queue release];
  state.texture = nil;
  state.queue = nil;
  state.width = 0;
  state.height = 0;
}

} // namespace aster
