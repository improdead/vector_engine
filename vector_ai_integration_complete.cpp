// VectorAI Sidebar Integration for editor_node.cpp
// This file contains the complete integration code to be manually applied

// =============================================================================
// 1. INCLUDE SECTION (Add after line 162, after editor_theme_manager.h)
// =============================================================================
#include "editor/vector_ai/vector_ai_sidebar.h"

// =============================================================================
// 2. METHOD IMPLEMENTATION (Add after _version_control_menu_option method)
// =============================================================================
void EditorNode::_on_vector_ai_pressed() {
	// Toggle the VectorAI sidebar visibility
	vector_ai_sidebar_visible = !vector_ai_sidebar_visible;
	vector_ai_sidebar->set_visible(vector_ai_sidebar_visible);
	
	// Update the button state
	vector_ai_button->set_pressed(vector_ai_sidebar_visible);
}

// =============================================================================
// 3. DOCK REGISTRATION (Add after line 7473, after history_dock registration)
// =============================================================================
	// VectorAI: Bottom right, for AI assistance
	vector_ai_sidebar = memnew(VectorAISidebar);
	editor_dock_manager->add_dock(vector_ai_sidebar, TTR("VectorAI"), EditorDockManager::DOCK_SLOT_RIGHT_BR, nullptr, "Script");
	vector_ai_sidebar_visible = true;

// =============================================================================
// 4. BUTTON CREATION (Add in constructor around line 7270, near main_editor_button_hb)
// =============================================================================
	// Create VectorAI toggle button
	vector_ai_button = memnew(Button);
	vector_ai_button->set_theme_type_variation("FlatButton");
	vector_ai_button->set_text("🤖");
	vector_ai_button->set_toggle_mode(true);
	vector_ai_button->set_focus_mode(Control::FOCUS_NONE);
	vector_ai_button->set_pressed(true); // Start pressed since sidebar is visible
	vector_ai_button->set_tooltip_text("Toggle VectorAI Sidebar");
	vector_ai_button->connect(SceneStringName(pressed), callable_mp(this, &EditorNode::_on_vector_ai_pressed));
	// Add to title bar or appropriate menu area - adjust position as needed
	title_bar->add_child(vector_ai_button); // Or add to right_menu_hb

// =============================================================================
// 5. DEFAULT LAYOUT UPDATE (Add after dock configuration around line 7480)
// =============================================================================
	default_layout->set_value(docks_section, "dock_6", "VectorAI");

// =============================================================================
// 6. ADDITIONAL: Method Declaration for editor_node.h (should already exist)
// =============================================================================
// void _on_vector_ai_pressed(); // Add to private methods section

// =============================================================================
// 7. BIND METHOD (Add to _bind_methods if needed)
// =============================================================================
// ClassDB::bind_method(D_METHOD("_on_vector_ai_pressed"), &EditorNode::_on_vector_ai_pressed); 