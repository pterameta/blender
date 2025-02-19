/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "material.h"

#include <Python.h>
#include <unicodeobject.h>

#include <pxr/imaging/hd/material.h>
#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/usdImaging/usdImaging/materialParamUtils.h>

#include "MEM_guardedalloc.h"

#include "BKE_lib_id.h"
#include "BKE_material.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"
#include "RNA_types.h"

#include "DEG_depsgraph_query.h"

#include "bpy_rna.h"

#include "hydra_scene_delegate.h"
#include "image.h"

#include "intern/usd_exporter_context.h"
#include "intern/usd_writer_material.h"

namespace blender::io::hydra {

MaterialData::MaterialData(HydraSceneDelegate *scene_delegate,
                           const Material *material,
                           pxr::SdfPath const &prim_id)
    : IdData(scene_delegate, &material->id, prim_id)
{
}

void MaterialData::init()
{
  ID_LOGN(1, "");
  double_sided = (((Material *)id)->blend_flag & MA_BL_CULL_BACKFACE) == 0;
  material_network_map_ = pxr::VtValue();

  /* Create temporary in memory stage. */
  pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  pxr::UsdTimeCode time = pxr::UsdTimeCode::Default();
  pxr::SdfPath material_library_path("/_materials");
  pxr::SdfPath material_path = material_library_path.AppendChild(
      pxr::TfToken(prim_id.GetElementString()));

  /* Create USD export content to reuse USD file export code. */
  USDExportParams export_params;
  export_params.relative_paths = false;
  export_params.export_textures = false; /* Don't copy all textures, is slow. */
  export_params.evaluation_mode = DEG_get_mode(scene_delegate_->depsgraph);

  usd::USDExporterContext export_context{scene_delegate_->bmain,
                                         scene_delegate_->depsgraph,
                                         stage,
                                         material_library_path,
                                         time,
                                         export_params,
                                         image_cache_file_path()};

  /* Create USD material. */
  pxr::UsdShadeMaterial usd_material = usd::create_usd_material(
      export_context, material_path, (Material *)id, "st");

  /* Convert USD material to Hydra material network map, adapted for render delegate. */
  const pxr::HdRenderDelegate *render_delegate =
      scene_delegate_->GetRenderIndex().GetRenderDelegate();
  const pxr::TfTokenVector contextVector = render_delegate->GetMaterialRenderContexts();
  pxr::TfTokenVector shaderSourceTypes = render_delegate->GetShaderSourceTypes();

  pxr::HdMaterialNetworkMap network_map;

  if (pxr::UsdShadeShader surface = usd_material.ComputeSurfaceSource(contextVector)) {
    pxr::UsdImagingBuildHdMaterialNetworkFromTerminal(surface.GetPrim(),
                                                      pxr::HdMaterialTerminalTokens->surface,
                                                      shaderSourceTypes,
                                                      contextVector,
                                                      &network_map,
                                                      time);
  }

  material_network_map_ = pxr::VtValue(network_map);
}

void MaterialData::insert()
{
  ID_LOGN(1, "");
  scene_delegate_->GetRenderIndex().InsertSprim(
      pxr::HdPrimTypeTokens->material, scene_delegate_, prim_id);
}

void MaterialData::remove()
{
  ID_LOG(1, "");
  scene_delegate_->GetRenderIndex().RemoveSprim(pxr::HdPrimTypeTokens->material, prim_id);
}

void MaterialData::update()
{
  ID_LOGN(1, "");
  bool prev_double_sided = double_sided;
  init();
  scene_delegate_->GetRenderIndex().GetChangeTracker().MarkSprimDirty(prim_id,
                                                                      pxr::HdMaterial::AllDirty);
  if (prev_double_sided != double_sided) {
    for (auto &obj_data : scene_delegate_->objects_.values()) {
      MeshData *m_data = dynamic_cast<MeshData *>(obj_data.get());
      if (m_data) {
        m_data->update_double_sided(this);
      }
    }
    scene_delegate_->instancer_data_->update_double_sided(this);
  }
}

pxr::VtValue MaterialData::get_data(pxr::TfToken const & /* key */) const
{
  return pxr::VtValue();
}

pxr::VtValue MaterialData::get_material_resource() const
{
  return material_network_map_;
}

pxr::HdCullStyle MaterialData::cull_style() const
{
  return double_sided ? pxr::HdCullStyle::HdCullStyleNothing : pxr::HdCullStyle::HdCullStyleBack;
}

}  // namespace blender::io::hydra
