# Canvas

The Canvas API is a high-level rendering abstraction over OpenGL. It provides GPU resource management (shaders, textures, buffers, meshes), a command-list based draw model, and built-in renderers for common tasks (2D shapes, PBR, text, skyboxes, grids). Headers live in `include/rndr/canvas/` and implementations in `src/canvas/`. Include `rndr/canvas/canvas.hpp` to pull in all core types.

All types live in the `Rndr::Canvas` namespace.

## Error handling

Nothing in Canvas throws. Objects are built by a static `Create` (or a `From*` factory) returning
`Opal::Expected<T, Rndr::ErrorCode>`, anything else fallible returns an `Expected` or an `ErrorCode`, and
the reason is logged at error level so the code does not have to carry the detail. Misuse of the API is
`ErrorCode::InvalidArgument`; a region, layer or mip level that does not fit is `ErrorCode::OutOfBounds`; a
failing GL call maps through `GlErrorToErrorCode` in `rndr/canvas/gl-result.hpp`, which also carries the
`RNDR_CANVAS_CHECK` macros that propagate codes inside the implementation. Forge and audio report the same
way - see the error handling sections of [docs/forge.md](forge.md) and [docs/audio.md](audio.md).

`ShaderCompiler` is the one exception to "nothing throws": it is shared between the rendering APIs and
reports by throwing. The shader factories catch at that boundary and turn it into
`ErrorCode::ShaderCompilationError`, so nothing that escapes Canvas is an exception.

The snippets below unwrap results with `.GetValue()` for brevity; real code checks `HasValue()` first, or
fails the way `samples/window/window-sample.cpp` does.

## Core Concepts

### Context

The Context represents a live graphics backend and the on-screen presentation surface. Only one Context can exist at a time. It manages the default framebuffer (handle 0) owned by the windowing system.

```cpp
#include "rndr/canvas/canvas.hpp"

// Initialize the graphics backend.
Canvas::ContextDesc desc;
desc.color_format = Canvas::Format::RGBA8;
desc.depth_stencil_format = Canvas::Format::D24S8;
desc.vsync_enabled = true;

auto context = Canvas::Context::CreateContext(window, desc).GetValue();

// In the game loop:
// ... record and execute draw commands ...
context.Present();

// On window resize:
context.Resize(new_width, new_height);
```

The Context is move-only. Its destructor tears down the GL backend.

### DrawList

A DrawList records rendering commands (set render target, clear, draw, dispatch) and executes them in a single batch. Commands are consumed on execute -- the list object is reusable across frames.

```cpp
Canvas::DrawList draw_list;

// Bind the default framebuffer.
draw_list.SetRenderTarget(context);

// Or bind an off-screen render target.
draw_list.SetRenderTarget(render_target);

// Clear.
draw_list.Clear({0.1f, 0.1f, 0.1f, 1.0f});

// Draw geometry.
draw_list.Draw(mesh, brush);
draw_list.DrawInstanced(mesh, brush, 100);

// Compute dispatch.
draw_list.Dispatch(compute_brush, 64, 64);

// GPU debug markers.
draw_list.BeginEvent("Shadow Pass");
// ... draws ...
draw_list.EndEvent("Shadow Pass");

// GPU timestamps.
draw_list.WriteTimestamp(query);

// Execute all recorded commands and reset.
draw_list.Execute();
```

All referenced Mesh and Brush objects must remain valid until `Execute()` is called.

### Shader

Shaders are cross-compiled from Slang source to GLSL and linked into an OpenGL program. Entry points are auto-discovered from `[shader("vertex")]`, `[shader("fragment")]`, and `[shader("compute")]` annotations. Shader reflection data (uniforms, textures, vertex layout) is extracted automatically.

```cpp
// Single source file with both vertex and fragment entry points.
auto shader = Canvas::Shader::FromSource("shaders/pbr.slang", "PBR").GetValue();

// Separate vertex and fragment files.
auto shader = Canvas::Shader::FromSources("shaders/vert.slang", "shaders/frag.slang").GetValue();

// From in-memory source strings.
auto shader = Canvas::Shader::FromSourceInMemory(slang_source).GetValue();

// Query reflection data.
const auto& params = shader.GetParameters();
const auto* mvp_param = shader.FindParameter("mvp");
const auto& vertex_layout = shader.GetVertexLayout();

// Compute shader thread group size.
const auto& threads = shader.GetNumThreads();
```

#### Shader Parameters

Reflection produces `ShaderParameter` entries for every uniform, texture, sampler, storage buffer, and varying. Uniform parameters carry byte offset and size information for their parent UBO:

```
ConstantBuffer<Material> material;    -->  { name="material", size=0  }  (UBO declaration)
                                          { name="color",    size=16, offset=0  }
                                          { name="roughness",size=4,  offset=16 }

float4x4 mvp;                        -->  { name="mvp", size=64, offset=0 }  (implicit default UBO)
```

Parameter categories: `Uniform`, `Texture`, `Sampler`, `StorageBuffer`, `VaryingInput`, `VaryingOutput`.

### Brush

The Brush collects all rendering state: shader, blend mode, depth/stencil, rasterizer settings, and resource bindings. Named after the Canvas metaphor -- "how you paint", not "what you paint on".

```cpp
Canvas::BrushDesc desc;
desc.blend_mode = Canvas::BlendMode::Alpha;
desc.depth_test = true;
desc.depth_write = true;
desc.cull_mode = Canvas::CullMode::Back;

Canvas::Brush brush(desc, "MyMaterial");
brush.SetShader(shader);

// Bind resources by name (must match shader declarations).
brush.SetTexture("albedo_texture", albedo_tex);
brush.SetBuffer("instance_data", ssbo);
brush.SetUniform("mvp", view_projection);
brush.SetUniform("light_colors", 0, light0_color);  // Array element.
```

#### Uniform Buffer Management

When a shader is assigned via `SetShader()`, the Brush inspects reflection data and automatically creates GPU uniform buffers for each UBO binding point. The workflow is:

1. `brush.SetShader(shader)` -- creates UBO slots from reflection.
2. `brush.SetUniform("mvp", m)` -- writes into the correct UBO's CPU staging data.
3. The DrawList calls `brush.Apply()` internally, which uploads dirty UBOs and binds all state.

If `SetUniform()` is called with a name that doesn't match any shader parameter, the value is stored in a fallback list accessible via `GetUniforms()`.

#### Pipeline State

| Method | Default | Description |
|---|---|---|
| `SetBlendMode(mode)` | `None` | `None`, `Alpha`, `Additive`, `Multiply` |
| `SetDepthTest(enabled)` | `false` | Enable depth testing |
| `SetDepthWrite(enabled)` | `true` | Enable depth writes |
| `SetDepthCompare(func)` | `Less` | `Less`, `LessEqual`, `Greater`, `GreaterEqual`, `Equal`, `NotEqual`, `Always`, `Never` |
| `SetCullMode(mode)` | `Back` | `None`, `Back`, `Front` |
| `SetWindingOrder(order)` | `CCW` | `CCW`, `CW` |
| `SetFillMode(mode)` | `Solid` | `Solid`, `Wireframe` |
| `SetDepthBias(factor, units)` | `0, 0` | Polygon offset for shadow acne / z-fighting |
| `SetScissorTest(enabled)` | `false` | Enable scissor test (discard fragments outside the rectangle) |
| `SetScissor(x, y, w, h)` | `0, 0, 0, 0` | Scissor rectangle in window pixels, origin at lower-left |

### Mesh

Geometry data paired with a vertex layout. Owns GPU resources (VAO, VBO, optional IBO). Supports both immediate creation and dynamic append/upload for batching.

```cpp
// Create from vertex + index data.
Canvas::VertexLayout layout;
layout.Add(Canvas::Attrib::Position, Canvas::Format::Float3);
layout.Add(Canvas::Attrib::Normal, Canvas::Format::Float3);
layout.Add(Canvas::Attrib::UV, Canvas::Format::Float2);

auto mesh = Canvas::Mesh::Create(layout, vertex_bytes, index_bytes, "Cube").GetValue();

// Dynamic mesh (pre-allocate, then append per frame).
auto dynamic_mesh = Canvas::Mesh::Create(layout, max_vertices, max_indices, "DynamicMesh").GetValue();

// Each frame:
dynamic_mesh.Clear();
dynamic_mesh.Append(new_vertex_data, new_index_data);
dynamic_mesh.Upload();
```

Vertex data stride is validated against the layout at construction. Index data uses `u32` indices.

### Texture

GPU texture resource supporting 2D, 2D array, and cubemap types. Loaded from files (PNG, JPEG, HDR via stbi; KTX/KTX2 when the Forge API is enabled) or created programmatically.

```cpp
// Load from file.
Canvas::TextureDesc desc;
desc.min_filter = Canvas::TextureFilter::Linear;
desc.mag_filter = Canvas::TextureFilter::Linear;
desc.wrap_u = Canvas::TextureWrap::Repeat;
desc.wrap_v = Canvas::TextureWrap::Repeat;
desc.use_mips = true;

auto texture = Canvas::Texture::FromFile("textures/brick.png", desc, true, "Brick").GetValue();

// Create programmatically.
Canvas::TextureDesc rt_desc;
rt_desc.width = 1024;
rt_desc.height = 1024;
rt_desc.format = Canvas::Format::RGBA16F;

auto hdr_texture = Canvas::Texture::Create(rt_desc, {}, "HDR Buffer").GetValue();

// Upload new data (whole base-mip Texture2D).
texture.Update(pixel_data);

// Update a sub-region of a Texture2D, optionally at a mip level.
texture.UpdateRegion(region_pixels, /*x*/ 16, /*y*/ 16, /*width*/ 32, /*height*/ 32);
texture.UpdateRegion(mip1_pixels, 0, 0, 32, 32, /*mip_level*/ 1);

// Update one layer of a Texture2DArray or one face of a CubeMap.
array_texture.UpdateLayer(layer_pixels, /*layer*/ 2);
cubemap.UpdateLayer(face_pixels, /*face*/ 3);
```

All update methods require the data view to be exactly `width * height * pixel_size` bytes for the
targeted region/layer, and report `ErrorCode::InvalidArgument` otherwise; a region, layer or mip level
that does not fit is `ErrorCode::OutOfBounds`. `Update`/`UpdateRegion` are Texture2D-only; `UpdateLayer`
is for `Texture2DArray` and `CubeMap`.

Textures can also be read back to the CPU. Each `Read*` method allocates and returns an
`Opal::DynamicArray<u8>` sized to the requested region/layer:

```cpp
Opal::DynamicArray<u8> pixels = texture.Read();                 // whole base-mip Texture2D
Opal::DynamicArray<u8> region = texture.ReadRegion(16, 16, 32, 32);  // Texture2D sub-region
Opal::DynamicArray<u8> face   = cubemap.ReadLayer(/*face*/ 3);  // one array layer / cube face
```

`Read`/`ReadRegion` are Texture2D-only; `ReadLayer` is for `Texture2DArray` and `CubeMap`. Readback is
not supported for multi-sample textures.

Texture types: `Texture2D`, `Texture2DArray`, `CubeMap`.

Filters: `Nearest`, `Linear`.

Wrap modes: `Clamp`, `Border`, `Repeat`, `MirrorRepeat`, `MirrorOnce`.

### Buffer

General-purpose GPU data buffer for vertex, index, uniform, or storage usage.

```cpp
auto ssbo = Canvas::Buffer::Create(Canvas::BufferUsage::Storage, byte_size, 0, init_data, "InstanceSSBO").GetValue();

// Upload new data.
ssbo.Update(new_data);
```

### RenderTarget

Off-screen surface for rendering to textures. Supports up to 4 color attachments and an optional depth/stencil attachment. Color attachments can be sampled as textures for post-processing. Attachments are either created and owned by the render target, or borrowed from textures the caller owns.

```cpp
auto rt_desc = Canvas::RenderTargetDesc()
    .AddColor(1024, 1024, Canvas::Format::RGBA16F)
    .AddColor(1024, 1024, Canvas::Format::RGBA8)
    .SetDepthStencil(1024, 1024);

auto target = Canvas::RenderTarget::Create(rt_desc, "GBuffer").GetValue();

// Use in a draw list.
draw_list.SetRenderTarget(target);
draw_list.Clear({0, 0, 0, 1});
// ... render scene ...
draw_list.Execute();

// Sample the color attachment as a texture.
post_brush.SetTexture("scene_color", target.GetColorAttachment(0));
```

#### Rendering into an existing texture

`AddColor` and `SetDepthStencil` also accept a `Texture` the caller already owns. The render target
renders into it without taking ownership -- destroying the render target leaves the texture intact, and
the texture must outlive the render target.

Both overloads take an optional mip level and layer. A negative layer, the default, renders into the
whole texture; a non-negative layer selects one array layer or one cube map face.

```cpp
Canvas::TextureDesc face_desc;
face_desc.width = 512;
face_desc.height = 512;
face_desc.type = Canvas::TextureType::CubeMap;
auto cubemap = Canvas::Texture::Create(face_desc).GetValue();

for (Rndr::i32 face = 0; face < 6; ++face)
{
    // Render into one face of a cubemap the caller owns.
    auto face_target = Canvas::RenderTargetDesc().AddColor(cubemap, 0, face);
    auto target = Canvas::RenderTarget::Create(face_target, "CubeFace").GetValue();

    draw_list.SetRenderTarget(target);
    // ... render the face ...
    draw_list.Execute();
}
```

The render target reports `GetWidth()` / `GetHeight()` for the attached mip level, so rendering into
mip 2 of a 1024x1024 texture reports 256x256.

#### Depth-only render targets

A render target with a depth/stencil attachment needs no color attachment. Color output is disabled,
which is what a shadow map pass wants.

```cpp
auto shadow_desc = Canvas::RenderTargetDesc().SetDepthStencil(2048, 2048, Canvas::Format::D32F);
auto shadow_map = Canvas::RenderTarget::Create(shadow_desc, "ShadowMap").GetValue();

draw_list.SetRenderTarget(shadow_map);
draw_list.ClearDepthStencil();
// ... render occluders ...
draw_list.Execute();

lighting_brush.SetTexture("shadow_map", shadow_map.GetDepthStencilAttachment());
```

### Bitmap

CPU-side image storage with per-pixel read/write access. Useful for procedural texture generation or CPU-side image processing before uploading to the GPU.

```cpp
Canvas::Bitmap bitmap(256, 256, 1, Canvas::Format::RGBA8);

// Write pixels.
bitmap.SetPixel(10, 20, 0, {1.0f, 0.0f, 0.0f, 1.0f});

// Read pixels (normalized floats for byte formats, raw for float formats).
Vector4f pixel = bitmap.GetPixel(10, 20);

// Upload to GPU.
Canvas::TextureDesc desc;
desc.width = bitmap.GetWidth();
desc.height = bitmap.GetHeight();
desc.format = bitmap.GetFormat();
auto tex = Canvas::Texture::Create(desc, bitmap.GetDataView()).GetValue();
```

Supported formats: `R8`, `RG8`, `RGB8`, `RGBA8`, `SRGB8`, `SRGBA8`, `R16F`, `RG16F`, `RGBA16F`, `R32F`, `RG32F`, `RGBA32F`.

### VertexLayout

Describes the format of vertex data. Separate from Brush because it's intrinsic to the mesh, not the rendering style. Can be inferred from shader reflection or constructed manually.

```cpp
Canvas::VertexLayout layout;
layout.Add(Canvas::Attrib::Position, Canvas::Format::Float3);
layout.Add(Canvas::Attrib::Normal, Canvas::Format::Float3);
layout.Add(Canvas::Attrib::UV, Canvas::Format::Float2);

u32 stride = layout.GetStride();  // 32 bytes
```

Attributes: `Position`, `Normal`, `UV`, `Color`, `Tangent`.

### Projections

Utility functions for creating projection matrices. Both produce right-handed view space matrices that map Z to [-1, 1].

```cpp
auto ortho = Canvas::Orthographic(-500.0f, 500.0f, -100.0f, 100.0f, 0.1f, 100.0f);
auto persp = Canvas::Perspective(60.0f, width / height, 0.1f, 100.0f);
```

### Format

A unified enum covering both pixel formats and vertex attribute formats:

| Category | Formats |
|---|---|
| Byte pixel | `R8`, `RG8`, `RGB8`, `RGBA8`, `SRGB8`, `SRGBA8` |
| Half-float pixel | `R16F`, `RG16F`, `RGBA16F` |
| Float pixel | `R32F`, `RG32F`, `RGBA32F` |
| Depth/stencil | `D24S8`, `D32F` |
| Vertex float | `Float1`, `Float2`, `Float3`, `Float4` |
| Vertex int | `Int1`, `Int2`, `Int3`, `Int4` |

### ComputeList

Records compute dispatches with the same single-use semantics as DrawList. Note: the DrawList itself also supports `Dispatch()`, so ComputeList is useful when you want to separate compute work from rendering.

```cpp
Canvas::ComputeList compute_list;
compute_list.Dispatch(compute_shader, 64, 64, 1);
compute_list.Execute();
```

### TimestampQuery

GPU timestamp query. Records the moment the GPU reaches a point in the command stream, so you can measure how long GPU work took rather than how long the CPU took to submit it. One query is one point in time -- measuring a range takes two.

```cpp
auto start = Canvas::TimestampQuery::Create("FrameStart").GetValue();
auto end = Canvas::TimestampQuery::Create("FrameEnd").GetValue();

draw_list.WriteTimestamp(start);
draw_list.Draw(mesh, brush);
draw_list.WriteTimestamp(end);
draw_list.Execute();

// Later, once the GPU has caught up.
if (end.IsResultAvailable())
{
    const Rndr::f64 gpu_ms = Canvas::GetElapsedMilliseconds(start, end);
}
```

`Record()` writes a timestamp immediately instead of going through a DrawList, which is handy around code that does not record commands (for example a `Present()`).

Results are not ready when the query is recorded -- they arrive once the GPU executes that point in the stream. `GetResult()` blocks until then and stalls the pipeline; `IsResultAvailable()` and `TryGetResult()` do not. Read timings a frame or two late, keeping one query pair per frame in flight.

Timestamps are in nanoseconds with an implementation-defined origin, so only differences are meaningful. Like all OpenGL query objects, a query belongs to the context that created it and is not shared with other contexts.

### DrawCommandBuffer

Fixed-layout buffer for indirect draw commands. Templated on `DrawCommand` (non-indexed) or `DrawIndexedCommand` (indexed). Indirect drawing is currently commented out in the DrawList but the buffer types are available.

```cpp
Canvas::DrawCommandBuffer<Canvas::DrawIndexedCommand> cmd_buffer(1024);
```

## Built-in Renderers

All built-in renderers follow the same pattern: build with a static `Create` taking a Context reference, call `BeginFrame()` to reset per-frame state, issue draw calls, then call `Render(draw_list)` to record commands into a DrawList.

### ShapeRenderer

Immediate-mode 2D shape drawing. Coordinates are in screen space (pixels). All geometry is batched into a single mesh per frame.

```cpp
auto shapes = Canvas::ShapeRenderer::Create(context).GetValue();

// Each frame:
shapes.BeginFrame();
shapes.DrawRect({10, 10}, {200, 50}, {0.2f, 0.2f, 0.8f, 1.0f});
shapes.DrawCircle({400, 300}, 50.0f, {1, 0, 0, 1});
shapes.DrawLine({0, 0}, {800, 600}, {1, 1, 1, 1}, 2.0f);
shapes.DrawTriangle({100, 100}, {200, 100}, {150, 200}, {0, 1, 0, 1});
shapes.DrawArrow({300, 300}, {1, 0}, {1, 1, 0, 1}, 100.0f);
shapes.DrawBezierCubic({0, 0}, {100, 300}, {300, -100}, {400, 200}, {1, 0, 1, 1}, 2.0f, 16);
shapes.DrawBezierSquare({0, 0}, {200, 300}, {400, 0}, {0, 1, 1, 1});
shapes.Render(draw_list);
```

Available shapes:

| Method | Description |
|---|---|
| `DrawTriangle(a, b, c, color)` | Filled triangle |
| `DrawRect(bottom_left, size, color)` | Filled rectangle |
| `DrawLine(start, end, color, thickness)` | Line segment |
| `DrawArrow(start, direction, color, length, ...)` | Arrow with configurable head/body |
| `DrawCircle(center, radius, color, segments)` | Filled circle |
| `DrawBezierSquare(start, control, end, color, ...)` | Quadratic Bezier curve |
| `DrawBezierCubic(start, c0, c1, end, color, ...)` | Cubic Bezier curve |

### PbrRenderer

Physically-based 3D renderer with directional and point lights. Uses a single shader with a `material_flags` bitmask to select which textures to sample, avoiding shader permutations. Instances sharing the same geometry and texture set are batched into a single instanced draw call via an SSBO.

```cpp
auto pbr = Canvas::PbrRenderer::Create(context).GetValue();

// Each frame:
pbr.BeginFrame();
pbr.SetViewProjection(view_projection);
pbr.SetCameraPosition(camera_pos);

// Lights (up to 4).
pbr.AddDirectionalLight({0.5f, -1.0f, 0.3f}, {1, 1, 1, 1});
pbr.AddPointLight({0, 5, 0}, {1, 0.8f, 0.6f, 1});

// Materials.
Canvas::PbrMaterialDesc material;
material.albedo_color = {0.8f, 0.2f, 0.1f, 1.0f};
material.roughness = {0.4f, 0.4f, 0, 0};
material.metallic_factor = 0.9f;
material.albedo_texture = &brick_texture;  // Optional, overrides albedo_color.

// Draw primitives (geometry is generated and cached).
pbr.DrawCube(model_transform, material);
pbr.DrawSphere(model_transform, material, 1.0f, 1.0f, 32, 32);

// Draw arbitrary meshes (uploaded and cached by key).
pbr.DrawMesh("helmet", mesh_data, model_transform, material);

// Load and draw a model file (.gltf, .obj, etc. via assimp).
auto model = pbr.LoadModel("models/helmet.gltf").GetValue();
pbr.DrawModel("helmet", model, model_transform);

// Submit to draw list.
pbr.Render(draw_list);
```

Display modes:
- `DrawAsLit()` -- Default PBR lighting.
- `DrawAsUnlit()` -- No lighting, show albedo only.
- `DrawAsNormals()` -- Visualize normals.
- `SetDrawFlags(flags)` -- Set custom draw flags.

Material textures (all optional): albedo, emissive, metallic/roughness, normal, ambient occlusion, opacity.

### BitmapTextRenderer

Renders text using a bitmap font atlas generated from a TrueType font via stb_truetype.

```cpp
Canvas::BitmapTextRendererDesc text_desc;
text_desc.font_file_path = "fonts/roboto.ttf";
text_desc.font_size = 32.0f;

auto text = Canvas::BitmapTextRenderer::Create(context, text_desc).GetValue();

// Each frame:
text.BeginFrame();
text.DrawText("Hello, Canvas!", {10, 50}, {1, 1, 1, 1});
text.Render(draw_list);

// Dynamically change font size.
text.UpdateFontSize(48.0f);
```

### GridRenderer

Renders an infinite ground-plane grid with colored axis lines (X in red, Z in blue).

```cpp
auto grid = Canvas::GridRenderer::Create(context).GetValue();

// Each frame (no BeginFrame needed):
grid.Render(draw_list, view_matrix, projection_matrix);
```

### CubemapRenderer

Renders a skybox from a cubemap texture using a full-screen triangle.

```cpp
auto skybox = Canvas::CubemapRenderer::Create(context).GetValue();
skybox.SetCubemap(cubemap_texture);

// Each frame:
skybox.Render(draw_list, inverse_view_projection);
```

## Complete Example

A minimal render loop drawing a PBR cube with a grid and skybox:

```cpp
#include "rndr/canvas/canvas.hpp"
#include "rndr/canvas/renderers/pbr-renderer.hpp"
#include "rndr/canvas/renderers/grid-renderer.hpp"
#include "rndr/canvas/renderers/cubemap-renderer.hpp"
#include "rndr/canvas/projections.hpp"

// Setup (once).
auto context = Canvas::Context::CreateContext(window).GetValue();
auto pbr = Canvas::PbrRenderer::Create(&context).GetValue();
auto grid = Canvas::GridRenderer::Create(&context).GetValue();
auto skybox = Canvas::CubemapRenderer::Create(&context).GetValue();

auto cubemap = Canvas::Texture::FromFile("textures/skybox.ktx").GetValue();
skybox.SetCubemap(cubemap);

Canvas::PbrMaterialDesc material;
material.albedo_color = {0.9f, 0.1f, 0.1f, 1.0f};
material.roughness = {0.3f, 0.3f, 0, 0};
material.metallic_factor = 0.8f;

// Frame loop.
while (running)
{
    auto vp = Canvas::Perspective(60.0f, aspect, 0.1f, 1000.0f) * view;

    Canvas::DrawList draw_list;
    draw_list.SetRenderTarget(context);
    draw_list.Clear({0.05f, 0.05f, 0.05f, 1.0f});

    // Skybox (render first, no depth write).
    skybox.Render(draw_list, inverse_vp);

    // Grid.
    grid.Render(draw_list, view, projection);

    // PBR scene.
    pbr.BeginFrame();
    pbr.SetViewProjection(vp);
    pbr.SetCameraPosition(cam_pos);
    pbr.AddDirectionalLight({1, -1, 1}, {1, 1, 1, 1});
    pbr.DrawCube(Matrix4x4f::Identity(), material);
    pbr.Render(draw_list);

    draw_list.Execute();
    context.Present();
}
```

## Design Notes

- **Move-only semantics** -- GPU resources (Context, Shader, Texture, Buffer, Mesh, RenderTarget, Brush, TimestampQuery) use move-only semantics to prevent accidental resource duplication. Use `Clone()` for explicit deep copies.
- **RAII** -- All GPU resources are released in destructors. Call `Destroy()` for early release.
- **Single-use command lists** -- DrawList and ComputeList record commands then execute and reset. The list objects themselves are reusable across frames.
- **Reflection-driven UBO management** -- The Brush automatically creates GPU uniform buffers from shader reflection, removing the need to manually manage UBO layouts.
- **Geometry caching** -- PbrRenderer caches geometry and batches instances sharing the same mesh and texture set into instanced draw calls.
- **Slang shaders** -- All shaders are written in Slang and cross-compiled to GLSL at runtime.