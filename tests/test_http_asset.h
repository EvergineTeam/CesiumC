/**
 * @file test_http_asset.h
 * @brief Canned HTTP payloads for the host-callback accessor tests.
 *
 * Strings rather than files, for the same reason as test_gltf_asset.h: no working directory,
 * no network, identical on all seven platforms. These are what the fake host serves.
 *
 * The first document is deliberately a tileset whose root carries no content. That makes it
 * exactly one HTTP request -- fetch tileset.json, root becomes available, nothing further --
 * which is what lets a test assert "the response reached cesium-native" without also
 * depending on b3dm parsing.
 */

#ifndef TEST_HTTP_ASSET_H
#define TEST_HTTP_ASSET_H

static const char* const kTestTilesetUrl = "https://fake.test/tileset.json";

/* One request, and the root is available. geometricError 0 on the root means nothing
   refines, so no child is ever requested. */
static const char* const kTestTilesetJson = R"JSON({
  "asset": { "version": "1.0" },
  "geometricError": 100.0,
  "root": {
    "boundingVolume": { "region": [-0.001, -0.001, 0.001, 0.001, 0.0, 10.0] },
    "geometricError": 0.0,
    "refine": "REPLACE"
  }
})JSON";

/* The same, plus a content URI, to check that a relative reference is resolved against the
   tileset URL rather than passed through. The second request must arrive as
   "https://fake.test/tile.b3dm". */
static const char* const kTestTilesetJsonWithContent = R"JSON({
  "asset": { "version": "1.0" },
  "geometricError": 100.0,
  "root": {
    "boundingVolume": { "region": [-0.001, -0.001, 0.001, 0.001, 0.0, 10.0] },
    "geometricError": 0.0,
    "refine": "REPLACE",
    "content": { "uri": "tile.b3dm" }
  }
})JSON";

static const char* const kExpectedResolvedContentUrl = "https://fake.test/tile.b3dm";

#endif /* TEST_HTTP_ASSET_H */
