# Writes every C++ block of docs/AI-CPP-GUIDE.md into Source/GuideExamples.h and .cpp, in document order.
# A block whose first line is a comment naming a .cpp file goes to the .cpp, like in a real project;
# "fragment" blocks become function bodies in the .cpp; "sketch" blocks are skipped.
# ASCII only: Windows PowerShell 5.1 reads scripts without a BOM in the ANSI code page.
param([string]$Guide = "$PSScriptRoot/../../docs/AI-CPP-GUIDE.md", [string]$Output = "$PSScriptRoot/Source/GuideExamples")
$ErrorActionPreference = 'Stop'
$text = [IO.File]::ReadAllText((Resolve-Path $Guide), [Text.Encoding]::UTF8) -replace "`r`n", "`n"
$fragment = '^// ' + -join [char[]](0x0444, 0x0440, 0x0430, 0x0433, 0x043C, 0x0435, 0x043D, 0x0442)
$sketch = '^// ' + -join [char[]](0x044D, 0x0441, 0x043A, 0x0438, 0x0437)
$fence = '(?s)' + [string][char]96 * 3 + 'cpp\n(.*?)' + [string][char]96 * 3
$note = "// Generated from docs/AI-CPP-GUIDE.md by Extract-Guide.ps1. Change the guide, not this file.`n"
$header = New-Object System.Text.StringBuilder
$source = New-Object System.Text.StringBuilder
[void]$header.Append($note + "#pragma once`n#include <Engine/AI/Ai.h>`n")
[void]$source.Append($note + "#include `"GuideExamples.h`"`n#include <Engine/MainGame/BaseGameClass/Game.h>`n")
$index = 0
foreach ($block in [regex]::Matches($text, $fence)) {
    $index++
    $code = $block.Groups[1].Value
    if ($code -match $sketch) { continue }
    $lines = $code.TrimEnd("`n") -split "`n" | Where-Object { $_ -ne '#pragma once' -and $_ -notmatch '^#include "' }
    $body = $lines -join "`n"
    if ($code -match $fragment) {
        [void]$source.Append("// ---- guide block $index`n[[maybe_unused]] void GuideFragment$index(Ai::Context& ctx, Ai::Controller& ai, Ai::World& world, Ai::StateBuilder& s, Ai::MachineBuilder& m, Game& game, ComponentId batchId)`n{`n$body`n}`n")
    } elseif ($code -match '^//[^\n]*\.cpp') {
        [void]$source.Append("// ---- guide block $index`n$body`n")
    } else {
        [void]$header.Append("// ---- guide block $index`n$body`n")
    }
}
$utf8 = New-Object Text.UTF8Encoding($false)
foreach ($file in @(@("$Output.h", $header.ToString()), @("$Output.cpp", $source.ToString()))) {
    $old = if (Test-Path $file[0]) { [IO.File]::ReadAllText($file[0], [Text.Encoding]::UTF8) } else { '' }
    if ($file[1] -ne $old) { [IO.File]::WriteAllText($file[0], $file[1], $utf8); Write-Output "$(Split-Path $file[0] -Leaf) updated" }
}
Write-Output "$index guide blocks"
