// Author: Faruk Alpay
// Do not remove this notice.

#include "aster/platform/window.hpp"

#include "aster/input/input_codes.hpp"
#include "aster/render/software_framebuffer.hpp"

#import <Cocoa/Cocoa.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace aster {
bool presentNativeFrameInView(void *view_pointer, bool vsync);
}

namespace {

struct NativeWindowState {
  NSWindow *window = nil;
  NSView *view = nil;
  NSObject<NSWindowDelegate> *delegate = nil;
  aster::ControlSnapshot input;
  aster::CursorMode cursor_mode = aster::CursorMode::Normal;
  bool should_close = false;
  bool scale_framebuffer_to_display = false;
  bool cursor_hidden = false;
  bool vsync = true;
};

void addUnique(std::vector<int> &values, const int value) {
  if (std::find(values.begin(), values.end(), value) == values.end()) {
    values.push_back(value);
  }
}

void removeValue(std::vector<int> &values, const int value) {
  values.erase(std::remove(values.begin(), values.end(), value), values.end());
}

int keyFromEvent(NSEvent *event) {
  switch ([event keyCode]) {
  case 53:
    return aster::code(aster::Key::Escape);
  case 15:
    return aster::code(aster::Key::R);
  case 0:
    return aster::code(aster::Key::A);
  case 123:
    return aster::code(aster::Key::Left);
  case 2:
    return aster::code(aster::Key::D);
  case 124:
    return aster::code(aster::Key::Right);
  case 13:
    return aster::code(aster::Key::W);
  case 126:
    return aster::code(aster::Key::Up);
  case 1:
    return aster::code(aster::Key::S);
  case 125:
    return aster::code(aster::Key::Down);
  case 49:
    return aster::code(aster::Key::Space);
  case 56:
  case 60:
    return aster::code(aster::Key::LeftShift);
  case 48:
    return aster::code(aster::Key::Tab);
  case 55:
    return aster::code(aster::Key::LeftSuper);
  case 54:
    return aster::code(aster::Key::RightSuper);
  case 14:
    return aster::code(aster::Key::E);
  case 12:
    return aster::code(aster::Key::Q);
  case 18:
    return aster::code(aster::Key::Num1);
  case 19:
    return aster::code(aster::Key::Num2);
  case 20:
    return aster::code(aster::Key::Num3);
  case 21:
    return aster::code(aster::Key::Num4);
  case 23:
    return aster::code(aster::Key::Num5);
  case 22:
    return aster::code(aster::Key::Num6);
  default:
    return -1;
  }
}

void updateModifierKeys(NativeWindowState *state, NSEvent *event) {
  const NSEventModifierFlags flags = [event modifierFlags];
  const bool shift = (flags & NSEventModifierFlagShift) != 0;
  const bool command = (flags & NSEventModifierFlagCommand) != 0;
  if (shift) {
    addUnique(state->input.pressed_keys, aster::code(aster::Key::LeftShift));
  } else {
    removeValue(state->input.pressed_keys, aster::code(aster::Key::LeftShift));
  }
  if (command) {
    addUnique(state->input.pressed_keys, aster::code(aster::Key::LeftSuper));
    addUnique(state->input.pressed_keys, aster::code(aster::Key::RightSuper));
  } else {
    removeValue(state->input.pressed_keys, aster::code(aster::Key::LeftSuper));
    removeValue(state->input.pressed_keys, aster::code(aster::Key::RightSuper));
  }
}

} // namespace

@interface AsterWindowDelegate : NSObject <NSWindowDelegate> {
@public
  NativeWindowState *state_;
}
- (instancetype)initWithState:(NativeWindowState *)state;
@end

@implementation AsterWindowDelegate
- (instancetype)initWithState:(NativeWindowState *)state {
  self = [super init];
  if (self != nil) {
    state_ = state;
  }
  return self;
}
- (BOOL)windowShouldClose:(id)sender {
  (void)sender;
  state_->should_close = true;
  return NO;
}
@end

@interface AsterContentView : NSView {
@public
  NativeWindowState *state_;
  NSMutableData *pixels_;
  int pixel_width_;
  int pixel_height_;
}
- (instancetype)initWithState:(NativeWindowState *)state;
- (void)presentFramebuffer;
@end

@implementation AsterContentView
- (instancetype)initWithState:(NativeWindowState *)state {
  self = [super initWithFrame:NSZeroRect];
  if (self != nil) {
    state_ = state;
    pixels_ = nil;
    pixel_width_ = 0;
    pixel_height_ = 0;
    [self setWantsLayer:YES];
    [self.window setAcceptsMouseMovedEvents:YES];
  }
  return self;
}
- (BOOL)isFlipped {
  return YES;
}
- (BOOL)acceptsFirstResponder {
  return YES;
}
- (void)viewDidMoveToWindow {
  [self.window setAcceptsMouseMovedEvents:YES];
}
- (void)updatePointer:(NSEvent *)event {
  if (state_->cursor_mode == aster::CursorMode::Disabled) {
    state_->input.pointer =
        state_->input.pointer +
        aster::Vec2{static_cast<float>([event deltaX]), static_cast<float>([event deltaY])};
    return;
  }
  NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
  state_->input.pointer = {static_cast<float>(point.x), static_cast<float>(point.y)};
}
- (void)keyDown:(NSEvent *)event {
  const int key = keyFromEvent(event);
  if (key >= 0) {
    addUnique(state_->input.pressed_keys, key);
  }
}
- (void)keyUp:(NSEvent *)event {
  const int key = keyFromEvent(event);
  if (key >= 0) {
    removeValue(state_->input.pressed_keys, key);
  }
}
- (void)flagsChanged:(NSEvent *)event {
  updateModifierKeys(state_, event);
}
- (void)mouseMoved:(NSEvent *)event {
  [self updatePointer:event];
}
- (void)mouseDragged:(NSEvent *)event {
  [self updatePointer:event];
}
- (void)rightMouseDragged:(NSEvent *)event {
  [self updatePointer:event];
}
- (void)mouseDown:(NSEvent *)event {
  [self updatePointer:event];
  addUnique(state_->input.pressed_mouse_buttons, aster::code(aster::MouseButton::Left));
}
- (void)mouseUp:(NSEvent *)event {
  [self updatePointer:event];
  removeValue(state_->input.pressed_mouse_buttons, aster::code(aster::MouseButton::Left));
}
- (void)rightMouseDown:(NSEvent *)event {
  [self updatePointer:event];
  addUnique(state_->input.pressed_mouse_buttons, aster::code(aster::MouseButton::Right));
}
- (void)rightMouseUp:(NSEvent *)event {
  [self updatePointer:event];
  removeValue(state_->input.pressed_mouse_buttons, aster::code(aster::MouseButton::Right));
}
- (void)scrollWheel:(NSEvent *)event {
  state_->input.scroll.x += static_cast<float>([event scrollingDeltaX]);
  state_->input.scroll.y += static_cast<float>([event scrollingDeltaY]);
}
- (void)presentFramebuffer {
  const aster::SoftwareFrameBuffer &framebuffer = aster::activeFrameBuffer();
  if (framebuffer.empty()) {
    return;
  }
  pixel_width_ = framebuffer.width();
  pixel_height_ = framebuffer.height();
  const std::span<const std::uint8_t> bytes = framebuffer.rgba8();
  pixels_ = [NSMutableData dataWithBytes:bytes.data() length:bytes.size()];
  [self setNeedsDisplay:YES];
  [self displayIfNeeded];
}
- (void)drawRect:(NSRect)dirtyRect {
  (void)dirtyRect;
  if (pixels_ == nil || pixel_width_ <= 0 || pixel_height_ <= 0) {
    [[NSColor blackColor] setFill];
    NSRectFill([self bounds]);
    return;
  }
  CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
  CGDataProviderRef provider = CGDataProviderCreateWithCFData((__bridge CFDataRef)pixels_);
  const CGBitmapInfo bitmap_info =
      static_cast<CGBitmapInfo>(kCGImageAlphaLast) | kCGBitmapByteOrder32Big;
  CGImageRef image = CGImageCreate(pixel_width_, pixel_height_, 8, 32,
                                   static_cast<std::size_t>(pixel_width_) * 4u, color_space,
                                   bitmap_info, provider, nullptr, false,
                                   kCGRenderingIntentDefault);
  CGContextRef context = [[NSGraphicsContext currentContext] CGContext];
  const CGRect bounds = NSRectToCGRect([self bounds]);
  CGContextDrawImage(context, bounds, image);
  CGImageRelease(image);
  CGDataProviderRelease(provider);
  CGColorSpaceRelease(color_space);
}
@end

namespace aster {

struct WindowImpl {
  NativeWindowState state;
};

Window::Window(const EngineConfig &config) : impl_(std::make_unique<WindowImpl>()) {
  scale_framebuffer_to_display_ = config.scale_framebuffer_to_display;
  impl_->state.scale_framebuffer_to_display = config.scale_framebuffer_to_display;

  [NSApplication sharedApplication];
  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
  [NSApp finishLaunching];

  const NSRect rect = NSMakeRect(80.0, 80.0, static_cast<CGFloat>(config.initial_width),
                                static_cast<CGFloat>(config.initial_height));
  const NSWindowStyleMask style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                  NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
  NSWindow *window = [[NSWindow alloc] initWithContentRect:rect
                                                 styleMask:style
                                                   backing:NSBackingStoreBuffered
                                                     defer:NO];
  if (window == nil) {
    throw std::runtime_error("Failed to create the application window.");
  }

  impl_->state.window = window;
  impl_->state.delegate = [[AsterWindowDelegate alloc] initWithState:&impl_->state];
  [window setDelegate:impl_->state.delegate];
  [window setTitle:[NSString stringWithUTF8String:config.application_name]];
  AsterContentView *view = [[AsterContentView alloc] initWithState:&impl_->state];
  impl_->state.view = view;
  [window setContentView:view];
  [window makeFirstResponder:view];
  [window makeKeyAndOrderFront:nil];
  [NSApp activateIgnoringOtherApps:YES];
  setVsync(config.enable_vsync);
}

Window::~Window() {
  if (impl_ != nullptr && impl_->state.cursor_mode == CursorMode::Disabled) {
    CGAssociateMouseAndMouseCursorPosition(true);
  }
  if (impl_ != nullptr && impl_->state.cursor_hidden) {
    [NSCursor unhide];
  }
}

Window::Window(Window &&other) noexcept = default;
Window &Window::operator=(Window &&other) noexcept = default;

bool Window::isOpen() const {
  return impl_ != nullptr && !impl_->state.should_close;
}

void Window::pollEvents() {
  if (impl_ == nullptr) {
    return;
  }
  impl_->state.input.scroll = {};
  for (;;) {
    NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                        untilDate:[NSDate distantPast]
                                           inMode:NSDefaultRunLoopMode
                                          dequeue:YES];
    if (event == nil) {
      break;
    }
    [NSApp sendEvent:event];
  }
}

void Window::swapBuffers() {
  if (impl_ == nullptr || impl_->state.view == nil) {
    return;
  }
  if (aster::presentNativeFrameInView(impl_->state.view, impl_->state.vsync)) {
    return;
  }
  [(AsterContentView *)impl_->state.view presentFramebuffer];
}

void Window::setVsync(const bool enabled) {
  if (impl_ != nullptr) {
    impl_->state.vsync = enabled;
  }
}

void Window::setCursorMode(const CursorMode mode) {
  if (impl_ == nullptr) {
    return;
  }
  if (impl_->state.cursor_mode == mode) {
    return;
  }
  if (impl_->state.cursor_mode == CursorMode::Disabled && mode != CursorMode::Disabled) {
    CGAssociateMouseAndMouseCursorPosition(true);
  }
  impl_->state.cursor_mode = mode;
  const bool hide = mode != CursorMode::Normal;
  if (hide && !impl_->state.cursor_hidden) {
    [NSCursor hide];
    impl_->state.cursor_hidden = true;
  } else if (!hide && impl_->state.cursor_hidden) {
    [NSCursor unhide];
    impl_->state.cursor_hidden = false;
  }
  if (mode == CursorMode::Disabled && impl_->state.view != nil) {
    const NSRect bounds = [impl_->state.view bounds];
    impl_->state.input.pointer = {static_cast<float>(bounds.size.width) * 0.5f,
                                  static_cast<float>(bounds.size.height) * 0.5f};
    CGAssociateMouseAndMouseCursorPosition(false);
  }
}

void Window::requestClose() {
  if (impl_ != nullptr) {
    impl_->state.should_close = true;
  }
}

std::pair<int, int> Window::windowSize() const {
  if (impl_ == nullptr || impl_->state.view == nil) {
    return {1, 1};
  }
  const NSRect bounds = [impl_->state.view bounds];
  return {std::max(1, static_cast<int>(std::round(bounds.size.width))),
          std::max(1, static_cast<int>(std::round(bounds.size.height)))};
}

std::pair<int, int> Window::framebufferSize() const {
  if (impl_ == nullptr || impl_->state.view == nil || !scale_framebuffer_to_display_) {
    return windowSize();
  }
  const NSRect backing = [impl_->state.view convertRectToBacking:[impl_->state.view bounds]];
  return {std::max(1, static_cast<int>(std::round(backing.size.width))),
          std::max(1, static_cast<int>(std::round(backing.size.height)))};
}

ControlSnapshot Window::captureControls(const ControlScheme &scheme) const {
  (void)scheme;
  return impl_ == nullptr ? ControlSnapshot{} : impl_->state.input;
}

} // namespace aster
