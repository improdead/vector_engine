/**************************************************************************/
/*  claude_api.cpp                                                        */
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

#include "claude_api.h"
#include "core/os/os.h"
#include "scene/main/http_request.h"

ClaudeAPI *ClaudeAPI::singleton = nullptr;
const char *ClaudeAPI::API_URL = "https://api.anthropic.com/v1/messages";
const char *ClaudeAPI::API_VERSION = "2023-06-01";
const char *ClaudeAPI::DEFAULT_MODEL = "claude-3-5-sonnet-20241022";
const int ClaudeAPI::MAX_TOKENS = 8192;

// Define system prompts for different modes
const char *ClaudeAPI::ASK_MODE_SYSTEM_PROMPT = R"(You are a Godot assistant named VectorAI.

You are currently in ASK MODE:
- This is a READ-ONLY mode for understanding and debugging.
- Your primary role is to help users understand their Godot project.
- DO NOT generate or modify any code or files.
- Focus on:
  * Explaining code behavior and Godot concepts
  * Debugging issues and suggesting fixes
  * Auditing code for best practices
  * Answering questions about the project
- When suggesting fixes, explain them clearly but DO NOT implement them
- If code changes are needed, suggest switching to COMPOSER mode

Files passed as context include:
- Active scene: {active_scene}
- Attached scripts: {attached_scripts}
- Manually attached: {attached_files}

Remember: You are a knowledgeable guide helping users understand their Godot project better.
)";

const char *ClaudeAPI::COMPOSER_MODE_SYSTEM_PROMPT = R"(
You are a Godot assistant named VectorAI in COMPOSER MODE.

CRITICAL: When generating code, you MUST:
1. ALWAYS wrap code in proper markdown code blocks
2. Use ```tscn for scene files
3. Use ```gdscript for script files  
4. NEVER show code in regular text - ONLY in code blocks
5. Keep explanations brief - focus on code generation
6. ALWAYS specify file paths before code blocks

RESPONSE FORMAT FOR COMPOSER MODE:
- Brief explanation (1-2 sentences max)
- File path: res://filename.extension
- Code blocks with proper language tags
- No lengthy discussions - just working code

EXAMPLE RESPONSE FORMAT:
Creating a simple player scene with embedded script:

File: res://Player.tscn
```tscn
[gd_scene load_steps=2 format=3]

[sub_resource type="GDScript" id="PlayerScript"]
script/source = "extends CharacterBody2D

func _ready():
    print('Player ready!')
"

[node name="Player" type="CharacterBody2D"]
script = SubResource("PlayerScript")
```

CRITICAL RULES:
- NEVER reference external resources that don't exist
- ALWAYS use built-in Godot resources when possible
- NEVER mix 2D and 3D node types in the same scene
- Keep responses concise and code-focused
- ALWAYS validate TSCN structure before outputting
- ALWAYS specify complete file paths

Files passed as context:
- Active scene: {active_scene}
- Attached scripts: {attached_scripts}  
- Manually attached: {attached_files}
)";

ClaudeAPI::ClaudeAPI() {
	singleton = this;
	model = DEFAULT_MODEL;
	debug_mode = false;
	current_mode = MODE_ASK; // Default to safer Ask mode
	request_in_progress = false;

	// Initialize HTTP request node
	http_request = memnew(HTTPRequest);
	add_child(http_request);
	http_request->connect("request_completed", callable_mp(this, &ClaudeAPI::_on_request_completed));

	if (ProjectSettings::get_singleton()->has_setting("vector_ai/claude_api_key")) {
		api_key = ProjectSettings::get_singleton()->get_setting("vector_ai/claude_api_key");
	}
}

ClaudeAPI::~ClaudeAPI() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

void ClaudeAPI::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_api_key", "api_key"), &ClaudeAPI::set_api_key);
	ClassDB::bind_method(D_METHOD("get_api_key"), &ClaudeAPI::get_api_key);
	ClassDB::bind_method(D_METHOD("has_api_key"), &ClaudeAPI::has_api_key);

	ClassDB::bind_method(D_METHOD("set_debug_mode", "enabled"), &ClaudeAPI::set_debug_mode);
	ClassDB::bind_method(D_METHOD("get_debug_mode"), &ClaudeAPI::get_debug_mode);
	
	ClassDB::bind_method(D_METHOD("set_mode", "mode"), &ClaudeAPI::set_mode);
	ClassDB::bind_method(D_METHOD("get_mode"), &ClaudeAPI::get_mode);
	
	ClassDB::bind_method(D_METHOD("set_active_scene", "scene_path"), &ClaudeAPI::set_active_scene);
	ClassDB::bind_method(D_METHOD("set_attached_scripts", "script_paths"), &ClaudeAPI::set_attached_scripts);
	ClassDB::bind_method(D_METHOD("set_file_context", "file_context"), &ClaudeAPI::set_file_context);
	ClassDB::bind_method(D_METHOD("clear_context"), &ClaudeAPI::clear_context);

	ClassDB::bind_method(D_METHOD("send_message", "message"), &ClaudeAPI::send_message);
	ClassDB::bind_method(D_METHOD("add_to_history", "role", "content"), &ClaudeAPI::add_to_history);
	ClassDB::bind_method(D_METHOD("clear_history"), &ClaudeAPI::clear_history);

	ClassDB::bind_method(D_METHOD("set_response_callback", "callback"), &ClaudeAPI::set_response_callback);
	ClassDB::bind_method(D_METHOD("set_error_callback", "callback"), &ClaudeAPI::set_error_callback);
	
	ClassDB::bind_method(D_METHOD("_on_request_completed", "result", "response_code", "headers", "body"), &ClaudeAPI::_on_request_completed);
}

ClaudeAPI *ClaudeAPI::get_singleton() {
	return singleton;
}

void ClaudeAPI::set_api_key(const String &p_api_key) {
	api_key = p_api_key;

	// Save to project settings for persistence
	ProjectSettings::get_singleton()->set_setting("vector_ai/claude_api_key", p_api_key);
	ProjectSettings::get_singleton()->save();
}

String ClaudeAPI::get_api_key() const {
	return api_key;
}

bool ClaudeAPI::has_api_key() const {
	return !api_key.is_empty();
}

void ClaudeAPI::set_debug_mode(bool p_enabled) {
	debug_mode = p_enabled;
}

bool ClaudeAPI::get_debug_mode() const {
	return debug_mode;
}

void ClaudeAPI::set_mode(int p_mode) {
	current_mode = p_mode;
	
	// Clear conversation history when switching modes to avoid confusion
	clear_history();
	
	String mode_name = (current_mode == MODE_ASK) ? "Ask Mode (Read-Only)" : "Composer Mode (Read-Write)";
	print_line("VectorAI API: Mode switched to: " + mode_name + " (value: " + itos(current_mode) + ")");
}

int ClaudeAPI::get_mode() const {
	return current_mode;
}

void ClaudeAPI::set_active_scene(const String &p_scene_path) {
	active_scene_path = p_scene_path;
}

void ClaudeAPI::set_attached_scripts(const Vector<String> &p_script_paths) {
	attached_script_paths = p_script_paths;
}

void ClaudeAPI::set_file_context(const String &p_file_context) {
	attached_file_context = p_file_context;
}

void ClaudeAPI::clear_context() {
	active_scene_path = "";
	attached_script_paths.clear();
	attached_file_context = "";
}

String ClaudeAPI::_build_system_prompt() const {
	String system_prompt;
	String script_list;
	
	// Build a list of attached scripts for the prompt
	for (int i = 0; i < attached_script_paths.size(); i++) {
		if (i > 0) {
			script_list += ", ";
		}
		script_list += attached_script_paths[i];
	}
	
	// Select the appropriate template based on mode and convert to String properly
	if (current_mode == MODE_ASK) {
		system_prompt = String(ASK_MODE_SYSTEM_PROMPT);
		if (debug_mode) {
			print_line("VectorAI API: Using ASK_MODE_SYSTEM_PROMPT (mode = " + itos(current_mode) + ")");
		}
	} else {
		system_prompt = String(COMPOSER_MODE_SYSTEM_PROMPT);
		if (debug_mode) {
			print_line("VectorAI API: Using COMPOSER_MODE_SYSTEM_PROMPT (mode = " + itos(current_mode) + ")");
		}
	}
	
	// Replace placeholders with actual context
	system_prompt = system_prompt.replace("{active_scene}", active_scene_path.is_empty() ? "None" : active_scene_path);
	system_prompt = system_prompt.replace("{attached_scripts}", script_list.is_empty() ? "None" : script_list);
	system_prompt = system_prompt.replace("{attached_files}", attached_file_context.is_empty() ? "None" : attached_file_context);
	
	if (debug_mode) {
		print_line("VectorAI API: System prompt length: " + itos(system_prompt.length()) + " characters");
	}
	
	return system_prompt;
}

void ClaudeAPI::send_message(const String &p_message) {
	// Validate API key
	if (api_key.is_empty()) {
		if (error_callback.is_valid()) {
			error_callback.call("API key not set. Please set your Claude API key in the settings.");
		}
		return;
	}

	// Update this to match modern Anthropic API keys
	if (!api_key.begins_with("sk-ant-")) {
		if (error_callback.is_valid()) {
			error_callback.call("Invalid API key format. Claude API keys should start with 'sk-ant-'");
		}
		return;
	}

	if (request_in_progress) {
		if (error_callback.is_valid()) {
			error_callback.call("A request is already in progress. Please wait for it to complete.");
		}
		return;
	}

	// Prepare the request headers
	PackedStringArray headers;
	headers.push_back("Content-Type: application/json");
	headers.push_back("x-api-key: " + api_key);
	headers.push_back("anthropic-version: " + String(API_VERSION));

	// Prepare the request body
	Dictionary body;
	body["model"] = DEFAULT_MODEL;
	body["max_tokens"] = MAX_TOKENS;

	// Get system prompt based on current mode and context
	String system_prompt = _build_system_prompt();
	
	// Add system message
	if (!system_prompt.is_empty()) {
		body["system"] = system_prompt;
	}
	
	// Prepare messages array
	Array messages;

	// Add conversation history (limit to last 10 messages to avoid token limits)
	int start_idx = MAX(0, conversation_history.size() - 10);
	for (int i = start_idx; i < conversation_history.size(); i++) {
		Dictionary message;
		message["role"] = conversation_history[i].role;
		message["content"] = conversation_history[i].content;
		messages.push_back(message);
	}

	// Add the current user message
	Dictionary user_message;
	user_message["role"] = "user";
	user_message["content"] = p_message;
	messages.push_back(user_message);

	// Add messages to body
	body["messages"] = messages;

	// Convert the body to JSON
	String json_body = JSON::stringify(body);

	if (debug_mode) {
		print_line("Sending request to Claude API...");
		print_line("Current mode: " + String(current_mode == MODE_ASK ? "Ask Mode" : "Composer Mode"));
		print_line("Using model: " + String(DEFAULT_MODEL));
		print_line("Max tokens: " + itos(MAX_TOKENS));
		print_line("Message length: " + itos(p_message.length()));
	}

	// Store the user message for history
	pending_user_message = p_message;
	request_in_progress = true;

	// Make the HTTP request
	Error err = http_request->request(API_URL, headers, HTTPClient::METHOD_POST, json_body);
	
	if (err != OK) {
		String error_msg = "Failed to send request to Claude API: " + itos(err);
		if (debug_mode) {
			print_line(error_msg);
		}
		if (error_callback.is_valid()) {
			error_callback.call(error_msg);
		}
		request_in_progress = false;
		return;
	}

	if (debug_mode) {
		print_line("HTTP request sent successfully, waiting for response...");
	}
}

void ClaudeAPI::_on_request_completed(int p_result, int p_response_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	request_in_progress = false;

	if (debug_mode) {
		print_line("Request completed. Result: " + itos(p_result) + ", Response code: " + itos(p_response_code));
		print_line("Response body size: " + itos(p_body.size()) + " bytes");
	}

	// Check for network errors
	if (p_result != HTTPRequest::RESULT_SUCCESS) {
		String error_msg = "Network error: ";
		switch (p_result) {
			case HTTPRequest::RESULT_CANT_CONNECT:
				error_msg += "Can't connect to server";
				break;
			case HTTPRequest::RESULT_CANT_RESOLVE:
				error_msg += "Can't resolve hostname";
				break;
			case HTTPRequest::RESULT_CONNECTION_ERROR:
				error_msg += "Connection error";
				break;
			case HTTPRequest::RESULT_TLS_HANDSHAKE_ERROR:
				error_msg += "TLS handshake error";
				break;
			case HTTPRequest::RESULT_NO_RESPONSE:
				error_msg += "No response from server";
				break;
			case HTTPRequest::RESULT_BODY_SIZE_LIMIT_EXCEEDED:
				error_msg += "Response too large";
				break;
			case HTTPRequest::RESULT_REQUEST_FAILED:
				error_msg += "Request failed";
				break;
			case HTTPRequest::RESULT_DOWNLOAD_FILE_CANT_OPEN:
				error_msg += "Can't open download file";
				break;
			case HTTPRequest::RESULT_DOWNLOAD_FILE_WRITE_ERROR:
				error_msg += "Download file write error";
				break;
			case HTTPRequest::RESULT_REDIRECT_LIMIT_REACHED:
				error_msg += "Redirect limit reached";
				break;
			case HTTPRequest::RESULT_TIMEOUT:
				error_msg += "Request timeout";
				break;
			default:
				error_msg += "Unknown error (" + itos(p_result) + ")";
				break;
		}
		
		if (debug_mode) {
			print_line(error_msg);
		}
		if (error_callback.is_valid()) {
			error_callback.call(error_msg);
		}
		return;
	}

	// Check HTTP response code
	if (p_response_code != 200) {
		String error_text;
		if (p_body.size() > 0) {
			error_text = String::utf8((const char *)p_body.ptr(), p_body.size());
		} else {
			error_text = "Unknown error";
		}
		
		String error_msg = "API returned error " + itos(p_response_code);
		
		// Try to parse error details from response
		JSON json;
		Error json_err = json.parse(error_text);
		if (json_err == OK) {
			Variant result = json.get_data();
			if (result.get_type() == Variant::DICTIONARY) {
				Dictionary error_data = result;
				if (error_data.has("error")) {
					Dictionary error_info = error_data["error"];
					if (error_info.has("message")) {
						error_msg += ": " + String(error_info["message"]);
					}
				}
			}
		} else {
			error_msg += ": " + error_text.substr(0, 200); // First 200 chars
		}
		
		if (debug_mode) {
			print_line("API Error Response: " + error_text);
		}

		if (error_callback.is_valid()) {
			error_callback.call(error_msg);
		}
		return;
	}

	// Parse successful response
	if (p_body.size() == 0) {
		String error_msg = "Empty response from Claude API";
		if (debug_mode) {
			print_line(error_msg);
		}
		if (error_callback.is_valid()) {
			error_callback.call(error_msg);
		}
		return;
	}

	String json_string = String::utf8((const char *)p_body.ptr(), p_body.size());
	
	if (debug_mode) {
		print_line("Response preview: " + json_string.substr(0, 200) + "...");
	}

	JSON json;
	Error err = json.parse(json_string);
	if (err != OK) {
		if (debug_mode) {
			print_line("JSON parse error: " + itos(err));
		}
		if (error_callback.is_valid()) {
			error_callback.call("Failed to parse response JSON: " + itos(err));
		}
		return;
	}

	// Extract the response text from Claude
	Variant result = json.get_data();
	if (result.get_type() != Variant::DICTIONARY) {
		String error_msg = "Invalid response format - not a dictionary";
		if (debug_mode) {
			print_line(error_msg + ". Type: " + itos(result.get_type()));
		}
		if (error_callback.is_valid()) {
			error_callback.call(error_msg);
		}
		return;
	}

	Dictionary response_data = result;

	if (debug_mode) {
		Array keys = response_data.keys();
		String keys_str = "Response keys: ";
		for (int i = 0; i < keys.size(); i++) {
			keys_str += String(keys[i]) + ", ";
		}
		print_line(keys_str);
	}

	// Check for content in the response
	if (!response_data.has("content") || response_data["content"].get_type() != Variant::ARRAY) {
		String error_msg = "No content array found in response";
		if (debug_mode) {
			print_line(error_msg);
		}
		if (error_callback.is_valid()) {
			error_callback.call(error_msg);
		}
		return;
	}

	Array content = response_data["content"];
	String response_text;

	if (debug_mode) {
		print_line("Content array size: " + itos(content.size()));
	}

	// Extract text from content items
	for (int i = 0; i < content.size(); i++) {
		Variant content_item_var = content[i];
		if (content_item_var.get_type() != Variant::DICTIONARY) {
			continue;
		}
		
		Dictionary content_item = content_item_var;
		
		if (debug_mode && content_item.has("type")) {
			print_line("Content item " + itos(i) + " type: " + String(content_item["type"]));
		}

		if (content_item.has("type") && content_item["type"] == "text" && content_item.has("text")) {
			response_text += String(content_item["text"]);
		}
	}

	if (debug_mode) {
		print_line("Extracted response text length: " + itos(response_text.length()));
	}

	// Process the response text
	if (response_text.is_empty()) {
		String error_msg = "Received empty response from Claude API";
		if (debug_mode) {
			print_line("Warning: " + error_msg);
		}
		if (error_callback.is_valid()) {
			error_callback.call(error_msg);
		}
		return;
	}

	// Add to conversation history
	add_to_history("user", pending_user_message);
	add_to_history("assistant", response_text);

	// Call the response callback
	if (response_callback.is_valid()) {
		if (debug_mode) {
			print_line("Calling response callback with " + itos(response_text.length()) + " characters");
		}
		response_callback.call(response_text);
	} else {
		if (debug_mode) {
			print_line("Warning: No response callback set!");
		}
	}
}

void ClaudeAPI::add_to_history(const String &p_role, const String &p_content) {
	Message msg;
	msg.role = p_role;
	msg.content = p_content;
	conversation_history.push_back(msg);
}

void ClaudeAPI::clear_history() {
	conversation_history.clear();
}

void ClaudeAPI::set_response_callback(const Callable &p_callback) {
	response_callback = p_callback;
	if (debug_mode) {
		print_line("Response callback set");
	}
}

void ClaudeAPI::set_error_callback(const Callable &p_callback) {
	error_callback = p_callback;
	if (debug_mode) {
		print_line("Error callback set");
	}
}
