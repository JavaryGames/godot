/*************************************************************************/
/*  import_dock.cpp                                                      */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2018 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2018 Godot Engine contributors (cf. AUTHORS.md)    */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "import_dock.h"

#include "core/os/dir_access.h"
#include "editor_node.h"

class ImportDockParameters : public Object {
	GDCLASS(ImportDockParameters, Object)
public:
	Map<StringName, Variant> values;
	List<PropertyInfo> properties;
	Ref<ResourceImporter> importer;
	Vector<String> paths;

	bool _set(const StringName &p_name, const Variant &p_value) {

		if (values.has(p_name)) {
			values[p_name] = p_value;
			return true;
		}

		return false;
	}

	bool _get(const StringName &p_name, Variant &r_ret) const {

		if (values.has(p_name)) {
			r_ret = values[p_name];
			return true;
		}

		return false;
	}
	void _get_property_list(List<PropertyInfo> *p_list) const {

		for (const List<PropertyInfo>::Element *E = properties.front(); E; E = E->next()) {
			if (!importer->get_option_visibility(E->get().name, values))
				continue;
			p_list->push_back(E->get());
		}
	}

	void update() {
		_change_notify();
	}
};

void ImportDock::set_edit_path(const String &p_path) {

	Ref<ConfigFile> config;
	config.instance();
	Error err = config->load(p_path + ".import");
	if (err != OK) {
		err = config->load(ResourceFormatImporter::get_singleton()->get_import_base_path(p_path) + ".import");
		if (err != OK) {
			clear();
			return;
		}
	}

	params->importer = ResourceFormatImporter::get_singleton()->get_importer_by_name(config->get_value("remap", "importer"));
	if (params->importer.is_null()) {
		clear();
		return;
	}

	_update_options(config);

	List<Ref<ResourceImporter> > importers;
	ResourceFormatImporter::get_singleton()->get_importers_for_extension(p_path.get_extension(), &importers);
	List<Pair<String, String> > importer_names;

	for (List<Ref<ResourceImporter> >::Element *E = importers.front(); E; E = E->next()) {
		importer_names.push_back(Pair<String, String>(E->get()->get_visible_name(), E->get()->get_importer_name()));
	}

	importer_names.sort_custom<PairSort<String, String> >();

	import_as->clear();

	for (List<Pair<String, String> >::Element *E = importer_names.front(); E; E = E->next()) {
		import_as->add_item(E->get().first);
		import_as->set_item_metadata(import_as->get_item_count() - 1, E->get().second);
		if (E->get().second == params->importer->get_importer_name()) {
			import_as->select(import_as->get_item_count() - 1);
		}
	}

	bool force_save_status = false;
	if (config->has_section_key("remap", "force_save")) {
		force_save_status = config->get_value("remap", "force_save");
	}

	params->paths.clear();
	params->paths.push_back(p_path);
	import->set_disabled(false);
	import_as->set_disabled(false);
	force_save->set_disabled(false);
	force_save->set_pressed(force_save_status);

	imported->set_text(p_path.get_file());
}

void ImportDock::_update_options(const Ref<ConfigFile> &p_config) {

	List<ResourceImporter::ImportOption> options;
	params->importer->get_import_options(&options);

	params->properties.clear();
	params->values.clear();

	bool has_custom_defaults = ProjectSettings::get_singleton()->has_setting("importer_defaults/" + params->importer->get_importer_name());
	Dictionary custom_defaults;
	if (has_custom_defaults) {
		custom_defaults = ProjectSettings::get_singleton()->get("importer_defaults/" + params->importer->get_importer_name());
	}

	if (p_config->has_section_key("remap", "force_save")) {
		bool force_save_status = p_config->get_value("remap", "force_save");
		this->force_save->set_pressed(force_save_status);
	}

	for (List<ResourceImporter::ImportOption>::Element *E = options.front(); E; E = E->next()) {

		params->properties.push_back(E->get().option);
		if (p_config.is_valid() && p_config->has_section_key("params", E->get().option.name)) {
			params->values[E->get().option.name] = p_config->get_value("params", E->get().option.name);
		} else if (has_custom_defaults) {
			params->values[E->get().option.name] = custom_defaults[E->get().option.name];
		} else {
			params->values[E->get().option.name] = E->get().default_value;
		}
	}

	params->update();

	preset->get_popup()->clear();

	if (params->importer->get_preset_count() == 0) {
		preset->get_popup()->add_item(TTR("Default"));
	} else {
		for (int i = 0; i < params->importer->get_preset_count(); i++) {
			preset->get_popup()->add_item(params->importer->get_preset_name(i));
		}
	}

	preset->get_popup()->add_separator();
	preset->get_popup()->add_item(vformat(TTR("Set as Default for '%s'"), params->importer->get_visible_name()), ITEM_SET_AS_DEFAULT);
	if (ProjectSettings::get_singleton()->has_setting("importer_defaults/" + params->importer->get_importer_name())) {
		preset->get_popup()->add_item(TTR("Load Default"), ITEM_LOAD_DEFAULT);
		preset->get_popup()->add_separator();
		preset->get_popup()->add_item(vformat(TTR("Clear Default for '%s'"), params->importer->get_visible_name()), ITEM_CLEAR_DEFAULT);
	}
}

void ImportDock::set_edit_multiple_paths(const Vector<String> &p_paths) {

	clear();

	bool force_save_status = false;

	//use the value that is repeated the mot
	Map<String, Dictionary> value_frequency;

	for (int i = 0; i < p_paths.size(); i++) {

		Ref<ConfigFile> config;
		config.instance();
		String import_config_path = p_paths[i] + ".import";
		Error err = config->load(import_config_path);

		if (err != OK) {
			import_config_path = ResourceFormatImporter::get_singleton()->get_import_base_path(p_paths[i]) + ".import";
			Error err = config->load(import_config_path);

			ERR_CONTINUE(err != OK);
		}

		if (i == 0) {
			params->importer = ResourceFormatImporter::get_singleton()->get_importer_by_name(config->get_value("remap", "importer"));
			if (params->importer.is_null()) {
				clear();
				return;
			}
		}

		if (config->has_section_key("remap", "force_save")) {
			force_save_status = config->get_value("remap", "force_save");
		}

		if (config->has_section("params")) {
			List<String> keys;
			config->get_section_keys("params", &keys);

			for (List<String>::Element *E = keys.front(); E; E = E->next()) {

				if (!value_frequency.has(E->get())) {
					value_frequency[E->get()] = Dictionary();
				}

				Variant value = config->get_value("params", E->get());

				if (value_frequency[E->get()].has(value)) {
					value_frequency[E->get()][value] = int(value_frequency[E->get()][value]) + 1;
				} else {
					value_frequency[E->get()][value] = 1;
				}
			}
		}
	}

	ERR_FAIL_COND(params->importer.is_null());

	List<ResourceImporter::ImportOption> options;
	params->importer->get_import_options(&options);

	params->properties.clear();
	params->values.clear();

	bool has_custom_defaults = ProjectSettings::get_singleton()->has_setting("importer_defaults/" + params->importer->get_importer_name());
	Dictionary custom_defaults;
	if (has_custom_defaults) {
		custom_defaults = ProjectSettings::get_singleton()->get("importer_defaults/" + params->importer->get_importer_name());
	}

	for (List<ResourceImporter::ImportOption>::Element *E = options.front(); E; E = E->next()) {

		params->properties.push_back(E->get().option);

		if (value_frequency.has(E->get().option.name)) {

			Dictionary d = value_frequency[E->get().option.name];
			int freq = 0;
			List<Variant> v;
			d.get_key_list(&v);
			Variant value;
			for (List<Variant>::Element *F = v.front(); F; F = F->next()) {
				int f = d[F->get()];
				if (f > freq) {
					value = F->get();
				}
			}

			params->values[E->get().option.name] = value;
		} else if (has_custom_defaults) {
			params->values[E->get().option.name] = custom_defaults[E->get().option.name];
		} else {
			params->values[E->get().option.name] = E->get().default_value;
		}
	}

	params->update();

	List<Ref<ResourceImporter> > importers;
	ResourceFormatImporter::get_singleton()->get_importers_for_extension(p_paths[0].get_extension(), &importers);
	List<Pair<String, String> > importer_names;

	for (List<Ref<ResourceImporter> >::Element *E = importers.front(); E; E = E->next()) {
		importer_names.push_back(Pair<String, String>(E->get()->get_visible_name(), E->get()->get_importer_name()));
	}

	importer_names.sort_custom<PairSort<String, String> >();

	import_as->clear();

	for (List<Pair<String, String> >::Element *E = importer_names.front(); E; E = E->next()) {
		import_as->add_item(E->get().first);
		import_as->set_item_metadata(import_as->get_item_count() - 1, E->get().second);
		if (E->get().second == params->importer->get_importer_name()) {
			import_as->select(import_as->get_item_count() - 1);
		}
	}

	preset->get_popup()->clear();

	if (params->importer->get_preset_count() == 0) {
		preset->get_popup()->add_item(TTR("Default"));
	} else {
		for (int i = 0; i < params->importer->get_preset_count(); i++) {
			preset->get_popup()->add_item(params->importer->get_preset_name(i));
		}
	}

	params->paths = p_paths;
	import->set_disabled(false);
	import_as->set_disabled(false);
	force_save->set_disabled(false);
	force_save->set_pressed(force_save_status);

	imported->set_text(itos(p_paths.size()) + TTR(" Files"));
}

void ImportDock::_importer_selected(int i_idx) {
	String name = import_as->get_selected_metadata();
	Ref<ResourceImporter> importer = ResourceFormatImporter::get_singleton()->get_importer_by_name(name);
	ERR_FAIL_COND(importer.is_null());

	params->importer = importer;

	Ref<ConfigFile> config;
	if (params->paths.size()) {
		config.instance();
		String import_config_file = params->paths[0] + ".import";
		Error err = config->load(import_config_file);
		if (err != OK) {
			import_config_file = ResourceFormatImporter::get_singleton()->get_import_base_path(params->paths[0]) + ".import";
			err = config->load(import_config_file);
			if (err != OK) {
				config.unref();
			}
		}
	}
	_update_options(config);
}

void ImportDock::_preset_selected(int p_idx) {

	int item_id = preset->get_popup()->get_item_id(p_idx);

	switch (item_id) {
		case ITEM_SET_AS_DEFAULT: {
			Dictionary d;

			for (const List<PropertyInfo>::Element *E = params->properties.front(); E; E = E->next()) {
				d[E->get().name] = params->values[E->get().name];
			}

			ProjectSettings::get_singleton()->set("importer_defaults/" + params->importer->get_importer_name(), d);
			ProjectSettings::get_singleton()->save();

		} break;
		case ITEM_LOAD_DEFAULT: {

			ERR_FAIL_COND(!ProjectSettings::get_singleton()->has_setting("importer_defaults/" + params->importer->get_importer_name()));

			Dictionary d = ProjectSettings::get_singleton()->get("importer_defaults/" + params->importer->get_importer_name());
			List<Variant> v;
			d.get_key_list(&v);

			for (List<Variant>::Element *E = v.front(); E; E = E->next()) {
				params->values[E->get()] = d[E->get()];
			}
			params->update();

		} break;
		case ITEM_CLEAR_DEFAULT: {

			ProjectSettings::get_singleton()->set("importer_defaults/" + params->importer->get_importer_name(), Variant());
			ProjectSettings::get_singleton()->save();

		} break;
		default: {

			List<ResourceImporter::ImportOption> options;

			params->importer->get_import_options(&options, p_idx);

			for (List<ResourceImporter::ImportOption>::Element *E = options.front(); E; E = E->next()) {

				params->values[E->get().option.name] = E->get().default_value;
			}

			params->update();
		} break;
	}
}

void ImportDock::clear() {

	imported->set_text("");
	import->set_disabled(true);
	import_as->clear();
	import_as->set_disabled(true);
	force_save->set_pressed(false);
	force_save->set_disabled(true);
	params->values.clear();
	params->properties.clear();
	params->update();
	preset->get_popup()->clear();
}

void ImportDock::_reimport() {

	bool has_custom_default = ProjectSettings::get_singleton()->has_setting("importer_defaults/" + params->importer->get_importer_name());
	Dictionary custom_default;
	if (has_custom_default) {
		custom_default = ProjectSettings::get_singleton()->get("importer_defaults/" + params->importer->get_importer_name());
	}

	bool force_save_status = force_save->is_pressed();

	List<ResourceImporter::ImportOption> import_options;
	List<StringName> non_default_options;
	params->importer->get_import_options(&import_options);

	for (List<ResourceImporter::ImportOption>::Element *E = import_options.front(); E; E = E->next()) {
		String prop_name = E->get().option.name;
		Variant value = params->values[prop_name];
		if (force_save_status || (!has_custom_default && value != E->get().default_value) || (has_custom_default && value != custom_default[prop_name])) {
			non_default_options.push_back(prop_name);
		}
	}

	for (int i = 0; i < params->paths.size(); i++) {

		Ref<ConfigFile> config;
		config.instance();
		String import_config_path = params->paths[i] + ".import";
		Error err = config->load(import_config_path);
		if (err != OK) {
			import_config_path = ResourceFormatImporter::get_singleton()->get_import_base_path(params->paths[i]) + ".import";
			err = config->load(import_config_path);
		}
		ERR_CONTINUE(err != OK);

		// Remove .import file from the current location, since it'll be recreated
		DirAccess *da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
		da->remove(import_config_path);

		config->set_value("remap", "importer", params->importer->get_importer_name());
		config->set_value("remap", "force_save", force_save_status);
		config->erase_section("params");

		for (List<StringName>::Element *E = non_default_options.front(); E; E = E->next()) {
			config->set_value("params", E->get(), params->values[E->get()]);
		}

		String save_base_path;
		if (non_default_options.size() == 0) {
			save_base_path = ResourceFormatImporter::get_singleton()->get_import_base_path(params->paths[i]);
		} else {
			save_base_path = params->paths[i];
		}
		config->save(save_base_path + ".import");
	}

	EditorFileSystem::get_singleton()->reimport_files(params->paths);
	EditorFileSystem::get_singleton()->emit_signal("filesystem_changed"); //it changed, so force emitting the signal
}

void ImportDock::_notification(int p_what) {
	switch (p_what) {

		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {

			imported->add_style_override("normal", get_stylebox("normal", "LineEdit"));
		} break;

		case NOTIFICATION_ENTER_TREE: {

			import_opts->edit(params);
		} break;
	}
}
void ImportDock::_bind_methods() {

	ClassDB::bind_method(D_METHOD("_reimport"), &ImportDock::_reimport);
	ClassDB::bind_method(D_METHOD("_preset_selected"), &ImportDock::_preset_selected);
	ClassDB::bind_method(D_METHOD("_importer_selected"), &ImportDock::_importer_selected);
}

void ImportDock::initialize_import_options() const {

	ERR_FAIL_COND(!import_opts || !params);

	import_opts->edit(params);
}

ImportDock::ImportDock() {

	set_name("Import");
	imported = memnew(Label);
	imported->add_style_override("normal", EditorNode::get_singleton()->get_gui_base()->get_stylebox("normal", "LineEdit"));
	imported->set_clip_text(true);
	add_child(imported);
	HBoxContainer *hb = memnew(HBoxContainer);
	add_margin_child(TTR("Import As:"), hb);
	import_as = memnew(OptionButton);
	import_as->connect("item_selected", this, "_importer_selected");
	hb->add_child(import_as);
	import_as->set_h_size_flags(SIZE_EXPAND_FILL);
	preset = memnew(MenuButton);
	preset->set_text(TTR("Preset..."));
	preset->get_popup()->connect("index_pressed", this, "_preset_selected");
	hb->add_child(preset);

	import_opts = memnew(EditorInspector);
	add_child(import_opts);
	import_opts->set_v_size_flags(SIZE_EXPAND_FILL);

	hb = memnew(HBoxContainer);
	add_child(hb);
	force_save = memnew(CheckBox);
	force_save->set_text(TTR("Force save"));
	force_save->set_tooltip(TTR("Save settings even if they are default"));
	force_save->set_disabled(true);
	import = memnew(Button);
	import->set_text(TTR("Reimport"));
	import->connect("pressed", this, "_reimport");
	import->set_disabled(true);
	hb->add_spacer();
	hb->add_child(force_save);
	hb->add_spacer();
	hb->add_child(import);
	hb->add_spacer();

	params = memnew(ImportDockParameters);
}

ImportDock::~ImportDock() {

	memdelete(params);
}
