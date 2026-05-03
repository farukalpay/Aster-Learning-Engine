#ifndef _desktop_window3_native_h_
#define _desktop_window3_native_h_

#ifdef __cplusplus
extern "C" {
#endif


/*************************************************************************
 * Doxygen documentation
 *************************************************************************/

/*! @file desktop_window3native.h
 *  @brief The header of the native access functions.
 *
 *  This is the header file of the native access functions.  See @ref native for
 *  more information.
 */
/*! @defgroup native Native access
 *  @brief Functions related to accessing native handles.
 *
 *  **By using the native access functions you assert that you know what you're
 *  doing and how to fix problems caused by using them.  If you don't, you
 *  shouldn't be using them.**
 *
 *  Before the inclusion of @ref desktop_window3native.h, you may define zero or more
 *  window system API macro and zero or more context creation API macros.
 *
 *  The chosen backends must match those the library was compiled for.  Failure
 *  to do this will cause a link-time error.
 *
 *  The available window API macros are:
 *  * `DESKTOP_WINDOW_EXPOSE_NATIVE_WIN32`
 *  * `DESKTOP_WINDOW_EXPOSE_NATIVE_COCOA`
 *  * `DESKTOP_WINDOW_EXPOSE_NATIVE_X11`
 *  * `DESKTOP_WINDOW_EXPOSE_NATIVE_WAYLAND`
 *
 *  The available context API macros are:
 *  * `DESKTOP_WINDOW_EXPOSE_NATIVE_WGL`
 *  * `DESKTOP_WINDOW_EXPOSE_NATIVE_NSGL`
 *  * `DESKTOP_WINDOW_EXPOSE_NATIVE_GLX`
 *  * `DESKTOP_WINDOW_EXPOSE_NATIVE_EGL`
 *  * `DESKTOP_WINDOW_EXPOSE_NATIVE_OSMESA`
 *
 *  These macros select which of the native access functions that are declared
 *  and which platform-specific headers to include.  It is then up your (by
 *  definition platform-specific) code to handle which of these should be
 *  defined.
 *
 *  If you do not want the platform-specific headers to be included, define
 *  `DESKTOP_WINDOW_NATIVE_INCLUDE_NONE` before including the @ref desktop_window3native.h header.
 *
 *  @code
 *  #define DESKTOP_WINDOW_EXPOSE_NATIVE_WIN32
 *  #define DESKTOP_WINDOW_EXPOSE_NATIVE_WGL
 *  #define DESKTOP_WINDOW_NATIVE_INCLUDE_NONE
 *  #include <DESKTOP_WINDOW/desktop_window3native.h>
 *  @endcode
 */


/*************************************************************************
 * System headers and types
 *************************************************************************/

#if !defined(DESKTOP_WINDOW_NATIVE_INCLUDE_NONE)

 #if defined(DESKTOP_WINDOW_EXPOSE_NATIVE_WIN32) || defined(DESKTOP_WINDOW_EXPOSE_NATIVE_WGL)
  /* This is a workaround for the fact that desktop_window3.h needs to export APIENTRY (for
   * example to allow applications to correctly declare a GL_KHR_debug callback)
   * but windows.h assumes no one will define APIENTRY before it does
   */
  #if defined(DESKTOP_WINDOW_APIENTRY_DEFINED)
   #undef APIENTRY
   #undef DESKTOP_WINDOW_APIENTRY_DEFINED
  #endif
  #include <windows.h>
 #endif

 #if defined(DESKTOP_WINDOW_EXPOSE_NATIVE_COCOA) || defined(DESKTOP_WINDOW_EXPOSE_NATIVE_NSGL)
  #if defined(__OBJC__)
   #import <Cocoa/Cocoa.h>
  #else
   #include <ApplicationServices/ApplicationServices.h>
   #include <objc/objc.h>
  #endif
 #endif

 #if defined(DESKTOP_WINDOW_EXPOSE_NATIVE_X11) || defined(DESKTOP_WINDOW_EXPOSE_NATIVE_GLX)
  #include <X11/Xlib.h>
  #include <X11/extensions/Xrandr.h>
 #endif

 #if defined(DESKTOP_WINDOW_EXPOSE_NATIVE_WAYLAND)
  #include <wayland-client.h>
 #endif

 #if defined(DESKTOP_WINDOW_EXPOSE_NATIVE_WGL)
  /* WGL is declared by windows.h */
 #endif
 #if defined(DESKTOP_WINDOW_EXPOSE_NATIVE_NSGL)
  /* NSGL is declared by Cocoa.h */
 #endif
 #if defined(DESKTOP_WINDOW_EXPOSE_NATIVE_GLX)
  /* This is a workaround for the fact that desktop_window3.h defines GLAPIENTRY because by
   * default it also acts as an DesktopGraphics header
   * However, glx.h will include gl.h, which will define it unconditionally
   */
  #if defined(DESKTOP_WINDOW_GLAPIENTRY_DEFINED)
   #undef GLAPIENTRY
   #undef DESKTOP_WINDOW_GLAPIENTRY_DEFINED
  #endif
  #include <GL/glx.h>
 #endif
 #if defined(DESKTOP_WINDOW_EXPOSE_NATIVE_EGL)
  #include <EGL/egl.h>
 #endif
 #if defined(DESKTOP_WINDOW_EXPOSE_NATIVE_OSMESA)
  /* This is a workaround for the fact that desktop_window3.h defines GLAPIENTRY because by
   * default it also acts as an DesktopGraphics header
   * However, osmesa.h will include gl.h, which will define it unconditionally
   */
  #if defined(DESKTOP_WINDOW_GLAPIENTRY_DEFINED)
   #undef GLAPIENTRY
   #undef DESKTOP_WINDOW_GLAPIENTRY_DEFINED
  #endif
  #include <GL/osmesa.h>
 #endif

#endif /*DESKTOP_WINDOW_NATIVE_INCLUDE_NONE*/


/*************************************************************************
 * Functions
 *************************************************************************/

#if defined(DESKTOP_WINDOW_EXPOSE_NATIVE_WIN32)
/*! @brief Returns the adapter device name of the specified monitor.
 *
 *  @return The UTF-8 encoded adapter device name (for example `\\.\DISPLAY1`)
 *  of the specified monitor, or `NULL` if an [error](@ref error_handling)
 *  occurred.
 *
 *  @errors Possible errors include @ref DESKTOP_WINDOW_NOT_INITIALIZED and @ref
 *  DESKTOP_WINDOW_PLATFORM_UNAVAILABLE.
 *
 *  @thread_safety This function may be called from any thread.  Access is not
 *  synchronized.
 *
 *  @since Added in version 3.1.
 *
 *  @ingroup native
 */
DESKTOP_WINDOWAPI const char* desktop_windowGetWin32Adapter(DESKTOP_WINDOWmonitor* monitor);

/*! @brief Returns the display device name of the specified monitor.
 *
 *  @return The UTF-8 encoded display device name (for example
 *  `\\.\DISPLAY1\Monitor0`) of the specified monitor, or `NULL` if an
 *  [error](@ref error_handling) occurred.
 *
 *  @errors Possible errors include @ref DESKTOP_WINDOW_NOT_INITIALIZED and @ref
 *  DESKTOP_WINDOW_PLATFORM_UNAVAILABLE.
 *
 *  @thread_safety This function may be called from any thread.  Access is not
 *  synchronized.
 *
 *  @since Added in version 3.1.
 *
 *  @ingroup native
 */
DESKTOP_WINDOWAPI const char* desktop_windowGetWin32Monitor(DESKTOP_WINDOWmonitor* monitor);

/*! @brief Returns the `HWND` of the specified window.
 *
 *  @return The `HWND` of the specified window, or `NULL` if an
 *  [error](@ref error_handling) occurred.
 *
 *  @errors Possible errors include @ref DESKTOP_WINDOW_NOT_INITIALIZED and @ref
 *  DESKTOP_WINDOW_PLATFORM_UNAVAILABLE.
 *
 *  @remark The `HDC` associated with the window can be queried with the
 *  [GetDC](https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getdc)
 *  function.
 *  @code
 *  HDC dc = GetDC(desktop_windowGetWin32Window(window));
 *  @endcode
 *  This DC is private and does not need to be released.
 *
 *  @thread_safety This function may be called from any thread.  Access is not
 *  synchronized.
 *
 *  @since Added in version 3.0.
 *
 *  @ingroup native
 */
DESKTOP_WINDOWAPI HWND desktop_windowGetWin32Window(DESKTOP_WINDOWwindow* window);
#endif

#if defined(DESKTOP_WINDOW_EXPOSE_NATIVE_WGL)
/*! @brief Returns the `HGLRC` of the specified window.
 *
 *  @return The `HGLRC` of the specified window, or `NULL` if an
 *  [error](@ref error_handling) occurred.
 *
 *  @errors Possible errors include @ref DESKTOP_WINDOW_NOT_INITIALIZED, @ref
 *  DESKTOP_WINDOW_PLATFORM_UNAVAILABLE and @ref DESKTOP_WINDOW_NO_WINDOW_CONTEXT.
 *
 *  @remark The `HDC` associated with the window can be queried with the
 *  [GetDC](https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getdc)
 *  function.
 *  @code
 *  HDC dc = GetDC(desktop_windowGetWin32Window(window));
 *  @endcode
 *  This DC is private and does not need to be released.
 *
 *  @thread_safety This function may be called from any thread.  Access is not
 *  synchronized.
 *
 *  @since Added in version 3.0.
 *
 *  @ingroup native
 */
DESKTOP_WINDOWAPI HGLRC desktop_windowGetWGLContext(DESKTOP_WINDOWwindow* window);
#endif

#if defined(DESKTOP_WINDOW_EXPOSE_NATIVE_COCOA)
/*! @brief Returns the `CGDirectDisplayID` of the specified monitor.
 *
 *  @return The `CGDirectDisplayID` of the specified monitor, or
 *  `kCGNullDirectDisplay` if an [error](@ref error_handling) occurred.
 *
 *  @errors Possible errors include @ref DESKTOP_WINDOW_NOT_INITIALIZED and @ref
 *  DESKTOP_WINDOW_PLATFORM_UNAVAILABLE.
 *
 *  @thread_safety This function may be called from any thread.  Access is not
 *  synchronized.
 *
 *  @since Added in version 3.1.
 *
 *  @ingroup native
 */
DESKTOP_WINDOWAPI CGDirectDisplayID desktop_windowGetCocoaMonitor(DESKTOP_WINDOWmonitor* monitor);

/*! @brief Returns the `NSWindow` of the specified window.
 *
 *  @return The `NSWindow` of the specified window, or `nil` if an
 *  [error](@ref error_handling) occurred.
 *
 *  @errors Possible errors include @ref DESKTOP_WINDOW_NOT_INITIALIZED and @ref
 *  DESKTOP_WINDOW_PLATFORM_UNAVAILABLE.
 *
 *  @thread_safety This function may be called from any thread.  Access is not
 *  synchronized.
 *
 *  @since Added in version 3.0.
 *
 *  @ingroup native
 */
DESKTOP_WINDOWAPI id desktop_windowGetCocoaWindow(DESKTOP_WINDOWwindow* window);

/*! @brief Returns the `NSView` of the specified window.
 *
 *  @return The `NSView` of the specified window, or `nil` if an
 *  [error](@ref error_handling) occurred.
 *
 *  @errors Possible errors include @ref DESKTOP_WINDOW_NOT_INITIALIZED and @ref
 *  DESKTOP_WINDOW_PLATFORM_UNAVAILABLE.
 *
 *  @thread_safety This function may be called from any thread.  Access is not
 *  synchronized.
 *
 *  @since Added in version 3.4.
 *
 *  @ingroup native
 */
DESKTOP_WINDOWAPI id desktop_windowGetCocoaView(DESKTOP_WINDOWwindow* window);
#endif

#if defined(DESKTOP_WINDOW_EXPOSE_NATIVE_NSGL)
/*! @brief Returns the `NSDesktopGraphicsContext` of the specified window.
 *
 *  @return The `NSDesktopGraphicsContext` of the specified window, or `nil` if an
 *  [error](@ref error_handling) occurred.
 *
 *  @errors Possible errors include @ref DESKTOP_WINDOW_NOT_INITIALIZED, @ref
 *  DESKTOP_WINDOW_PLATFORM_UNAVAILABLE and @ref DESKTOP_WINDOW_NO_WINDOW_CONTEXT.
 *
 *  @thread_safety This function may be called from any thread.  Access is not
 *  synchronized.
 *
 *  @since Added in version 3.0.
 *
 *  @ingroup native
 */
DESKTOP_WINDOWAPI id desktop_windowGetNSGLContext(DESKTOP_WINDOWwindow* window);
#endif

#if defined(DESKTOP_WINDOW_EXPOSE_NATIVE_X11)
/*! @brief Returns the `Display` used by DESKTOP_WINDOW.
 *
 *  @return The `Display` used by DESKTOP_WINDOW, or `NULL` if an
 *  [error](@ref error_handling) occurred.
 *
 *  @errors Possible errors include @ref DESKTOP_WINDOW_NOT_INITIALIZED and @ref
 *  DESKTOP_WINDOW_PLATFORM_UNAVAILABLE.
 *
 *  @thread_safety This function may be called from any thread.  Access is not
 *  synchronized.
 *
 *  @since Added in version 3.0.
 *
 *  @ingroup native
 */
DESKTOP_WINDOWAPI Display* desktop_windowGetX11Display(void);

/*! @brief Returns the `RRCrtc` of the specified monitor.
 *
 *  @return The `RRCrtc` of the specified monitor, or `None` if an
 *  [error](@ref error_handling) occurred.
 *
 *  @errors Possible errors include @ref DESKTOP_WINDOW_NOT_INITIALIZED and @ref
 *  DESKTOP_WINDOW_PLATFORM_UNAVAILABLE.
 *
 *  @thread_safety This function may be called from any thread.  Access is not
 *  synchronized.
 *
 *  @since Added in version 3.1.
 *
 *  @ingroup native
 */
DESKTOP_WINDOWAPI RRCrtc desktop_windowGetX11Adapter(DESKTOP_WINDOWmonitor* monitor);

/*! @brief Returns the `RROutput` of the specified monitor.
 *
 *  @return The `RROutput` of the specified monitor, or `None` if an
 *  [error](@ref error_handling) occurred.
 *
 *  @errors Possible errors include @ref DESKTOP_WINDOW_NOT_INITIALIZED and @ref
 *  DESKTOP_WINDOW_PLATFORM_UNAVAILABLE.
 *
 *  @thread_safety This function may be called from any thread.  Access is not
 *  synchronized.
 *
 *  @since Added in version 3.1.
 *
 *  @ingroup native
 */
DESKTOP_WINDOWAPI RROutput desktop_windowGetX11Monitor(DESKTOP_WINDOWmonitor* monitor);

/*! @brief Returns the `Window` of the specified window.
 *
 *  @return The `Window` of the specified window, or `None` if an
 *  [error](@ref error_handling) occurred.
 *
 *  @errors Possible errors include @ref DESKTOP_WINDOW_NOT_INITIALIZED and @ref
 *  DESKTOP_WINDOW_PLATFORM_UNAVAILABLE.
 *
 *  @thread_safety This function may be called from any thread.  Access is not
 *  synchronized.
 *
 *  @since Added in version 3.0.
 *
 *  @ingroup native
 */
DESKTOP_WINDOWAPI Window desktop_windowGetX11Window(DESKTOP_WINDOWwindow* window);

/*! @brief Sets the current primary selection to the specified string.
 *
 *  @param[in] string A UTF-8 encoded string.
 *
 *  @errors Possible errors include @ref DESKTOP_WINDOW_NOT_INITIALIZED, @ref
 *  DESKTOP_WINDOW_PLATFORM_UNAVAILABLE and @ref DESKTOP_WINDOW_PLATFORM_ERROR.
 *
 *  @pointer_lifetime The specified string is copied before this function
 *  returns.
 *
 *  @thread_safety This function must only be called from the main thread.
 *
 *  @sa @ref clipboard
 *  @sa desktop_windowGetX11SelectionString
 *  @sa desktop_windowSetClipboardString
 *
 *  @since Added in version 3.3.
 *
 *  @ingroup native
 */
DESKTOP_WINDOWAPI void desktop_windowSetX11SelectionString(const char* string);

/*! @brief Returns the contents of the current primary selection as a string.
 *
 *  If the selection is empty or if its contents cannot be converted, `NULL`
 *  is returned and a @ref DESKTOP_WINDOW_FORMAT_UNAVAILABLE error is generated.
 *
 *  @return The contents of the selection as a UTF-8 encoded string, or `NULL`
 *  if an [error](@ref error_handling) occurred.
 *
 *  @errors Possible errors include @ref DESKTOP_WINDOW_NOT_INITIALIZED, @ref
 *  DESKTOP_WINDOW_PLATFORM_UNAVAILABLE and @ref DESKTOP_WINDOW_PLATFORM_ERROR.
 *
 *  @pointer_lifetime The returned string is allocated and freed by DESKTOP_WINDOW. You
 *  should not free it yourself. It is valid until the next call to @ref
 *  desktop_windowGetX11SelectionString or @ref desktop_windowSetX11SelectionString, or until the
 *  library is terminated.
 *
 *  @thread_safety This function must only be called from the main thread.
 *
 *  @sa @ref clipboard
 *  @sa desktop_windowSetX11SelectionString
 *  @sa desktop_windowGetClipboardString
 *
 *  @since Added in version 3.3.
 *
 *  @ingroup native
 */
DESKTOP_WINDOWAPI const char* desktop_windowGetX11SelectionString(void);
#endif

#if defined(DESKTOP_WINDOW_EXPOSE_NATIVE_GLX)
/*! @brief Returns the `GLXContext` of the specified window.
 *
 *  @return The `GLXContext` of the specified window, or `NULL` if an
 *  [error](@ref error_handling) occurred.
 *
 *  @errors Possible errors include @ref DESKTOP_WINDOW_NOT_INITIALIZED, @ref
 *  DESKTOP_WINDOW_NO_WINDOW_CONTEXT and @ref DESKTOP_WINDOW_PLATFORM_UNAVAILABLE.
 *
 *  @thread_safety This function may be called from any thread.  Access is not
 *  synchronized.
 *
 *  @since Added in version 3.0.
 *
 *  @ingroup native
 */
DESKTOP_WINDOWAPI GLXContext desktop_windowGetGLXContext(DESKTOP_WINDOWwindow* window);

/*! @brief Returns the `GLXWindow` of the specified window.
 *
 *  @return The `GLXWindow` of the specified window, or `None` if an
 *  [error](@ref error_handling) occurred.
 *
 *  @errors Possible errors include @ref DESKTOP_WINDOW_NOT_INITIALIZED, @ref
 *  DESKTOP_WINDOW_NO_WINDOW_CONTEXT and @ref DESKTOP_WINDOW_PLATFORM_UNAVAILABLE.
 *
 *  @thread_safety This function may be called from any thread.  Access is not
 *  synchronized.
 *
 *  @since Added in version 3.2.
 *
 *  @ingroup native
 */
DESKTOP_WINDOWAPI GLXWindow desktop_windowGetGLXWindow(DESKTOP_WINDOWwindow* window);
#endif

#if defined(DESKTOP_WINDOW_EXPOSE_NATIVE_WAYLAND)
/*! @brief Returns the `struct wl_display*` used by DESKTOP_WINDOW.
 *
 *  @return The `struct wl_display*` used by DESKTOP_WINDOW, or `NULL` if an
 *  [error](@ref error_handling) occurred.
 *
 *  @errors Possible errors include @ref DESKTOP_WINDOW_NOT_INITIALIZED and @ref
 *  DESKTOP_WINDOW_PLATFORM_UNAVAILABLE.
 *
 *  @thread_safety This function may be called from any thread.  Access is not
 *  synchronized.
 *
 *  @since Added in version 3.2.
 *
 *  @ingroup native
 */
DESKTOP_WINDOWAPI struct wl_display* desktop_windowGetWaylandDisplay(void);

/*! @brief Returns the `struct wl_output*` of the specified monitor.
 *
 *  @return The `struct wl_output*` of the specified monitor, or `NULL` if an
 *  [error](@ref error_handling) occurred.
 *
 *  @errors Possible errors include @ref DESKTOP_WINDOW_NOT_INITIALIZED and @ref
 *  DESKTOP_WINDOW_PLATFORM_UNAVAILABLE.
 *
 *  @thread_safety This function may be called from any thread.  Access is not
 *  synchronized.
 *
 *  @since Added in version 3.2.
 *
 *  @ingroup native
 */
DESKTOP_WINDOWAPI struct wl_output* desktop_windowGetWaylandMonitor(DESKTOP_WINDOWmonitor* monitor);

/*! @brief Returns the main `struct wl_surface*` of the specified window.
 *
 *  @return The main `struct wl_surface*` of the specified window, or `NULL` if
 *  an [error](@ref error_handling) occurred.
 *
 *  @errors Possible errors include @ref DESKTOP_WINDOW_NOT_INITIALIZED and @ref
 *  DESKTOP_WINDOW_PLATFORM_UNAVAILABLE.
 *
 *  @thread_safety This function may be called from any thread.  Access is not
 *  synchronized.
 *
 *  @since Added in version 3.2.
 *
 *  @ingroup native
 */
DESKTOP_WINDOWAPI struct wl_surface* desktop_windowGetWaylandWindow(DESKTOP_WINDOWwindow* window);
#endif

#if defined(DESKTOP_WINDOW_EXPOSE_NATIVE_EGL)
/*! @brief Returns the `EGLDisplay` used by DESKTOP_WINDOW.
 *
 *  @return The `EGLDisplay` used by DESKTOP_WINDOW, or `EGL_NO_DISPLAY` if an
 *  [error](@ref error_handling) occurred.
 *
 *  @errors Possible errors include @ref DESKTOP_WINDOW_NOT_INITIALIZED.
 *
 *  @remark Because EGL is initialized on demand, this function will return
 *  `EGL_NO_DISPLAY` until the first context has been created via EGL.
 *
 *  @thread_safety This function may be called from any thread.  Access is not
 *  synchronized.
 *
 *  @since Added in version 3.0.
 *
 *  @ingroup native
 */
DESKTOP_WINDOWAPI EGLDisplay desktop_windowGetEGLDisplay(void);

/*! @brief Returns the `EGLContext` of the specified window.
 *
 *  @return The `EGLContext` of the specified window, or `EGL_NO_CONTEXT` if an
 *  [error](@ref error_handling) occurred.
 *
 *  @errors Possible errors include @ref DESKTOP_WINDOW_NOT_INITIALIZED and @ref
 *  DESKTOP_WINDOW_NO_WINDOW_CONTEXT.
 *
 *  @thread_safety This function may be called from any thread.  Access is not
 *  synchronized.
 *
 *  @since Added in version 3.0.
 *
 *  @ingroup native
 */
DESKTOP_WINDOWAPI EGLContext desktop_windowGetEGLContext(DESKTOP_WINDOWwindow* window);

/*! @brief Returns the `EGLSurface` of the specified window.
 *
 *  @return The `EGLSurface` of the specified window, or `EGL_NO_SURFACE` if an
 *  [error](@ref error_handling) occurred.
 *
 *  @errors Possible errors include @ref DESKTOP_WINDOW_NOT_INITIALIZED and @ref
 *  DESKTOP_WINDOW_NO_WINDOW_CONTEXT.
 *
 *  @thread_safety This function may be called from any thread.  Access is not
 *  synchronized.
 *
 *  @since Added in version 3.0.
 *
 *  @ingroup native
 */
DESKTOP_WINDOWAPI EGLSurface desktop_windowGetEGLSurface(DESKTOP_WINDOWwindow* window);
#endif

#if defined(DESKTOP_WINDOW_EXPOSE_NATIVE_OSMESA)
/*! @brief Retrieves the color buffer associated with the specified window.
 *
 *  @param[in] window The window whose color buffer to retrieve.
 *  @param[out] width Where to store the width of the color buffer, or `NULL`.
 *  @param[out] height Where to store the height of the color buffer, or `NULL`.
 *  @param[out] format Where to store the OSMesa pixel format of the color
 *  buffer, or `NULL`.
 *  @param[out] buffer Where to store the address of the color buffer, or
 *  `NULL`.
 *  @return `DESKTOP_WINDOW_TRUE` if successful, or `DESKTOP_WINDOW_FALSE` if an
 *  [error](@ref error_handling) occurred.
 *
 *  @errors Possible errors include @ref DESKTOP_WINDOW_NOT_INITIALIZED and @ref
 *  DESKTOP_WINDOW_NO_WINDOW_CONTEXT.
 *
 *  @thread_safety This function may be called from any thread.  Access is not
 *  synchronized.
 *
 *  @since Added in version 3.3.
 *
 *  @ingroup native
 */
DESKTOP_WINDOWAPI int desktop_windowGetOSMesaColorBuffer(DESKTOP_WINDOWwindow* window, int* width, int* height, int* format, void** buffer);

/*! @brief Retrieves the depth buffer associated with the specified window.
 *
 *  @param[in] window The window whose depth buffer to retrieve.
 *  @param[out] width Where to store the width of the depth buffer, or `NULL`.
 *  @param[out] height Where to store the height of the depth buffer, or `NULL`.
 *  @param[out] bytesPerValue Where to store the number of bytes per depth
 *  buffer element, or `NULL`.
 *  @param[out] buffer Where to store the address of the depth buffer, or
 *  `NULL`.
 *  @return `DESKTOP_WINDOW_TRUE` if successful, or `DESKTOP_WINDOW_FALSE` if an
 *  [error](@ref error_handling) occurred.
 *
 *  @errors Possible errors include @ref DESKTOP_WINDOW_NOT_INITIALIZED and @ref
 *  DESKTOP_WINDOW_NO_WINDOW_CONTEXT.
 *
 *  @thread_safety This function may be called from any thread.  Access is not
 *  synchronized.
 *
 *  @since Added in version 3.3.
 *
 *  @ingroup native
 */
DESKTOP_WINDOWAPI int desktop_windowGetOSMesaDepthBuffer(DESKTOP_WINDOWwindow* window, int* width, int* height, int* bytesPerValue, void** buffer);

/*! @brief Returns the `OSMesaContext` of the specified window.
 *
 *  @return The `OSMesaContext` of the specified window, or `NULL` if an
 *  [error](@ref error_handling) occurred.
 *
 *  @errors Possible errors include @ref DESKTOP_WINDOW_NOT_INITIALIZED and @ref
 *  DESKTOP_WINDOW_NO_WINDOW_CONTEXT.
 *
 *  @thread_safety This function may be called from any thread.  Access is not
 *  synchronized.
 *
 *  @since Added in version 3.3.
 *
 *  @ingroup native
 */
DESKTOP_WINDOWAPI OSMesaContext desktop_windowGetOSMesaContext(DESKTOP_WINDOWwindow* window);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _desktop_window3_native_h_ */

