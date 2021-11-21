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

#include "pipe_info_ui.hpp"

PipeInfoUi::PipeInfoUi(BaseObjectType* cobject,
                       const Glib::RefPtr<Gtk::Builder>& builder,
                       PipeManager* pm_ptr,
                       PresetsManager* presets_manager)
    : Gtk::Box(cobject),
      pm(pm_ptr),
      presets_manager(presets_manager),
      ts(std::make_unique<TestSignals>(pm_ptr)),
      sie_settings(Gio::Settings::create("com.github.wwmm.easyeffects.streaminputs")),
      soe_settings(Gio::Settings::create("com.github.wwmm.easyeffects.streamoutputs")),
      input_devices_model(Gio::ListStore<NodeInfoHolder>::create()),
      output_devices_model(Gio::ListStore<NodeInfoHolder>::create()),
      modules_model(Gio::ListStore<ModuleInfoHolder>::create()),
      clients_model(Gio::ListStore<ClientInfoHolder>::create()),
      autoloading_output_model(Gio::ListStore<PresetsAutoloadingHolder>::create()),
      autoloading_input_model(Gio::ListStore<PresetsAutoloadingHolder>::create()),
      output_presets_string_list(Gtk::StringList::create({"initial_value"})),
      input_presets_string_list(Gtk::StringList::create({"initial_value"})) {
  setup_dropdown_devices(dropdown_input_devices, input_devices_model);
  setup_dropdown_devices(dropdown_output_devices, output_devices_model);

  setup_dropdown_devices(dropdown_autoloading_input_devices, input_devices_model);
  setup_dropdown_devices(dropdown_autoloading_output_devices, output_devices_model);

  setup_dropdown_presets(PresetType::input, input_presets_string_list);
  setup_dropdown_presets(PresetType::output, output_presets_string_list);

  setup_listview_autoloading(PresetType::input, listview_autoloading_input, autoloading_input_model);
  setup_listview_autoloading(PresetType::output, listview_autoloading_output, autoloading_output_model);

  setup_listview_modules();
  setup_listview_clients();

  dropdown_input_devices->property_selected_item().signal_changed().connect([=, this]() {
    if (dropdown_input_devices->get_selected_item() == nullptr) {
      return;
    }

    auto holder = std::dynamic_pointer_cast<NodeInfoHolder>(dropdown_input_devices->get_selected_item());

    sie_settings->set_string("input-device", holder->name);
  });

  dropdown_output_devices->property_selected_item().signal_changed().connect([=, this]() {
    if (dropdown_output_devices->get_selected_item() == nullptr) {
      return;
    }

    auto holder = std::dynamic_pointer_cast<NodeInfoHolder>(dropdown_output_devices->get_selected_item());

    soe_settings->set_string("output-device", holder->name);
  });

  // setting the displayed entry to the right value

  {
    auto holder_selected = std::dynamic_pointer_cast<NodeInfoHolder>(dropdown_input_devices->get_selected_item());

    if (holder_selected != nullptr) {
      const auto input_device_name = sie_settings->get_string("input-device").raw();

      if (holder_selected->name != input_device_name) {
        for (guint n = 0U; n < input_devices_model->get_n_items(); n++) {
          if (input_devices_model->get_item(n)->name == input_device_name) {
            dropdown_input_devices->set_selected(n);
          }
        }
      }
    }
  }

  {
    auto holder_selected = std::dynamic_pointer_cast<NodeInfoHolder>(dropdown_output_devices->get_selected_item());

    if (holder_selected != nullptr) {
      const auto output_device_name = soe_settings->get_string("output-device").raw();

      if (holder_selected->name != output_device_name) {
        for (guint n = 0U; n < output_devices_model->get_n_items(); n++) {
          if (output_devices_model->get_item(n)->name == output_device_name) {
            dropdown_output_devices->set_selected(n);
          }
        }
      }
    }
  }

  stack->connect_property_changed("visible-child", sigc::mem_fun(*this, &PipeInfoUi::on_stack_visible_child_changed));

  use_default_input->property_active().signal_changed().connect([=, this]() {
    if (use_default_input->get_active()) {
      sie_settings->set_string("input-device", pm->default_input_device.name);

      auto holder = std::dynamic_pointer_cast<NodeInfoHolder>(dropdown_input_devices->get_selected_item());

      if (holder != nullptr) {
        if (holder->name != pm->default_input_device.name) {
          for (guint n = 0U; n < input_devices_model->get_n_items(); n++) {
            if (input_devices_model->get_item(n)->name == pm->default_input_device.name) {
              dropdown_input_devices->set_selected(n);

              break;
            }
          }
        }
      }
    }
  });

  use_default_output->property_active().signal_changed().connect([=, this]() {
    if (use_default_output->get_active()) {
      soe_settings->set_string("output-device", pm->default_output_device.name);

      auto holder_selected = std::dynamic_pointer_cast<NodeInfoHolder>(dropdown_output_devices->get_selected_item());

      if (holder_selected != nullptr) {
        if (holder_selected->name != pm->default_output_device.name) {
          for (guint n = 0U; n < output_devices_model->get_n_items(); n++) {
            if (output_devices_model->get_item(n)->name == pm->default_output_device.name) {
              dropdown_output_devices->set_selected(n);

              break;
            }
          }
        }
      }
    }
  });

  autoloading_add_output_profile->signal_clicked().connect([=, this]() {
    if (dropdown_autoloading_output_devices->get_selected_item() == nullptr) {
      return;
    }

    auto holder = std::dynamic_pointer_cast<NodeInfoHolder>(dropdown_autoloading_output_devices->get_selected_item());

    std::string device_profile;

    for (const auto& device : pm->list_devices) {
      if (device.id == holder->device_id) {
        device_profile = device.output_route_name;

        break;
      }
    }

    // first we remove any autoloading profile associated to the target device so that our ui is updated

    for (guint n = 0; n < autoloading_output_model->get_n_items(); n++) {
      const auto item = autoloading_output_model->get_item(n);

      if (holder->name == item->device && device_profile == item->device_profile) {
        presets_manager->remove_autoload(PresetType::output, item->preset_name, item->device, item->device_profile);

        break;
      }
    }

    const auto preset_name =
        dropdown_autoloading_output_presets->get_selected_item()->get_property<Glib::ustring>("string").raw();

    presets_manager->add_autoload(PresetType::output, preset_name, holder->name, device_profile);
  });

  autoloading_add_input_profile->signal_clicked().connect([=, this]() {
    if (dropdown_autoloading_input_devices->get_selected_item() == nullptr) {
      return;
    }

    auto holder = std::dynamic_pointer_cast<NodeInfoHolder>(dropdown_autoloading_input_devices->get_selected_item());

    std::string device_profile;

    for (const auto& device : pm->list_devices) {
      if (device.id == holder->device_id) {
        device_profile = device.input_route_name;

        break;
      }
    }

    // first we remove any autoloading profile associated to the target device so that our ui is updated

    for (guint n = 0; n < autoloading_input_model->get_n_items(); n++) {
      const auto item = autoloading_input_model->get_item(n);

      if (holder->name == item->device && device_profile == item->device_profile) {
        presets_manager->remove_autoload(PresetType::input, item->preset_name, item->device, item->device_profile);

        break;
      }
    }

    const auto preset_name =
        dropdown_autoloading_input_presets->get_selected_item()->get_property<Glib::ustring>("string").raw();

    presets_manager->add_autoload(PresetType::input, preset_name, holder->name, device_profile);
  });

  connections.push_back(pm->sink_added.connect([=, this](const NodeInfo info) {
    for (guint n = 0U; n < output_devices_model->get_n_items(); n++) {
      if (output_devices_model->get_item(n)->id == info.id) {
        return;
      }
    }

    output_devices_model->append(NodeInfoHolder::create(info));
  }));

  connections.push_back(pm->sink_removed.connect([=, this](const NodeInfo info) {
    for (guint n = 0U; n < output_devices_model->get_n_items(); n++) {
      if (output_devices_model->get_item(n)->id == info.id) {
        output_devices_model->remove(n);

        return;
      }
    }
  }));

  connections.push_back(pm->source_added.connect([=, this](const NodeInfo info) {
    for (guint n = 0U; n < input_devices_model->get_n_items(); n++) {
      if (input_devices_model->get_item(n)->id == info.id) {
        return;
      }
    }

    input_devices_model->append(NodeInfoHolder::create(info));
  }));

  connections.push_back(pm->source_removed.connect([=, this](const NodeInfo info) {
    for (guint n = 0U; n < input_devices_model->get_n_items(); n++) {
      if (input_devices_model->get_item(n)->id == info.id) {
        input_devices_model->remove(n);

        return;
      }
    }
  }));

  connections.push_back(presets_manager->user_output_preset_created.connect([=, this](const std::string& preset_name) {
    if (preset_name.empty()) {
      util::warning(log_tag + "can't retrieve information about the preset file");

      return;
    }

    for (guint n = 0; n < output_presets_string_list->get_n_items(); n++) {
      if (output_presets_string_list->get_string(n).raw() == preset_name) {
        return;
      }
    }

    output_presets_string_list->append(preset_name);
  }));

  connections.push_back(presets_manager->user_output_preset_removed.connect([=, this](const std::string& preset_name) {
    if (preset_name.empty()) {
      util::warning(log_tag + "can't retrieve information about the preset file");

      return;
    }

    for (guint n = 0; n < output_presets_string_list->get_n_items(); n++) {
      if (output_presets_string_list->get_string(n).raw() == preset_name) {
        output_presets_string_list->remove(n);

        return;
      }
    }
  }));

  connections.push_back(presets_manager->user_input_preset_created.connect([=, this](const std::string& preset_name) {
    if (preset_name.empty()) {
      util::warning(log_tag + "can't retrieve information about the preset file");

      return;
    }

    for (guint n = 0; n < input_presets_string_list->get_n_items(); n++) {
      if (input_presets_string_list->get_string(n).raw() == preset_name) {
        return;
      }
    }

    input_presets_string_list->append(preset_name);
  }));

  connections.push_back(presets_manager->user_input_preset_removed.connect([=, this](const std::string& preset_name) {
    if (preset_name.empty()) {
      util::warning(log_tag + "can't retrieve information about the preset file");

      return;
    }

    for (guint n = 0; n < input_presets_string_list->get_n_items(); n++) {
      if (input_presets_string_list->get_string(n).raw() == preset_name) {
        input_presets_string_list->remove(n);

        return;
      }
    }
  }));

  connections.push_back(
      presets_manager->autoload_output_profiles_changed.connect([=, this](const std::vector<nlohmann::json>& profiles) {
        std::vector<Glib::RefPtr<PresetsAutoloadingHolder>> list;

        for (const auto& json : profiles) {
          const auto device = json.value("device", "");
          const auto device_profile = json.value("device-profile", "");
          const auto preset_name = json.value("preset-name", "");

          list.push_back(PresetsAutoloadingHolder::create(device, device_profile, preset_name));
        }

        autoloading_output_model->splice(0, autoloading_output_model->get_n_items(), list);
      }));

  connections.push_back(
      presets_manager->autoload_input_profiles_changed.connect([=, this](const std::vector<nlohmann::json>& profiles) {
        std::vector<Glib::RefPtr<PresetsAutoloadingHolder>> list;

        for (const auto& json : profiles) {
          const auto device = json.value("device", "");
          const auto device_profile = json.value("device-profile", "");
          const auto preset_name = json.value("preset-name", "");

          list.push_back(PresetsAutoloadingHolder::create(device, device_profile, preset_name));
        }

        autoloading_input_model->splice(0, autoloading_input_model->get_n_items(), list);
      }));

  update_modules_info();
  update_clients_info();
}

PipeInfoUi::~PipeInfoUi() {
  for (auto& c : connections) {
    c.disconnect();
  }

  util::debug(log_tag + "destroyed");
}

auto PipeInfoUi::add_to_stack(Gtk::Stack* stack, PipeManager* pm, PresetsManager* presets_manager) -> PipeInfoUi* {
  const auto builder = Gtk::Builder::create_from_resource("/com/github/wwmm/easyeffects/ui/pipe_info.ui");

  auto* const ui = Gtk::Builder::get_widget_derived<PipeInfoUi>(builder, "top_box", pm, presets_manager);

  stack->add(*ui, "pipe_info");

  return ui;
}

void PipeInfoUi::setup_dropdown_devices(Gtk::DropDown* dropdown,
                                        const Glib::RefPtr<Gio::ListStore<NodeInfoHolder>>& model) {
  // setting the dropdown model and factory

  const auto selection_model = Gtk::SingleSelection::create(model);

  dropdown->set_model(selection_model);

  auto factory = Gtk::SignalListItemFactory::create();

  dropdown->set_factory(factory);

  // setting the factory callbacks

  factory->signal_setup().connect([=](const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto* const box = Gtk::make_managed<Gtk::Box>();
    auto* const label = Gtk::make_managed<Gtk::Label>();
    auto* const icon = Gtk::make_managed<Gtk::Image>();

    label->set_hexpand(true);
    label->set_halign(Gtk::Align::START);

    icon->set_from_icon_name("emblem-system-symbolic");

    box->set_spacing(6);
    box->append(*icon);
    box->append(*label);

    // setting list_item data

    list_item->set_data("name", label);
    list_item->set_data("icon", icon);

    list_item->set_child(*box);
  });

  factory->signal_bind().connect([=, this](const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto* const label = static_cast<Gtk::Label*>(list_item->get_data("name"));
    auto* const icon = static_cast<Gtk::Image*>(list_item->get_data("icon"));

    auto holder = std::dynamic_pointer_cast<NodeInfoHolder>(list_item->get_item());

    if (holder->media_class == pm->media_class_sink) {
      icon->set_from_icon_name("audio-card-symbolic");
    } else if (holder->media_class == pm->media_class_source) {
      icon->set_from_icon_name("audio-input-microphone-symbolic");
    }

    label->set_name(holder->name);
    label->set_text(holder->name);
  });
}

void PipeInfoUi::setup_dropdown_presets(PresetType preset_type, const Glib::RefPtr<Gtk::StringList>& string_list) {
  Gtk::DropDown* dropdown = nullptr;

  switch (preset_type) {
    case PresetType::input:
      dropdown = dropdown_autoloading_input_presets;

      break;
    case PresetType::output:
      dropdown = dropdown_autoloading_output_presets;

      break;
  }

  string_list->remove(0);

  for (const auto& name : presets_manager->get_names(preset_type)) {
    string_list->append(name);
  }

  // sorter

  const auto sorter =
      Gtk::StringSorter::create(Gtk::PropertyExpression<Glib::ustring>::create(GTK_TYPE_STRING_OBJECT, "string"));

  const auto sort_list_model = Gtk::SortListModel::create(string_list, sorter);

  // setting the dropdown model and factory

  const auto selection_model = Gtk::SingleSelection::create(sort_list_model);

  dropdown->set_model(selection_model);

  auto factory = Gtk::SignalListItemFactory::create();

  dropdown->set_factory(factory);

  // setting the factory callbacks

  factory->signal_setup().connect([=](const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto* const box = Gtk::make_managed<Gtk::Box>();
    auto* const label = Gtk::make_managed<Gtk::Label>();
    auto* const icon = Gtk::make_managed<Gtk::Image>();

    label->set_hexpand(true);
    label->set_halign(Gtk::Align::START);

    icon->set_from_icon_name("emblem-system-symbolic");

    box->set_spacing(6);
    box->append(*icon);
    box->append(*label);

    // setting list_item data

    list_item->set_data("name", label);

    list_item->set_child(*box);
  });

  factory->signal_bind().connect([=](const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto* const label = static_cast<Gtk::Label*>(list_item->get_data("name"));

    const auto name = list_item->get_item()->get_property<Glib::ustring>("string");

    label->set_name(name);
    label->set_text(name);
  });
}

void PipeInfoUi::setup_listview_autoloading(PresetType preset_type,
                                            Gtk::ListView* listview,
                                            const Glib::RefPtr<Gio::ListStore<PresetsAutoloadingHolder>>& model) {
  const auto profiles = presets_manager->get_autoload_profiles(preset_type);

  for (const auto& json : profiles) {
    const auto device = json.value("device", "");
    const auto device_profile = json.value("device-profile", "");
    const auto preset_name = json.value("preset-name", "");

    model->append(PresetsAutoloadingHolder::create(device, device_profile, preset_name));
  }

  // setting the listview model and factory

  listview->set_model(Gtk::NoSelection::create(model));

  auto factory = Gtk::SignalListItemFactory::create();

  listview->set_factory(factory);

  // setting the factory callbacks

  factory->signal_setup().connect([=](const Glib::RefPtr<Gtk::ListItem>& list_item) {
    const auto b = Gtk::Builder::create_from_resource("/com/github/wwmm/easyeffects/ui/autoload_row.ui");

    auto* const top_box = b->get_widget<Gtk::Box>("top_box");

    list_item->set_data("device", b->get_widget<Gtk::Label>("device"));
    list_item->set_data("device_profile", b->get_widget<Gtk::Label>("device_profile"));
    list_item->set_data("preset_name", b->get_widget<Gtk::Label>("preset_name"));
    list_item->set_data("remove", b->get_widget<Gtk::Button>("remove"));

    list_item->set_child(*top_box);
  });

  factory->signal_bind().connect([=, this](const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto* const device = static_cast<Gtk::Label*>(list_item->get_data("device"));
    auto* const device_profile = static_cast<Gtk::Label*>(list_item->get_data("device_profile"));
    auto* const preset_name = static_cast<Gtk::Label*>(list_item->get_data("preset_name"));
    auto* const remove = static_cast<Gtk::Button*>(list_item->get_data("remove"));

    auto holder = std::dynamic_pointer_cast<PresetsAutoloadingHolder>(list_item->get_item());

    device->set_text(holder->device);
    device_profile->set_text(holder->device_profile);
    preset_name->set_text(holder->preset_name);

    remove->update_property(
        Gtk::Accessible::Property::LABEL,
        util::glib_value(Glib::ustring(_("Remove Autoloading Preset")) + " " + holder->preset_name));

    auto connection_remove = remove->signal_clicked().connect([=, this]() {
      presets_manager->remove_autoload(preset_type, holder->preset_name, holder->device, holder->device_profile);
    });

    list_item->set_data("connection_remove", new sigc::connection(connection_remove),
                        Glib::destroy_notify_delete<sigc::connection>);
  });

  factory->signal_unbind().connect([=](const Glib::RefPtr<Gtk::ListItem>& list_item) {
    if (auto* connection = static_cast<sigc::connection*>(list_item->get_data("connection_remove"))) {
      connection->disconnect();

      list_item->set_data("connection_remove", nullptr);
    }
  });
}

void PipeInfoUi::setup_listview_modules() {
  // setting the listview model and factory

  listview_modules->set_model(Gtk::NoSelection::create(modules_model));

  auto factory = Gtk::SignalListItemFactory::create();

  listview_modules->set_factory(factory);

  // setting the factory callbacks

  factory->signal_setup().connect([](const Glib::RefPtr<Gtk::ListItem>& list_item) {
    const auto b = Gtk::Builder::create_from_resource("/com/github/wwmm/easyeffects/ui/module_info.ui");

    auto* const top_box = b->get_widget<Gtk::Box>("top_box");

    list_item->set_data("id", b->get_widget<Gtk::Label>("id"));
    list_item->set_data("name", b->get_widget<Gtk::Label>("name"));
    list_item->set_data("description", b->get_widget<Gtk::Label>("description"));

    list_item->set_child(*top_box);
  });

  factory->signal_bind().connect([](const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto* const id = static_cast<Gtk::Label*>(list_item->get_data("id"));
    auto* const name = static_cast<Gtk::Label*>(list_item->get_data("name"));
    auto* const description = static_cast<Gtk::Label*>(list_item->get_data("description"));

    auto holder = std::dynamic_pointer_cast<ModuleInfoHolder>(list_item->get_item());

    id->set_text(Glib::ustring::format(holder->info.id));
    name->set_text(holder->info.name);
    description->set_text(holder->info.description);
  });
}

void PipeInfoUi::setup_listview_clients() {
  // setting the listview model and factory

  listview_clients->set_model(Gtk::NoSelection::create(clients_model));

  auto factory = Gtk::SignalListItemFactory::create();

  listview_clients->set_factory(factory);

  // setting the factory callbacks

  factory->signal_setup().connect([](const Glib::RefPtr<Gtk::ListItem>& list_item) {
    const auto b = Gtk::Builder::create_from_resource("/com/github/wwmm/easyeffects/ui/client_info.ui");

    auto* const top_box = b->get_widget<Gtk::Box>("top_box");

    list_item->set_data("id", b->get_widget<Gtk::Label>("id"));
    list_item->set_data("name", b->get_widget<Gtk::Label>("name"));
    list_item->set_data("api", b->get_widget<Gtk::Label>("api"));
    list_item->set_data("access", b->get_widget<Gtk::Label>("access"));

    list_item->set_child(*top_box);
  });

  factory->signal_bind().connect([](const Glib::RefPtr<Gtk::ListItem>& list_item) {
    auto* const id = static_cast<Gtk::Label*>(list_item->get_data("id"));
    auto* const name = static_cast<Gtk::Label*>(list_item->get_data("name"));
    auto* const api = static_cast<Gtk::Label*>(list_item->get_data("api"));
    auto* const access = static_cast<Gtk::Label*>(list_item->get_data("access"));

    auto holder = std::dynamic_pointer_cast<ClientInfoHolder>(list_item->get_item());

    id->set_text(Glib::ustring::format(holder->info.id));
    name->set_text(holder->info.name);
    api->set_text(holder->info.api);
    access->set_text(holder->info.access);
  });
}

void PipeInfoUi::update_modules_info() {
  std::vector<Glib::RefPtr<ModuleInfoHolder>> values;

  for (const auto& info : pm->list_modules) {
    values.push_back(ModuleInfoHolder::create(info));
  }

  modules_model->splice(0, modules_model->get_n_items(), values);
}

void PipeInfoUi::update_clients_info() {
  std::vector<Glib::RefPtr<ClientInfoHolder>> values;

  for (const auto& info : pm->list_clients) {
    values.push_back(ClientInfoHolder::create(info));
  }

  clients_model->splice(0, clients_model->get_n_items(), values);
}

void PipeInfoUi::on_stack_visible_child_changed() {
  if (const auto name = stack->get_visible_child_name(); name == "page_modules") {
    update_modules_info();
  } else if (name == "page_clients") {
    update_clients_info();
  }
}
