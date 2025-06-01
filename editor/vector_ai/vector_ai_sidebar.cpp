/**************************************************************************/
/*  vector_ai_sidebar.cpp                                                 */
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

#include "vector_ai_sidebar.h"

#include "core/io/file_access.h"
#include "core/config/project_settings.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/editor_node.h"
#include "editor/themes/editor_scale.h"
#include "editor/editor_file_system.h"
#include "editor/plugins/script_editor_plugin.h"
#include "editor/filesystem_dock.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/separator.h"
#include "scene/resources/style_box_flat.h"
#include "core/os/time.h"

VectorAISidebar::VectorAISidebar() {
	set_name("VectorAI");
	set_custom_minimum_size(Size2(250, 0) * EDSCALE);
	
	_create_interface();
	_create_claude_api();
	_setup_connections();
}

VectorAISidebar::~VectorAISidebar() {
	if (file_dialog) {
		file_dialog->queue_free();
		file_dialog = nullptr;
	}
}

void VectorAISidebar::_setup_layout() {
	// Main vertical container for the entire sidebar
	main_vbox = memnew(VBoxContainer);
	main_vbox->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	main_vbox->add_theme_constant_override("separation", 0);
	add_child(main_vbox);
	
	// Set initial sizing
	main_vbox->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	main_vbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
}

void VectorAISidebar::_create_header_section() {
	// Header container with title and controls
	header_container = memnew(HBoxContainer);
	header_container->set_custom_minimum_size(Size2(0, HEADER_HEIGHT * EDSCALE));
	header_container->add_theme_constant_override("separation", 8 * EDSCALE);
	main_vbox->add_child(header_container);
	
	// Add some padding
	MarginContainer *header_margin = memnew(MarginContainer);
	header_margin->add_theme_constant_override("margin_left", 12 * EDSCALE);
	header_margin->add_theme_constant_override("margin_right", 12 * EDSCALE);
	header_margin->add_theme_constant_override("margin_top", 8 * EDSCALE);
	header_margin->add_theme_constant_override("margin_bottom", 8 * EDSCALE);
	header_container->add_child(header_margin);
	
	HBoxContainer *header_content = memnew(HBoxContainer);
	header_content->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	header_content->add_theme_constant_override("separation", 8 * EDSCALE);
	header_margin->add_child(header_content);
	
	// VectorAI title
	title_label = memnew(Label);
	title_label->set_text("VectorAI");
	title_label->add_theme_font_size_override("font_size", 16 * EDSCALE);
	title_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	header_content->add_child(title_label);
	
	// Mode dropdown
	mode_dropdown = memnew(OptionButton);
	mode_dropdown->add_item("Composer");
	mode_dropdown->add_item("Ask");
	mode_dropdown->select(0); // Default to Composer mode
	mode_dropdown->set_custom_minimum_size(Size2(80 * EDSCALE, 0));
	mode_dropdown->connect("item_selected", callable_mp(this, &VectorAISidebar::_on_mode_selected));
	header_content->add_child(mode_dropdown);
	
	// Settings button (for API key)
	settings_button = memnew(Button);
	settings_button->set_text("...");
	settings_button->set_custom_minimum_size(Size2(32 * EDSCALE, 32 * EDSCALE));
	settings_button->set_tooltip_text("Settings (API Key)");
	settings_button->connect("pressed", callable_mp(this, &VectorAISidebar::_on_settings_pressed));
	header_content->add_child(settings_button);
	
	// Add separator after header
	HSeparator *header_separator = memnew(HSeparator);
	main_vbox->add_child(header_separator);
}

void VectorAISidebar::_create_recent_chats_section() {
	// Recent chats section - hidden by default, only show when there are chats
	recent_chats_section = memnew(VBoxContainer);
	recent_chats_section->add_theme_constant_override("separation", 4 * EDSCALE);
	recent_chats_section->set_visible(false); // Hidden by default
	main_vbox->add_child(recent_chats_section);
	
	// Add margin container for padding
	MarginContainer *recent_margin = memnew(MarginContainer);
	recent_margin->add_theme_constant_override("margin_left", 12 * EDSCALE);
	recent_margin->add_theme_constant_override("margin_right", 12 * EDSCALE);
	recent_margin->add_theme_constant_override("margin_top", 8 * EDSCALE);
	recent_chats_section->add_child(recent_margin);
	
	VBoxContainer *recent_content = memnew(VBoxContainer);
	recent_content->add_theme_constant_override("separation", 4 * EDSCALE);
	recent_margin->add_child(recent_content);
	
	// Recent chats label
	recent_chats_label = memnew(Label);
	recent_chats_label->set_text("Recent chats");
	recent_chats_label->add_theme_font_size_override("font_size", 12 * EDSCALE);
	recent_chats_label->add_theme_color_override("font_color", Color(0.7, 0.7, 0.7));
	recent_content->add_child(recent_chats_label);
	
	// Scrollable recent chats list
	recent_chats_scroll = memnew(ScrollContainer);
	recent_chats_scroll->set_custom_minimum_size(Size2(0, 80 * EDSCALE));
	recent_chats_scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	recent_content->add_child(recent_chats_scroll);
	
	recent_chats_list = memnew(VBoxContainer);
	recent_chats_list->add_theme_constant_override("separation", 2 * EDSCALE);
	recent_chats_scroll->add_child(recent_chats_list);
	
	// See all button
	see_all_button = memnew(Button);
	see_all_button->set_text("See all");
	see_all_button->set_flat(true);
	see_all_button->set_h_size_flags(Control::SIZE_SHRINK_CENTER);
	see_all_button->add_theme_font_size_override("font_size", 11 * EDSCALE);
	recent_content->add_child(see_all_button);
}

void VectorAISidebar::_create_chat_area() {
	// Chat container that expands to fill remaining space
	chat_container = memnew(PanelContainer);
	chat_container->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	main_vbox->add_child(chat_container);
	
	// Chat area with proper styling
	chat_area = memnew(VBoxContainer);
	chat_area->add_theme_constant_override("separation", 0);
	chat_container->add_child(chat_area);
	
	// Scrollable chat messages
	chat_scroll = memnew(ScrollContainer);
	chat_scroll->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	chat_scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	chat_area->add_child(chat_scroll);
	
	// Messages container with better spacing
	chat_messages = memnew(VBoxContainer);
	chat_messages->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	chat_messages->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	chat_messages->add_theme_constant_override("separation", 16 * EDSCALE); // Increased spacing
	chat_scroll->add_child(chat_messages);
	
	// Add margin for chat messages - larger like Cursor
	MarginContainer *chat_margin = memnew(MarginContainer);
	chat_margin->add_theme_constant_override("margin_left", 20 * EDSCALE);
	chat_margin->add_theme_constant_override("margin_right", 20 * EDSCALE);
	chat_margin->add_theme_constant_override("margin_top", 24 * EDSCALE);
	chat_margin->add_theme_constant_override("margin_bottom", 24 * EDSCALE);
	chat_margin->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	chat_margin->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	chat_messages->add_child(chat_margin);
	
	// Welcome message with Cursor-style typography
	Label *welcome_label = memnew(Label);
	welcome_label->set_text("Welcome to VectorAI\n\nHow can I help you with your Godot project today?");
	welcome_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	welcome_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	welcome_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	welcome_label->add_theme_color_override("font_color", Color(0.6, 0.6, 0.6));
	welcome_label->add_theme_font_size_override("font_size", 16 * EDSCALE); // Larger font
	chat_margin->add_child(welcome_label);
}

void VectorAISidebar::_create_input_area() {
	// Add separator before input area
	HSeparator *input_separator = memnew(HSeparator);
	main_vbox->add_child(input_separator);
	
	// Input container - make it bigger like Cursor
	input_container = memnew(VBoxContainer);
	input_container->set_custom_minimum_size(Size2(0, 120 * EDSCALE)); // Increased from 80 to 120
	input_container->add_theme_constant_override("separation", 8 * EDSCALE);
	main_vbox->add_child(input_container);
	
	// Add margin for input area
	MarginContainer *input_margin = memnew(MarginContainer);
	input_margin->add_theme_constant_override("margin_left", 16 * EDSCALE);
	input_margin->add_theme_constant_override("margin_right", 16 * EDSCALE);
	input_margin->add_theme_constant_override("margin_top", 12 * EDSCALE);
	input_margin->add_theme_constant_override("margin_bottom", 12 * EDSCALE);
	input_container->add_child(input_margin);
	
	VBoxContainer *input_content = memnew(VBoxContainer);
	input_content->add_theme_constant_override("separation", 8 * EDSCALE);
	input_margin->add_child(input_content);
	
	// Input area with text field and buttons
	input_area = memnew(HBoxContainer);
	input_area->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	input_area->add_theme_constant_override("separation", 8 * EDSCALE);
	input_content->add_child(input_area);
	
	// Text input - much larger like Cursor
	input_text = memnew(TextEdit);
	input_text->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	input_text->set_custom_minimum_size(Size2(0, 80 * EDSCALE)); // Increased from 40 to 80
	input_text->set_line_wrapping_mode(TextEdit::LineWrappingMode::LINE_WRAPPING_BOUNDARY);
	input_text->connect("gui_input", callable_mp(this, &VectorAISidebar::_on_input_text_gui_input));
	input_text->connect("text_changed", callable_mp(this, &VectorAISidebar::_on_input_text_changed));
	input_text->set_placeholder("Ask VectorAI anything...");
	
	// Apply custom styling to input
	Ref<StyleBoxFlat> input_style;
	input_style.instantiate();
	input_style->set_bg_color(Color(0.08, 0.08, 0.08));
	input_style->set_corner_radius_all(8 * EDSCALE);
	input_style->set_content_margin_all(12 * EDSCALE);
	input_style->set_border_width_all(1);
	input_style->set_border_color(Color(0.2, 0.2, 0.2));
	input_text->add_theme_style_override("normal", input_style);
	
	// Apply focused state styling
	Ref<StyleBoxFlat> input_focus_style;
	input_focus_style.instantiate();
	input_focus_style->set_bg_color(Color(0.1, 0.1, 0.1));
	input_focus_style->set_corner_radius_all(8 * EDSCALE);
	input_focus_style->set_content_margin_all(12 * EDSCALE);
	input_focus_style->set_border_width_all(2);
	input_focus_style->set_border_color(Color(0.3, 0.5, 0.8));
	input_text->add_theme_style_override("focus", input_focus_style);
	
	input_area->add_child(input_text);
	
	// Button container for attach and send
	VBoxContainer *button_container = memnew(VBoxContainer);
	button_container->add_theme_constant_override("separation", 4 * EDSCALE);
	input_area->add_child(button_container);
	
	// Attach button (larger and styled)
	attach_button = memnew(Button);
	attach_button->set_text("+");
	attach_button->set_custom_minimum_size(Size2(40 * EDSCALE, 40 * EDSCALE));
	attach_button->set_tooltip_text("Attach file");
	attach_button->connect("pressed", callable_mp(this, &VectorAISidebar::_on_attach_pressed));
	
	// Style the attach button
	Ref<StyleBoxFlat> attach_style;
	attach_style.instantiate();
	attach_style->set_bg_color(Color(0.15, 0.15, 0.15));
	attach_style->set_corner_radius_all(6 * EDSCALE);
	attach_style->set_content_margin_all(8 * EDSCALE);
	attach_button->add_theme_style_override("normal", attach_style);
	
	button_container->add_child(attach_button);
	
	// Send button (larger and styled)
	send_button = memnew(Button);
	send_button->set_text(">");
	send_button->set_custom_minimum_size(Size2(40 * EDSCALE, 40 * EDSCALE));
	send_button->connect("pressed", callable_mp(this, &VectorAISidebar::_on_send_pressed));
	
	// Style the send button with accent color
	Ref<StyleBoxFlat> send_style;
	send_style.instantiate();
	send_style->set_bg_color(Color(0.2, 0.4, 0.8));
	send_style->set_corner_radius_all(6 * EDSCALE);
	send_style->set_content_margin_all(8 * EDSCALE);
	send_button->add_theme_style_override("normal", send_style);
	
	// Send button hover state
	Ref<StyleBoxFlat> send_hover_style;
	send_hover_style.instantiate();
	send_hover_style->set_bg_color(Color(0.25, 0.45, 0.85));
	send_hover_style->set_corner_radius_all(6 * EDSCALE);
	send_hover_style->set_content_margin_all(8 * EDSCALE);
	send_button->add_theme_style_override("hover", send_hover_style);
	
	button_container->add_child(send_button);
	
	// Token counter
	token_counter = memnew(Label);
	token_counter->set_text("0 chars");
	token_counter->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	token_counter->add_theme_color_override("font_color", Color(0.5, 0.5, 0.5));
	token_counter->add_theme_font_size_override("font_size", 11 * EDSCALE);
	input_content->add_child(token_counter);
}

void VectorAISidebar::_apply_sidebar_styling() {
	// Create message styles with Cursor-like appearance
	Ref<StyleBoxFlat> flat_style;

	// User message style (blue accent like Cursor)
	flat_style.instantiate();
	flat_style->set_bg_color(Color(0.2, 0.4, 0.8, 0.6));
	flat_style->set_corner_radius_all(12 * EDSCALE);
	flat_style->set_content_margin_all(16 * EDSCALE);
	flat_style->set_border_width_all(1);
	flat_style->set_border_color(Color(0.3, 0.5, 0.9, 0.3));
	user_message_style = flat_style;

	// Assistant message style (clean dark like Cursor)
	flat_style.instantiate();
	flat_style->set_bg_color(Color(0.08, 0.08, 0.08));
	flat_style->set_corner_radius_all(12 * EDSCALE);
	flat_style->set_content_margin_all(16 * EDSCALE);
	flat_style->set_border_width_all(1);
	flat_style->set_border_color(Color(0.15, 0.15, 0.15));
	assistant_message_style = flat_style;

	// System message style (subtle)
	flat_style.instantiate();
	flat_style->set_bg_color(Color(0.05, 0.05, 0.05));
	flat_style->set_corner_radius_all(8 * EDSCALE);
	flat_style->set_content_margin_all(12 * EDSCALE);
	system_message_style = flat_style;
	
	// Apply modern background styling to chat container
	Ref<StyleBoxFlat> chat_style;
	chat_style.instantiate();
	chat_style->set_bg_color(Color(0.03, 0.03, 0.03));
	chat_style->set_corner_radius_all(0);
	chat_container->add_theme_style_override("panel", chat_style);
}

void VectorAISidebar::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_send_pressed"), &VectorAISidebar::_on_send_pressed);
	ClassDB::bind_method(D_METHOD("_on_input_text_gui_input"), &VectorAISidebar::_on_input_text_gui_input);
	ClassDB::bind_method(D_METHOD("_on_input_text_changed"), &VectorAISidebar::_on_input_text_changed);
	ClassDB::bind_method(D_METHOD("_on_attach_pressed"), &VectorAISidebar::_on_attach_pressed);
	ClassDB::bind_method(D_METHOD("_on_file_selected", "path"), &VectorAISidebar::_on_file_selected);
	ClassDB::bind_method(D_METHOD("_on_mode_selected"), &VectorAISidebar::_on_mode_selected);
	ClassDB::bind_method(D_METHOD("_on_settings_pressed"), &VectorAISidebar::_on_settings_pressed);
	ClassDB::bind_method(D_METHOD("_on_settings_confirmed"), &VectorAISidebar::_on_settings_confirmed);
	ClassDB::bind_method(D_METHOD("_on_claude_response"), &VectorAISidebar::_on_claude_response);
	ClassDB::bind_method(D_METHOD("_on_claude_error"), &VectorAISidebar::_on_claude_error);
	ClassDB::bind_method(D_METHOD("_on_new_chat_pressed"), &VectorAISidebar::_on_new_chat_pressed);
	ClassDB::bind_method(D_METHOD("_on_recent_chat_selected"), &VectorAISidebar::_on_recent_chat_selected);
	ClassDB::bind_method(D_METHOD("_send_message_deferred", "message"), &VectorAISidebar::_send_message_deferred);
	ClassDB::bind_method(D_METHOD("_show_completion_message"), &VectorAISidebar::_show_completion_message);
}

void VectorAISidebar::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_POSTINITIALIZE: {
			_update_styles();
		} break;
		case NOTIFICATION_THEME_CHANGED: {
			_update_styles();
		} break;
	}
}

// Public interface methods
void VectorAISidebar::set_sidebar_width(int p_width) {
	p_width = CLAMP(p_width, MIN_SIDEBAR_WIDTH, MAX_SIDEBAR_WIDTH);
	set_custom_minimum_size(Size2(p_width * EDSCALE, 0));
}

int VectorAISidebar::get_sidebar_width() const {
	return get_custom_minimum_size().x / EDSCALE;
}

void VectorAISidebar::toggle_visibility() {
	set_visible(!is_visible());
}

void VectorAISidebar::show_sidebar() {
	set_visible(true);
}

void VectorAISidebar::hide_sidebar() {
	set_visible(false);
}

bool VectorAISidebar::is_sidebar_visible() const {
	return is_visible();
}

void VectorAISidebar::start_new_chat() {
	clear_current_chat();
	
	// Add this new chat to recent chats if it's not already the first one
	if (recent_chats.size() == 0 || recent_chats[0].title != "New Chat") {
		_add_recent_chat("New Chat", "Just started");
	}
	
	_add_claude_message("Hello! I'm VectorAI, ready to help with your Godot project. What would you like to work on?");
}

void VectorAISidebar::clear_current_chat() {
	// Clear all messages except the welcome message
	for (int i = chat_messages->get_child_count() - 1; i >= 1; i--) {
		Node *child = chat_messages->get_child(i);
		child->queue_free();
	}
}

void VectorAISidebar::load_chat_session(int p_index) {
	// Placeholder for loading chat sessions
	// This would load from saved chat history
	clear_current_chat();
	_add_claude_message("Loaded chat session " + itos(p_index + 1));
}

void VectorAISidebar::set_api_key(const String &p_api_key) {
	if (claude_api) {
		claude_api->set_api_key(p_api_key);
		is_api_key_set = claude_api->has_api_key();
	}
}

bool VectorAISidebar::has_api_key() const {
	return is_api_key_set;
}

// This is a placeholder - the full implementation would include all the methods
// from the original VectorAIPanel that handle chat, code generation, etc.
// For now, I'll implement the basic structure and key methods.

void VectorAISidebar::_add_user_message(const String &p_text) {
	Control *message = _create_message_panel("You", p_text);
	if (message) {
		chat_messages->add_child(message);
		call_deferred("_scroll_to_bottom");
	}
}

void VectorAISidebar::_add_claude_message(const String &p_text, bool p_is_thinking) {
	Control *message = _create_message_panel("VectorAI", p_text);
	if (message) {
		if (p_is_thinking) {
			message->set_meta("is_thinking", true);
		}
		chat_messages->add_child(message);
		call_deferred("_scroll_to_bottom");
	}
}

Control *VectorAISidebar::_create_message_panel(const String &p_sender, const String &p_text) {
	PanelContainer *panel = memnew(PanelContainer);
	panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	
	// Apply styling based on sender
	if (p_sender == "You") {
		panel->add_theme_style_override("panel", user_message_style);
	} else {
		panel->add_theme_style_override("panel", assistant_message_style);
	}
	
	VBoxContainer *content = memnew(VBoxContainer);
	content->add_theme_constant_override("separation", 8 * EDSCALE);
	panel->add_child(content);
	
	// Sender label with better typography
	Label *sender_label = memnew(Label);
	sender_label->set_text(p_sender);
	sender_label->add_theme_font_size_override("font_size", 13 * EDSCALE);
	
	if (p_sender == "You") {
		sender_label->add_theme_color_override("font_color", Color(0.9, 0.95, 1.0));
	} else {
		sender_label->add_theme_color_override("font_color", Color(0.8, 0.8, 0.8));
	}
	
	content->add_child(sender_label);
	
	// Message text with improved typography
	RichTextLabel *message_label = memnew(RichTextLabel);
	message_label->set_text(p_text);
	message_label->set_fit_content(true);
	message_label->set_use_bbcode(true);
	message_label->set_selection_enabled(true);
	message_label->add_theme_font_size_override("normal_font_size", 14 * EDSCALE);
	message_label->add_theme_font_size_override("mono_font_size", 13 * EDSCALE);
	
	// Set text color based on sender
	if (p_sender == "You") {
		message_label->add_theme_color_override("default_color", Color(0.95, 0.95, 1.0));
	} else {
		message_label->add_theme_color_override("default_color", Color(0.9, 0.9, 0.9));
	}
	
	content->add_child(message_label);
	
	return panel;
}

void VectorAISidebar::_scroll_to_bottom() {
	if (chat_scroll) {
		chat_scroll->call_deferred("ensure_control_visible", chat_messages->get_child(chat_messages->get_child_count() - 1));
	}
}

void VectorAISidebar::_update_recent_chats() {
	// Clear existing recent chats
	for (int i = 0; i < recent_chats_list->get_child_count(); i++) {
		recent_chats_list->get_child(i)->queue_free();
	}
	
	// Only show recent chats section if we have actual chat history
	if (recent_chats.size() == 0) {
		recent_chats_section->set_visible(false);
		return;
	}
	
	recent_chats_section->set_visible(true);
	
	// Add actual recent chats from history
	for (int i = 0; i < recent_chats.size(); i++) {
		// Use a panel container for better styling
		PanelContainer *item_panel = memnew(PanelContainer);
		item_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		
		// Custom styling for recent chat items
		Ref<StyleBoxFlat> item_style;
		item_style.instantiate();
		item_style->set_bg_color(Color(0.06, 0.06, 0.06));
		item_style->set_corner_radius_all(3 * EDSCALE);
		item_style->set_content_margin_all(6 * EDSCALE);
		item_panel->add_theme_style_override("panel", item_style);
		
		VBoxContainer *item_content = memnew(VBoxContainer);
		item_content->add_theme_constant_override("separation", 1 * EDSCALE);
		item_panel->add_child(item_content);
		
		// Title label
		Label *title_label = memnew(Label);
		title_label->set_text(recent_chats[i].title);
		title_label->add_theme_font_size_override("font_size", 11 * EDSCALE);
		title_label->add_theme_color_override("font_color", Color(0.85, 0.85, 0.85));
		title_label->set_clip_contents(true);
		item_content->add_child(title_label);
		
		// Time label
		Label *time_label = memnew(Label);
		time_label->set_text(recent_chats[i].timestamp);
		time_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
		time_label->add_theme_font_size_override("font_size", 9 * EDSCALE);
		time_label->add_theme_color_override("font_color", Color(0.45, 0.45, 0.45));
		item_content->add_child(time_label);
		
		// Make the panel clickable
		Button *click_area = memnew(Button);
		click_area->set_flat(true);
		click_area->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
		click_area->connect("pressed", callable_mp(this, &VectorAISidebar::_on_recent_chat_selected).bind(i));
		item_panel->add_child(click_area);
		
		recent_chats_list->add_child(item_panel);
	}
}

// Event handlers (placeholders for now - full implementation would include all functionality)
void VectorAISidebar::_on_send_pressed() {
	String message = input_text->get_text().strip_edges();
	if (message.is_empty()) {
		return;
	}
	
	// If this is the first user message, create a recent chat entry
	if (recent_chats.size() == 0) {
		String chat_title = message.length() > 30 ? message.substr(0, 30) + "..." : message;
		_add_recent_chat(chat_title, message);
	}
	
	_add_user_message(message);
	input_text->clear();
	_add_claude_message("Thinking...", true);
	
	call_deferred("_send_message_deferred", message);
}

void VectorAISidebar::_on_input_text_gui_input(const Ref<InputEvent> &p_event) {
	Ref<InputEventKey> k = p_event;
	if (k.is_valid() && k->is_pressed() && !k->is_echo()) {
		if (k->get_keycode() == Key::ENTER && !k->is_shift_pressed()) {
			_on_send_pressed();
			get_viewport()->set_input_as_handled();
		}
	}
}

void VectorAISidebar::_on_input_text_changed() {
	String text = input_text->get_text();
	token_counter->set_text(itos(text.length()) + " chars");
}

void VectorAISidebar::_on_attach_pressed() {
	if (!file_dialog) {
		file_dialog = memnew(EditorFileDialog);
		file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
		file_dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
		file_dialog->set_title("Attach Scene File");
		file_dialog->clear_filters();
		file_dialog->add_filter("*.tscn", "Godot Scene");
		file_dialog->connect("file_selected", callable_mp(this, &VectorAISidebar::_on_file_selected));
		add_child(file_dialog);
	}
	
	file_dialog->popup_centered_ratio();
}

void VectorAISidebar::_on_file_selected(const String &p_path) {
	attached_file_path = p_path;
	_add_claude_message("Attached: " + p_path.get_file());
}

void VectorAISidebar::_on_mode_selected(int p_index) {
	int new_mode = (p_index == 0) ? ClaudeAPI::MODE_COMPOSER : ClaudeAPI::MODE_ASK;
	claude_api->set_mode(new_mode);
	composer_mode_active = (new_mode == ClaudeAPI::MODE_COMPOSER);
	
	// Update styling based on mode
	_update_mode_styling();
	
	String mode_description = composer_mode_active ? 
		"Switched to Composer Mode - I can generate and modify code." :
		"Switched to Ask Mode - I'll explain and help you understand your project.";
	_add_claude_message(mode_description);
}

void VectorAISidebar::_update_mode_styling() {
	// Update send button color based on mode
	Ref<StyleBoxFlat> send_style;
	send_style.instantiate();
	send_style->set_corner_radius_all(6 * EDSCALE);
	send_style->set_content_margin_all(8 * EDSCALE);
	
	Ref<StyleBoxFlat> send_hover_style;
	send_hover_style.instantiate();
	send_hover_style->set_corner_radius_all(6 * EDSCALE);
	send_hover_style->set_content_margin_all(8 * EDSCALE);
	
	if (composer_mode_active) {
		// Composer mode: Blue accent (creative/generative)
		send_style->set_bg_color(Color(0.2, 0.4, 0.8));
		send_hover_style->set_bg_color(Color(0.25, 0.45, 0.85));
		
		// Update input placeholder
		input_text->set_placeholder("What would you like me to create?");
	} else {
		// Ask mode: Green accent (analytical/helpful)
		send_style->set_bg_color(Color(0.2, 0.7, 0.4));
		send_hover_style->set_bg_color(Color(0.25, 0.75, 0.45));
		
		// Update input placeholder
		input_text->set_placeholder("What would you like to know?");
	}
	
	send_button->add_theme_style_override("normal", send_style);
	send_button->add_theme_style_override("hover", send_hover_style);
}

void VectorAISidebar::_on_settings_pressed() {
	// Simple API key input dialog
	AcceptDialog *dialog = memnew(AcceptDialog);
	dialog->set_title("VectorAI Settings");
	
	VBoxContainer *vbox = memnew(VBoxContainer);
	vbox->add_theme_constant_override("separation", 8 * EDSCALE);
	dialog->add_child(vbox);
	
	Label *label = memnew(Label);
	label->set_text("Claude API Key:");
	vbox->add_child(label);
	
	LineEdit *line_edit = memnew(LineEdit);
	line_edit->set_placeholder("sk-ant-...");
	line_edit->set_custom_minimum_size(Size2(300 * EDSCALE, 0));
	if (claude_api && claude_api->has_api_key()) {
		String key = claude_api->get_api_key();
		line_edit->set_text(key.substr(0, 12) + "..." + key.substr(key.length() - 4));
	}
	vbox->add_child(line_edit);
	
	dialog->connect("confirmed", callable_mp(this, &VectorAISidebar::_on_settings_confirmed).bind(line_edit));
	add_child(dialog);
	dialog->popup_centered();
}

void VectorAISidebar::_on_settings_confirmed(LineEdit *p_line_edit) {
	String api_key = p_line_edit->get_text().strip_edges();
	if (!api_key.is_empty() && api_key.begins_with("sk-ant-")) {
		set_api_key(api_key);
		_add_claude_message("API key updated successfully!");
	}
}

void VectorAISidebar::_on_claude_response(const String &p_response) {
	// Remove thinking message
	for (int i = chat_messages->get_child_count() - 1; i >= 0; i--) {
		Control *message = Object::cast_to<Control>(chat_messages->get_child(i));
		if (message && message->has_meta("is_thinking")) {
			message->queue_free();
			break;
		}
	}
	
	_add_claude_message(p_response);
}

void VectorAISidebar::_on_claude_error(const String &p_error) {
	// Remove thinking message
	for (int i = chat_messages->get_child_count() - 1; i >= 0; i--) {
		Control *message = Object::cast_to<Control>(chat_messages->get_child(i));
		if (message && message->has_meta("is_thinking")) {
			message->queue_free();
			break;
		}
	}
	
	_add_claude_message("Error: " + p_error);
}

void VectorAISidebar::_on_new_chat_pressed() {
	start_new_chat();
}

void VectorAISidebar::_on_recent_chat_selected(int p_index) {
	load_chat_session(p_index);
}

// Placeholder methods - these would contain the full implementation from VectorAIPanel
void VectorAISidebar::_send_message_deferred(const String &p_message) {
	if (claude_api) {
		claude_api->send_message(p_message);
	}
}

void VectorAISidebar::_detect_code_changes(const String &p_response) {
	// Placeholder - would contain full code detection logic
}

bool VectorAISidebar::_extract_code_block(const String &p_text, String &r_code, String &r_file_path) {
	// Placeholder - would contain full code extraction logic
	return false;
}

bool VectorAISidebar::_extract_multiple_code_blocks(const String &p_text, Vector<Dictionary> &r_code_blocks) {
	// Placeholder - would contain full multi-block extraction logic
	return false;
}

void VectorAISidebar::_auto_apply_changes(const String &p_code, const String &p_target_file) {
	// Placeholder - would contain full auto-apply logic
}

void VectorAISidebar::_reload_project() {
	EditorNode *editor = EditorNode::get_singleton();
	if (editor) {
		EditorFileSystem *efs = EditorFileSystem::get_singleton();
		if (efs) {
			efs->scan();
		}
	}
	call_deferred("_show_completion_message");
}

void VectorAISidebar::_show_completion_message() {
	_add_claude_message("Files created and project updated! Check the FileSystem dock to see your new files.");
}

void VectorAISidebar::_update_styles() {
	// Update styles when theme changes
	_apply_sidebar_styling();
}

void VectorAISidebar::_create_interface() {
	// Create the main UI layout
	_setup_layout();
	_create_header_section();
	_create_recent_chats_section();
	// Removed _create_suggested_section() - no suggestions by default
	_create_chat_area();
	_create_input_area();
	_apply_sidebar_styling();
	
	// Don't initialize with placeholder recent chats - start empty
	// _update_recent_chats(); // Commented out - start with empty recent chats
}

void VectorAISidebar::_create_claude_api() {
	// Initialize Claude API
	claude_api = memnew(ClaudeAPI);
	add_child(claude_api);
	claude_api->set_response_callback(callable_mp(this, &VectorAISidebar::_on_claude_response));
	claude_api->set_error_callback(callable_mp(this, &VectorAISidebar::_on_claude_error));
	claude_api->set_debug_mode(false);
	
	// Check if API key is set
	is_api_key_set = claude_api->has_api_key();
	composer_mode_active = true; // Default to Composer mode
	
	// Apply initial mode styling
	call_deferred("_update_mode_styling");
}

void VectorAISidebar::_setup_connections() {
	// Set up any additional connections needed
	// This can be expanded later if more connections are needed
}

void VectorAISidebar::_add_recent_chat(const String &p_title, const String &p_preview) {
	// Create new chat session
	ChatSession new_chat;
	new_chat.title = p_title;
	new_chat.timestamp = "now";
	new_chat.preview_text = p_preview;
	new_chat.message_count = 1;
	new_chat.is_pinned = false;
	
	// Add to beginning of recent chats
	recent_chats.insert(0, new_chat);
	
	// Limit to 10 recent chats
	if (recent_chats.size() > 10) {
		recent_chats.resize(10);
	}
	
	// Update the UI
	_update_recent_chats();
} 