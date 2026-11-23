# Video binding

This C target binds the public mpv API in the module that created the player's
handle. It does not download, link or bundle a separate mpv binary. Windows/Linux
CMake and Apple CocoaPods/SwiftPM compile the same source.

The public headers in `include/mpv` are the unmodified upstream headers at mpv
commit `09cf7a1` (client API 2.7), including the Direct3D 11 render backend in
`render_d3d11.h` used by the Windows renderer. They retain their ISC license
notices. Update them when the renderer uses a new API; downloaded import
libraries are unnecessary.

Since client API 2.7, libmpv draws with the gpu-next renderer when
`MPV_RENDER_PARAM_RENDERER` is unset, and falls back to the gpu renderer when
gpu-next cannot initialize on the given context. The renderers leave it unset;
with OpenGL, gpu-next needs GLES 3.0 (or desktop GL with GLSL 1.30), so the
Linux renderer creates a GLES 3 context when the display supports one. With
Direct3D 11, gpu-next runs libplacebo's D3D11 backend on the device the Windows
renderer creates on Flutter's adapter and draws straight into its shared mailbox
textures; this needs a libmpv built against libplacebo with D3D11 support.

The owning Dart player supplies its resolved module path to
`media_kit_mpv_initialize`. Initialization is serialized and publishes all symbols
at once. Every renderer must initialize before calling the binding. A successful
binding retains the library until process exit, independently of Flutter engine
lifetimes. Player/render-context teardown stays with the existing owners.
