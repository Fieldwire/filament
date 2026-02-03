# TinyBVH Picking Fix - Changes Summary

## ✅ IT WORKS NOW! Here's what changed:

---

## 🔴 CRITICAL FIX #1: Double-Move Bug
**File:** `libs/gltfio/src/AssetLoader.cpp` (line ~904)

**What was wrong:**
```cpp
fAsset->mPickingRegistry.registerMesh(entity, std::move(meshData));
fAsset->mTinyBVHPickingRegistry.registerMesh(entity, std::move(meshData)); // ❌ EMPTY!
```

**What we fixed:**
```cpp
MeshData meshDataCopy = meshData;  // ✅ Make copy first
fAsset->mPickingRegistry.registerMesh(entity, std::move(meshData));
fAsset->mTinyBVHPickingRegistry.registerMesh(entity, std::move(meshDataCopy)); // ✅ Has data!
```

**Result:** TinyBVH went from **0 vertices** → **14,556 vertices** ✅

---

## 🔴 CRITICAL FIX #2: Missing rD Initialization
**File:** `libs/gltfio/src/TinyBVHPickingRegistry.cpp` (3 places: lines ~379, ~475, ~567)

**What was wrong:**
```cpp
tinybvh::Ray ray;
ray.O = tinybvh::bvhvec3(localO.x, localO.y, localO.z);
ray.D = tinybvh::bvhvec3(localD.x, localD.y, localD.z);
ray.hit.t = best.distance;
// ❌ ray.rD was NEVER set! (needed for AABB tests)
```

**What we fixed:**
```cpp
tinybvh::bvhvec3 origin(localO.x, localO.y, localO.z);
tinybvh::bvhvec3 direction(localD.x, localD.y, localD.z);
tinybvh::Ray ray(origin, direction, best.distance);
// ✅ Constructor automatically sets rD = 1/D
```

**Result:** BVH can now traverse the tree and find intersections ✅

---

## 🔵 BONUS: Debug Output Added
**File:** `samples/gltf_viewer.cpp` (line ~646)

Added comparison output to verify both registries work:
```
Entity 0: PickingRegistry ✓ (14556 verts), TinyBVH ✓ (14556 verts)
PickingRegistry HIT: triangle=5999, distance=4.23866
TinyBVH HIT: triangle=5999, distance=4.23866
```

This is optional debug output - can be removed if you don't need it.

---

## 📊 Total Changes

| File | Critical? | What Changed |
|------|-----------|--------------|
| AssetLoader.cpp | ✅ YES | Added 3 lines to copy mesh data |
| TinyBVHPickingRegistry.cpp | ✅ YES | Changed 12 lines (3 locations) for Ray init |
| gltf_viewer.cpp | 🔵 Optional | Added ~50 lines of debug output |

**Total critical code changes: ~15 lines**

---

## 🎯 Why It Was Broken

1. **Double-move:** In C++, `std::move()` transfers ownership and empties the source. Can't move twice!
2. **Missing rD:** TinyBVH needs `rD` (reciprocal direction = 1/D) for AABB intersection math. Without it, rays can't traverse the BVH tree.

---

## ✅ Verification

**BEFORE:**
- TinyBVH had 0 vertices ❌
- TinyBVH returned "NO HIT" ❌
- Error: "BVH not built" ❌

**AFTER:**
- TinyBVH has 14,556 vertices ✅
- TinyBVH HIT: triangle=5999 ✅
- Both registries pick the same triangle ✅

---

