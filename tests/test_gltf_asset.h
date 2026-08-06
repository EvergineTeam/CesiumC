/**
 * @file test_gltf_asset.h
 * @brief One small glTF 2.0 document, embedded as a string, and the numbers it should
 *        produce.
 *
 * It exists because forty of the forty-nine functions in cesium_gltf.h need a model to read,
 * and the only way to obtain one is cesium_gltf_reader_read. Loading a file from disk would
 * make the tests depend on a working directory; fetching one would make them depend on a
 * network. A string depends on neither and is the same on all seven platforms.
 *
 * Deliberately not minimal. A one-triangle document with no material would exercise the
 * accessors and nothing else, so this carries a texture, a sampler, an image, a two-level
 * node hierarchy and a node using TRS rather than a matrix -- each of which is the only way
 * to reach a particular getter.
 *
 * The buffer is 42 bytes: three vec3 float32 positions (36) followed by three uint16 indices
 * (6). Written out here rather than generated, so the expected values below can be read
 * against it:
 *
 *   positions   (0,0,0)  (1,0,0)  (0,1,0)
 *   indices     0, 1, 2
 */

#ifndef TEST_GLTF_ASSET_H
#define TEST_GLTF_ASSET_H

static const char* const kTestGltf = R"GLTF({
  "asset": { "version": "2.0", "generator": "CesiumC test suite" },
  "scene": 0,
  "scenes": [ { "nodes": [ 0 ] } ],
  "nodes": [
    {
      "name": "root",
      "mesh": 0,
      "children": [ 1 ],
      "translation": [ 1.0, 2.0, 3.0 ],
      "rotation": [ 0.0, 0.0, 0.0, 1.0 ],
      "scale": [ 2.0, 2.0, 2.0 ]
    },
    { "name": "child" }
  ],
  "meshes": [
    {
      "name": "triangle",
      "primitives": [
        {
          "attributes": { "POSITION": 0 },
          "indices": 1,
          "material": 0,
          "mode": 4
        }
      ]
    }
  ],
  "materials": [
    {
      "name": "red",
      "pbrMetallicRoughness": {
        "baseColorFactor": [ 1.0, 0.0, 0.0, 1.0 ],
        "baseColorTexture": { "index": 0 },
        "metallicFactor": 0.25,
        "roughnessFactor": 0.75
      },
      "alphaMode": "OPAQUE",
      "doubleSided": true
    }
  ],
  "textures": [ { "source": 0, "sampler": 0 } ],
  "samplers": [
    { "magFilter": 9729, "minFilter": 9987, "wrapS": 33071, "wrapT": 33071 }
  ],
  "images": [
    { "uri": "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQdz36AAAADElEQVQI12Ng+M8AAAMBAQB7pbHEAAAAAElFTkSuQmCC" }
  ],
  "accessors": [
    {
      "bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
      "min": [ 0.0, 0.0, 0.0 ], "max": [ 1.0, 1.0, 0.0 ]
    },
    { "bufferView": 1, "componentType": 5123, "count": 3, "type": "SCALAR" }
  ],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0,  "byteLength": 36 },
    { "buffer": 0, "byteOffset": 36, "byteLength": 6 }
  ],
  "buffers": [
    {
      "byteLength": 42,
      "uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIA"
    }
  ]
})GLTF";

/* What the document above declares. Written as constants so a test reads as a comparison
   rather than as a pile of magic numbers, and so a change to the document has one place to
   update. */
enum {
    kExpectedSceneCount = 1,
    kExpectedNodeCount = 2,
    kExpectedMeshCount = 1,
    kExpectedMaterialCount = 1,
    kExpectedTextureCount = 1,
    kExpectedImageCount = 1,
    kExpectedAccessorCount = 2,
    kExpectedBufferCount = 1,
    kExpectedBufferViewCount = 2,
    kExpectedAnimationCount = 0,
    kExpectedSkinCount = 0,

    kPositionAccessor = 0,
    kIndicesAccessor = 1,
    kVertexCount = 3,

    /* glTF component types, from the specification. */
    kComponentTypeUnsignedShort = 5123,
    kComponentTypeFloat = 5126,
    /* Primitive mode 4 is TRIANGLES. */
    kModeTriangles = 4,
    /* Sampler filter and wrap constants, as written in the document above. */
    kMagFilterLinear = 9729,
    kMinFilterLinearMipmapLinear = 9987,
    kWrapClampToEdge = 33071
};

#endif /* TEST_GLTF_ASSET_H */
