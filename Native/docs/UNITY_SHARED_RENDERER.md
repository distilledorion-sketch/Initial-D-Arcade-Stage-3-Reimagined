# Unity shared-device renderer

The isolated Unity copy keeps the .29 renderer's shaders, materials, source
lighting/fog, mirrors, projected headlights, and ordered menu overlay passes.
Its new output path uses the host's Direct3D 11 device directly; the completed
frame is never read back or uploaded by the Unity display path.

## Bridge contract

```cpp
bool Renderer::initializeSharedDevice(ID3D11Device*, int width, int height);
ID3D11Texture2D* Renderer::sharedTexture() const;
std::uint64_t Renderer::sharedTextureGeneration() const;
```

Initialization takes its own COM reference to the supplied device. It creates
a private deferred context and a one-mip `B8G8R8A8_UNORM` target with both render
target and shader resource bind flags. The returned texture pointer is borrowed
and is valid until resize or renderer destruction. Successful resize increments
the generation; the bridge must refresh the Unity external texture. Device
shutdown/reset requires destruction and recreation on the render thread.

All renderer operations, including imports and destruction, belong inside the
native render-event thread boundary. The methods are not concurrent entry
points. `draw`, `loadTextures`, and `resize` submit their own command lists;
there is no additional begin/end call. Source HUD canvases still use the
existing dynamic texture uploads; they are not a CPU copy of the rendered frame.

Unity imports the pointer using `Texture2D.CreateExternalTexture`, BGRA32,
no mip chain, `linear: true`. The display shader must preserve the renderer's
existing display RGB; project color management and vertical orientation must
be checked inside the actual Unity editor/player. No editor was installed by
this task.

## Host state and synchronization

Native rendering records on its private deferred context. Each successful
submission uses `FinishCommandList(FALSE)` followed by
`hostImmediate->ExecuteCommandList(list, TRUE)`. The runtime saves and restores
the complete immediate context, including stages unused by this renderer.
Native rendering never clears the host's state. Executing on Unity's render
thread/device orders the native target writes ahead of subsequent Unity
sampling without a second device, shared handle, CPU fence, or frame upload.

Failed recorded work is discarded before another draw. GPU timestamp queries
and optional diagnostic staging readback are read through the immediate
context because deferred contexts do not support query/readback retrieval.
These diagnostics are not part of the display path.

Standalone `initialize(HWND,...)` and offscreen hardware/WARP initialization
retain their own immediate context and the existing presentation behavior.

## Native validation

`tests/unity_shared_renderer_tests.cpp` creates a hidden WARP device acting as
the host. It installs distinctive output targets, raster/blend/depth settings,
vertex/index buffers, shaders, texture/sampler, all six shader-stage constant
buffers, viewport, and scissor. These bindings must survive native initialize,
texture import, draw, diagnostic readback, resize, and destruction. One fixture
keeps the native target bound as a host SRV while the native list writes it.

Twelve complete BGRA framebuffers are compared exactly with the standalone
immediate renderer. Cases cover textures/mips, independent light scopes,
source fog, flat showroom/environment material, projected destination-color
blend, billboards, main/rear visibility masks, rear view, source-size overlays,
additive overlays, and final fade. Resize generation and shared-device GPU
timestamp readback are also checked. This test requires no game data or saves.

The test is a D3D11 integration proof, not proof of editor import, managed
texture lifetime, or Unity display scheduling; those belong to bridge/player
validation. No interactive FPS claim follows from the WARP test.

## Primary API references

- [Microsoft: ExecuteCommandList and RestoreContextState](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-executecommandlist)
- [Unity: Texture2D.CreateExternalTexture](https://docs.unity3d.com/ScriptReference/Texture2D.CreateExternalTexture.html)

The implementation requires Direct3D 11 feature level 11.0 or later. It does
not implement a Direct3D 12, Vulkan, Metal, or OpenGL backend.
