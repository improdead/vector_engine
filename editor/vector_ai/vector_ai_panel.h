/**************************************************************************/
/*  vector_ai_panel.h                                                     */
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

#ifndef VECTOR_AI_PANEL_H
#define VECTOR_AI_PANEL_H

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
// Removing direct include of editor_node.h to break circular dependency
// Forward declare EditorNode instead

// Forward declarations
class EditorFileDialog;
class EditorNode;

class VectorAIPanel : public PanelContainer {
	GDCLASS(VectorAIPanel, PanelContainer);

private:
	// UI Components - Main Panel
	VBoxContainer *main_vbox = nullptr;
	HBoxContainer *header = nullptr;
	Label *title_label = nullptr;
	Button *close_button = nullptr;
	HBoxContainer *toolbar = nullptr;
	Button *attach_button = nullptr;
	OptionButton *mode_dropdown = nullptr;
	Button *api_key_button = nullptr;
	ScrollContainer *chat_scroll = nullptr;
	VBoxContainer *chat_messages = nullptr;
	HBoxContainer *input_area = nullptr;
	TextEdit *input_text = nullptr;
	Button *send_button = nullptr;
	Label *token_counter = nullptr;
	
	// Code Preview components
	PanelContainer *code_preview_panel = nullptr;
	VBoxContainer *preview_vbox = nullptr;
	Label *preview_title = nullptr;
	CodeEdit *code_preview = nullptr;
	HBoxContainer *preview_actions = nullptr;
	Button *apply_button = nullptr;
	Button *discard_button = nullptr;
	String preview_target_file;
	String original_file_content;

	// Claude API
	ClaudeAPI *claude_api = nullptr;

	// Chat state
	String attached_file_path;
	String attached_file_content;
	bool is_api_key_set = false;
	bool composer_mode_active = false;
	bool code_preview_visible = false;

	// Auto-file reading state
	String current_attached_file;
	bool auto_attach_enabled;
	Timer *auto_refresh_timer;

	// Status step system
	Control *status_container;
	VBoxContainer *status_steps;
	Control *current_status_message;
	String current_step;

	// Real-time streaming
	bool streaming_active;
	Timer *stream_timer;

	// Processing state system
	enum ProcessingState {
		STATE_IDLE,
		STATE_THINKING,
		STATE_GENERATING,
		STATE_IMPLEMENTING,
		STATE_COMPLETING
	};
	ProcessingState current_processing_state;
	Timer *status_update_timer;

	// UI Styling
	Ref<StyleBox> user_message_style;
	Ref<StyleBox> assistant_message_style;
	Ref<StyleBox> system_message_style;

	// File dialog
	EditorFileDialog *file_dialog = nullptr;

	// Dependency tracking
	struct DependencyInfo {
		String path;
		String code;
		String type; // "script", "scene", "resource"
		bool created;
		Vector<String> dependencies;
	};
	HashMap<String, DependencyInfo> pending_dependencies;
	Vector<String> processing_order;

	// Helper methods
	void _add_user_message(const String &p_text);
	void _add_claude_message(const String &p_text, bool p_is_thinking = false);
	Control *_create_message_panel(const String &p_sender, const String &p_text);
	void _scroll_to_bottom();
	void _update_api_key_button();
	void _update_styles();
	void _start_typewriter_animation(Control *p_message);
	void _on_typewriter_tick(RichTextLabel *p_label);
	void _show_code_preview(const String &p_code, const String &p_target_file);
	void _hide_code_preview();
	void _detect_code_changes(const String &p_response);
	bool _extract_code_block(const String &p_text, String &r_code, String &r_file_path);
	bool _extract_multiple_code_blocks(const String &p_text, Vector<Dictionary> &r_code_blocks);
	void _make_file_backup(const String &p_file_path);
	void _apply_code_changes();
	void _auto_apply_changes(const String &p_code, const String &p_target_file);
	void _reload_project();
	void _show_completion_message();
	Dictionary _create_empty_scene_template(const String &p_type = "default", bool p_include_scripts = true, const String &p_base_name = "");
	void _create_scene_with_scripts(const String &p_scene_name, const String &p_scene_type, bool p_include_scripts);
	void _send_message_deferred(const String &p_message);
	void _handle_scene_dependencies(const String &p_scene_code, const String &p_scene_path);
	String _generate_script_template(const String &p_node_type, const String &p_class_name = "");
	bool _detect_and_fix_truncated_script(String &p_script_code);

	// Dependency tracking methods
	void _scan_for_dependencies(const String &p_response);
	void _extract_dependencies_from_scene(const String &p_scene_code, Vector<String> &r_dependencies);
	void _extract_dependencies_from_script(const String &p_script_code, Vector<String> &r_dependencies);
	void _process_dependencies();
	void _create_placeholder_resource(const String &p_resource_path);
	void _create_placeholder_scene(const String &p_scene_path);
	void _update_scene_format(String &p_scene_code);
	bool _has_all_dependencies(const String &p_path);

	// Code validation methods
	bool _validate_gdscript(const String &p_code, String &r_error_message);
	bool _validate_scene_file(const String &p_code, String &r_error_message);
	bool _validate_node_types(const String &p_scene_code, String &r_error_message);
	bool _check_for_theme_access(const String &p_code, String &r_warning_message);
	bool _check_for_instance_leaks(const String &p_code, String &r_warning_message);
	bool _find_and_update_scene_node(const String &p_tscn_content, const String &p_node_name, 
									const String &p_script_path, String &r_updated_content);
	bool _ensure_valid_scene_resources(String &p_scene_code);

	// Event handlers
	void _on_send_pressed();
	void _on_input_text_gui_input(const Ref<InputEvent> &p_event);
	void _on_attach_pressed();
	void _on_file_selected(const String &p_path);
	void _on_mode_selected(int p_index);
	void _on_api_key_pressed();
	void _on_api_key_confirmed(LineEdit *p_line_edit);
	void _on_claude_response(const String &p_response);
	void _on_claude_error(const String &p_error);
	void _on_close_pressed();
	void _on_apply_pressed();
	void _on_discard_pressed();
	void _on_input_text_changed();

	// Auto-read current file functionality
	void _auto_attach_current_file();
	String _get_current_script_path();
	String _get_current_scene_path();
	void _read_file_content(const String &p_path);

	// Step-by-step status system
	void _show_status_step(const String &p_step, const String &p_description = "");
	void _update_status_step(const String &p_step);
	void _complete_status_step();
	void _clear_status_steps();

	// Real-time text streaming
	void _start_text_streaming(const String &p_text, RichTextLabel *p_label);
	void _stream_text_tick(RichTextLabel *p_label, const String &p_full_text, int p_current_pos);

	// New processing state methods
	void _start_processing_sequence(const String &p_message);
	void _set_processing_state(int p_state);
	void _update_status_animation();
	
	// Improved code detection and application
	bool _response_contains_code(const String &p_response);
	bool _process_and_apply_code(const String &p_response, Vector<String> &r_modified_files);
	bool _extract_code_blocks_fast(const String &p_response, Vector<Dictionary> &r_code_blocks);
	bool _apply_code_block(const String &p_code, const String &p_file_path, const String &p_type);
	String _generate_scene_path(const String &p_code);
	String _generate_script_path(const String &p_code);
	bool _validate_tscn_code(const String &p_code);
	void _remove_thinking_messages();
	void _add_claude_message_with_streaming(const String &p_response);
	void _complete_processing();
	void _update_file_system_final();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	// Fixed size for the chat panel
	static const int PANEL_WIDTH = 450;
	static const int PANEL_HEIGHT = 375;

	VectorAIPanel();
	~VectorAIPanel();
};

#endif // VECTOR_AI_PANEL_H
