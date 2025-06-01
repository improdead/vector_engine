/**************************************************************************/
/*  claude_api.h                                                          */
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

#ifndef CLAUDE_API_H
#define CLAUDE_API_H

#include "core/io/http_client.h"
#include "core/io/json.h"
#include "core/io/marshalls.h"
#include "core/object/object.h"
#include "core/config/project_settings.h"
#include "scene/main/node.h"
#include "scene/main/http_request.h"

// Define the operation modes for VectorAI as a proper class enum
class ClaudeAPI : public Node {
	GDCLASS(ClaudeAPI, Node);

public:
	static const int MODE_ASK = 0;      // Read-only mode for explanations and debugging
	static const int MODE_COMPOSER = 1; // Read-write mode for generating/modifying code

private:
	static ClaudeAPI *singleton;

	// Claude API configuration
	static const char *API_URL;
	static const char *API_VERSION;
	static const char *DEFAULT_MODEL;
	static const int MAX_TOKENS;
	static const char *ASK_MODE_SYSTEM_PROMPT;
	static const char *COMPOSER_MODE_SYSTEM_PROMPT;

	String api_key;
	String model;
	bool debug_mode = false; // Control debug output
	int current_mode = MODE_ASK; // Default to Ask mode (safest)

	// Context tracking
	String active_scene_path;
	Vector<String> attached_script_paths;
	String attached_file_context;

	// HTTP request handling
	HTTPRequest *http_request = nullptr;
	bool request_in_progress = false;
	String pending_user_message;

	// Callback for response
	Callable response_callback;
	Callable error_callback;

	// Simple message history
	struct Message {
		String role;
		String content;
	};
	Vector<Message> conversation_history;

	// Build system prompt based on current mode and context
	String _build_system_prompt() const;
	
	void _on_request_completed(int p_result, int p_response_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);

protected:
	static void _bind_methods();

public:
	static ClaudeAPI *get_singleton();

	// API key management
	void set_api_key(const String &p_api_key);
	String get_api_key() const;
	bool has_api_key() const;

	// Debug mode
	void set_debug_mode(bool p_enabled);
	bool get_debug_mode() const;
	
	// Mode management
	void set_mode(int p_mode);
	int get_mode() const;
	
	// Context management
	void set_active_scene(const String &p_scene_path);
	void set_attached_scripts(const Vector<String> &p_script_paths);
	void set_file_context(const String &p_file_context);
	void clear_context();

	// Basic message handling
	void send_message(const String &p_message);
	void add_to_history(const String &p_role, const String &p_content);
	void clear_history();

	// Callbacks
	void set_response_callback(const Callable &p_callback);
	void set_error_callback(const Callable &p_callback);

	ClaudeAPI();
	~ClaudeAPI();
};

#endif // CLAUDE_API_H
