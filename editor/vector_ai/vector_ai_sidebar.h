/**************************************************************************/
/*  vector_ai_sidebar.h                                                   */
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

#ifndef VECTOR_AI_SIDEBAR_H
#define VECTOR_AI_SIDEBAR_H

#include "claude_api.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/text_edit.h"
#include "scene/gui/split_container.h"
#include "scene/gui/code_edit.h"
#include "scene/gui/separator.h"

// Forward declarations
class EditorFileDialog;

class VectorAISidebar : public Control {
	GDCLASS(VectorAISidebar, Control);

private:
	// Sidebar Layout Components
	VBoxContainer *main_vbox = nullptr;
	
	// Header Section
	HBoxContainer *header_container = nullptr;
	Label *title_label = nullptr;
	OptionButton *mode_dropdown = nullptr;
	Button *settings_button = nullptr;
	
	// Recent Chats Section
	VBoxContainer *recent_chats_section = nullptr;
	Label *recent_chats_label = nullptr;
	ScrollContainer *recent_chats_scroll = nullptr;
	VBoxContainer *recent_chats_list = nullptr;
	Button *see_all_button = nullptr;
	
	// Main Chat Area
	PanelContainer *chat_container = nullptr;
	VBoxContainer *chat_area = nullptr;
	ScrollContainer *chat_scroll = nullptr;
	VBoxContainer *chat_messages = nullptr;
	
	// Input Area (Bottom)
	VBoxContainer *input_container = nullptr;
	HBoxContainer *input_area = nullptr;
	TextEdit *input_text = nullptr;
	Button *attach_button = nullptr;
	Button *send_button = nullptr;
	Label *token_counter = nullptr;
	
	// Claude API
	ClaudeAPI *claude_api = nullptr;
	
	// State Management
	String attached_file_path;
	String attached_file_content;
	bool is_api_key_set = false;
	bool composer_mode_active = false;
	
	// UI Styling
	Ref<StyleBox> user_message_style;
	Ref<StyleBox> assistant_message_style;
	Ref<StyleBox> system_message_style;
	
	// File dialog
	EditorFileDialog *file_dialog = nullptr;
	
	// Recent chat management
	struct ChatSession {
		String title;
		String timestamp;
		String preview_text;
		int message_count;
		bool is_pinned;
	};
	Vector<ChatSession> recent_chats;
	
	// Dependency tracking (from original panel)
	struct DependencyInfo {
		String path;
		String code;
		String type; // "script", "scene", "resource"
		bool created;
		Vector<String> dependencies;
	};
	HashMap<String, DependencyInfo> pending_dependencies;
	Vector<String> processing_order;
	
	// Layout constants
	static const int DEFAULT_SIDEBAR_WIDTH = 400;
	static const int MIN_SIDEBAR_WIDTH = 300;
	static const int MAX_SIDEBAR_WIDTH = 600;
	static const int HEADER_HEIGHT = 50;
	static const int INPUT_AREA_MIN_HEIGHT = 80;
	
	// Helper methods for UI creation
	void _setup_layout();
	void _create_header_section();
	void _create_recent_chats_section();
	void _create_chat_area();
	void _create_input_area();
	void _apply_sidebar_styling();
	void _create_interface();
	void _create_claude_api();
	void _setup_connections();
	
	// Chat management methods
	void _add_user_message(const String &p_text);
	void _add_claude_message(const String &p_text, bool p_is_thinking = false);
	Control *_create_message_panel(const String &p_sender, const String &p_text);
	void _scroll_to_bottom();
	void _update_recent_chats();
	void _add_recent_chat(const String &p_title, const String &p_preview);
	
	// Original functionality (preserved from VectorAIPanel)
	void _detect_code_changes(const String &p_response);
	bool _extract_code_block(const String &p_text, String &r_code, String &r_file_path);
	bool _extract_multiple_code_blocks(const String &p_text, Vector<Dictionary> &r_code_blocks);
	void _auto_apply_changes(const String &p_code, const String &p_target_file);
	void _reload_project();
	void _show_completion_message();
	void _send_message_deferred(const String &p_message);
	
	// Event handlers
	void _on_send_pressed();
	void _on_input_text_gui_input(const Ref<InputEvent> &p_event);
	void _on_input_text_changed();
	void _on_attach_pressed();
	void _on_file_selected(const String &p_path);
	void _on_mode_selected(int p_index);
	void _on_settings_pressed();
	void _on_settings_confirmed(LineEdit *p_line_edit);
	void _on_claude_response(const String &p_response);
	void _on_claude_error(const String &p_error);
	void _on_new_chat_pressed();
	void _on_recent_chat_selected(int p_index);
	
	// Layout and theming
	void _update_styles();
	void _update_mode_styling();
	void _on_theme_changed();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	VectorAISidebar();
	~VectorAISidebar();
	
	// Public interface
	void set_sidebar_width(int p_width);
	int get_sidebar_width() const;
	void toggle_visibility();
	void show_sidebar();
	void hide_sidebar();
	bool is_sidebar_visible() const;
	
	// Chat management
	void start_new_chat();
	void load_chat_session(int p_index);
	void clear_current_chat();
	
	// API integration
	void set_api_key(const String &p_api_key);
	bool has_api_key() const;
};

#endif // VECTOR_AI_SIDEBAR_H 