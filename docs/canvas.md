# Canvas

The Canvas API is a high-level rendering abstraction over OpenGL. It provides GPU resource management (shaders, textures, buffers, meshes), a command-list based draw model, and built-in renderers for common tasks (2D shapes, PBR, text, skyboxes, grids). Headers live in `include/rndr/canvas/` and implementations in `src/canvas/`. Include `rndr/canvas/canvas.hpp` to pull in all core types.

All types live in the `Rndr::Canvas` namespace.

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

auto context = Canvas::Context::Init(window, desc);

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

// Execute all recorded commands and reset.
draw_list.Execute();
```

All referenced Mesh and Brush objects must remain valid until `Execute()` is called.

### Shader

Shaders are compiled from Slang source to SPIR-V and linked into an OpenGL program. Entry points are auto-discovered from `[shader("vertex")]`, `[shader("fragment")]`, and `[shader("compute")]` annotations. Shader reflection data (uniforms, textures, vertex layout) is extracted automatically.

```cpp
// Single source file with both vertex and fragment entry points.
auto shader = Canvas::Shader::FromSource("shaders/pbr.slang", "PBR");

// Separate vertex and fragment files.
auto shader = Canvas::Shader::FromSources("shaders/vert.slang", "shaders/frag.slang");

// From in-memory source strings.
auto shader = Canvas::Shader::FromSourceInMemory(slang_source);

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

### Mesh

Geometry data paired with a vertex layout. Owns GPU resources (VAO, VBO, optional IBO). Supports both immediate creation and dynamic append/upload for batching.

```cpp
// Create from vertex + index data.
Canvas::VertexLayout layout;
layout.Add(Canvas::Attrib::Position, Canvas::Format::Float3);
layout.Add(Canvas::Attrib::Normal, Canvas::Format::Float3);
layout.Add(Canvas::Attrib::UV, Canvas::Format::Float2);

Canvas::Mesh mesh(layout, vertex_bytes, index_bytes, "Cube");

// Dynamic mesh (pre-allocate, then append per frame).
Canvas::Mesh dynamic_mesh(layout, max_vertices, max_indices, "DynamicMesh");

// Each frame:
dynamic_mesh.Clear();
dynamic_mesh.Append(new_vertex_data, new_index_data);
dynamic_mesh.Upload();
```

Vertex data stride is validated against the layout at construction. Index data uses `u32` indices.

### Texture

GPU texture resource supporting 2D, 2D array, and cubemap types. Loaded from files (PNG, JPEG, HDR via stbi; KTX/KTX2 when advanced API is enabled) or created programmatically.

```cpp
// Load from file.
Canvas::TextureDesc desc;
desc.min_filter = Canvas::TextureFilter::Linear;
desc.mag_filter = Canvas::TextureFilter::Linear;
desc.wrap_u = Canvas::TextureWrap::Repeat;
desc.wrap_v = Canvas::TextureWrap::Repeat;
desc.use_mips = true;

auto texture = Canvas::Texture::FromFile(context, "textures/brick.png", desc, true, "Brick");

// Create programmatically.
Canvas::TextureDesc rt_desc;
rt_desc.width = 1024;
rt_desc.height = 1024;
rt_desc.format = Canvas::Format::RGBA16F;

Canvas::Texture hdr_texture(context, rt_desc, {}, "HDR Buffer");

// Upload new data.
texture.Update(pixel_data);
```

Texture types: `Texture2D`, `Texture2DArray`, `CubeMap`.

Filters: `Nearest`, `Linear`.

Wrap modes: `Clamp`, `Border`, `Repeat`, `MirrorRepeat`, `MirrorOnce`.

### Buffer

General-purpose GPU data buffer for vertex, index, uniform, or storage usage.

```cpp
Canvas::Buffer ssbo(Canvas::BufferUsage::Storage, byte_size, 0, init_data, "InstanceSSBO");

// Upload new data.
ssbo.Update(new_data);
```

### RenderTarget

Off-screen surface for rendering to textures. Supports up to 4 color attachments and an optional depth/stencil attachment. Color attachments can be sampled as textures for post-processing.

```cpp
auto rt_desc = Canvas::RenderTargetDesc()
    .AddColor(1024, 1024, Canvas::Format::RGBA16F)
    .AddColor(1024, 1024, Canvas::Format::RGBA8)
    .SetDepthStencil(1024, 1024);

Canvas::RenderTarget target(context, rt_desc, "GBuffer");

// Use in a draw list.
draw_list.SetRenderTarget(target);
draw_list.Clear({0, 0, 0, 1});
// ... render scene ...
draw_list.Execute();

// Sample the color attachment as a texture.
post_brush.SetTexture("scene_color", target.GetColorAttachment(0));
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
Canvas::Texture tex(context, desc, bitmap.GetDataView());
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

### DrawCommandBuffer

Fixed-layout buffer for indirect draw commands. Templated on `DrawCommand` (non-indexed) or `DrawIndexedCommand` (indexed). Indirect drawing is currently commented out in the DrawList but the buffer types are available.

```cpp
Canvas::DrawCommandBuffer<Canvas::DrawIndexedCommand> cmd_buffer(1024);
```

## Built-in Renderers

All built-in renderers follow the same pattern: construct with a Context reference, call `BeginFrame()` to reset per-frame state, issue draw calls, then call `Render(draw_list)` to record commands into a DrawList.

### ShapeRenderer

Immediate-mode 2D shape drawing. Coordinates are in screen space (pixels). All geometry is batched into a single mesh per frame.

```cpp
Canvas::ShapeRenderer shapes(context);

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
Canvas::PbrRenderer pbr(context);

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
auto model = pbr.LoadModel("models/helmet.gltf");
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

Canvas::BitmapTextRenderer text;
text.Init(context, text_desc);

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
Canvas::GridRenderer grid(context);

// Each frame (no BeginFrame needed):
grid.Render(draw_list, view_matrix, projection_matrix);
```

### CubemapRenderer

Renders a skybox from a cubemap texture using a full-screen triangle.

```cpp
Canvas::CubemapRenderer skybox(context);
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
auto context = Canvas::Context::Init(window);
Canvas::PbrRenderer pbr(&context);
Canvas::GridRenderer grid(&context);
Canvas::CubemapRenderer skybox(&context);

auto cubemap = Canvas::Texture::FromFile(context, "textures/skybox.ktx");
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

- **Move-only semantics** -- GPU resources (Context, Shader, Texture, Buffer, Mesh, RenderTarget, Brush) use move-only semantics to prevent accidental resource duplication. Use `Clone()` for explicit deep copies.
- **RAII** -- All GPU resources are released in destructors. Call `Destroy()` for early release.
- **Single-use command lists** -- DrawList and ComputeList record commands then execute and reset. The list objects themselves are reusable across frames.
- **Reflection-driven UBO management** -- The Brush automatically creates GPU uniform buffers from shader reflection, removing the need to manually manage UBO layouts.
- **Geometry caching** -- PbrRenderer caches geometry and batches instances sharing the same mesh and texture set into instanced draw calls.
- **Slang shaders** -- All shaders are written in Slang and compiled to SPIR-V at runtime.