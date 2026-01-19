package com.google.android.filament.gltf.fw

import android.content.Context
import org.json.JSONObject

/**
 * Helper class to manage triangle range mappings from JSON file.
 *
 * The JSON structure should be:
 * {
 *   "entityId_0": {
 *     "0": "someId",
 *     "120": "anotherId",
 *     "240": "yetAnotherId",
 *     ...
 *   }
 * }
 *
 * Where the keys (0, 120, 240) represent triangle index ranges.
 */
class TriangleMapping(context: Context, jsonAssetPath: String) {
    val mappingData: JSONObject

    init {
        val jsonString = try{
            context.assets.open(jsonAssetPath).use { inputStream ->
                inputStream.bufferedReader().use { it.readText() }
            }
        } catch (e: Exception) {
            logg("TriangleMapping", "Error reading JSON asset at $jsonAssetPath: ${e.message}")
            ""
        }
        mappingData = JSONObject(jsonString)
    }

    /**
     * Get the triangle range for a given entity and triangle index.
     * Returns a pair of (startIndex, endIndex) inclusive, or null if no mapping found.
     *
     * @param entityId The entity identifier
     * @param triangleIndex The vertex index (will be multiplied by 3 to get triangle index)
     */
    fun getTriangleRange(entityId: String, triangleIndex: Int): Pair<Int, Int>? {
        val entityMapping = mappingData.optJSONObject(entityId) ?: return null.also {
            logg("TriangleMapping", "No mapping found for entityId:", entityId)
            logg("TriangleMapping", "Available entityIds:", mappingData.keys().asSequence().toList())
        }

        // Convert vertex index to triangle index by multiplying by 3
        val actualTriangleIndex = triangleIndex * 3

        // Get all range keys and sort them
        val rangeKeys = mutableListOf<Int>()
        entityMapping.keys().forEach { key ->
            rangeKeys.add(key.toInt())
        }
        rangeKeys.sort()

        if (rangeKeys.isEmpty()) return null

        // Use binary search to find the insertion point
        // binarySearch returns (-(insertion point) - 1) if not found
        val searchResult = rangeKeys.binarySearch(actualTriangleIndex)

        val foundIndex = if (searchResult >= 0) {
            // Exact match found
            searchResult
        } else {
            // Not an exact match, get the insertion point
            val insertionPoint = -(searchResult + 1)
            // The range we want is the one before the insertion point
            if (insertionPoint > 0) insertionPoint - 1 else -1
        }

        if (foundIndex != -1 && foundIndex < rangeKeys.size) {
            val rangeStart = rangeKeys[foundIndex]
            val rangeEnd = if (foundIndex < rangeKeys.size - 1) {
                rangeKeys[foundIndex + 1] - 1
            } else {
                Int.MAX_VALUE
            }

            // Verify the triangle actually falls in this range
            if (actualTriangleIndex >= rangeStart && actualTriangleIndex <= rangeEnd) {
                return Pair(rangeStart, rangeEnd)
            }
        }

        return null
    }
}
