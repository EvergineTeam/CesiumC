/**
 * @file cesium_gltf.cpp
 * @brief C wrapper for CesiumGltfReader and read-only Model accessors.
 */

#include "cesium_internal.h"

#include <cesium/cesium_gltf.h>

#include <CesiumGltf/Model.h>
#include <CesiumGltfReader/GltfReader.h>

#include <cstddef>
#include <span>
#include <string>
#include <vector>

using namespace CesiumGltf;

// ---------- Opaque handle storage ----------

struct CesiumCGltfReader_T {
    CesiumGltfReader::GltfReader reader;
};

struct CesiumCGltfReaderResult_T {
    CesiumGltfReader::GltfReaderResult result;
};

// Cast helpers
static inline const CesiumCGltfReader_T* asReader(const CesiumCGltfReader* h) {
    return reinterpret_cast<const CesiumCGltfReader_T*>(h);
}

static inline const CesiumCGltfReaderResult_T* asResult(const CesiumCGltfReaderResult* h) {
    return reinterpret_cast<const CesiumCGltfReaderResult_T*>(h);
}

static inline const Model* getModel(const CesiumCGltfReaderResult* h) {
    auto* r = asResult(h);
    if (r && r->result.model.has_value()) {
        return &r->result.model.value();
    }
    return nullptr;
}

static inline const Model* asModel(const CesiumGltfModel* h) {
    return reinterpret_cast<const Model*>(h);
}

extern "C" {

// ---------- GltfReader ----------

CESIUM_API CesiumCGltfReader* cesium_gltf_reader_create(void) {
    CESIUM_TRY_BEGIN
    auto* wrapper = new CesiumCGltfReader_T();
    return reinterpret_cast<CesiumCGltfReader*>(wrapper);
    CESIUM_TRY_END
    return nullptr;
}

CESIUM_API void cesium_gltf_reader_destroy(CesiumCGltfReader* reader) {
    if (!reader) return;
    delete reinterpret_cast<CesiumCGltfReader_T*>(reader);
}

CESIUM_API CesiumCGltfReaderResult* cesium_gltf_reader_read(
    const CesiumCGltfReader* reader,
    const uint8_t* data,
    size_t data_size)
{
    CESIUM_TRY_BEGIN
    if (!reader || !data || data_size == 0) {
        cesium_set_last_error("Invalid arguments to cesium_gltf_reader_read");
        return nullptr;
    }
    const auto* r = asReader(reader);
    std::span<const std::byte> span(reinterpret_cast<const std::byte*>(data), data_size);
    auto* wrapper = new CesiumCGltfReaderResult_T();
    wrapper->result = r->reader.readGltf(span);
    return reinterpret_cast<CesiumCGltfReaderResult*>(wrapper);
    CESIUM_TRY_END
    return nullptr;
}

// ---------- GltfReaderResult ----------

CESIUM_API void cesium_gltf_reader_result_destroy(CesiumCGltfReaderResult* result) {
    if (!result) return;
    delete reinterpret_cast<CesiumCGltfReaderResult_T*>(result);
}

CESIUM_API int cesium_gltf_reader_result_has_model(const CesiumCGltfReaderResult* result) {
    if (!result) return 0;
    return asResult(result)->result.model.has_value() ? 1 : 0;
}

CESIUM_API int cesium_gltf_reader_result_get_error_count(const CesiumCGltfReaderResult* result) {
    if (!result) return 0;
    return static_cast<int>(asResult(result)->result.errors.size());
}

CESIUM_API const char* cesium_gltf_reader_result_get_error(
    const CesiumCGltfReaderResult* result,
    int index)
{
    if (!result) return "";
    const auto& errors = asResult(result)->result.errors;
    if (index < 0 || index >= static_cast<int>(errors.size())) return "";
    return errors[index].c_str();
}

CESIUM_API int cesium_gltf_reader_result_get_warning_count(const CesiumCGltfReaderResult* result) {
    if (!result) return 0;
    return static_cast<int>(asResult(result)->result.warnings.size());
}

CESIUM_API const char* cesium_gltf_reader_result_get_warning(
    const CesiumCGltfReaderResult* result,
    int index)
{
    if (!result) return "";
    const auto& warnings = asResult(result)->result.warnings;
    if (index < 0 || index >= static_cast<int>(warnings.size())) return "";
    return warnings[index].c_str();
}

// ---------- Model accessors ----------

CESIUM_API const CesiumGltfModel* cesium_gltf_reader_result_get_model(
    const CesiumCGltfReaderResult* result)
{
    const Model* model = getModel(result);
    return reinterpret_cast<const CesiumGltfModel*>(model);
}

CESIUM_API int cesium_gltf_model_get_mesh_count(const CesiumGltfModel* model) {
    if (!model) return 0;
    return static_cast<int>(asModel(model)->meshes.size());
}

CESIUM_API const char* cesium_gltf_model_get_mesh_name(
    const CesiumGltfModel* model,
    int meshIndex)
{
    if (!model) return "";
    const auto& meshes = asModel(model)->meshes;
    if (meshIndex < 0 || meshIndex >= static_cast<int>(meshes.size())) return "";
    return meshes[meshIndex].name.c_str();
}

CESIUM_API int cesium_gltf_model_get_material_count(const CesiumGltfModel* model) {
    if (!model) return 0;
    return static_cast<int>(asModel(model)->materials.size());
}

CESIUM_API int cesium_gltf_model_get_texture_count(const CesiumGltfModel* model) {
    if (!model) return 0;
    return static_cast<int>(asModel(model)->textures.size());
}

CESIUM_API int cesium_gltf_model_get_image_count(const CesiumGltfModel* model) {
    if (!model) return 0;
    return static_cast<int>(asModel(model)->images.size());
}

CESIUM_API int cesium_gltf_model_get_node_count(const CesiumGltfModel* model) {
    if (!model) return 0;
    return static_cast<int>(asModel(model)->nodes.size());
}

CESIUM_API int cesium_gltf_model_get_accessor_count(const CesiumGltfModel* model) {
    if (!model) return 0;
    return static_cast<int>(asModel(model)->accessors.size());
}

CESIUM_API int cesium_gltf_model_get_buffer_count(const CesiumGltfModel* model) {
    if (!model) return 0;
    return static_cast<int>(asModel(model)->buffers.size());
}

CESIUM_API int cesium_gltf_model_get_buffer_view_count(const CesiumGltfModel* model) {
    if (!model) return 0;
    return static_cast<int>(asModel(model)->bufferViews.size());
}

CESIUM_API int cesium_gltf_model_get_scene_count(const CesiumGltfModel* model) {
    if (!model) return 0;
    return static_cast<int>(asModel(model)->scenes.size());
}

CESIUM_API int cesium_gltf_model_get_animation_count(const CesiumGltfModel* model) {
    if (!model) return 0;
    return static_cast<int>(asModel(model)->animations.size());
}

CESIUM_API int cesium_gltf_model_get_skin_count(const CesiumGltfModel* model) {
    if (!model) return 0;
    return static_cast<int>(asModel(model)->skins.size());
}

} // extern "C"
