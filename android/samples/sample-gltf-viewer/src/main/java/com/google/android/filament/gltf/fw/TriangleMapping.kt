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
    private val mappingData: JSONObject

    init {
        val jsonString = context.assets.open(jsonAssetPath).use { inputStream ->
            inputStream.bufferedReader().use { it.readText() }
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
        val entityMapping = mappingData.optJSONObject(entityId) ?: mappingData.optJSONObject(mappingData.keys().next())

        // Convert vertex index to triangle index by multiplying by 3
        val actualTriangleIndex = triangleIndex * 3

        // Get all range keys and sort them
        val rangeKeys = mutableListOf<Int>()
        entityMapping.keys().forEach { key ->
            rangeKeys.add(key.toInt())
        }
        rangeKeys.sort()

        if (rangeKeys.isEmpty()) return null

        // Find which range the triangle falls into
        for (i in rangeKeys.indices) {
            val rangeStart = rangeKeys[i]
            val rangeEnd = if (i < rangeKeys.size - 1) {
                rangeKeys[i + 1] - 1
            } else {
                // For the last range, use a large number or you can determine max from data
                Int.MAX_VALUE
            }

            if (actualTriangleIndex >= rangeStart && actualTriangleIndex <= rangeEnd) {
                return Pair(rangeStart, rangeEnd)
            }
        }

        return null
    }
}
