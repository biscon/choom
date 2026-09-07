#pragma once
#include "engine/assets/ModelAssets.h"
#include <external/cgltf.h>

namespace engine
{
// A null glTF material is its defined default, not an unknown non-glTF asset.
inline void ReadGltfRasterMetadata(ModelMaterialAsset &material, const cgltf_material *source)
{
    material.sidedness = source && source->double_sided ? ModelMaterialSidedness::DoubleSided
                                                        : ModelMaterialSidedness::SingleSided;
    const auto alpha = source ? source->alpha_mode : cgltf_alpha_mode_opaque;
    material.alphaMode = alpha == cgltf_alpha_mode_opaque
                             ? ModelMaterialAlphaMode::Opaque
                             : (alpha == cgltf_alpha_mode_mask ? ModelMaterialAlphaMode::Mask
                                                               : ModelMaterialAlphaMode::Blend);
}
} // namespace engine
