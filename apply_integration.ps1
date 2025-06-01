# PowerShell script to integrate VectorAI sidebar into editor_node.cpp

# Read the original file
$content = Get-Content "editor\editor_node.cpp"
$newContent = @()

$includeAdded = $false
$methodAdded = $false
$dockAdded = $false
$buttonAdded = $false
$layoutAdded = $false

for ($i = 0; $i -lt $content.Length; $i++) {
    $line = $content[$i]
    
    # 1. Add include after editor_theme_manager.h
    if ($line -match '#include "editor/themes/editor_theme_manager.h"' -and !$includeAdded) {
        $newContent += $line
        $newContent += '#include "editor/vector_ai/vector_ai_sidebar.h"'
        $includeAdded = $true
        Write-Host "Added include statement"
        continue
    }
    
    # 2. Add method after _version_control_menu_option
    if ($line -match '^\s*\}\s*$' -and $i -gt 0 -and $content[$i-1] -match 'break;' -and !$methodAdded) {
        if ($content[$i-10..$i] -join '' -match '_version_control_menu_option') {
            $newContent += $line
            $newContent += ''
            $newContent += 'void EditorNode::_on_vector_ai_pressed() {'
            $newContent += '	// Toggle the VectorAI sidebar visibility'
            $newContent += '	vector_ai_sidebar_visible = !vector_ai_sidebar_visible;'
            $newContent += '	vector_ai_sidebar->set_visible(vector_ai_sidebar_visible);'
            $newContent += '	'
            $newContent += '	// Update the button state'
            $newContent += '	vector_ai_button->set_pressed(vector_ai_sidebar_visible);'
            $newContent += '}'
            $methodAdded = $true
            Write-Host "Added method implementation"
            continue
        }
    }
    
    # 3. Add dock after history_dock
    if ($line -match 'editor_dock_manager->add_dock\(history_dock' -and !$dockAdded) {
        $newContent += $line
        $newContent += ''
        $newContent += '	// VectorAI: Bottom right, for AI assistance'
        $newContent += '	vector_ai_sidebar = memnew(VectorAISidebar);'
        $newContent += '	editor_dock_manager->add_dock(vector_ai_sidebar, TTR("VectorAI"), EditorDockManager::DOCK_SLOT_RIGHT_BR, nullptr, "Script");'
        $newContent += '	vector_ai_sidebar_visible = true;'
        $dockAdded = $true
        Write-Host "Added dock registration"
        continue
    }
    
    # 4. Add button after main_editor_button_hb creation
    if ($line -match 'main_editor_button_hb = memnew\(HBoxContainer\)' -and !$buttonAdded) {
        $newContent += $line
        # Find the next few lines to add after title_bar->add_child(main_editor_button_hb)
        $j = $i + 1
        while ($j -lt $content.Length) {
            $nextLine = $content[$j]
            $newContent += $nextLine
            if ($nextLine -match 'title_bar->add_child\(main_editor_button_hb\)') {
                $newContent += ''
                $newContent += '	// Create VectorAI toggle button'
                $newContent += '	vector_ai_button = memnew(Button);'
                $newContent += '	vector_ai_button->set_theme_type_variation("FlatButton");'
                $newContent += '	vector_ai_button->set_text("🤖");'
                $newContent += '	vector_ai_button->set_toggle_mode(true);'
                $newContent += '	vector_ai_button->set_focus_mode(Control::FOCUS_NONE);'
                $newContent += '	vector_ai_button->set_pressed(true);'
                $newContent += '	vector_ai_button->set_tooltip_text("Toggle VectorAI Sidebar");'
                $newContent += '	vector_ai_button->connect(SceneStringName(pressed), callable_mp(this, &EditorNode::_on_vector_ai_pressed));'
                $newContent += '	title_bar->add_child(vector_ai_button);'
                $buttonAdded = $true
                Write-Host "Added button creation"
                $i = $j
                break
            }
            $j++
        }
        continue
    }
    
    # 5. Add layout after dock_5 configuration
    if ($line -match 'default_layout->set_value\(docks_section, "dock_5"' -and !$layoutAdded) {
        $newContent += $line
        $newContent += '	default_layout->set_value(docks_section, "dock_6", "VectorAI");'
        $layoutAdded = $true
        Write-Host "Added layout configuration"
        continue
    }
    
    # Add the current line
    $newContent += $line
}

# Write the modified content back to the file
$newContent | Set-Content "editor\editor_node.cpp"

Write-Host "Integration complete!"
Write-Host "Include added: $includeAdded"
Write-Host "Method added: $methodAdded"
Write-Host "Dock added: $dockAdded"
Write-Host "Button added: $buttonAdded"
Write-Host "Layout added: $layoutAdded" 