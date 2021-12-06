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

#include "equalizer_band_box.hpp"

namespace ui::equalizer_band_box {

using namespace std::string_literals;

auto constexpr log_tag = "equalizer_band_box: ";

struct _EqualizerBandBox {
  GtkBox parent_instance;

  GtkComboBoxText *band_type, *band_mode, *band_slope;

  GtkLabel *band_label, *band_width, *band_quality_label;

  GtkButton *reset_frequency, *reset_quality;

  GtkToggleButton *band_solo, *band_mute;

  GtkScale* band_scale;

  GtkSpinButton *band_frequency, *band_quality;

  GSettings* settings;

  int index;

  std::vector<sigc::connection> connections;

  std::vector<gulong> gconnections;
};

G_DEFINE_TYPE(EqualizerBandBox, equalizer_band_box, GTK_TYPE_BOX)

auto set_band_label(EqualizerBandBox* self, double value) -> const char* {
  if (self->band_frequency == nullptr) {
    return nullptr;
  }

  if (value > 1000.0) {
    return g_strdup(fmt::format("{0:.1f} kHz", value / 1000.0).c_str());
  } else {
    return g_strdup(fmt::format("{0:.1f} Hz", value).c_str());
  }
}

auto set_band_scale_sensitive(EqualizerBandBox* self, int active_id) -> gboolean {
  if (self->band_type == nullptr) {
    return 0;
  }

  auto text = gtk_combo_box_get_active_id(GTK_COMBO_BOX(self->band_type));

  if (g_strcmp0(text, "Off") == 0 || g_strcmp0(text, "Hi-pass") == 0 || g_strcmp0(text, "Lo-pass") == 0) {
    return 0;
  }

  return 1;
}

void on_update_quality_width(GtkSpinButton* band_frequency,
                             GtkSpinButton* band_quality,
                             GtkLabel* band_quality_label,
                             GtkLabel* band_width) {
  const auto q = gtk_spin_button_get_value(band_quality);

  gtk_label_set_text(band_quality_label, fmt::format("Q {0:.2f}", q).c_str());

  if (q > 0.0) {
    const auto f = gtk_spin_button_get_value(band_frequency);
    gtk_label_set_text(band_width, fmt::format("{0:.1f} Hz", f / q).c_str());
  } else {
    gtk_label_set_text(band_width, _("infinity"));
  }
}

void setup(EqualizerBandBox* self, GSettings* settings, int index) {
  self->index = index;
  self->settings = settings;

  g_signal_connect(self->reset_frequency, "clicked", G_CALLBACK(+[](GtkButton* btn, EqualizerBandBox* self) {
                     g_settings_reset(self->settings, tags::equalizer::band_frequency[self->index]);
                   }),
                   self);

  g_signal_connect(self->reset_quality, "clicked", G_CALLBACK(+[](GtkButton* btn, EqualizerBandBox* self) {
                     g_settings_reset(self->settings, tags::equalizer::band_q[self->index]);
                   }),
                   self);

  g_settings_bind(settings, tags::equalizer::band_gain[index], gtk_range_get_adjustment(GTK_RANGE(self->band_scale)),
                  "value", G_SETTINGS_BIND_DEFAULT);

  g_settings_bind(settings, tags::equalizer::band_frequency[index],
                  gtk_spin_button_get_adjustment(self->band_frequency), "value", G_SETTINGS_BIND_DEFAULT);

  g_settings_bind(settings, tags::equalizer::band_q[index], gtk_spin_button_get_adjustment(self->band_quality), "value",
                  G_SETTINGS_BIND_DEFAULT);

  g_settings_bind(settings, tags::equalizer::band_solo[index], self->band_solo, "active", G_SETTINGS_BIND_DEFAULT);

  g_settings_bind(settings, tags::equalizer::band_mute[index], self->band_mute, "active", G_SETTINGS_BIND_DEFAULT);

  g_settings_bind(settings, tags::equalizer::band_type[index], self->band_type, "active-id", G_SETTINGS_BIND_DEFAULT);

  g_settings_bind(settings, tags::equalizer::band_mode[index], self->band_mode, "active-id", G_SETTINGS_BIND_DEFAULT);

  g_settings_bind(settings, tags::equalizer::band_slope[index], self->band_slope, "active-id", G_SETTINGS_BIND_DEFAULT);
}

void dispose(GObject* object) {
  auto* self = EE_EQUALIZER_BAND_BOX(object);

  for (auto& c : self->connections) {
    c.disconnect();
  }

  for (auto& handler_id : self->gconnections) {
    g_signal_handler_disconnect(self->settings, handler_id);
  }

  self->connections.clear();
  self->gconnections.clear();

  util::debug(log_tag + "index: "s + std::to_string(self->index) + " disposed"s);

  G_OBJECT_CLASS(equalizer_band_box_parent_class)->dispose(object);
}

void equalizer_band_box_class_init(EqualizerBandBoxClass* klass) {
  auto* object_class = G_OBJECT_CLASS(klass);
  auto* widget_class = GTK_WIDGET_CLASS(klass);

  object_class->dispose = dispose;

  gtk_widget_class_set_template_from_resource(widget_class, "/com/github/wwmm/easyeffects/ui/equalizer_band.ui");

  gtk_widget_class_bind_template_child(widget_class, EqualizerBandBox, band_type);
  gtk_widget_class_bind_template_child(widget_class, EqualizerBandBox, band_mode);
  gtk_widget_class_bind_template_child(widget_class, EqualizerBandBox, band_slope);
  gtk_widget_class_bind_template_child(widget_class, EqualizerBandBox, band_label);
  gtk_widget_class_bind_template_child(widget_class, EqualizerBandBox, band_width);
  gtk_widget_class_bind_template_child(widget_class, EqualizerBandBox, band_quality_label);
  gtk_widget_class_bind_template_child(widget_class, EqualizerBandBox, reset_frequency);
  gtk_widget_class_bind_template_child(widget_class, EqualizerBandBox, reset_quality);
  gtk_widget_class_bind_template_child(widget_class, EqualizerBandBox, band_solo);
  gtk_widget_class_bind_template_child(widget_class, EqualizerBandBox, band_mute);
  gtk_widget_class_bind_template_child(widget_class, EqualizerBandBox, band_scale);
  gtk_widget_class_bind_template_child(widget_class, EqualizerBandBox, band_frequency);
  gtk_widget_class_bind_template_child(widget_class, EqualizerBandBox, band_quality);

  gtk_widget_class_bind_template_callback(widget_class, set_band_scale_sensitive);
  gtk_widget_class_bind_template_callback(widget_class, set_band_label);
}

void equalizer_band_box_init(EqualizerBandBox* self) {
  gtk_widget_init_template(GTK_WIDGET(self));

  prepare_scale<"">(self->band_scale);

  prepare_spinbutton<"Hz">(self->band_frequency);
  prepare_spinbutton<"">(self->band_quality);
}

auto create() -> EqualizerBandBox* {
  return static_cast<EqualizerBandBox*>(g_object_new(EE_TYPE_EQUALIZER_BAND_BOX, nullptr));
}

}  // namespace ui::equalizer_band_box