package com.google.android.filament.utils

import com.google.android.filament.Engine
import com.google.android.filament.IndexBuffer
import com.google.android.filament.VertexBuffer
import com.google.android.filament.VertexBuffer.AttributeType
import com.google.android.filament.VertexBuffer.VertexAttribute
import java.nio.Buffer
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.sin

class Mesher(val engine: Engine) {

    fun createRhombus(): Buffers {
        val floatSize = 4
        val shortSize = 2
        // A vertex is a position + a color:
        // 3 floats for XYZ position, 1 integer for color
        val vertexSize = 3 * floatSize/**/

        val vertices = floatArrayOf(
            -1f, -0.5f, 0.05f,   // Bottom left coordinate
            1f, -0.5f, 0.05f,   // Bottom right coordinate
            1f, 0.5f, -3f,  // Top right coordinate
            -1f, 0.5f, -3f   // Top left coordinate
        )

        val vertexData = ByteBuffer.allocate(vertices.count() * vertexSize)
            // It is important to respect the native byte order
            .order(ByteOrder.nativeOrder())
            .apply { vertices.forEach { putFloat(it) } }
            // Make sure the cursor is pointing in the right place in the byte buffer
            .flip()

        // Declare the layout of our mesh
        val vertexBuffer = VertexBuffer.Builder()
            .bufferCount(2)
            .vertexCount(vertices.count())
            // Because we interleave position and color data we must specify offset and stride
            // We could use de-interleaved data by declaring two buffers and giving each
            // attribute a different buffer index
            .attribute(VertexAttribute.POSITION, 0, AttributeType.FLOAT3)
            .attribute(VertexAttribute.UV0, 1, AttributeType.FLOAT2)
            .build(engine)

        // Feed the vertex data to the mesh
        // We only set 1 buffer because the data is interleaved
        vertexBuffer.setBufferAt(engine, 0, vertexData)

        // UV coordinates for placing a water drop-like image
        val uvCoordinates = floatArrayOf(
            0.0f, 1.0f,   // UV coordinate for Vertex 1 (Top left)
            0.0f, 0.0f,   // UV coordinate for Vertex 2 (Bottom left) - flipped
            1.0f, 0.0f,   // UV coordinate for Vertex 3 (Bottom right) - flipped
            1.0f, 1.0f    // UV coordinate for Vertex 4 (Top right)
        )
        vertexBuffer.setBufferAt(engine, 1, ByteBuffer.allocate(4 * 2 * 4)
            .order(ByteOrder.nativeOrder())
            .apply { uvCoordinates.forEach { putFloat(it) } }
            .flip()
        )

        // Create the indices
        val indexData = ByteBuffer.allocate(6 * shortSize)
            .order(ByteOrder.nativeOrder())
            .putShort(0)
            .putShort(1)
            .putShort(2)
            .putShort(0)
            .putShort(2)
            .putShort(3)
            .flip()

        val indexBuffer = IndexBuffer.Builder()
            .indexCount(6)
            .bufferType(IndexBuffer.Builder.IndexType.USHORT)
            .build(engine)
        indexBuffer.setBuffer(engine, indexData)

        return Buffers(vertexBuffer, indexBuffer)
    }

    fun createSquareMeshWithTexture(x: Float, y: Float, z: Float): Buffers {
        val floatSize = 4
        val shortSize = 2
        // A vertex is a position + a color:
        // 3 floats for XYZ position, 1 integer for color
        val vertexSize = 3 * floatSize

        val xx = 100f;
        val yy = 100f

        val topLeft = Float3(x - xx, y - yy, z)
        val botLeft = Float3(x - xx, y + yy, z)
        val botRight = Float3(x + xx, y + yy, z)
        val topRight = Float3(x + xx, y - yy, z)

        logg("x: $x y: $y topLeft: $topLeft botRight: $botRight botLeftX: ${botLeft.x}")

        val vertices = floatArrayOf(
            topLeft.x, topLeft.y, topLeft.z,   // Vertex 2 BottomLeft
            botLeft.x, botLeft.y, botLeft.z,
            botRight.x, botRight.y, botRight.z,
            topRight.x, topRight.y, topRight.z
        )

        val vertexData = ByteBuffer.allocate(vertices.count() * vertexSize)
            // It is important to respect the native byte order
            .order(ByteOrder.nativeOrder())
            .apply { vertices.forEach { putFloat(it) } }
            // Make sure the cursor is pointing in the right place in the byte buffer
            .flip()

        // Declare the layout of our mesh
        val vertexBuffer = VertexBuffer.Builder()
            .bufferCount(2)
            .vertexCount(vertices.count())
            // Because we interleave position and color data we must specify offset and stride
            // We could use de-interleaved data by declaring two buffers and giving each
            // attribute a different buffer index
            .attribute(VertexAttribute.POSITION, 0, AttributeType.FLOAT3)
            .attribute(VertexAttribute.UV0, 1, AttributeType.FLOAT2)
            .build(engine)

        // Feed the vertex data to the mesh
        // We only set 1 buffer because the data is interleaved
        vertexBuffer.setBufferAt(engine, 0, vertexData)

        // UV coordinates for placing a water drop-like image
        val uvCoordinates = floatArrayOf(
            topLeft.x, topLeft.y, // UV coordinate for Vertex 1 (Top left)
            botLeft.x, botLeft.y, // UV coordinate for Vertex 2 (Bottom left) - flipped
            botRight.x, botRight.y, // UV coordinate for Vertex 3 (Bottom right) - flipped
            topRight.x, topRight.y// UV coordinate for Vertex 4 (Top right)
        )
        vertexBuffer.setBufferAt(engine, 1, ByteBuffer.allocate(4 * 2 * 4)
            .order(ByteOrder.nativeOrder())
            .apply { uvCoordinates.forEach { putFloat(it) } }
            .flip()
        )

        // Create the indices
        val indexData = ByteBuffer.allocate(6 * shortSize)
            .order(ByteOrder.nativeOrder())
            .putShort(0)
            .putShort(1)
            .putShort(2)
            .putShort(0)
            .putShort(2)
            .putShort(3)
            .flip()

        val indexBuffer = IndexBuffer.Builder()
            .indexCount(6)
            .bufferType(IndexBuffer.Builder.IndexType.USHORT)
            .build(engine)
        indexBuffer.setBuffer(engine, indexData)

        return Buffers(vertexBuffer, indexBuffer)
    }

    private fun createMesh(engine: Engine): Buffers {
        val intSize = 4
        val floatSize = 4
        val shortSize = 2
        // A vertex is a position + a color:
        // 3 floats for XYZ position, 1 integer for color
        val vertexSize = 3 * floatSize + intSize

        // Define a vertex and a function to put a vertex in a ByteBuffer
        data class Vertex(val x: Float, val y: Float, val z: Float, val color: Int)

        fun ByteBuffer.put(v: Vertex): ByteBuffer {
            putFloat(v.x)
            putFloat(v.y)
            putFloat(v.z)
            putInt(v.color)
            return this
        }

        // We are going to generate a single triangle
        val vertexCount = 3
        val a1 = PI * 2.0 / 3.0
        val a2 = PI * 4.0 / 3.0

        val vertexData = ByteBuffer.allocate(vertexCount * vertexSize)
            // It is important to respect the native byte order
            .order(ByteOrder.nativeOrder())
            .put(Vertex(1.0f, 0.0f, 0.0f, 0xffff0000.toInt()))
            .put(Vertex(cos(a1).toFloat(), sin(a1).toFloat(), 0.0f, 0xff00ff00.toInt()))
            .put(Vertex(cos(a2).toFloat(), sin(a2).toFloat(), 0.0f, 0xff0000ff.toInt()))
            // Make sure the cursor is pointing in the right place in the byte buffer
            .flip()

        // Declare the layout of our mesh
        val vertexBuffer = VertexBuffer.Builder()
            .bufferCount(1)
            .vertexCount(vertexCount)
            // Because we interleave position and color data we must specify offset and stride
            // We could use de-interleaved data by declaring two buffers and giving each
            // attribute a different buffer index
            .attribute(VertexAttribute.POSITION, 0, AttributeType.FLOAT3, 0, vertexSize)
            .attribute(VertexAttribute.COLOR, 0, AttributeType.UBYTE4, 3 * floatSize, vertexSize)
            // We store colors as unsigned bytes but since we want values between 0 and 1
            // in the material (shaders), we must mark the attribute as normalized
            //.normalized(VertexAttribute.COLOR)
            .build(engine)

        // Feed the vertex data to the mesh
        // We only set 1 buffer because the data is interleaved
        vertexBuffer.setBufferAt(engine, 0, vertexData)

        // Create the indices
        val indexData = ByteBuffer.allocate(vertexCount * shortSize)
            .order(ByteOrder.nativeOrder())
            .putShort(0)
            .putShort(1)
            .putShort(2)
            .flip()

        val indexBuffer = IndexBuffer.Builder()
            .indexCount(3)
            .bufferType(IndexBuffer.Builder.IndexType.USHORT)
            .build(engine)
        indexBuffer.setBuffer(engine, indexData)

        return Buffers(vertexBuffer, indexBuffer)
    }

    private fun createTriangle(vertexSize: Int): Buffer {
        // Define a vertex and a function to put a vertex in a ByteBuffer
        data class Vertex(val x: Float, val y: Float, val z: Float, val color: Int)

        fun ByteBuffer.put(v: Vertex): ByteBuffer {
            putFloat(v.x)
            putFloat(v.y)
            putFloat(v.z)
            putInt(v.color)
            return this
        }

        // We are going to generate a single triangle
        val vertexCount = 3
        val a1 = PI * 2.0 / 3.0
        val a2 = PI * 4.0 / 3.0

        val vertexData = ByteBuffer.allocate(vertexCount * vertexSize)
            // It is important to respect the native byte order
            .order(ByteOrder.nativeOrder())
            .put(Vertex(1.0f, 0.0f, 0.0f, 0xffff0000.toInt()))
            .put(Vertex(cos(a1).toFloat(), sin(a1).toFloat(), 0.0f, 0xff00ff00.toInt()))
            .put(Vertex(cos(a2).toFloat(), sin(a2).toFloat(), 0.0f, 0xff0000ff.toInt()))
            // Make sure the cursor is pointing in the right place in the byte buffer
            .flip()

        return vertexData
    }

    fun createSquare(engine: Engine): Buffers {
        val floatSize = 4
        val shortSize = 2

        val vertices = floatArrayOf(
            -0.5f, 0.5f, 0.0f,
            -0.5f, -0.5f, 0.0f,
            0.5f, -0.5f, 0.0f,

            -0.5f, 0.5f, 0.0f,
            0.5f, -0.5f, 0.0f,
            0.5f, 0.5f, 0.0f
        )

        val vertexCount = vertices.size / 3 // 3 points equal 1 vertex
        val vertexSize = floatSize * 3 // 3 = xyz of one vertex

        val vertexData = ByteBuffer.allocate(vertexCount * vertexSize)
            // It is important to respect the native byte order
            .order(ByteOrder.nativeOrder())
            .apply { vertices.forEach { putFloat(it) } }
            // Make sure the cursor is pointing in the right place in the byte buffer
            .flip()

        // Declare the layout of our mesh
        val vertexBuffer = VertexBuffer.Builder()
            .bufferCount(1)
            .vertexCount(vertexCount)
            // Because we interleave position and color data we must specify offset and stride
            // We could use de-interleaved data by declaring two buffers and giving each
            // attribute a different buffer index
            .attribute(VertexAttribute.POSITION, 0, AttributeType.FLOAT3, 0, vertexSize)
            //.attribute(VertexAttribute.COLOR, 0, AttributeType.UBYTE4, 3 * floatSize, vertexSize)
            // We store colors as unsigned bytes but since we want values between 0 and 1
            // in the material (shaders), we must mark the attribute as normalized
            //.normalized(VertexAttribute.COLOR)
            .build(engine)

        // Feed the vertex data to the mesh
        // We only set 1 buffer because the data is interleaved
        vertexBuffer.setBufferAt(engine, 0, vertexData)

        // Create the indices
        val indexData = ByteBuffer.allocate(vertexCount * shortSize)
            .order(ByteOrder.nativeOrder())
            .putShort(0)
            .putShort(1)
            .putShort(2)
            .putShort(3)
            .putShort(4)
            .putShort(5)
            .flip()

        val indexBuffer = IndexBuffer.Builder()
            .indexCount(6)
            .bufferType(IndexBuffer.Builder.IndexType.USHORT)
            .build(engine)
        indexBuffer.setBuffer(engine, indexData)

        val uvCoordinates = shortArrayOf(
            0, 0,   // UV coordinate for Vertex 1
            0, 1,   // UV coordinate for Vertex 2
            1, 1,   // UV coordinate for Vertex 3
            1, 0    // UV coordinate for Vertex 4
        )
        val uvVertexCount = uvCoordinates.size / 2
        val uvVertexSize = shortSize * 2

        val vertexUvData = ByteBuffer.allocate(uvVertexCount * uvVertexSize)
            // It is important to respect the native byte order
            .order(ByteOrder.nativeOrder())
            .apply { uvCoordinates.forEach { putShort(it) } }
            // Make sure the cursor is pointing in the right place in the byte buffer
            .flip()

        val vertexUvBuffer = VertexBuffer.Builder()
            .bufferCount(1)
            .vertexCount(uvVertexCount)
            .attribute(VertexAttribute.UV0, 1, AttributeType.SHORT2, 0, uvVertexSize)
            .build(engine)

        vertexUvBuffer.setBufferAt(engine, 1, vertexUvData)

        // Create the indices
        val indexDataUv = ByteBuffer.allocate(uvVertexCount * shortSize)
            .order(ByteOrder.nativeOrder())
            .putShort(0)
            .putShort(1)
            .putShort(2)
            .putShort(3)
            .flip()

        val indexBufferUv = IndexBuffer.Builder()
            .indexCount(4)
            .bufferType(IndexBuffer.Builder.IndexType.USHORT)
            .build(engine)
        indexBufferUv.setBuffer(engine, indexDataUv)

        return Buffers(vertexBuffer, indexBuffer)
    }
}
