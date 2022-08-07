/*
 *  Copyright © 2017-2022 Wellington Wallace
 *
 *  This file is part of EasyEffects.
 *
 *  EasyEffects is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  EasyEffects is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with EasyEffects.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "plugin_preset_base.hpp"

PluginPresetBase::PluginPresetBase(const char* schema_id,
                                   const char* schema_path_input,
                                   const char* schema_path_output,
                                   PresetType preset_type,
                                   const int& index)
    : index(index), preset_type(preset_type) {
  switch (preset_type) {
    case PresetType::input:
      section = "input";

      settings = g_settings_new_with_path(schema_id, schema_path_input);
      break;
    case PresetType::output:
      section = "output";

      settings = g_settings_new_with_path(schema_id, schema_path_output);
      break;
  }
}

PluginPresetBase::~PluginPresetBase() {
  g_object_unref(settings);
}
