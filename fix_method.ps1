# Fix missing method - add _on_vector_ai_pressed after line 322
$content = Get-Content "editor\editor_node.cpp"
$newContent = @()

for ($i = 0; $i -lt $content.Length; $i++) {
    $newContent += $content[$i]
    
    # Check if this is line 322 (closing brace of _version_control_menu_option)
    if ($i -eq 321) {  # Line 322 is index 321
        $newContent += ""
        $newContent += "void EditorNode::_on_vector_ai_pressed() {"
        $newContent += "`t// Toggle the VectorAI sidebar visibility"
        $newContent += "`tvector_ai_sidebar_visible = !vector_ai_sidebar_visible;"
        $newContent += "`tvector_ai_sidebar->set_visible(vector_ai_sidebar_visible);"
        $newContent += "`t"
        $newContent += "`t// Update the button state"
        $newContent += "`tvector_ai_button->set_pressed(vector_ai_sidebar_visible);"
        $newContent += "}"
        Write-Host "Added method at line 322"
    }
}

$newContent | Set-Content "editor\editor_node.cpp"
Write-Host "Fixed method implementation!" 