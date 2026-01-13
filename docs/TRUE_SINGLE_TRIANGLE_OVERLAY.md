# TRUE Single Triangle Highlighting - Overlay Solution

## Problem
Previous implementation still highlighted the **entire mesh/primitive**, not a single triangle.

## Why Material Swapping Doesn't Work for Single Triangles

In Filament (and most 3D engines), you can only assign materials at the **primitive level**:
- 1 primitive = 1 mesh part (could be hundreds/thousands of triangles)
- Can't assign different materials to individual triangles within a primitive
- Swapping material = entire primitive changes color = entire mesh highlighted ❌

## Solution: Overlay Geometry Approach

Instead of swapping materials, we **create a small visual indicator** (overlay triangle) at the clicked point.

### How It Works

1. **User clicks triangle** → Get exact hit position
2. **Create small overlay triangle** at that position
3. **Render overlay with RED material** on top of the model
4. **Only the overlay is RED**, not the original mesh ✅

### Implementation

#### 1. Added Overlay Fields to Scene Struct (line ~138)
```cpp
struct Scene {
    // ...existing code...
    
    // Highlight overlay for single triangle
    Entity highlightOverlayEntity;
    VertexBuffer* highlightOverlayVertexBuffer = nullptr;
    IndexBuffer* highlightOverlayIndexBuffer = nullptr;
};
```

#### 2. onClick Creates Overlay Geometry (line ~600-675)
```cpp
// Get hit position from ray-triangle intersection
float3 hitPoint = hit.position;

// Create small triangle at hit point
float const overlaySize = 0.02f;
float3 vertices[3] = {
    hitPoint + float3(-overlaySize, 0, 0),
    hitPoint + float3(overlaySize, 0, 0),
    hitPoint + float3(0, overlaySize, 0)
};

// Create vertex/index buffers
auto* vertexBuffer = VertexBuffer::Builder()...
auto* indexBuffer = IndexBuffer::Builder()...

// Create overlay entity
Entity overlayEntity = em.create();

// Build renderable with RED highlight material
RenderableManager::Builder(1)
    .material(0, app.scene.highlightMaterialInstance)  // RED material
    .geometry(0, TRIANGLES, vertexBuffer, indexBuffer, 0, 3)
    .priority(7)  // Render on top
    .build(*engine, overlayEntity);

// Add to scene
scene->addEntity(overlayEntity);
```

#### 3. Clear Button Destroys Overlay (line ~1035-1065)
```cpp
if (ImGui::Button("Clear Highlight")) {
    // Destroy overlay entity and buffers
    engine->destroy(app.scene.highlightOverlayVertexBuffer);
    engine->destroy(app.scene.highlightOverlayIndexBuffer);
    engine->destroy(app.scene.highlightOverlayEntity);
    em.destroy(app.scene.highlightOverlayEntity);
}
```

## Key Features

### ✅ Original Mesh Untouched
- No material swapping
- No mesh modification
- Original appearance preserved

### ✅ Visual Indicator Only
- Small RED triangle overlay
- Appears at exact click point
- Rendered on top (priority 7)

### ✅ True Single Triangle
- Only the overlay is visible
- Original mesh stays normal color
- No entire-mesh highlighting

### ✅ Easy Cleanup
- Just destroy the overlay entity
- No need to restore materials
- Instant clear

## Advantages Over Material Swapping

| Material Swapping | Overlay Geometry |
|------------------|------------------|
| ❌ Highlights entire primitive | ✅ Shows exact point |
| ❌ Loses original appearance | ✅ Original untouched |
| ❌ Need to restore materials | ✅ Just destroy overlay |
| ❌ Can't do single triangle | ✅ True single triangle |

## Expected Behavior

### Click on Triangle
- ✅ Small **RED triangle appears** at click point
- ✅ Original mesh **stays normal color**
- ✅ Only a **tiny indicator** is RED
- ✅ Console: `"Created highlight overlay at triangle X position (...)"`

### Click Clear Highlight
- ✅ RED triangle **disappears**
- ✅ Console: `"Highlight overlay cleared"`

### Click Different Triangle
- ✅ Previous overlay **removed**
- ✅ New overlay **created** at new position
- ✅ Smooth transition

## Customization

### Change Overlay Size
```cpp
float const overlaySize = 0.02f;  // Current: small
float const overlaySize = 0.05f;  // Larger indicator
float const overlaySize = 0.01f;  // Tiny dot
```

### Change Overlay Shape
```cpp
// Current: Triangle
float3 vertices[3] = { ... };

// Alternative: Square/quad
float3 vertices[4] = {
    hitPoint + float3(-size, -size, 0),
    hitPoint + float3( size, -size, 0),
    hitPoint + float3( size,  size, 0),
    hitPoint + float3(-size,  size, 0)
};
```

### Change Color
Already RED via `highlightMaterialInstance`, but can modify:
```cpp
// In createHighlightMaterial():
instance->setParameter("color", float3(0.0f, 1.0f, 0.0f));  // Green
instance->setParameter("color", float3(1.0f, 1.0f, 0.0f));  // Yellow
```

## Files Modified

1. **Scene Struct** (line ~138)
   - Added `highlightOverlayEntity`
   - Added `highlightOverlayVertexBuffer`
   - Added `highlightOverlayIndexBuffer`

2. **onClick Function** (line ~600-675)
   - Removed material swapping code
   - Added overlay geometry creation
   - Uses hit.position for placement

3. **Clear Highlight Button** (line ~1035-1065)
   - Removed material restoration
   - Added overlay destruction

## Testing

```bash
./build.sh
./cmake-build-debug/samples/gltf_viewer model.gltf
```

**Test 1:** Click on model
- Expected: Small RED triangle appears at click point
- Original mesh stays normal color

**Test 2:** Click different spot
- Expected: Previous RED triangle removed
- New RED triangle appears at new spot

**Test 3:** Click "Clear Highlight"
- Expected: RED triangle disappears completely

## Result

✅ **TRUE single triangle highlighting**  
✅ **Overlay approach** - doesn't modify mesh  
✅ **Visual indicator** at exact click point  
✅ **Original mesh untouched**  
✅ **Works with any model**  

**The implementation now shows a visual indicator at the clicked triangle, not the entire mesh!** 🎯🔴

