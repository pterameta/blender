/* SPDX-FileCopyrightText: 2019 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "usd.h"

#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/common.h>

struct Depsgraph;
struct Main;

namespace blender::io::usd {

class USDHierarchyIterator;

struct USDExporterContext {
  Main *bmain;
  Depsgraph *depsgraph;
  const pxr::UsdStageRefPtr stage;
  const pxr::SdfPath usd_path;
  pxr::UsdTimeCode time_code;
  const USDExportParams &export_params;
  std::string export_file_path;
};

}  // namespace blender::io::usd
