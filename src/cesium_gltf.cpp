/**
 * @file cesium_gltf.cpp
 * @brief C wrapper for CesiumGltfReader and read-only Model accessors.
 */

#include "cesium_internal.h"

#include <cesium/cesium_gltf.h>

#include <CesiumGltf/Model.h>
#include <CesiumGltf/Accessor.h>
#include <CesiumGltf/ImageAsset.h>
#include <CesiumGltfReader/GltfReader.h>
#include <CesiumGltfWriter/GltfWriter.h>
#include <CesiumGltfContent/GltfUtilities.h>
#include <CesiumGltfContent/ImageManipulation.h>

#include <cstddef>
#include <cstring>
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

// ---------- Mesh / Primitive accessors ----------

CESIUM_API int cesium_gltf_mesh_get_primitive_count(
    const CesiumGltfModel* model,
    int meshIndex)
{
    if (!model) return 0;
    const auto& meshes = asModel(model)->meshes;
    if (meshIndex < 0 || meshIndex >= static_cast<int>(meshes.size())) return 0;
    return static_cast<int>(meshes[meshIndex].primitives.size());
}

CESIUM_API int cesium_gltf_primitive_get_mode(
    const CesiumGltfModel* model,
    int meshIndex,
    int primitiveIndex)
{
    if (!model) return 4; // TRIANGLES default
    const auto& meshes = asModel(model)->meshes;
    if (meshIndex < 0 || meshIndex >= static_cast<int>(meshes.size())) return 4;
    const auto& prims = meshes[meshIndex].primitives;
    if (primitiveIndex < 0 || primitiveIndex >= static_cast<int>(prims.size())) return 4;
    return prims[primitiveIndex].mode;
}

CESIUM_API int cesium_gltf_primitive_get_material_index(
    const CesiumGltfModel* model,
    int meshIndex,
    int primitiveIndex)
{
    if (!model) return -1;
    const auto& meshes = asModel(model)->meshes;
    if (meshIndex < 0 || meshIndex >= static_cast<int>(meshes.size())) return -1;
    const auto& prims = meshes[meshIndex].primitives;
    if (primitiveIndex < 0 || primitiveIndex >= static_cast<int>(prims.size())) return -1;
    return prims[primitiveIndex].material;
}

CESIUM_API int cesium_gltf_primitive_get_indices_accessor_index(
    const CesiumGltfModel* model,
    int meshIndex,
    int primitiveIndex)
{
    if (!model) return -1;
    const auto& meshes = asModel(model)->meshes;
    if (meshIndex < 0 || meshIndex >= static_cast<int>(meshes.size())) return -1;
    const auto& prims = meshes[meshIndex].primitives;
    if (primitiveIndex < 0 || primitiveIndex >= static_cast<int>(prims.size())) return -1;
    return prims[primitiveIndex].indices;
}

CESIUM_API int cesium_gltf_primitive_get_attribute_count(
    const CesiumGltfModel* model,
    int meshIndex,
    int primitiveIndex)
{
    if (!model) return 0;
    const auto& meshes = asModel(model)->meshes;
    if (meshIndex < 0 || meshIndex >= static_cast<int>(meshes.size())) return 0;
    const auto& prims = meshes[meshIndex].primitives;
    if (primitiveIndex < 0 || primitiveIndex >= static_cast<int>(prims.size())) return 0;
    return static_cast<int>(prims[primitiveIndex].attributes.size());
}

// Helper to get the Nth entry from the attributes map by iteration order.
static const std::pair<const std::string, int32_t>* getNthAttribute(
    const Model* m, int meshIndex, int primitiveIndex, int attributeIndex)
{
    const auto& meshes = m->meshes;
    if (meshIndex < 0 || meshIndex >= static_cast<int>(meshes.size())) return nullptr;
    const auto& prims = meshes[meshIndex].primitives;
    if (primitiveIndex < 0 || primitiveIndex >= static_cast<int>(prims.size())) return nullptr;
    const auto& attrs = prims[primitiveIndex].attributes;
    if (attributeIndex < 0 || attributeIndex >= static_cast<int>(attrs.size())) return nullptr;
    auto it = attrs.begin();
    std::advance(it, attributeIndex);
    return &(*it);
}

CESIUM_API const char* cesium_gltf_primitive_get_attribute_name(
    const CesiumGltfModel* model,
    int meshIndex,
    int primitiveIndex,
    int attributeIndex)
{
    if (!model) return "";
    const auto* entry = getNthAttribute(asModel(model), meshIndex, primitiveIndex, attributeIndex);
    return entry ? entry->first.c_str() : "";
}

CESIUM_API int cesium_gltf_primitive_get_attribute_accessor_index(
    const CesiumGltfModel* model,
    int meshIndex,
    int primitiveIndex,
    int attributeIndex)
{
    if (!model) return -1;
    const auto* entry = getNthAttribute(asModel(model), meshIndex, primitiveIndex, attributeIndex);
    return entry ? entry->second : -1;
}

CESIUM_API int cesium_gltf_primitive_find_attribute_accessor_index(
    const CesiumGltfModel* model,
    int meshIndex,
    int primitiveIndex,
    const char* attributeName)
{
    if (!model || !attributeName) return -1;
    const auto& meshes = asModel(model)->meshes;
    if (meshIndex < 0 || meshIndex >= static_cast<int>(meshes.size())) return -1;
    const auto& prims = meshes[meshIndex].primitives;
    if (primitiveIndex < 0 || primitiveIndex >= static_cast<int>(prims.size())) return -1;
    const auto& attrs = prims[primitiveIndex].attributes;
    auto it = attrs.find(attributeName);
    return (it != attrs.end()) ? it->second : -1;
}

// ---------- Accessor data resolution ----------

CESIUM_API int cesium_gltf_accessor_get_data(
    const CesiumGltfModel* model,
    int accessorIndex,
    CesiumAccessorData* out)
{
    if (!model || !out) return 0;
    const auto* m = asModel(model);
    if (accessorIndex < 0 || accessorIndex >= static_cast<int>(m->accessors.size())) return 0;

    const auto& accessor = m->accessors[accessorIndex];
    if (accessor.bufferView < 0 || accessor.bufferView >= static_cast<int>(m->bufferViews.size()))
        return 0;

    const auto& bufferView = m->bufferViews[accessor.bufferView];
    if (bufferView.buffer < 0 || bufferView.buffer >= static_cast<int>(m->buffers.size()))
        return 0;

    const auto& buffer = m->buffers[bufferView.buffer];
    const auto& data = buffer.cesium.data;

    int8_t numComponents = accessor.computeNumberOfComponents();
    int8_t componentSize = accessor.computeByteSizeOfComponent();
    if (numComponents <= 0 || componentSize <= 0) return 0;

    int64_t elementSize = static_cast<int64_t>(numComponents) * componentSize;
    int64_t stride = bufferView.byteStride.has_value()
        ? bufferView.byteStride.value()
        : elementSize;

    int64_t offset = bufferView.byteOffset + accessor.byteOffset;
    if (offset < 0 || offset >= static_cast<int64_t>(data.size())) return 0;

    out->data = data.data() + offset;
    out->stride = static_cast<size_t>(stride);
    out->count = static_cast<int32_t>(accessor.count);
    out->componentType = accessor.componentType;
    out->numberOfComponents = numComponents;
    out->byteLength = static_cast<size_t>(bufferView.byteLength);
    return 1;
}

// ---------- Scene graph / Node accessors ----------

CESIUM_API int cesium_gltf_model_get_default_scene(const CesiumGltfModel* model) {
    if (!model) return -1;
    return asModel(model)->scene;
}

CESIUM_API int cesium_gltf_scene_get_node_count(
    const CesiumGltfModel* model,
    int sceneIndex)
{
    if (!model) return 0;
    const auto& scenes = asModel(model)->scenes;
    if (sceneIndex < 0 || sceneIndex >= static_cast<int>(scenes.size())) return 0;
    return static_cast<int>(scenes[sceneIndex].nodes.size());
}

CESIUM_API int cesium_gltf_scene_get_node(
    const CesiumGltfModel* model,
    int sceneIndex,
    int index)
{
    if (!model) return -1;
    const auto& scenes = asModel(model)->scenes;
    if (sceneIndex < 0 || sceneIndex >= static_cast<int>(scenes.size())) return -1;
    const auto& nodes = scenes[sceneIndex].nodes;
    if (index < 0 || index >= static_cast<int>(nodes.size())) return -1;
    return nodes[index];
}

CESIUM_API int cesium_gltf_node_get_mesh(
    const CesiumGltfModel* model,
    int nodeIndex)
{
    if (!model) return -1;
    const auto& nodes = asModel(model)->nodes;
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(nodes.size())) return -1;
    return nodes[nodeIndex].mesh;
}

CESIUM_API int cesium_gltf_node_get_children_count(
    const CesiumGltfModel* model,
    int nodeIndex)
{
    if (!model) return 0;
    const auto& nodes = asModel(model)->nodes;
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(nodes.size())) return 0;
    return static_cast<int>(nodes[nodeIndex].children.size());
}

CESIUM_API int cesium_gltf_node_get_child(
    const CesiumGltfModel* model,
    int nodeIndex,
    int childIndex)
{
    if (!model) return -1;
    const auto& nodes = asModel(model)->nodes;
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(nodes.size())) return -1;
    const auto& children = nodes[nodeIndex].children;
    if (childIndex < 0 || childIndex >= static_cast<int>(children.size())) return -1;
    return children[childIndex];
}

CESIUM_API int cesium_gltf_node_get_matrix(
    const CesiumGltfModel* model,
    int nodeIndex,
    double out[16])
{
    if (!model || !out) return 0;
    const auto& nodes = asModel(model)->nodes;
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(nodes.size())) return 0;
    const auto& mat = nodes[nodeIndex].matrix;
    if (mat.size() == 16) {
        std::memcpy(out, mat.data(), 16 * sizeof(double));
        // Check if this is the identity matrix (i.e. TRS is used instead)
        static const double identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
        if (std::memcmp(out, identity, 16 * sizeof(double)) == 0) {
            // Could be identity, but also could be explicit identity.
            // Return 1 only if the node doesn't have non-default TRS values.
            const auto& n = nodes[nodeIndex];
            bool hasTRS =
                (n.translation.size() == 3 && (n.translation[0] != 0 || n.translation[1] != 0 || n.translation[2] != 0)) ||
                (n.rotation.size() == 4 && (n.rotation[0] != 0 || n.rotation[1] != 0 || n.rotation[2] != 0 || n.rotation[3] != 1)) ||
                (n.scale.size() == 3 && (n.scale[0] != 1 || n.scale[1] != 1 || n.scale[2] != 1));
            return hasTRS ? 0 : 1;
        }
        return 1;
    }
    // No explicit matrix; fill identity
    static const double identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    std::memcpy(out, identity, 16 * sizeof(double));
    return 0;
}

CESIUM_API void cesium_gltf_node_get_translation(
    const CesiumGltfModel* model,
    int nodeIndex,
    double out[3])
{
    if (!model || !out) return;
    const auto& nodes = asModel(model)->nodes;
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(nodes.size())) {
        out[0] = out[1] = out[2] = 0.0;
        return;
    }
    const auto& t = nodes[nodeIndex].translation;
    if (t.size() == 3) {
        out[0] = t[0]; out[1] = t[1]; out[2] = t[2];
    } else {
        out[0] = out[1] = out[2] = 0.0;
    }
}

CESIUM_API void cesium_gltf_node_get_rotation(
    const CesiumGltfModel* model,
    int nodeIndex,
    double out[4])
{
    if (!model || !out) return;
    const auto& nodes = asModel(model)->nodes;
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(nodes.size())) {
        out[0] = out[1] = out[2] = 0.0; out[3] = 1.0;
        return;
    }
    const auto& r = nodes[nodeIndex].rotation;
    if (r.size() == 4) {
        out[0] = r[0]; out[1] = r[1]; out[2] = r[2]; out[3] = r[3];
    } else {
        out[0] = out[1] = out[2] = 0.0; out[3] = 1.0;
    }
}

CESIUM_API void cesium_gltf_node_get_scale(
    const CesiumGltfModel* model,
    int nodeIndex,
    double out[3])
{
    if (!model || !out) return;
    const auto& nodes = asModel(model)->nodes;
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(nodes.size())) {
        out[0] = out[1] = out[2] = 1.0;
        return;
    }
    const auto& s = nodes[nodeIndex].scale;
    if (s.size() == 3) {
        out[0] = s[0]; out[1] = s[1]; out[2] = s[2];
    } else {
        out[0] = out[1] = out[2] = 1.0;
    }
}

// ---------- Material / Texture / Sampler / Image ----------

static CesiumTextureInfo makeTextureInfo(const CesiumGltf::TextureInfo& ti, double scale) {
    return { ti.index, static_cast<int32_t>(ti.texCoord), scale };
}

static CesiumTextureInfo emptyTextureInfo() {
    return { -1, 0, 1.0 };
}

CESIUM_API int cesium_gltf_material_get_data(
    const CesiumGltfModel* model,
    int materialIndex,
    CesiumMaterialData* out)
{
    if (!model || !out) return 0;
    const auto* m = asModel(model);
    if (materialIndex < 0 || materialIndex >= static_cast<int>(m->materials.size())) return 0;

    const auto& mat = m->materials[materialIndex];

    // PBR defaults
    out->baseColorFactor[0] = 1; out->baseColorFactor[1] = 1;
    out->baseColorFactor[2] = 1; out->baseColorFactor[3] = 1;
    out->metallicFactor = 1.0;
    out->roughnessFactor = 1.0;
    out->baseColorTexture = emptyTextureInfo();
    out->metallicRoughnessTexture = emptyTextureInfo();

    if (mat.pbrMetallicRoughness.has_value()) {
        const auto& pbr = mat.pbrMetallicRoughness.value();
        if (pbr.baseColorFactor.size() >= 4) {
            out->baseColorFactor[0] = pbr.baseColorFactor[0];
            out->baseColorFactor[1] = pbr.baseColorFactor[1];
            out->baseColorFactor[2] = pbr.baseColorFactor[2];
            out->baseColorFactor[3] = pbr.baseColorFactor[3];
        }
        out->metallicFactor = pbr.metallicFactor;
        out->roughnessFactor = pbr.roughnessFactor;
        if (pbr.baseColorTexture.has_value())
            out->baseColorTexture = makeTextureInfo(pbr.baseColorTexture.value(), 1.0);
        if (pbr.metallicRoughnessTexture.has_value())
            out->metallicRoughnessTexture = makeTextureInfo(pbr.metallicRoughnessTexture.value(), 1.0);
    }

    // Emissive
    out->emissiveFactor[0] = 0; out->emissiveFactor[1] = 0; out->emissiveFactor[2] = 0;
    if (mat.emissiveFactor.size() >= 3) {
        out->emissiveFactor[0] = mat.emissiveFactor[0];
        out->emissiveFactor[1] = mat.emissiveFactor[1];
        out->emissiveFactor[2] = mat.emissiveFactor[2];
    }

    // Alpha
    out->alphaMode = 0; // OPAQUE
    if (mat.alphaMode == "MASK") out->alphaMode = 1;
    else if (mat.alphaMode == "BLEND") out->alphaMode = 2;
    out->alphaCutoff = mat.alphaCutoff;
    out->doubleSided = mat.doubleSided ? 1 : 0;

    // Normal / Occlusion / Emissive textures
    if (mat.normalTexture.has_value())
        out->normalTexture = makeTextureInfo(mat.normalTexture.value(), mat.normalTexture.value().scale);
    else
        out->normalTexture = emptyTextureInfo();

    if (mat.occlusionTexture.has_value())
        out->occlusionTexture = makeTextureInfo(mat.occlusionTexture.value(), mat.occlusionTexture.value().strength);
    else
        out->occlusionTexture = emptyTextureInfo();

    if (mat.emissiveTexture.has_value())
        out->emissiveTexture = makeTextureInfo(mat.emissiveTexture.value(), 1.0);
    else
        out->emissiveTexture = emptyTextureInfo();

    return 1;
}

CESIUM_API int cesium_gltf_texture_get_source(
    const CesiumGltfModel* model,
    int textureIndex)
{
    if (!model) return -1;
    const auto& textures = asModel(model)->textures;
    if (textureIndex < 0 || textureIndex >= static_cast<int>(textures.size())) return -1;
    return textures[textureIndex].source;
}

CESIUM_API int cesium_gltf_texture_get_sampler(
    const CesiumGltfModel* model,
    int textureIndex)
{
    if (!model) return -1;
    const auto& textures = asModel(model)->textures;
    if (textureIndex < 0 || textureIndex >= static_cast<int>(textures.size())) return -1;
    return textures[textureIndex].sampler;
}

CESIUM_API int cesium_gltf_sampler_get_data(
    const CesiumGltfModel* model,
    int samplerIndex,
    CesiumSamplerData* out)
{
    if (!model || !out) return 0;
    const auto& samplers = asModel(model)->samplers;
    if (samplerIndex < 0 || samplerIndex >= static_cast<int>(samplers.size())) return 0;
    const auto& s = samplers[samplerIndex];
    out->magFilter = s.magFilter.has_value() ? s.magFilter.value() : -1;
    out->minFilter = s.minFilter.has_value() ? s.minFilter.value() : -1;
    out->wrapS = s.wrapS;
    out->wrapT = s.wrapT;
    return 1;
}

CESIUM_API int cesium_gltf_image_get_data(
    const CesiumGltfModel* model,
    int imageIndex,
    CesiumImageData* out)
{
    if (!model || !out) return 0;
    const auto& images = asModel(model)->images;
    if (imageIndex < 0 || imageIndex >= static_cast<int>(images.size())) return 0;
    const auto& img = images[imageIndex];
    if (!img.pAsset) return 0;
    const auto& asset = *img.pAsset;
    if (asset.pixelData.empty()) return 0;
    out->pixelData = asset.pixelData.data();
    out->pixelDataSize = asset.pixelData.size();
    out->width = asset.width;
    out->height = asset.height;
    out->channels = asset.channels;
    out->bytesPerChannel = asset.bytesPerChannel;
    return 1;
}

// ---------- GLB serialization ----------

CESIUM_API int cesium_gltf_model_write_glb(
    const CesiumGltfModel* model,
    uint8_t** out_data,
    size_t* out_size)
{
    if (!model || !out_data || !out_size) return 0;

    CESIUM_TRY_BEGIN
    // Copy the model so we can modify it for GLB export
    Model copy = *asModel(model);

    // Ensure at least one buffer exists
    if (copy.buffers.empty()) {
        copy.buffers.emplace_back();
    }

    // Re-encode images that only exist as decoded ImageAssets (pAsset)
    // but don't have valid encoded data in a buffer.
    for (Image& image : copy.images) {
        if (!image.pAsset || image.pAsset->pixelData.empty())
            continue;

        // Check if existing bufferView reference already has valid data
        bool hasValidBufferData = false;
        if (image.bufferView >= 0 &&
            image.bufferView < static_cast<int32_t>(copy.bufferViews.size())) {
            const auto& bv = copy.bufferViews[image.bufferView];
            if (bv.buffer >= 0 &&
                bv.buffer < static_cast<int32_t>(copy.buffers.size())) {
                const auto& buf = copy.buffers[bv.buffer];
                if (bv.byteOffset + bv.byteLength <=
                    static_cast<int64_t>(buf.cesium.data.size())) {
                    hasValidBufferData = true;
                }
            }
        }

        if (hasValidBufferData)
            continue;

        // Re-encode the decoded pixels as PNG
        std::vector<std::byte> pngData =
            CesiumGltfContent::ImageManipulation::savePng(*image.pAsset);
        if (pngData.empty())
            continue;

        // Append PNG data to buffer[0]
        Buffer& buf = copy.buffers[0];
        int64_t offset = static_cast<int64_t>(buf.cesium.data.size());
        buf.cesium.data.insert(
            buf.cesium.data.end(), pngData.begin(), pngData.end());

        // Create a new bufferView for this image
        BufferView& bv = copy.bufferViews.emplace_back();
        bv.buffer = 0;
        bv.byteOffset = offset;
        bv.byteLength = static_cast<int64_t>(pngData.size());

        // Point the image to the new bufferView
        image.bufferView = static_cast<int32_t>(copy.bufferViews.size() - 1);
        image.mimeType = "image/png";
        image.uri = std::nullopt;
    }
    
    // Collapse all buffers into one for GLB
    CesiumGltfContent::GltfUtilities::collapseToSingleBuffer(copy);

    std::span<const std::byte> bufferData;
    if (!copy.buffers.empty() && !copy.buffers[0].cesium.data.empty()) {
        bufferData = std::span<const std::byte>(
            copy.buffers[0].cesium.data.data(),
            copy.buffers[0].cesium.data.size());
    }

    CesiumGltfWriter::GltfWriter writer;
    auto result = writer.writeGlb(copy, bufferData);

    if (!result.errors.empty() || result.gltfBytes.empty()) {
        if (!result.errors.empty())
            cesium_set_last_error(result.errors[0].c_str());
        return 0;
    }

    // Transfer ownership to a heap allocation the caller can free
    auto size = result.gltfBytes.size();
    auto* data = new uint8_t[size];
    std::memcpy(data, result.gltfBytes.data(), size);

    *out_data = data;
    *out_size = size;
    return 1;
    CESIUM_TRY_END
    return 0;
}

CESIUM_API void cesium_gltf_free_glb(uint8_t* data) {
    delete[] data;
}

} // extern "C"
