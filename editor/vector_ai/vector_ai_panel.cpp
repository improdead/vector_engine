	/**************************************************************************/
/*  vector_ai_panel.cpp                                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "vector_ai_panel.h"

#include "core/crypto/crypto.h"
#include "core/crypto/crypto_core.h"
#include "core/io/file_access.h"
#include "core/core_bind.h"
#include "core/config/project_settings.h"
#include "core/math/random_number_generator.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/editor_node.h"
#include "editor/editor_interface.h"
#include "editor/themes/editor_scale.h"
#include "editor/editor_file_system.h"
#include "editor/plugins/script_editor_plugin.h"
#include "editor/filesystem_dock.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/separator.h"
#include "scene/resources/style_box_flat.h"
#include "modules/gdscript/gdscript_parser.h"
#include "scene/resources/packed_scene.h"
#include "core/io/resource_loader.h"

VectorAIPanel::VectorAIPanel() {
	// Set fixed size for the panel
	set_custom_minimum_size(Size2(PANEL_WIDTH, PANEL_HEIGHT) * EDSCALE);

	// Initialize Claude API
	claude_api = memnew(ClaudeAPI);
	add_child(claude_api);
	claude_api->set_response_callback(callable_mp(this, &VectorAIPanel::_on_claude_response));
	claude_api->set_error_callback(callable_mp(this, &VectorAIPanel::_on_claude_error));
	claude_api->set_debug_mode(true); // Enable debug output to help diagnose issues

	// Check if API key is set
	is_api_key_set = claude_api->has_api_key();
	composer_mode_active = false; // Start in Ask mode
	code_preview_visible = false;

	// Initialize auto-attach functionality
	auto_attach_enabled = true;
	current_attached_file = "";
	streaming_active = false;
	current_step = "";

	// Create timers
	auto_refresh_timer = memnew(Timer);
	auto_refresh_timer->set_wait_time(2.0); // Check every 2 seconds
	auto_refresh_timer->set_autostart(true);
	auto_refresh_timer->connect("timeout", callable_mp(this, &VectorAIPanel::_auto_attach_current_file));
	add_child(auto_refresh_timer);

	stream_timer = memnew(Timer);
	stream_timer->set_wait_time(0.03); // Stream at 30fps
	add_child(stream_timer);

	// Initialize status update timer for animated status steps
	status_update_timer = memnew(Timer);
	status_update_timer->set_wait_time(0.5); // Update every 500ms
	status_update_timer->connect("timeout", callable_mp(this, &VectorAIPanel::_update_status_animation));
	add_child(status_update_timer);
	
	// Initialize processing state
	current_processing_state = STATE_IDLE;

	// Create UI
	MarginContainer *margin = memnew(MarginContainer);
	margin->add_theme_constant_override("margin_right", 10 * EDSCALE);
	margin->add_theme_constant_override("margin_top", 10 * EDSCALE);
	margin->add_theme_constant_override("margin_left", 10 * EDSCALE);
	margin->add_theme_constant_override("margin_bottom", 10 * EDSCALE);
	add_child(margin);

	main_vbox = memnew(VBoxContainer);
	margin->add_child(main_vbox);

	// Header
	header = memnew(HBoxContainer);
	main_vbox->add_child(header);

	title_label = memnew(Label);
	title_label->set_text("VectorAI Chat");
	// Don't use specific font paths, use default font
	title_label->add_theme_font_size_override("font_size", 14 * EDSCALE);
	header->add_child(title_label);
	header->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	Control *spacer = memnew(Control);
	spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	header->add_child(spacer);

	close_button = memnew(Button);
	close_button->set_flat(true);
	close_button->set_text("×");
	close_button->connect("pressed", callable_mp(this, &VectorAIPanel::_on_close_pressed));
	header->add_child(close_button);

	// Toolbar
	toolbar = memnew(HBoxContainer);
	main_vbox->add_child(toolbar);

	attach_button = memnew(Button);
	attach_button->set_text("📎");
	attach_button->set_tooltip_text("Attach TSCN file");
	attach_button->connect("pressed", callable_mp(this, &VectorAIPanel::_on_attach_pressed));
	toolbar->add_child(attach_button);

	mode_dropdown = memnew(OptionButton);
	mode_dropdown->add_item("Ask Mode");      // index 0 -> Ask mode
	mode_dropdown->add_item("Composer Mode"); // index 1 -> Composer mode
	mode_dropdown->select(0); // Default to Ask mode (safer)
	mode_dropdown->connect("item_selected", callable_mp(this, &VectorAIPanel::_on_mode_selected));
	toolbar->add_child(mode_dropdown);

	api_key_button = memnew(Button);
	api_key_button->set_text("Set API Key");
	api_key_button->set_tooltip_text("Set your Claude API key");
	api_key_button->connect("pressed", callable_mp(this, &VectorAIPanel::_on_api_key_pressed));
	toolbar->add_child(api_key_button);

	// Update API key button state
	_update_api_key_button();

	// Separator
	HSeparator *separator1 = memnew(HSeparator);
	main_vbox->add_child(separator1);

	// Chat area
	chat_scroll = memnew(ScrollContainer);
	chat_scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	chat_scroll->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	chat_scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);

	// Set a minimum height for the chat area to ensure it takes up available space
	chat_scroll->set_custom_minimum_size(Size2(0, 200 * EDSCALE));
	main_vbox->add_child(chat_scroll);

	chat_messages = memnew(VBoxContainer);
	chat_messages->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	chat_messages->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	chat_messages->add_theme_constant_override("separation", 10 * EDSCALE);
	chat_scroll->add_child(chat_messages);

	// Separator
	HSeparator *separator2 = memnew(HSeparator);
	main_vbox->add_child(separator2);

	// Status step container (hidden by default)
	status_container = memnew(MarginContainer);
	status_container->set_visible(false);
	status_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	status_container->add_theme_constant_override("margin_left", 5 * EDSCALE);
	status_container->add_theme_constant_override("margin_right", 5 * EDSCALE);
	status_container->add_theme_constant_override("margin_top", 5 * EDSCALE);
	status_container->add_theme_constant_override("margin_bottom", 5 * EDSCALE);
	main_vbox->add_child(status_container);

	// Status steps container
	status_steps = memnew(VBoxContainer);
	status_steps->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	status_steps->add_theme_constant_override("separation", 3 * EDSCALE);
	status_container->add_child(status_steps);

	// Input area
	input_area = memnew(HBoxContainer);
	input_area->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	input_area->set_custom_minimum_size(Size2(0, 70 * EDSCALE));
	main_vbox->add_child(input_area);

	input_text = memnew(TextEdit);
	input_text->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	input_text->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	input_text->set_custom_minimum_size(Size2(0, 60 * EDSCALE));
	input_text->set_line_wrapping_mode(TextEdit::LineWrappingMode::LINE_WRAPPING_BOUNDARY);
	input_text->connect("gui_input", callable_mp(this, &VectorAIPanel::_on_input_text_gui_input));
	input_text->set_placeholder("Type your message here...");
	input_area->add_child(input_text);

	send_button = memnew(Button);
	send_button->set_text("Send");
	send_button->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	send_button->connect("pressed", callable_mp(this, &VectorAIPanel::_on_send_pressed));
	input_area->add_child(send_button);

	// Add token counter label
	token_counter = memnew(Label);
	token_counter->set_text("0 chars (0 tokens)");
	token_counter->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	token_counter->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	token_counter->set_vertical_alignment(VERTICAL_ALIGNMENT_BOTTOM);
	token_counter->add_theme_color_override("font_color", Color(0.5, 0.5, 0.5));
	token_counter->add_theme_font_size_override("font_size", 10 * EDSCALE);
	input_area->add_child(token_counter);
	
	// Connect text changed signal
	input_text->connect("text_changed", callable_mp(this, &VectorAIPanel::_on_input_text_changed));

	// Create message styles with safe colors - no theme access here
	Ref<StyleBoxFlat> flat_style;

	flat_style.instantiate();
	flat_style->set_bg_color(Color(0.3, 0.3, 0.3));
	flat_style->set_corner_radius_all(5 * EDSCALE);
	flat_style->set_content_margin_all(10 * EDSCALE);
	user_message_style = flat_style;

	flat_style.instantiate();
	flat_style->set_bg_color(Color(0.2, 0.2, 0.3));
	flat_style->set_corner_radius_all(5 * EDSCALE);
	flat_style->set_content_margin_all(10 * EDSCALE);
	assistant_message_style = flat_style;

	flat_style.instantiate();
	flat_style->set_bg_color(Color(0.15, 0.15, 0.15));
	flat_style->set_corner_radius_all(5 * EDSCALE);
	flat_style->set_content_margin_all(10 * EDSCALE);
	system_message_style = flat_style;

	// Set initial mode to Ask (safer default)
	claude_api->set_mode(ClaudeAPI::MODE_ASK);
	composer_mode_active = false;

	// Add welcome message
	_add_claude_message("Welcome to VectorAI Chat. I'm starting in Ask Mode (Read-Only). Switch to Composer Mode if you want me to generate code.");

	if (!is_api_key_set) {
		_add_claude_message("Please set your Claude API key to start using me.");
	}

	// Create code preview panel (hidden by default)
	code_preview_panel = memnew(PanelContainer);
	code_preview_panel->set_visible(false);
	code_preview_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	main_vbox->add_child(code_preview_panel);

	preview_vbox = memnew(VBoxContainer);
	code_preview_panel->add_child(preview_vbox);

	preview_title = memnew(Label);
	preview_title->set_text("Code Preview");
	preview_vbox->add_child(preview_title);

	code_preview = memnew(CodeEdit);
	code_preview->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	code_preview->set_custom_minimum_size(Size2(0, 200 * EDSCALE));
	preview_vbox->add_child(code_preview);

	// Remove buttons since we'll auto-apply changes
	// We'll keep the container for future use
	preview_actions = memnew(HBoxContainer);
	preview_vbox->add_child(preview_actions);

	// Keep only the apply button for debugging/future use but hide it
	apply_button = memnew(Button);
	apply_button->set_text("Apply Changes");
	apply_button->connect("pressed", callable_mp(this, &VectorAIPanel::_on_apply_pressed));
	apply_button->set_visible(false);
	preview_actions->add_child(apply_button);

	// Discard button is completely removed, but we'll keep the reference for now
	discard_button = nullptr;
	
	print_line("VectorAI Panel: Initialized with Ask Mode as default");
}

VectorAIPanel::~VectorAIPanel() {
	// Clean up resources to prevent memory leaks
	if (claude_api != nullptr) {
		claude_api->queue_free();
		claude_api = nullptr;
	}
	
	// Clear style references
	user_message_style.unref();
	assistant_message_style.unref();
	system_message_style.unref();

	// Clean up file dialog if it exists
	if (file_dialog != nullptr) {
		if (file_dialog->is_inside_tree()) {
			file_dialog->queue_free();
		}
		file_dialog = nullptr;
	}
}

void VectorAIPanel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_send_pressed"), &VectorAIPanel::_on_send_pressed);
	ClassDB::bind_method(D_METHOD("_on_input_text_gui_input"), &VectorAIPanel::_on_input_text_gui_input);
	ClassDB::bind_method(D_METHOD("_on_input_text_changed"), &VectorAIPanel::_on_input_text_changed);
	ClassDB::bind_method(D_METHOD("_on_attach_pressed"), &VectorAIPanel::_on_attach_pressed);
	ClassDB::bind_method(D_METHOD("_on_file_selected", "path"), &VectorAIPanel::_on_file_selected);
	ClassDB::bind_method(D_METHOD("_on_mode_selected"), &VectorAIPanel::_on_mode_selected);
	ClassDB::bind_method(D_METHOD("_on_api_key_pressed"), &VectorAIPanel::_on_api_key_pressed);
	ClassDB::bind_method(D_METHOD("_on_api_key_confirmed"), &VectorAIPanel::_on_api_key_confirmed);
	ClassDB::bind_method(D_METHOD("_on_claude_response"), &VectorAIPanel::_on_claude_response);
	ClassDB::bind_method(D_METHOD("_on_claude_error"), &VectorAIPanel::_on_claude_error);
	ClassDB::bind_method(D_METHOD("_on_close_pressed"), &VectorAIPanel::_on_close_pressed);
	ClassDB::bind_method(D_METHOD("_on_apply_pressed"), &VectorAIPanel::_on_apply_pressed);
	ClassDB::bind_method(D_METHOD("_on_discard_pressed"), &VectorAIPanel::_on_discard_pressed);
	ClassDB::bind_method(D_METHOD("_update_styles"), &VectorAIPanel::_update_styles);
	ClassDB::bind_method(D_METHOD("_send_message_deferred", "message"), &VectorAIPanel::_send_message_deferred);
	ClassDB::bind_method(D_METHOD("_handle_scene_dependencies", "scene_code", "scene_path"), &VectorAIPanel::_handle_scene_dependencies);
	ClassDB::bind_method(D_METHOD("_start_typewriter_animation", "message"), &VectorAIPanel::_start_typewriter_animation);
	ClassDB::bind_method(D_METHOD("_show_completion_message"), &VectorAIPanel::_show_completion_message);
	
	// Auto-attach functionality
	ClassDB::bind_method(D_METHOD("_auto_attach_current_file"), &VectorAIPanel::_auto_attach_current_file);
	ClassDB::bind_method(D_METHOD("_read_file_content", "path"), &VectorAIPanel::_read_file_content);
	
	// Status step system
	ClassDB::bind_method(D_METHOD("_show_status_step", "step", "description"), &VectorAIPanel::_show_status_step);
	ClassDB::bind_method(D_METHOD("_update_status_step", "step"), &VectorAIPanel::_update_status_step);
	ClassDB::bind_method(D_METHOD("_complete_status_step"), &VectorAIPanel::_complete_status_step);
	ClassDB::bind_method(D_METHOD("_clear_status_steps"), &VectorAIPanel::_clear_status_steps);
	
	// Real-time streaming
	ClassDB::bind_method(D_METHOD("_start_text_streaming", "text", "label"), &VectorAIPanel::_start_text_streaming);
	ClassDB::bind_method(D_METHOD("_stream_text_tick", "label", "full_text", "current_pos"), &VectorAIPanel::_stream_text_tick);
	
	// New processing state methods
	ClassDB::bind_method(D_METHOD("_start_processing_sequence", "message"), &VectorAIPanel::_start_processing_sequence);
	ClassDB::bind_method(D_METHOD("_set_processing_state", "state"), &VectorAIPanel::_set_processing_state);
	ClassDB::bind_method(D_METHOD("_update_status_animation"), &VectorAIPanel::_update_status_animation);
	ClassDB::bind_method(D_METHOD("_complete_processing"), &VectorAIPanel::_complete_processing);
	ClassDB::bind_method(D_METHOD("_update_file_system_final"), &VectorAIPanel::_update_file_system_final);
}

void VectorAIPanel::_on_close_pressed() {
	// Hide the panel
	set_visible(false);
}

void VectorAIPanel::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_POSTINITIALIZE: {
			// Update styles after theme is initialized
			_update_styles();

			// Use default font sizes rather than specific fonts
			title_label->add_theme_font_size_override("font_size", 14 * EDSCALE);
			preview_title->add_theme_font_size_override("font_size", 14 * EDSCALE);
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			// Update styles when theme changes
			_update_styles();
		} break;
	}
}

void VectorAIPanel::_update_styles() {
	// Update styles based on theme but avoid accessing theme items directly
	// Instead use fixed colors that complement the editor theme
	if (user_message_style.is_valid()) {
		Ref<StyleBoxFlat> user_flat = Object::cast_to<StyleBoxFlat>(user_message_style.ptr());
		if (user_flat.is_valid()) {
			// Use a fixed color instead of theme color to avoid early access issues
			user_flat->set_bg_color(Color(0.3, 0.3, 0.3));
		}
	}
	if (assistant_message_style.is_valid()) {
		Ref<StyleBoxFlat> assistant_flat = Object::cast_to<StyleBoxFlat>(assistant_message_style.ptr());
		if (assistant_flat.is_valid()) {
			// Use a fixed color instead of theme color to avoid early access issues
			assistant_flat->set_bg_color(Color(0.2, 0.2, 0.3));
		}
	}
	if (system_message_style.is_valid()) {
		Ref<StyleBoxFlat> system_flat = Object::cast_to<StyleBoxFlat>(system_message_style.ptr());
		if (system_flat.is_valid()) {
			// Use a fixed color instead of theme color to avoid early access issues
			system_flat->set_bg_color(Color(0.15, 0.15, 0.15));
		}
	}
}

void VectorAIPanel::_on_send_pressed() {
	String input_text_content = input_text->get_text().strip_edges();
	
	if (input_text_content.is_empty()) {
		return;
	}

	// Add user message immediately
	_add_user_message(input_text_content);
	
	// Clear input
	input_text->clear();
	
	// Start processing sequence with proper state management
	_start_processing_sequence(input_text_content);
}

void VectorAIPanel::_on_input_text_gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventKey> k = p_event;
	
	if (k.is_valid() && k->is_pressed() && !k->is_echo()) {
		if (k->get_keycode() == Key::ENTER) {
			bool shift_pressed = k->is_shift_pressed();
			
			if (!shift_pressed) {
				// Send message when Enter is pressed without Shift
				_on_send_pressed();
				
				// Mark the event as handled to prevent default behavior
				if (get_viewport()) {
					get_viewport()->set_input_as_handled();
				}
			}
		}
	}
}

void VectorAIPanel::_on_attach_pressed() {
	if (file_dialog == nullptr) {
		file_dialog = memnew(EditorFileDialog);
	file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
		file_dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
		file_dialog->set_title(TTR("Attach Scene File"));
		file_dialog->clear_filters();
		file_dialog->add_filter("*.tscn", TTR("Godot Scene"));
	file_dialog->connect("file_selected", callable_mp(this, &VectorAIPanel::_on_file_selected));
		add_child(file_dialog);
	}
	
	file_dialog->popup_centered_ratio();
}

void VectorAIPanel::_on_file_selected(const String &p_path) {
	// Check if the file exists
	if (!FileAccess::exists(p_path)) {
		_add_claude_message("Error: The selected file does not exist.");
		return;
	}
	
	// Check file extension
	if (!p_path.ends_with(".tscn")) {
		_add_claude_message("Error: Please select a valid scene file (.tscn).");
		return;
	}
	
	// Store the path
	attached_file_path = p_path;

	// Try to load the file content
	Error err = OK;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);
	
	if (err != OK) {
		_add_claude_message("Error: Failed to open the file. Error code: " + itos(err));
		return;
	}
	
	attached_file_content = f->get_as_text();
	
	if (attached_file_content.is_empty()) {
		_add_claude_message("Error: The selected file is empty or could not be read.");
		return;
	}
	
	// Inform user of successful attachment
	_add_claude_message("Successfully attached: " + p_path.get_file());
	
	// Update Claude API with context
	if (claude_api) {
		claude_api->set_active_scene(attached_file_path);
		claude_api->set_file_context(attached_file_content);
	}
}

void VectorAIPanel::_on_mode_selected(int p_index) {
	// Update the Claude API mode based on selection
	// Fixed mapping: index 0 is Ask mode, index 1 is Composer mode
	int new_mode = (p_index == 0) ? ClaudeAPI::MODE_ASK : ClaudeAPI::MODE_COMPOSER;
	
	print_line("VectorAI: Mode dropdown selection: " + itos(p_index) + " -> API mode: " + itos(new_mode));
	
	claude_api->set_mode(new_mode);

	// Track if we're in composer mode
	composer_mode_active = (new_mode == ClaudeAPI::MODE_COMPOSER);
	
	print_line("VectorAI: Composer mode active: " + String(composer_mode_active ? "true" : "false"));

	// Update UI to reflect the current mode - get the mode name from claude_api
	String mode_description;
	if (new_mode == ClaudeAPI::MODE_ASK) {
		mode_description = "I'm now in Ask Mode (Read-Only). I'll explain and help you understand your project, but won't make any changes.";
	} else {
		mode_description = "I'm now in Composer Mode (Read-Write). I can generate and modify code when you ask me to. All changes will be applied automatically.";
	}

	_add_claude_message(mode_description);
	
	// Also enable debug mode to see what's happening
	claude_api->set_debug_mode(true);
}

void VectorAIPanel::_on_api_key_pressed() {
	// Create API key dialog
	AcceptDialog *dialog = memnew(AcceptDialog);
	dialog->set_title("Claude API Key");
	dialog->set_min_size(Size2(400, 150) * EDSCALE);

	// Add line edit for API key
	VBoxContainer *vbox = memnew(VBoxContainer);
	dialog->add_child(vbox);

	Label *label = memnew(Label);
	label->set_text("Enter your Claude API key:");
	vbox->add_child(label);

	LineEdit *line_edit = memnew(LineEdit);
	line_edit->set_placeholder("sk-ant-api...");
	line_edit->set_secret(true); // Hide the API key as it's typed
	line_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	vbox->add_child(line_edit);

	if (is_api_key_set) {
		line_edit->set_text(claude_api->get_api_key());
	}

	// Add the dialog to the scene
	add_child(dialog);
	dialog->popup_centered();

	// Connect the confirmed signal
	dialog->connect("confirmed", callable_mp(this, &VectorAIPanel::_on_api_key_confirmed).bind(line_edit), CONNECT_ONE_SHOT);
}

void VectorAIPanel::_on_api_key_confirmed(LineEdit *p_line_edit) {
	String key = p_line_edit->get_text().strip_edges();
	if (!key.is_empty()) {
		claude_api->set_api_key(key);
		// Disable debug mode to prevent system messages
		claude_api->set_debug_mode(false);
		is_api_key_set = true;
		_update_api_key_button();
		_add_claude_message("API key set successfully. I'm ready to help you with your Godot project.");
	} else {
		_add_claude_message("API key cannot be empty. Please provide a valid Claude API key.");
	}
}

void VectorAIPanel::_on_claude_response(const String &p_response) {
	print_line("VectorAI: Received response, length: " + itos(p_response.length()));
	
	// Update processing state
	_set_processing_state(STATE_GENERATING);
	
	// Remove any thinking messages
	_remove_thinking_messages();

	if (composer_mode_active && !p_response.is_empty()) {
		print_line("VectorAI: Processing composer mode response");
		
		// Check if response contains code blocks
		if (_response_contains_code(p_response)) {
			_set_processing_state(STATE_IMPLEMENTING);
			
			// Process code blocks and apply them
			Vector<String> modified_files;
			bool success = _process_and_apply_code(p_response, modified_files);
			
			if (success && modified_files.size() > 0) {
				_set_processing_state(STATE_COMPLETING);
				
				// Show success message with file list
				String message = "✅ **Code Applied Successfully!**\n\nModified files:\n";
				for (int i = 0; i < modified_files.size(); i++) {
					message += "📄 " + modified_files[i] + "\n";
				}
				message += "\n🎉 Ready for testing!";
				_add_claude_message(message);
				
				// Complete after short delay
				Timer *complete_timer = memnew(Timer);
				complete_timer->set_wait_time(1.0);
				complete_timer->set_one_shot(true);
				complete_timer->connect("timeout", callable_mp(this, &VectorAIPanel::_complete_processing));
				add_child(complete_timer);
				complete_timer->start();
			} else {
				// Show error or fallback to regular message
				_set_processing_state(STATE_IDLE);
				_add_claude_message(p_response);
			}
		} else {
			// No code detected, show as regular message
			_set_processing_state(STATE_IDLE);
			_add_claude_message(p_response);
		}
	} else {
		// Ask mode - show full response
		_set_processing_state(STATE_IDLE);
		_add_claude_message_with_streaming(p_response);
	}
}

void VectorAIPanel::_detect_code_changes(const String &p_response) {
	// Clear previous dependency tracking data
	pending_dependencies.clear();
	processing_order.clear();
	
	// Extract code blocks from the response
	Vector<Dictionary> all_code_blocks;
	
	// Try to extract multiple code blocks
	if (_extract_multiple_code_blocks(p_response, all_code_blocks)) {
		// Code blocks were found, we'll process them in order
	} else {
		// Try to extract a single code block
		String code;
		String file_path;
		if (_extract_code_block(p_response, code, file_path)) {
			// Add as a single-item array to use the same processing logic
			Dictionary block;
			block["code"] = code;
			block["file_path"] = file_path;
			all_code_blocks.push_back(block);
		} else {
			// No code blocks found
			return;
		}
	}
	
	// First, scan the response for mentions of files that might not be in code blocks
	_scan_for_dependencies(p_response);
	
	// Add all code blocks to the dependency system
	for (int i = 0; i < all_code_blocks.size(); i++) {
		String code = all_code_blocks[i]["code"];
		String file_path = all_code_blocks[i]["file_path"];
		
		// Determine the file type based on extension
		String type = "resource";
		if (file_path.ends_with(".gd")) {
			type = "script";
		} else if (file_path.ends_with(".tscn")) {
			type = "scene";
		}
		
		// Create/update dependency info
		DependencyInfo dependency;
		dependency.path = file_path;
		dependency.code = code;
		dependency.type = type;
		dependency.created = false;
		
		// Extract dependencies from this file
		if (type == "scene") {
			_extract_dependencies_from_scene(code, dependency.dependencies);
		} else if (type == "script") {
			_extract_dependencies_from_script(code, dependency.dependencies);
		}
		
		// Add to pending dependencies
		pending_dependencies[file_path] = dependency;
	}
	
	// Process dependencies in the correct order
	_process_dependencies();
}

void VectorAIPanel::_scan_for_dependencies(const String &p_response) {
	// Regex patterns to find file references in text
	// Enhanced pattern to capture more context about files
	static RegEx resource_regex;
	if (!resource_regex.is_valid()) {
		resource_regex.compile("(?:create|need|requires?|missing|using|include|import|load|add|generate|make|create)\\s+(?:a|an|the)?\\s*(?:new)?\\s*(?:file|script|scene|resource|subscene|tileset|asset)?\\s*(?:called|named)?\\s*[`'\"]?([\\w\\.\\-/]+\\.(tres|tscn|gd|res|import|shader))[`'\"]?");
	}
	
	// Specific pattern for subscenes
	static RegEx subscene_regex;
	if (!subscene_regex.is_valid()) {
		subscene_regex.compile("(?:player|character|enemy|item|ui|menu|hud|level|world|button|panel|container|node)\\s+(?:scene|subscene|component)\\s*(?:called|named)?\\s*[`'\"]?([\\w\\-/]+)[`'\"]?");
	}
	
	// Look for mentions of resources in the text
	TypedArray<RegExMatch> matches = resource_regex.search_all(p_response);
	for (int i = 0; i < matches.size(); i++) {
		Ref<RegExMatch> match = matches[i];
		if (match.is_valid()) {
			String resource_path = match->get_string(1);
			if (!resource_path.is_empty()) {
				// Ensure proper path format
				if (!resource_path.begins_with("res://")) {
					resource_path = "res://" + resource_path;
				}
				
				// Check if this resource is already in our tracking
				if (!pending_dependencies.has(resource_path) && !FileAccess::exists(resource_path)) {
					String ext = resource_path.get_extension();
					String type = "resource";
					if (ext == "gd") {
						type = "script";
					} else if (ext == "tscn") {
						type = "scene";
					}
					
					// Add as a pending dependency that needs to be created
					DependencyInfo dependency;
					dependency.path = resource_path;
					dependency.code = ""; // Will create placeholder
					dependency.type = type;
					dependency.created = false;
					pending_dependencies[resource_path] = dependency;
				}
			}
		}
	}
	
	// Look for subscene mentions
	matches = subscene_regex.search_all(p_response);
	for (int i = 0; i < matches.size(); i++) {
		Ref<RegExMatch> match = matches[i];
		if (match.is_valid()) {
			String scene_name = match->get_string(1);
			if (!scene_name.is_empty()) {
				// Add proper extension if not present
				if (!scene_name.ends_with(".tscn")) {
					scene_name += ".tscn";
				}
				
				// Ensure proper path format
				String resource_path = scene_name;
				if (!resource_path.begins_with("res://")) {
					resource_path = "res://" + resource_path;
				}
				
				// Check if this scene is already in our tracking
				if (!pending_dependencies.has(resource_path) && !FileAccess::exists(resource_path)) {
					// Add as a pending dependency that needs to be created
					DependencyInfo dependency;
					dependency.path = resource_path;
					dependency.code = ""; // Will create placeholder
					dependency.type = "scene";
					dependency.created = false;
					pending_dependencies[resource_path] = dependency;
					
					// Also check if we should create a script for this scene
					String script_path = resource_path.get_basename() + ".gd";
					if (!pending_dependencies.has(script_path) && !FileAccess::exists(script_path)) {
						DependencyInfo script_dep;
						script_dep.path = script_path;
						script_dep.code = "# Generated by VectorAI as a dependency placeholder\nextends Node2D\n\nfunc _ready():\n\tpass\n";
						script_dep.type = "script";
						script_dep.created = false;
						pending_dependencies[script_path] = script_dep;
					}
				}
			}
		}
	}
	
	// Scan for explicit lists of files to create
	static RegEx file_list_regex;
	if (!file_list_regex.is_valid()) {
		file_list_regex.compile("(?:create|make|generate|need)\\s+(?:the\\s+)?(?:following|these)\\s+(?:files|scenes|scripts):\\s*(?:\\n|\\r|\\s)*(.+?)(?:(?:\\n\\n)|$|\\Z)");
	}
	
	matches = file_list_regex.search_all(p_response);
	for (int i = 0; i < matches.size(); i++) {
		Ref<RegExMatch> match = matches[i];
		if (match.is_valid()) {
			String file_list = match->get_string(1);
			
			// Look for file names in list items
			static RegEx list_item_regex;
			if (!list_item_regex.is_valid()) {
				list_item_regex.compile("(?:[-*•]|\\d+\\.)\\s*([\\w\\-/]+(?:\\.[\\w]+)?)");
			}
			
			TypedArray<RegExMatch> item_matches = list_item_regex.search_all(file_list);
			for (int j = 0; j < item_matches.size(); j++) {
				Ref<RegExMatch> item_match = item_matches[j];
				if (item_match.is_valid()) {
					String file_name = item_match->get_string(1);
					if (!file_name.is_empty()) {
						// Determine file type from extension or context
						String ext = file_name.get_extension();
						String type = "resource";
						String resource_path = file_name;
						
						// Add default extension if missing
						if (ext.is_empty()) {
							// Check context for type hints
							if (file_list.to_lower().find("script") != -1 || 
								file_name.find("Controller") != -1 || 
								file_name.find("Manager") != -1) {
								resource_path += ".gd";
								type = "script";
							} else {
								resource_path += ".tscn";
								type = "scene";
							}
						} else {
							// Set type based on extension
							if (ext == "gd") {
								type = "script";
							} else if (ext == "tscn") {
								type = "scene";
							}
						}
						
						// Ensure proper path format
						if (!resource_path.begins_with("res://")) {
							resource_path = "res://" + resource_path;
						}
						
						// Add as pending dependency if not already tracked
						if (!pending_dependencies.has(resource_path) && !FileAccess::exists(resource_path)) {
							DependencyInfo dependency;
							dependency.path = resource_path;
							dependency.code = ""; // Will create placeholder
							dependency.type = type;
							dependency.created = false;
							pending_dependencies[resource_path] = dependency;
							
							// For scenes, check if we need associated scripts
							// Fixed the comparison that was causing the warning
							if (type == "scene" && file_list.to_lower().find("no script") == -1) {
								String script_path = resource_path.get_basename() + ".gd";
								if (!pending_dependencies.has(script_path) && !FileAccess::exists(script_path)) {
									DependencyInfo script_dep;
									script_dep.path = script_path;
									script_dep.code = "# Generated by VectorAI as a dependency placeholder\nextends Node2D\n\nfunc _ready():\n\tpass\n";
									script_dep.type = "script";
									script_dep.created = false;
									pending_dependencies[script_path] = script_dep;
								}
							}
						}
					}
				}
			}
		}
	}
	
	// Also scan for file requirements in the descriptions, not just in code
	// This helps catch references to files that Claude mentioned but didn't include code for
	static RegEx description_regex;
	if (!description_regex.is_valid()) {
		description_regex.compile("(?:refer(?:s|ring)?|depend(?:s|ing)?|based on|needs|using)\\s+(?:the|a|an)?\\s+(?:script|scene|resource|file)\\s+[`'\"]([\\w\\-/\\.]+)[`'\"]");
	}
	
	matches = description_regex.search_all(p_response);
	for (int i = 0; i < matches.size(); i++) {
		Ref<RegExMatch> match = matches[i];
		if (match.is_valid()) {
			String file_name = match->get_string(1);
			if (!file_name.is_empty()) {
				String ext = file_name.get_extension();
				String type = "resource";
				String resource_path = file_name;
				
				// Add default extension if missing
				if (ext.is_empty()) {
					if (p_response.to_lower().find(file_name + ".gd") != -1 || 
						p_response.to_lower().find("script " + file_name) != -1) {
						resource_path += ".gd";
						type = "script";
					} else {
						resource_path += ".tscn";
						type = "scene";
					}
				} else {
					// Set type based on extension
					if (ext == "gd") {
						type = "script";
					} else if (ext == "tscn") {
						type = "scene";
					}
				}
				
				// Ensure proper path format
				if (!resource_path.begins_with("res://")) {
					resource_path = "res://" + resource_path;
				}
				
				// Add as pending dependency if not already tracked
				if (!pending_dependencies.has(resource_path) && !FileAccess::exists(resource_path)) {
					DependencyInfo dependency;
					dependency.path = resource_path;
					dependency.code = ""; // Will create placeholder
					dependency.type = type;
					dependency.created = false;
					pending_dependencies[resource_path] = dependency;
				}
			}
		}
	}
}

void VectorAIPanel::_extract_dependencies_from_scene(const String &p_scene_code, Vector<String> &r_dependencies) {
	// Extract script references - more flexible pattern
	static RegEx script_regex;
	if (!script_regex.is_valid()) {
		script_regex.compile("script\\s*=\\s*(?:ExtResource|Resource)\\(\\s*[\"']([^\"']+)[\"']\\s*\\)");
	}
	
	// Extract external resource references
	static RegEx resource_regex;
	if (!resource_regex.is_valid()) {
		resource_regex.compile("(?:ExtResource|Resource)\\(\\s*[\"']([^\"']+)[\"']\\s*\\)");
	}
	
	// Extract instance references
	static RegEx instance_regex;
	if (!instance_regex.is_valid()) {
		instance_regex.compile("(?:instance|packed_scene)\\s*=\\s*(?:ExtResource|Resource)\\(\\s*[\"']([^\"']+)[\"']\\s*\\)");
	}
	
	// Extract child scene references (specifically for Godot 4)
	static RegEx child_scene_regex;
	if (!child_scene_regex.is_valid()) {
		child_scene_regex.compile("\\[node\\s+name=\\s*[\"'][^\"']*[\"']\\s+(?:instance|parent)=\\s*[\"']([^\"']+)[\"']");
	}
	
	// Look for all dependencies
	TypedArray<RegExMatch> matches;
	
	// Check for script dependencies
	matches = script_regex.search_all(p_scene_code);
	for (int i = 0; i < matches.size(); i++) {
		Ref<RegExMatch> match = matches[i];
		if (match.is_valid()) {
			String path = match->get_string(1);
			if (!path.is_empty()) {
				// Ensure proper path format
				if (!path.begins_with("res://")) {
					path = "res://" + path;
				}
				if (!r_dependencies.has(path)) {
					r_dependencies.push_back(path);
				}
			}
		}
	}
	
	// Check for other resources
	matches = resource_regex.search_all(p_scene_code);
	for (int i = 0; i < matches.size(); i++) {
		Ref<RegExMatch> match = matches[i];
		if (match.is_valid()) {
			String path = match->get_string(1);
			if (!path.is_empty()) {
				// Ensure proper path format
				if (!path.begins_with("res://")) {
					path = "res://" + path;
				}
				if (!r_dependencies.has(path)) {
					r_dependencies.push_back(path);
				}
			}
		}
	}
	
	// Check for instance dependencies
	matches = instance_regex.search_all(p_scene_code);
	for (int i = 0; i < matches.size(); i++) {
		Ref<RegExMatch> match = matches[i];
		if (match.is_valid()) {
			String path = match->get_string(1);
			if (!path.is_empty()) {
				// Ensure proper path format
				if (!path.begins_with("res://")) {
					path = "res://" + path;
				}
				if (!r_dependencies.has(path)) {
					r_dependencies.push_back(path);
				}
			}
		}
	}
	
	// Check for child scene dependencies
	matches = child_scene_regex.search_all(p_scene_code);
	for (int i = 0; i < matches.size(); i++) {
		Ref<RegExMatch> match = matches[i];
		if (match.is_valid()) {
			String path = match->get_string(1);
			if (!path.is_empty()) {
				// Ensure proper path format
				if (!path.begins_with("res://")) {
					path = "res://" + path;
				}
				if (!r_dependencies.has(path)) {
					r_dependencies.push_back(path);
				}
			}
		}
	}
}

void VectorAIPanel::_extract_dependencies_from_script(const String &p_script_code, Vector<String> &r_dependencies) {
	// Extract preload references
	static RegEx preload_regex;
	if (!preload_regex.is_valid()) {
		preload_regex.compile("preload\\(\\s*[\"']([^\"']+)[\"']\\s*\\)");
	}
	
	// Extract load references
	static RegEx load_regex;
	if (!load_regex.is_valid()) {
		load_regex.compile("load\\(\\s*[\"']([^\"']+)[\"']\\s*\\)");
	}
	
	// Extract direct references to resources
	static RegEx resource_regex;
	if (!resource_regex.is_valid()) {
		resource_regex.compile("res://[\\w\\.\\-/]+\\.(tres|tscn|gd|res|import|shader)");
	}
	
	// Look for all dependencies
	TypedArray<RegExMatch> matches;
	
	// Check for preload dependencies
	matches = preload_regex.search_all(p_script_code);
	for (int i = 0; i < matches.size(); i++) {
		Ref<RegExMatch> match = matches[i];
		if (match.is_valid()) {
			String path = match->get_string(1);
			if (!path.is_empty()) {
				// Ensure proper path format
				if (!path.begins_with("res://")) {
					path = "res://" + path;
				}
				if (!r_dependencies.has(path)) {
					r_dependencies.push_back(path);
				}
			}
		}
	}
	
	// Check for load dependencies
	matches = load_regex.search_all(p_script_code);
	for (int i = 0; i < matches.size(); i++) {
		Ref<RegExMatch> match = matches[i];
		if (match.is_valid()) {
			String path = match->get_string(1);
			if (!path.is_empty()) {
				// Ensure proper path format
				if (!path.begins_with("res://")) {
					path = "res://" + path;
				}
				if (!r_dependencies.has(path)) {
					r_dependencies.push_back(path);
				}
			}
		}
	}
	
	// Check for direct resource references
	matches = resource_regex.search_all(p_script_code);
	for (int i = 0; i < matches.size(); i++) {
		Ref<RegExMatch> match = matches[i];
		if (match.is_valid()) {
			String path = match->get_string(0); // Get the full match
			if (!path.is_empty() && !r_dependencies.has(path)) {
				r_dependencies.push_back(path);
			}
		}
	}
}

void VectorAIPanel::_process_dependencies() {
	// First, update any Godot 2/3 scene format to Godot 4
	for (KeyValue<String, DependencyInfo> &E : pending_dependencies) {
		if (E.value.type == "scene" && !E.value.code.is_empty() && E.value.code.find("format=2") != -1) {
			_update_scene_format(E.value.code);
		}
	}
	
	// Create a processing order - resources first, then scripts, then scenes
	// But always prioritize dependencies that are referenced by others
	Vector<String> dependency_counts;
	HashMap<String, int> reference_count;
	
	// First, count references
	for (KeyValue<String, DependencyInfo> &E : pending_dependencies) {
		// Initialize count if needed
		if (!reference_count.has(E.key)) {
			reference_count[E.key] = 0;
		}
		
		// Count references to other dependencies
		for (int i = 0; i < E.value.dependencies.size(); i++) {
			String dep_path = E.value.dependencies[i];
			if (pending_dependencies.has(dep_path)) {
				if (!reference_count.has(dep_path)) {
					reference_count[dep_path] = 1;
				} else {
					reference_count[dep_path]++;
				}
			}
		}
	}
	
	// Then add resources
	for (KeyValue<String, DependencyInfo> &E : pending_dependencies) {
		if (E.value.type == "resource" && !E.value.path.ends_with(".gd") && !E.value.path.ends_with(".tscn")) {
			processing_order.push_back(E.key);
		}
	}
	
	// Then add scripts
	for (KeyValue<String, DependencyInfo> &E : pending_dependencies) {
		if (E.value.type == "script") {
			processing_order.push_back(E.key);
		}
	}
	
	// Finally add scenes, ordered by dependency count (least referenced first)
	Vector<Pair<String, int>> scene_order;
	for (KeyValue<String, DependencyInfo> &E : pending_dependencies) {
		if (E.value.type == "scene") {
			int ref_count = reference_count.has(E.key) ? reference_count[E.key] : 0;
			scene_order.push_back(Pair<String, int>(E.key, ref_count));
		}
	}
	
	// Sort scenes by reference count
	scene_order.sort_custom<PairSort<String, int>>();
	
	// Add scenes to processing order
	for (int i = 0; i < scene_order.size(); i++) {
		processing_order.push_back(scene_order[i].first);
	}
	
	// Process in order
	for (int i = 0; i < processing_order.size(); i++) {
		String path = processing_order[i];
		DependencyInfo &info = pending_dependencies[path];
		
		// Check if missing dependencies need placeholder files
		for (int j = 0; j < info.dependencies.size(); j++) {
			String dep_path = info.dependencies[j];
			
			// If the dependency doesn't exist and isn't in our pending list, create a placeholder
			if (!FileAccess::exists(dep_path) && !pending_dependencies.has(dep_path)) {
				String ext = dep_path.get_extension();
				if (ext == "gd") {
					// Create empty script placeholder
					DependencyInfo script_dep;
					script_dep.path = dep_path;
					script_dep.type = "script";
					script_dep.created = false;
					script_dep.code = "# Generated by VectorAI as a dependency placeholder\nextends Node\n\nfunc _ready():\n\tpass\n";
					pending_dependencies[dep_path] = script_dep;
					
					// Add to processing before the current file
					processing_order.insert(i, dep_path);
					i++; // Skip ahead to account for the inserted item
				} else if (ext == "tscn") {
					// Create empty scene placeholder
					DependencyInfo scene_dep;
					scene_dep.path = dep_path;
					scene_dep.type = "scene";
					scene_dep.created = false;
					
					// Generate a unique ID for the scene
					String uid_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
					String uid = "uid://";
					RandomNumberGenerator rng;
					rng.randomize();
					for (int k = 0; k < 22; k++) {
						uid += uid_chars[rng.randi() % uid_chars.length()];
					}
					
					// Create a better placeholder based on the filename
					String node_name = dep_path.get_basename().get_file();
					String node_type = "Node2D";
					
					// Try to infer the node type from the name
					if (node_name.to_lower().find("control") != -1 || 
						node_name.to_lower().find("panel") != -1 || 
						node_name.to_lower().find("ui") != -1 ||
						node_name.to_lower().find("menu") != -1) {
						node_type = "Control";
					} else if (node_name.to_lower().find("sprite") != -1) {
						node_type = "Sprite2D";
					} else if (node_name.to_lower().find("player") != -1 || 
							   node_name.to_lower().find("character") != -1 || 
							   node_name.to_lower().find("enemy") != -1) {
						node_type = "CharacterBody2D";
					} else if (node_name.to_lower().find("3d") != -1) {
						node_type = "Node3D";
					}
					
					scene_dep.code = "[gd_scene format=3 uid=\"" + uid + "\"]\n\n[node name=\"" + node_name + "\" type=\"" + node_type + "\"]\n";
					
					// Add a script reference if a matching script exists or is pending
					String script_path = dep_path.get_basename() + ".gd";
					if (FileAccess::exists(script_path) || pending_dependencies.has(script_path)) {
						scene_dep.code += "script = ExtResource(\"" + script_path.get_file() + "\")\n";
					}
					
					pending_dependencies[dep_path] = scene_dep;
					
					// Add to processing before the current file
					processing_order.insert(i, dep_path);
					i++; // Skip ahead to account for the inserted item
				} else if (ext == "tres") {
					// Create placeholder resource
					_create_placeholder_resource(dep_path);
				}
			}
		}
		
		// Now apply this file
		if (!info.created) {
			if (!info.code.is_empty()) {
				// Apply the code changes directly
				_auto_apply_changes(info.code, info.path);
			} else {
				// Create a placeholder if needed
				if (info.type == "script") {
					// Try to determine a better script template based on filename
					String file_name = info.path.get_file().to_lower();
					String base_type = "Node";
					String class_name = info.path.get_basename().get_file().capitalize().replace(" ", "");
					
					if (file_name.find("player") != -1 || file_name.find("character") != -1 || file_name.find("enemy") != -1) {
						base_type = "CharacterBody2D";
					} else if (file_name.find("ui") != -1 || file_name.find("menu") != -1 || file_name.find("button") != -1) {
						base_type = "Control";
					} else if (file_name.find("sprite") != -1) {
						base_type = "Sprite2D";
					} else if (file_name.find("3d") != -1) {
						base_type = "Node3D";
					} else if (file_name.find("resource") != -1) {
						base_type = "Resource";
					}
					
					info.code = _generate_script_template(base_type, class_name);
					_auto_apply_changes(info.code, info.path);
				} else if (info.type == "scene") {
					_create_placeholder_scene(info.path);
				} else if (info.path.ends_with(".tres")) {
					_create_placeholder_resource(info.path);
				}
			}
			info.created = true;
		}
	}
}

void VectorAIPanel::_create_placeholder_resource(const String &p_resource_path) {
	// Create directory if needed
	String dir = p_resource_path.get_base_dir();
	if (!DirAccess::exists(dir)) {
		Error err = DirAccess::make_dir_recursive_absolute(dir);
		if (err != OK) {
			_add_claude_message("Error: Failed to create directory for " + p_resource_path + ". Error code: " + itos(err));
			return;
		}
	}
	
	// Determine resource type from extension
	String resource_type = "Resource";
	String extension = p_resource_path.get_extension();
	if (extension == "tres") {
		// Try to determine more specific type based on filename
		String filename = p_resource_path.get_file().to_lower();
		if (filename.find("tileset") != -1) {
			resource_type = "TileSet";
		} else if (filename.find("theme") != -1) {
			resource_type = "Theme";
		} else if (filename.find("material") != -1) {
			resource_type = "Material";
		} else if (filename.find("font") != -1) {
			resource_type = "Font";
		} else if (filename.find("texture") != -1 || filename.find("image") != -1) {
			resource_type = "Texture2D";
		}
	}
	
	// Create basic placeholder resource
	String content = "[gd_resource type=\"" + resource_type + "\" format=3]\n\n[resource]\n";
	
	// Add resource-specific properties if needed
	if (resource_type == "TileSet") {
		content += "tile_shape = 0\n";
	}
	
	// Write file
	Error err = OK;
	Ref<FileAccess> f = FileAccess::open(p_resource_path, FileAccess::WRITE, &err);
	if (err != OK) {
		_add_claude_message("Error: Failed to create placeholder resource " + p_resource_path + ". Error code: " + itos(err));
		return;
	}
	
	f->store_string(content);
	_add_claude_message("Created placeholder resource: " + p_resource_path.get_file());
}

void VectorAIPanel::_create_placeholder_scene(const String &p_scene_path) {
    // Create directory if needed
    String dir = p_scene_path.get_base_dir();
    if (!DirAccess::exists(dir)) {
        Error err = DirAccess::make_dir_recursive_absolute(dir);
        if (err != OK) {
            _add_claude_message("Error: Failed to create directory for " + p_scene_path + ". Error code: " + itos(err));
            return;
        }
    }
    
    // Generate a unique ID for the scene
    String uid_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    String uid = "uid://";
    RandomNumberGenerator rng;
    rng.randomize();
    for (int i = 0; i < 22; i++) {
        uid += uid_chars[rng.randi() % uid_chars.length()];
    }
    
    // Create a better placeholder based on the filename
    String node_name = p_scene_path.get_basename().get_file();
    String node_type = "Node2D";
    
    // Try to infer the node type from the name
    if (node_name.to_lower().find("control") != -1 || 
        node_name.to_lower().find("panel") != -1 || 
        node_name.to_lower().find("ui") != -1 ||
        node_name.to_lower().find("menu") != -1) {
        node_type = "Control";
    } else if (node_name.to_lower().find("sprite") != -1) {
        node_type = "Sprite2D";
    } else if (node_name.to_lower().find("player") != -1 || 
             node_name.to_lower().find("character") != -1 || 
             node_name.to_lower().find("enemy") != -1) {
        node_type = "CharacterBody2D";
    } else if (node_name.to_lower().find("3d") != -1) {
        node_type = "Node3D";
    } else if (node_name.to_lower().find("tile") != -1 || 
             node_name.to_lower().find("map") != -1) {
        node_type = "TileMap";
    }
    
    // Build the scene content
    String content = "[gd_scene format=3 uid=\"" + uid + "\"]\n\n";
    content += "[node name=\"" + node_name + "\" type=\"" + node_type + "\"]\n";
    
    // Write scene file
    Error err = OK;
    Ref<FileAccess> f = FileAccess::open(p_scene_path, FileAccess::WRITE, &err);
    if (err != OK) {
        _add_claude_message("Error: Failed to create placeholder scene " + p_scene_path + ". Error code: " + itos(err));
        return;
    }
    
    f->store_string(content);
    _add_claude_message("Created placeholder scene: " + p_scene_path.get_file());
    
    // Force a file system scan to update dependencies
    EditorFileSystem::get_singleton()->scan();
}

void VectorAIPanel::_update_scene_format(String &p_scene_code) {
	// Convert from Godot 2/3 format to Godot 4 format
	
	// Update format version
	p_scene_code = p_scene_code.replace("[gd_scene load_steps=", "[gd_scene format=3 load_steps=");
	p_scene_code = p_scene_code.replace("[gd_scene format=2", "[gd_scene format=3");
	
	// Generate a unique ID for the scene if not present
	if (p_scene_code.find("uid=") == -1) {
		String uid_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
		String uid = "uid://";
		RandomNumberGenerator rng;
		rng.randomize();
		for (int i = 0; i < 22; i++) {
			uid += uid_chars[rng.randi() % uid_chars.length()];
		}
		
		// Add UID if not present
		p_scene_code = p_scene_code.replace("[gd_scene format=3", "[gd_scene format=3 uid=\"" + uid + "\"");
	}
	
	// Fix resource references
	static RegEx ext_resource_regex;
	if (!ext_resource_regex.is_valid()) {
		ext_resource_regex.compile("ExtResource\\(\\s*(\\d+)\\s*\\)");
	}
	
	TypedArray<RegExMatch> matches = ext_resource_regex.search_all(p_scene_code);
	for (int i = 0; i < matches.size(); i++) {
		Ref<RegExMatch> match = matches[i];
		String id = match->get_string(1);
		p_scene_code = p_scene_code.replace("ExtResource(" + id + ")", "ExtResource(\"" + id + "\")");
	}
	
	// Fix sub-resource references
	static RegEx sub_resource_regex;
	if (!sub_resource_regex.is_valid()) {
		sub_resource_regex.compile("SubResource\\(\\s*(\\d+)\\s*\\)");
	}
	
	matches = sub_resource_regex.search_all(p_scene_code);
	for (int i = 0; i < matches.size(); i++) {
		Ref<RegExMatch> match = matches[i];
		String id = match->get_string(1);
		p_scene_code = p_scene_code.replace("SubResource(" + id + ")", "SubResource(\"" + id + "\")");
	}
	
	// Fix double quotes if necessary
	p_scene_code = p_scene_code.replace("\"\"", "\"");
	
	// Update array types
	p_scene_code = p_scene_code.replace("PoolIntArray", "PackedInt32Array");
	p_scene_code = p_scene_code.replace("PoolByteArray", "PackedByteArray");
	p_scene_code = p_scene_code.replace("PoolRealArray", "PackedFloat32Array");
	p_scene_code = p_scene_code.replace("PoolStringArray", "PackedStringArray");
	p_scene_code = p_scene_code.replace("PoolVector2Array", "PackedVector2Array");
	p_scene_code = p_scene_code.replace("PoolVector3Array", "PackedVector3Array");
	p_scene_code = p_scene_code.replace("PoolColorArray", "PackedColorArray");
	
	// Update class names
	p_scene_code = p_scene_code.replace("Spatial", "Node3D");
	p_scene_code = p_scene_code.replace("KinematicBody", "CharacterBody3D");
	p_scene_code = p_scene_code.replace("KinematicBody2D", "CharacterBody2D");
	p_scene_code = p_scene_code.replace("RigidBody", "RigidBody3D");
	p_scene_code = p_scene_code.replace("StaticBody", "StaticBody3D");
	p_scene_code = p_scene_code.replace("MeshInstance", "MeshInstance3D");
	p_scene_code = p_scene_code.replace("Sprite ", "Sprite2D ");
	p_scene_code = p_scene_code.replace("type=\"Sprite\"", "type=\"Sprite2D\"");
	p_scene_code = p_scene_code.replace("AnimatedSprite ", "AnimatedSprite2D ");
	p_scene_code = p_scene_code.replace("type=\"AnimatedSprite\"", "type=\"AnimatedSprite2D\"");
	p_scene_code = p_scene_code.replace("CollisionShape ", "CollisionShape3D ");
	p_scene_code = p_scene_code.replace("type=\"CollisionShape\"", "type=\"CollisionShape3D\"");
	p_scene_code = p_scene_code.replace("Camera ", "Camera3D ");
	p_scene_code = p_scene_code.replace("type=\"Camera\"", "type=\"Camera3D\"");
	p_scene_code = p_scene_code.replace("Light ", "Light3D ");
	p_scene_code = p_scene_code.replace("type=\"Light\"", "type=\"Light3D\"");
	p_scene_code = p_scene_code.replace("Position3D", "Marker3D");
	p_scene_code = p_scene_code.replace("Position2D", "Marker2D");
	
	// Update property names
	p_scene_code = p_scene_code.replace("use_in_baked_light", "bake_mode");
	p_scene_code = p_scene_code.replace("transform/", "");
	p_scene_code = p_scene_code.replace("z/z", "z_index");
	p_scene_code = p_scene_code.replace("xy_scale", "scale");
	p_scene_code = p_scene_code.replace("\"texture\"", "\"texture\"");
	
	// Fix common script reference errors
	static RegEx script_path_regex;
	if (!script_path_regex.is_valid()) {
		script_path_regex.compile("script\\s*=\\s*ExtResource\\(\"([^\"]+)\"\\)");
	}
	
	matches = script_path_regex.search_all(p_scene_code);
	for (int i = 0; i < matches.size(); i++) {
		Ref<RegExMatch> match = matches[i];
		String script_path = match->get_string(1);
		
		// If script path doesn't include .gd extension, add it
		if (!script_path.ends_with(".gd") && !script_path.ends_with(".vs") && !script_path.ends_with(".cs")) {
			String new_path = script_path + ".gd";
			p_scene_code = p_scene_code.replace("script = ExtResource(\"" + script_path + "\")", 
												"script = ExtResource(\"" + new_path + "\")");
		}
	}
}

bool VectorAIPanel::_has_all_dependencies(const String &p_path) {
	// Check if a file has all its dependencies available
	if (!pending_dependencies.has(p_path)) {
		return true; // Not tracked, assume it's fine
	}
	
	const DependencyInfo &info = pending_dependencies[p_path];
	
	// Check each dependency
	for (int i = 0; i < info.dependencies.size(); i++) {
		String dep_path = info.dependencies[i];
		
		// Check if the dependency exists or is being created
		if (!FileAccess::exists(dep_path) && 
			(!pending_dependencies.has(dep_path) || !pending_dependencies[dep_path].created)) {
			return false;
		}
	}
	
	return true;
}

void VectorAIPanel::_on_claude_error(const String &p_error) {
	print_line("VectorAI: Received error: " + p_error);
	
	// Clear status steps and show error
	_clear_status_steps();
	
	// Remove "Thinking..." message if it exists (legacy support)
	for (int i = 0; i < chat_messages->get_child_count(); i++) {
		Control *message = Object::cast_to<Control>(chat_messages->get_child(i));
		if (message && message->has_meta("is_thinking")) {
			print_line("VectorAI: Removing thinking message due to error");
			message->queue_free();
			break;
		}
	}

	// Add error message to chat
	_add_claude_message("❌ **Error**: " + p_error);
}

void VectorAIPanel::_add_user_message(const String &p_text) {
	// Add descriptive prefix based on composer mode
	String display_text = p_text;
	if (composer_mode_active) {
		if (p_text.begins_with("Create")) {
			display_text = "🎨 " + p_text;
		} else if (p_text.begins_with("Edit")) {
			display_text = "✏️ " + p_text;
		} else if (p_text.begins_with("Add")) {
			display_text = "➕ " + p_text;
		} else if (p_text.begins_with("Remove")) {
			display_text = "➖ " + p_text;
		}
	}

	Control *message = _create_message_panel("You", display_text);

	// Find the panel container in the message
	HBoxContainer *hbox = Object::cast_to<HBoxContainer>(Object::cast_to<MarginContainer>(message)->get_child(0));
	PanelContainer *panel = nullptr;

	// User messages are on the right (second child of hbox)
	if (hbox && hbox->get_child_count() >= 2) {
		panel = Object::cast_to<PanelContainer>(hbox->get_child(1));
	}

	if (panel) {
		panel->add_theme_style_override("panel", user_message_style);
	}

	chat_messages->add_child(message);
	_scroll_to_bottom();
}

void VectorAIPanel::_add_claude_message(const String &p_text, bool p_is_thinking) {
	Control *message = _create_message_panel("VectorAI", p_text);

	// Find the panel container in the message
	HBoxContainer *hbox = Object::cast_to<HBoxContainer>(Object::cast_to<MarginContainer>(message)->get_child(0));
	PanelContainer *panel = nullptr;

	// Assistant messages are on the left (first child of hbox)
	if (hbox && hbox->get_child_count() >= 1) {
		panel = Object::cast_to<PanelContainer>(hbox->get_child(0));
	}

	if (panel) {
		panel->add_theme_style_override("panel", assistant_message_style);
	}

	// If this is a "Thinking..." message, mark it
	if (p_is_thinking || p_text.begins_with("Thinking...")) {
		message->set_meta("is_thinking", true);
	}

	chat_messages->add_child(message);
	_scroll_to_bottom();

	// Start typewriter animation, but only if the message isn't too long
	// to avoid performance issues and only if not thinking
	if (!p_is_thinking && p_text.length() < 1000) {
		VBoxContainer *vbox = nullptr;
		if (panel && panel->is_inside_tree()) {
			vbox = Object::cast_to<VBoxContainer>(panel->get_child(0));
		}
		
		if (vbox && vbox->is_inside_tree()) {
			RichTextLabel *message_label = Object::cast_to<RichTextLabel>(vbox->get_child(1));
			if (message_label && message_label->is_inside_tree()) {
				// Store the label in the message so we can find it later
				message->set_meta("message_label", message_label);
				
				message_label->set_visible_characters(0);
				message_label->set_visible_characters_behavior(TextServer::VC_CHARS_BEFORE_SHAPING);
				
				// Use a safer approach for typewriter effect
				call_deferred("_start_typewriter_animation", message);
			}
		}
	}
}

void VectorAIPanel::_start_typewriter_animation(Control *p_message) {
	// Make sure we're still in the tree
	if (!is_inside_tree() || !p_message || !p_message->is_inside_tree()) {
		return;
	}
	
	if (p_message->has_meta("message_label")) {
		Variant label_var = p_message->get_meta("message_label");
		Object *obj = label_var;
		RichTextLabel *label = Object::cast_to<RichTextLabel>(obj);
		
		if (label && label->is_inside_tree()) {
			// Schedule the first tick
			Ref<SceneTreeTimer> timer = get_tree()->create_timer(0.02);
			timer->connect("timeout", callable_mp(this, &VectorAIPanel::_on_typewriter_tick).bind(label));
		}
	}
}

void VectorAIPanel::_on_typewriter_tick(RichTextLabel *p_label) {
	// Safety check - make sure we and the label are still in the tree
	if (!is_inside_tree() || !p_label || !p_label->is_inside_tree()) {
		return;
	}

	int current_chars = p_label->get_visible_characters();
	int total_chars = p_label->get_total_character_count();
	
	if (current_chars < total_chars) {
		// Calculate how many characters to show (stream faster for longer texts)
		int chars_per_tick = MAX(1, total_chars / 200); // Adjust speed based on length
		int new_pos = MIN(current_chars + chars_per_tick, total_chars);
		
		// Update visible characters
		p_label->set_visible_characters(new_pos);
		
		// Schedule next tick
		Ref<SceneTreeTimer> timer = get_tree()->create_timer(0.02);
		// Use a weak reference binding to avoid crashes if panel is destroyed
		if (timer.is_valid() && is_inside_tree()) {
			timer->connect("timeout", callable_mp(this, &VectorAIPanel::_on_typewriter_tick).bind(p_label));
		}
	}
}

Control *VectorAIPanel::_create_message_panel(const String &p_sender, const String &p_text) {
	// Create a container for the entire message
	MarginContainer *container = memnew(MarginContainer);
	container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	container->add_theme_constant_override("margin_top", 5 * EDSCALE);
	container->add_theme_constant_override("margin_bottom", 5 * EDSCALE);

	// Create a horizontal container to position the message
	HBoxContainer *hbox = memnew(HBoxContainer);
	hbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	container->add_child(hbox);

	// Create the message panel
	PanelContainer *panel = memnew(PanelContainer);
	panel->set_h_size_flags(Control::SIZE_FILL);

	// Set width to 70% of panel width
	panel->set_custom_minimum_size(Size2(PANEL_WIDTH * 0.7, 0));

	VBoxContainer *vbox = memnew(VBoxContainer);
	panel->add_child(vbox);

	Label *sender_label = memnew(Label);
	sender_label->set_text(p_sender);
	sender_label->add_theme_font_size_override("font_size", 14 * EDSCALE);
	sender_label->add_theme_color_override("font_color", Color(0.5, 0.5, 0.5));
	vbox->add_child(sender_label);

	RichTextLabel *message_label = memnew(RichTextLabel);
	message_label->set_text(p_text);
	message_label->set_fit_content(true);
	message_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	message_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	message_label->set_scroll_active(false); // Let the parent ScrollContainer handle scrolling
	message_label->set_selection_enabled(true); // Allow text selection
	vbox->add_child(message_label);

	// Position user messages on the right, others on the left
	if (p_sender == "You") {
		// User message - right aligned
		Control *spacer = memnew(Control);
		spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		hbox->add_child(spacer);
		hbox->add_child(panel);
	} else {
		// AI or system message - left aligned
		hbox->add_child(panel);
		Control *spacer = memnew(Control);
		spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		hbox->add_child(spacer);
	}

	return container;
}

void VectorAIPanel::_scroll_to_bottom() {
	chat_scroll->set_v_scroll(chat_scroll->get_v_scroll_bar()->get_max());
}

void VectorAIPanel::_update_api_key_button() {
	if (is_api_key_set) {
		api_key_button->set_text("API Key ✓");
		api_key_button->add_theme_color_override("font_color", Color(0.2, 0.8, 0.2));
	} else {
		api_key_button->set_text("Set API Key");
		api_key_button->add_theme_color_override("font_color", Color(0.8, 0.2, 0.2));
	}
}

void VectorAIPanel::_send_message_deferred(const String &p_message) {
	print_line("VectorAI: _send_message_deferred called with message length: " + itos(p_message.length()));
	
	// Make sure we still have a valid API reference (in case it was deleted)
	if (!is_inside_tree() || !claude_api || !claude_api->is_inside_tree()) {
		print_line("VectorAI: API connection lost or not in tree");
		_clear_status_steps();
		_add_claude_message("Error: API connection lost. Please try again.");
		return;
	}

	// Update status
	_update_status_step("🧠 Thinking");
	_show_status_step("🧠 Thinking", "Processing your request...");

	// Prepare the message with context
	String full_message = p_message;
	
	// Add attached file content if available
	if (!attached_file_path.is_empty() && !attached_file_content.is_empty()) {
		full_message += "\n\nAttached file: " + attached_file_path;
		
		// For large files, summarize instead of sending the full content
		if (attached_file_content.length() > 10000) {
			full_message += " (large file - " + String::num_int64(attached_file_content.length()) + " characters)";
				} else {
			// Add file content with proper formatting
			String ext = attached_file_path.get_extension();
			full_message += "\n\n```" + ext + "\n" + attached_file_content + "\n```";
		}
	}
	
	print_line("VectorAI: Sending message to Claude API, final length: " + itos(full_message.length()));
	
	// Send the message to Claude API
	claude_api->send_message(full_message);
}

bool VectorAIPanel::_extract_multiple_code_blocks(const String &p_text, Vector<Dictionary> &r_code_blocks) {
	// Look for code blocks in the format: ```language\n...\n```
	static RegEx regex;
	if (!regex.is_valid()) {
		// Use DOTALL mode ((?s)) to make . match newlines
		regex.compile("```(?:[a-zA-Z0-9_+-]+)?\\s*\\n((?s:.+?))\\n```");
	}

	// Look for file path indicators that may precede code blocks
	static RegEx file_regex;
	if (!file_regex.is_valid()) {
		file_regex.compile("(?:file:|path:|for |in |to |creates?|save|generating?|make|write)(?:the |file |a |an )?[`'\"]?([\\w\\.\\-/]+\\.[a-zA-Z0-9]+)[`'\"]?");
	}

	// Find all code blocks
	TypedArray<RegExMatch> matches = regex.search_all(p_text);
	if (matches.size() == 0) {
		return false;
	}

	// Process each code block
	for (int i = 0; i < matches.size(); i++) {
		Ref<RegExMatch> match = matches[i];
		if (match.is_valid()) {
			String code = match->get_string(1);
			String file_path;

			// Try to find a file path before this code block
			int block_pos = match->get_start(0);
			String text_before_block = p_text.substr(0, block_pos);
			
			// Look for the last file path mentioned before this code block
			TypedArray<RegExMatch> path_matches = file_regex.search_all(text_before_block);
			if (path_matches.size() > 0) {
				Ref<RegExMatch> path_match = path_matches[path_matches.size() - 1];
				if (path_match.is_valid()) {
					String path = path_match->get_string(1);
					if (!path.is_empty()) {
						// Make sure path is prefixed with res:// if needed
						if (!path.begins_with("res://") && !path.begins_with("/")) {
							file_path = "res://" + path;
						} else if (path.begins_with("/")) {
							file_path = "res://" + path.substr(1);
						} else {
							file_path = path;
						}
					}
				}
			}

			// If no file path was found, try to determine from content
			if (file_path.is_empty()) {
				// Check if this is a scene file (contains both TSCN and GDScript)
				if (code.begins_with("[gd_scene")) {
					// Generate a scene name based on content
					String scene_name = "scene_" + String::num_int64(OS::get_singleton()->get_unix_time());
					
					// Try to extract a better name from the node name
					static RegEx node_name_regex;
					if (!node_name_regex.is_valid()) {
						node_name_regex.compile("\\[node name=\"([^\"]+)\"");
					}
					
					Ref<RegExMatch> node_match = node_name_regex.search(code);
					if (node_match.is_valid()) {
						String node_name = node_match->get_string(1);
						if (!node_name.is_empty()) {
							scene_name = node_name.to_lower().replace(" ", "_");
						}
					}
					
					file_path = "res://" + scene_name + ".tscn";
				}
				else if (code.find("extends ") != -1 ||
						code.find("func ") != -1 ||
						code.find("class_name ") != -1) {
					// Generate a script name
					String script_name = "script_" + String::num_int64(OS::get_singleton()->get_unix_time());
					
					// Try to extract class_name if available
					static RegEx class_regex;
					if (!class_regex.is_valid()) {
						class_regex.compile("class_name\\s+([A-Za-z0-9_]+)");
					}
					
					Ref<RegExMatch> class_match = class_regex.search(code);
					if (class_match.is_valid()) {
						String class_name = class_match->get_string(1);
						if (!class_name.is_empty()) {
							script_name = class_name;
						}
					}
					
					file_path = "res://" + script_name + ".gd";
				}
				else {
					// Generic resource file
					String ext = ".txt";
					if (code.begins_with("{") || code.begins_with("[")) {
						ext = ".json";
					} else if (code.find("<") != -1 && code.find(">") != -1) {
						ext = ".xml";
					}
					
					file_path = "res://resource_" + String::num_int64(OS::get_singleton()->get_unix_time()) + ext;
				}
			}

			// Add code block to the result
			Dictionary block;
			block["code"] = code;
			block["file_path"] = file_path;
			r_code_blocks.push_back(block);
		}
	}

	return r_code_blocks.size() > 0;
}

bool VectorAIPanel::_extract_code_block(const String &p_text, String &r_code, String &r_file_path) {
	// Look for code blocks in the format: ```language\n...\n```
	static RegEx regex;
	if (!regex.is_valid()) {
		// Use DOTALL mode ((?s)) to make . match newlines
		regex.compile("```(?:[a-zA-Z0-9_+-]+)?\\s*\\n((?s:.+?))\\n```");
	}

	// Look for file path indicators
	static RegEx file_regex;
	if (!file_regex.is_valid()) {
		file_regex.compile("(?:file:|path:|in |to |for |save|generate|create|make|write)(?:the |file |a |an )?[`'\"]?([\\w\\.\\-/]+\\.[a-zA-Z0-9]+)[`'\"]?");
	}

	// First look for file path in the text
	Ref<RegExMatch> file_match = file_regex.search(p_text);
	if (file_match.is_valid()) {
		String path = file_match->get_string(1);
		if (!path.is_empty()) {
			// Make sure path is prefixed with res:// if needed
			if (!path.begins_with("res://") && !path.begins_with("/")) {
				r_file_path = "res://" + path;
			} else if (path.begins_with("/")) {
				r_file_path = "res://" + path.substr(1);
			} else {
				r_file_path = path;
			}
		}
	}

	// Search for code blocks
	TypedArray<RegExMatch> matches = regex.search_all(p_text);
	if (matches.size() > 0) {
		// Use the first code block found
		Ref<RegExMatch> match = matches[0];
		if (match.is_valid()) {
			String extracted_code = match->get_string(1);
			if (!extracted_code.is_empty()) {
				r_code = extracted_code;

				// If no file path was found, try to determine from content
				if (r_file_path.is_empty()) {
					if (extracted_code.begins_with("[gd_scene")) {
						// Generate a scene name with timestamp
						String scene_name = "scene_" + String::num_int64(OS::get_singleton()->get_unix_time());
						r_file_path = "res://" + scene_name + ".tscn";
					} else if (extracted_code.find("extends ") != -1 ||
							   extracted_code.find("func ") != -1 ||
							   extracted_code.find("class_name ") != -1) {
						// Generate a script name with timestamp or use class name
						String script_name = "script_" + String::num_int64(OS::get_singleton()->get_unix_time());
						r_file_path = "res://" + script_name + ".gd";
					} else {
						// Determine extension based on content
						String ext = ".txt";
						if (extracted_code.begins_with("{") || extracted_code.begins_with("[")) {
							ext = ".json";
						} else if (extracted_code.find("<") != -1 && extracted_code.find(">") != -1) {
							ext = ".xml";
						}
						
						r_file_path = "res://resource_" + String::num_int64(OS::get_singleton()->get_unix_time()) + ext;
					}
				}

				return true;
			}
		}
	}

	return false;
}

void VectorAIPanel::_auto_apply_changes(const String &p_code, const String &p_target_file) {
	print_line("VectorAI: Auto-applying changes to " + p_target_file);
	
	// Create directory if it doesn't exist (for new files)
	String dir = p_target_file.get_base_dir();
	if (!dir.is_empty() && !DirAccess::exists(dir)) {
		Error err = DirAccess::make_dir_recursive_absolute(dir);
		if (err != OK) {
			_add_claude_message("Error: Failed to create directory for " + p_target_file + ". Error code: " + itos(err));
			return;
		}
	}
	
	// Process the code based on file type
	String final_code = p_code;
	String ext = p_target_file.get_extension().to_lower();
	
	// Save the file
	Error err = OK;
	Ref<FileAccess> f = FileAccess::open(p_target_file, FileAccess::WRITE, &err);
	if (err != OK) {
		_add_claude_message("Error: Failed to save file " + p_target_file + ". Error code: " + itos(err));
		return;
	}
	f->store_string(final_code);
	
	// Report success
	String action = FileAccess::exists(p_target_file) ? "updated" : "created";
	_add_claude_message("Successfully " + action + " " + p_target_file.get_file());
	
	print_line("VectorAI: Successfully wrote " + itos(final_code.length()) + " characters to " + p_target_file);
	
	// Trigger resource reimport
	if (ResourceLoader::exists(p_target_file)) {
		EditorFileSystem::get_singleton()->update_file(p_target_file);
	}
}

// Auto-attach current file functionality
void VectorAIPanel::_auto_attach_current_file() {
	if (!auto_attach_enabled || !is_inside_tree()) {
		return;
	}
	
	String current_file;
	
	// First try to get current script
	current_file = _get_current_script_path();
	
	// If no script, try current scene
	if (current_file.is_empty()) {
		current_file = _get_current_scene_path();
	}
	
	// If we found a file and it's different from current, attach it
	if (!current_file.is_empty() && current_file != current_attached_file) {
		current_attached_file = current_file;
		_read_file_content(current_file);
		print_line("VectorAI: Auto-attached file: " + current_file);
	}
}

String VectorAIPanel::_get_current_script_path() {
	// Access ScriptEditor through EditorInterface
	ScriptEditor *script_editor = EditorInterface::get_singleton()->get_script_editor();
	if (!script_editor) {
		return "";
	}
	
	// Get currently edited script
	// Since _get_current_script is private, use get_open_scripts as fallback
	Vector<Ref<Script>> open_scripts = script_editor->get_open_scripts();
	if (open_scripts.size() > 0) {
		// Return the first script as a fallback - may not always be current but it's something
		Ref<Script> current_script = open_scripts[0];
		if (current_script.is_valid()) {
			String path = current_script->get_path();
			if (!path.is_empty()) {
				return path;
			}
		}
	}
	
	return "";
}

String VectorAIPanel::_get_current_scene_path() {
	// Get current scene from EditorInterface
	Node *edited_scene = EditorInterface::get_singleton()->get_edited_scene_root();
	if (edited_scene) {
		String path = edited_scene->get_scene_file_path();
		if (!path.is_empty()) {
			return path;
		}
	}
	
	return "";
}

void VectorAIPanel::_read_file_content(const String &p_path) {
	// Check if the file exists
	if (!FileAccess::exists(p_path)) {
		return;
	}
	
	// Try to load the file content
	Error err = OK;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);
	
	if (err != OK) {
		print_line("VectorAI: Failed to open auto-attached file: " + p_path);
		return;
	}
	
	String content = f->get_as_text();
	
	if (content.is_empty()) {
		return;
	}
	
	// Store the path and content
	attached_file_path = p_path;
	attached_file_content = content;
	
	// Update Claude API with context
	if (claude_api) {
		claude_api->set_active_scene(attached_file_path);
		claude_api->set_file_context(attached_file_content);
	}
	
	// Show a subtle notification (only if it's a new file)
	static String last_notified_file = "";
	if (p_path != last_notified_file) {
		last_notified_file = p_path;
		print_line("VectorAI: Auto-reading context from: " + p_path.get_file());
	}
}

// Status step system implementation
void VectorAIPanel::_show_status_step(const String &p_step, const String &p_description) {
	if (!status_container || !status_steps) {
		return;
	}
	
	// Make status container visible
	status_container->set_visible(true);
	
	// Create status step UI
	HBoxContainer *step_container = memnew(HBoxContainer);
	step_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	
	// Step icon/text
	Label *step_label = memnew(Label);
	step_label->set_text(p_step);
	step_label->add_theme_font_size_override("font_size", 12 * EDSCALE);
	step_label->add_theme_color_override("font_color", Color(0.8, 0.8, 0.9));
	step_container->add_child(step_label);
	
	// Description
	if (!p_description.is_empty()) {
		Label *desc_label = memnew(Label);
		desc_label->set_text(p_description);
		desc_label->add_theme_font_size_override("font_size", 10 * EDSCALE);
		desc_label->add_theme_color_override("font_color", Color(0.6, 0.6, 0.7));
		desc_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		step_container->add_child(desc_label);
	}
	
	// Add animated loading indicator for current step
	Control *spinner = memnew(Control);
	spinner->set_custom_minimum_size(Size2(16, 16) * EDSCALE);
	spinner->set_meta("is_spinner", true);
	step_container->add_child(spinner);
	
	// Store reference to current step
	current_status_message = step_container;
	current_step = p_step;
	
	status_steps->add_child(step_container);
	
	// Auto-scroll to show status
	_scroll_to_bottom();
}

void VectorAIPanel::_update_status_step(const String &p_step) {
	// This could be used to update an existing step, for now just track current step
	current_step = p_step;
}

void VectorAIPanel::_complete_status_step() {
	// Hide the status container after a short delay
	if (status_container) {
		// Create a timer to hide the status after 2 seconds
		Timer *hide_timer = memnew(Timer);
		hide_timer->set_wait_time(2.0);
		hide_timer->set_one_shot(true);
		hide_timer->connect("timeout", callable_mp(this, &VectorAIPanel::_clear_status_steps));
		add_child(hide_timer);
		hide_timer->start();
		
		// Change spinner to checkmark for completion
		if (current_status_message) {
			for (int i = 0; i < current_status_message->get_child_count(); i++) {
				Control *child = Object::cast_to<Control>(current_status_message->get_child(i));
				if (child && child->has_meta("is_spinner")) {
					// Replace spinner with checkmark
					Label *checkmark = memnew(Label);
					checkmark->set_text("✅");
					checkmark->add_theme_font_size_override("font_size", 12 * EDSCALE);
					current_status_message->remove_child(child);
					current_status_message->add_child(checkmark);
					child->queue_free();
					break;
				}
			}
		}
	}
}

void VectorAIPanel::_clear_status_steps() {
	if (status_container) {
		status_container->set_visible(false);
	}
	
	if (status_steps) {
		// Clear all step children
		for (int i = status_steps->get_child_count() - 1; i >= 0; i--) {
			Node *child = status_steps->get_child(i);
			if (child) {
				status_steps->remove_child(child);
				child->queue_free();
			}
		}
	}
	
	current_status_message = nullptr;
	current_step = "";
}

// Real-time text streaming implementation
void VectorAIPanel::_start_text_streaming(const String &p_text, RichTextLabel *p_label) {
	if (!p_label || !p_label->is_inside_tree() || !is_inside_tree()) {
		return;
	}
	
	// Start streaming from position 0
	streaming_active = true;
	call_deferred("_stream_text_tick", p_label, p_text, 0);
}

void VectorAIPanel::_stream_text_tick(RichTextLabel *p_label, const String &p_full_text, int p_current_pos) {
	// Safety checks
	if (!streaming_active || !p_label || !p_label->is_inside_tree() || !is_inside_tree()) {
		streaming_active = false;
		return;
	}
	
	int total_chars = p_full_text.length();
	
	if (p_current_pos < total_chars) {
		// Calculate how many characters to show (stream faster for longer texts)
		int chars_per_tick = MAX(1, total_chars / 200); // Adjust speed based on length
		int new_pos = MIN(p_current_pos + chars_per_tick, total_chars);
		
		// Update visible characters
		p_label->set_visible_characters(new_pos);
		
		// Schedule next tick
		if (new_pos < total_chars) {
			// Use a timer for consistent timing
			Timer *tick_timer = memnew(Timer);
			tick_timer->set_wait_time(0.02); // 50fps
			tick_timer->set_one_shot(true);
			tick_timer->connect("timeout", callable_mp(this, &VectorAIPanel::_stream_text_tick).bind(p_label, p_full_text, new_pos));
			add_child(tick_timer);
			tick_timer->start();
		} else {
			// Streaming complete
			streaming_active = false;
			p_label->set_visible_characters(-1); // Show all characters
		}
	} else {
		// Streaming complete
		streaming_active = false;
		p_label->set_visible_characters(-1); // Show all characters
	}
}

// New processing state methods
void VectorAIPanel::_start_processing_sequence(const String &p_message) {
	print_line("VectorAI: Starting processing sequence");
	
	// Set initial state
	_set_processing_state(STATE_THINKING);
	
	// Auto-attach current file if enabled
	if (auto_attach_enabled) {
		_auto_attach_current_file();
	}

	// Use call_deferred to prevent UI freezing
	call_deferred("_send_message_deferred", p_message);
}

void VectorAIPanel::_set_processing_state(int p_state) {
	current_processing_state = (ProcessingState)p_state;
	
	// Clear previous status
	_clear_status_steps();
	
	// Show appropriate status based on state
	switch (current_processing_state) {
		case STATE_THINKING:
			_show_status_step("🤔 Thinking", "Analyzing your request...");
			status_update_timer->start();
			break;
		case STATE_GENERATING:
			_show_status_step("💻 Generating", composer_mode_active ? "Creating code..." : "Preparing response...");
			break;
		case STATE_IMPLEMENTING:
			_show_status_step("⚙️ Implementing", "Applying changes to your project...");
			break;
		case STATE_COMPLETING:
			_show_status_step("✅ Completing", "Finalizing changes...");
			break;
		case STATE_IDLE:
			status_update_timer->stop();
			_clear_status_steps();
			break;
	}
}

void VectorAIPanel::_update_status_animation() {
	if (current_processing_state == STATE_THINKING && status_container && status_container->is_visible()) {
		// Animate thinking dots
		static int dot_count = 0;
		dot_count = (dot_count + 1) % 4;
		
		String dots = "";
		for (int i = 0; i < dot_count; i++) {
			dots += ".";
		}
		
		// Update the status message
		if (current_status_message) {
			for (int i = 0; i < current_status_message->get_child_count(); i++) {
				Label *label = Object::cast_to<Label>(current_status_message->get_child(i));
				if (label && label->get_text().begins_with("Analyzing")) {
					label->set_text("Analyzing your request" + dots);
					break;
				}
			}
		}
	}
}

bool VectorAIPanel::_response_contains_code(const String &p_response) {
	// Quick check for code block markers
	bool has_code = p_response.find("```") != -1;
	print_line("VectorAI: Response contains code blocks: " + String(has_code ? "YES" : "NO"));
	if (has_code) {
		int count = 0;
		int pos = 0;
		while ((pos = p_response.find("```", pos)) != -1) {
			count++;
			pos += 3;
		}
		print_line("VectorAI: Found " + itos(count) + " ``` markers in response");
	}
	return has_code;
}

bool VectorAIPanel::_process_and_apply_code(const String &p_response, Vector<String> &r_modified_files) {
	print_line("VectorAI: Processing and applying code from response");
	
	// Extract code blocks using improved method
	Vector<Dictionary> code_blocks;
	if (!_extract_code_blocks_fast(p_response, code_blocks)) {
		print_line("VectorAI: No code blocks found in response");
		return false;
	}
	
	print_line("VectorAI: Found " + itos(code_blocks.size()) + " code blocks");
	
	// Apply each code block
	bool success = true;
	for (int i = 0; i < code_blocks.size(); i++) {
		String code = code_blocks[i]["code"];
		String file_path = code_blocks[i]["file_path"];
		String type = code_blocks[i]["type"];
		
		print_line("VectorAI: Applying code block " + itos(i + 1) + ": " + file_path);
		
		if (_apply_code_block(code, file_path, type)) {
			r_modified_files.push_back(file_path.get_file());
		} else {
			success = false;
			print_line("VectorAI: Failed to apply code block: " + file_path);
		}
	}
	
	if (success && r_modified_files.size() > 0) {
		// Update file system
		_update_file_system_final();
	}
	
	return success;
}

bool VectorAIPanel::_extract_code_blocks_fast(const String &p_response, Vector<Dictionary> &r_code_blocks) {
	print_line("VectorAI: Starting code block extraction from response length: " + itos(p_response.length()));
	
	// Use a simpler approach - split by ``` markers
	Vector<String> parts = p_response.split("```");
	print_line("VectorAI: Split response into " + itos(parts.size()) + " parts");
	
	// We need at least 3 parts: before, code block, after
	if (parts.size() < 3) {
		print_line("VectorAI: Not enough parts for code blocks");
		return false;
	}
	
	// Process pairs of parts (language+code)
	for (int i = 1; i < parts.size() - 1; i += 2) {
		String language_and_code = parts[i];
		
		// Split by first newline to separate language from code
		int newline_pos = language_and_code.find("\n");
		if (newline_pos == -1) {
			print_line("VectorAI: No newline found in code block " + itos(i));
			continue;
		}
		
		String language = language_and_code.substr(0, newline_pos).strip_edges();
		String code = language_and_code.substr(newline_pos + 1);
		
		print_line("VectorAI: Processing code block " + itos((i-1)/2) + " - Language: '" + language + "', Code length: " + itos(code.length()));
		
		if (code.strip_edges().is_empty()) {
			print_line("VectorAI: Skipping empty code block");
			continue;
		}
		
		// Look for file path in the text before this code block
		String text_before = parts[i-1];
		String file_path;
		String type = "resource";
		
		// Look for "File: " pattern
		int file_pos = text_before.rfind("File:");
		if (file_pos != -1) {
			String file_line = text_before.substr(file_pos);
			int line_end = file_line.find("\n");
			if (line_end != -1) {
				file_line = file_line.substr(0, line_end);
			}
			
			// Extract the file path
			file_path = file_line.replace("File:", "").strip_edges();
			print_line("VectorAI: Found file path: " + file_path);
		}
		
		// If no file path found, generate one
		if (file_path.is_empty()) {
			print_line("VectorAI: No file path found, generating based on content");
			if (language == "tscn" || code.begins_with("[gd_scene")) {
				file_path = _generate_scene_path(code);
				type = "scene";
				print_line("VectorAI: Generated scene path: " + file_path);
			} else if (language == "gdscript" || language == "gd" || code.find("extends ") != -1) {
				file_path = _generate_script_path(code);
				type = "script";
				print_line("VectorAI: Generated script path: " + file_path);
			} else {
				print_line("VectorAI: Unknown code type '" + language + "', skipping block");
				continue;
			}
		} else {
			// Ensure proper path format
			if (!file_path.begins_with("res://")) {
				file_path = "res://" + file_path;
			}
			
			// Determine type from extension
			if (file_path.ends_with(".tscn")) {
				type = "scene";
			} else if (file_path.ends_with(".gd")) {
				type = "script";
			}
		}
		
		Dictionary block;
		block["code"] = code;
		block["file_path"] = file_path;
		block["type"] = type;
		r_code_blocks.push_back(block);
		
		print_line("VectorAI: Successfully extracted code block - Type: " + type + ", Path: " + file_path);
	}
	
	print_line("VectorAI: Total extracted code blocks: " + itos(r_code_blocks.size()));
	return r_code_blocks.size() > 0;
}

bool VectorAIPanel::_apply_code_block(const String &p_code, const String &p_file_path, const String &p_type) {
	print_line("VectorAI: Applying code block to " + p_file_path);
	
	// Validate code based on type
	if (p_type == "scene" && !_validate_tscn_code(p_code)) {
		print_line("VectorAI: TSCN validation failed for " + p_file_path);
		return false;
	}
	
	// Create directory if needed
	String dir = p_file_path.get_base_dir();
	if (!dir.is_empty() && !DirAccess::exists(dir)) {
		Error err = DirAccess::make_dir_recursive_absolute(dir);
		if (err != OK) {
			print_line("VectorAI: Failed to create directory: " + dir);
			return false;
		}
	}
	
	// Write file
	Error err = OK;
	Ref<FileAccess> f = FileAccess::open(p_file_path, FileAccess::WRITE, &err);
	if (err != OK) {
		print_line("VectorAI: Failed to open file for writing: " + p_file_path);
		return false;
	}
	
	f->store_string(p_code);
	print_line("VectorAI: Successfully wrote " + itos(p_code.length()) + " characters to " + p_file_path);
	
	return true;
}

String VectorAIPanel::_generate_scene_path(const String &p_code) {
	// Try to extract scene name from node name
	static RegEx node_regex;
	if (!node_regex.is_valid()) {
		node_regex.compile("\\[node name=\"([^\"]+)\"");
	}
	
	Ref<RegExMatch> match = node_regex.search(p_code);
	if (match.is_valid()) {
		String node_name = match->get_string(1);
		return "res://" + node_name + ".tscn";
	}
	
	// Fallback to timestamp-based name
	return "res://Scene_" + String::num_int64(OS::get_singleton()->get_unix_time()) + ".tscn";
}

String VectorAIPanel::_generate_script_path(const String &p_code) {
	// Try to extract class name
	static RegEx class_regex;
	if (!class_regex.is_valid()) {
		class_regex.compile("class_name\\s+([A-Za-z0-9_]+)");
	}
	
	Ref<RegExMatch> match = class_regex.search(p_code);
	if (match.is_valid()) {
		String class_name = match->get_string(1);
		return "res://" + class_name + ".gd";
	}
	
	// Fallback to timestamp-based name
	return "res://Script_" + String::num_int64(OS::get_singleton()->get_unix_time()) + ".gd";
}

bool VectorAIPanel::_validate_tscn_code(const String &p_code) {
	// Basic TSCN validation
	if (!p_code.begins_with("[gd_scene")) {
		return false;
	}
	
	// Check for required format
	if (p_code.find("format=3") == -1) {
		return false;
	}
	
	// Check for at least one node
	if (p_code.find("[node") == -1) {
		return false;
	}
	
	return true;
}

void VectorAIPanel::_remove_thinking_messages() {
	for (int i = chat_messages->get_child_count() - 1; i >= 0; i--) {
		Control *message = Object::cast_to<Control>(chat_messages->get_child(i));
		if (message && message->has_meta("is_thinking")) {
			message->queue_free();
		}
	}
}

void VectorAIPanel::_add_claude_message_with_streaming(const String &p_response) {
	Control *message = _create_message_panel("VectorAI", p_response);
	
	// Find the panel container in the message
	HBoxContainer *hbox = Object::cast_to<HBoxContainer>(Object::cast_to<MarginContainer>(message)->get_child(0));
	PanelContainer *panel = nullptr;

	// Assistant messages are on the left (first child of hbox)
	if (hbox && hbox->get_child_count() >= 1) {
		panel = Object::cast_to<PanelContainer>(hbox->get_child(0));
	}

	if (panel) {
		panel->add_theme_style_override("panel", assistant_message_style);
	}

	chat_messages->add_child(message);
	_scroll_to_bottom();

	// Start streaming text effect if response isn't too long
	if (p_response.length() < 2000) {
		VBoxContainer *vbox = nullptr;
		if (panel && panel->is_inside_tree()) {
			vbox = Object::cast_to<VBoxContainer>(panel->get_child(0));
		}
		
		if (vbox && vbox->is_inside_tree()) {
			RichTextLabel *message_label = Object::cast_to<RichTextLabel>(vbox->get_child(1));
			if (message_label && message_label->is_inside_tree()) {
				message_label->set_visible_characters(0);
				message_label->set_visible_characters_behavior(TextServer::VC_CHARS_BEFORE_SHAPING);
				
				// Start streaming
				call_deferred("_start_text_streaming", p_response, message_label);
			}
		}
	}
}

void VectorAIPanel::_complete_processing() {
	_set_processing_state(STATE_IDLE);
	print_line("VectorAI: Processing sequence completed");
}

void VectorAIPanel::_update_file_system_final() {
	// Force file system scan to update dependencies
	EditorFileSystem::get_singleton()->scan();
	print_line("VectorAI: File system scan triggered");
}