# TinyBVH - Fast BVH-Based Ray Tracing

## Source

`tiny_bvh.h` was copied from [https://github.com/jbikker/tinybvh](https://github.com/jbikker/tinybvh) as of commit **4b5b649**.

## Purpose

This library is used for **picking large models with even millions of triangles**. TinyBVH provides an efficient Bounding Volume Hierarchy (BVH) data structure that enables fast ray-triangle intersection tests, making it suitable for:

- Interactive triangle picking in large 3D models
- Real-time ray tracing against complex geometry
- Efficient spatial queries on massive triangle meshes

## Implementation Details

### Triangle Range Filtering with Callbacks

The **callback feature** allows skipping specific triangle ranges during ray traversal, which is essential for:
- Skipping a range of continuous triangles which helps picking the next object behind the hidden one

### Accepted Tradeoff

To use the callback feature effectively, **we have to write the Möller-Trumbore ray-triangle intersection algorithm ourselves** in the callback implementation. 

**Why?** The callback intercepts the intersection test before TinyBVH's built-in algorithm runs, giving us control to skip triangles but requiring us to implement the intersection math.

**Solution:** The Möller-Trumbore algorithm implementation can be **copied directly from the `tiny_bvh.h` file**, ensuring consistency with TinyBVH's internal implementation.

## License

TinyBVH is licensed under the MIT License. See the license header in `tiny_bvh.h` for full details.

Copyright (c) 2024-2025, Jacco Bikker / Breda University of Applied Sciences.

