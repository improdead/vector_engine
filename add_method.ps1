# Simple script to add the missing _on_vector_ai_pressed method

$content = Get-Content "editor\editor_node.cpp"
$newContent = @()

for ($i = 0; $i -lt $content.Length; $i++) {
    $line = $content[$i]
    $newContent += $line
    
    # Add method after the closing brace of _version_control_menu_option
    if ($line -match "^\}$" -and $i -gt 0 -and $content[$i-8] -match "_version_control_menu_option") {
        $newContent += ""
        $newContent += "void EditorNode::_on_vector_ai_pressed() {"
        $newContent += "`t// Toggle the VectorAI sidebar visibility"
        $newContent += "`tvector_ai_sidebar_visible = !vector_ai_sidebar_visible;"
        $newContent += "`tvector_ai_sidebar->set_visible(vector_ai_sidebar_visible);"
        $newContent += "`t"
        $newContent += "`t// Update the button state"
        $newContent += "`tvector_ai_button->set_pressed(vector_ai_sidebar_visible);"
        $newContent += "}"
        Write-Host "Added method implementation"
        break
    }
}

# Continue with the rest of the content
for ($j = $i + 1; $j -lt $content.Length; $j++) {
    $newContent += $content[$j]
}

$newContent | Set-Content "editor\editor_node.cpp"
Write-Host "Method addition complete!" 