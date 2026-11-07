param([string]$Repository='E:/Engine')
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath($Repository)
$version=(Get-Content -Raw "$repo/Engine/Version.json" | ConvertFrom-Json).version
$release="$repo/Game/build/x64/Release"
$destination="$repo/Engine/build/releases/PainEngine-$version-win64"
if(Test-Path -LiteralPath $destination){throw 'Choose a new version; existing package is preserved.'}
New-Item -ItemType Directory -Path $destination | Out-Null
Copy-Item -LiteralPath "$release/EngineLauncher.exe" -Destination $destination
foreach($name in @('Runtime','BasicExamples','Pong','SunGame','Battle','Katamari')){
    & robocopy "$release/$name" "$destination/$name" /E /XD .shadow_cache SDK /XF *.pdb *.exp *.lib '*Tests.exe' /NFL /NDL /NJH /NJS /NP | Out-Null
    if($LASTEXITCODE -ge 8){throw "Copy failed: $name"}
}
Copy-Item -LiteralPath "$release/Runtime/SDK" -Destination "$destination/Runtime" -Recurse
# Include the deployed VC runtime beside each standalone application as well as the host.
$dependencies=Get-ChildItem -LiteralPath "$release/Runtime" -File | Where-Object { $_.Name -like 'msvcp140*.dll' -or $_.Name -like 'vcruntime140*.dll' -or $_.Name -eq 'concrt140.dll' -or $_.Name -eq 'D3Dcompiler_47.dll' }
foreach($folder in @($destination,"$destination/BasicExamples","$destination/Pong","$destination/SunGame","$destination/Battle","$destination/Katamari")){
    foreach($file in $dependencies){Copy-Item -LiteralPath $file.FullName -Destination $folder}
}
& "$repo/Engine/BuildSupport/Deploy-D3DCompiler.ps1" -DestinationRoot $destination
New-Item -ItemType Directory -Path "$destination/Docs","$destination/Licenses" | Out-Null
Copy-Item -Path "$repo/Engine/docs/*.md","$repo/Engine/docs/API-REFERENCE.html" -Destination "$destination/Docs"
foreach($port in Get-ChildItem "$repo/Engine/vcpkg_installed/x64-windows/share" -Directory){
    $copyright=Join-Path $port.FullName 'copyright'
    if(Test-Path -LiteralPath $copyright){Copy-Item -LiteralPath $copyright -Destination "$destination/Licenses/$($port.Name).txt"}
}
Copy-Item "$repo/Engine/ThirdParty/GLM-master/copying.txt" "$destination/Licenses/glm.txt"
Copy-Item "$repo/Engine/ThirdParty/meshoptimizer/LICENSE.md" "$destination/Licenses/meshoptimizer.txt"
Copy-Item "$repo/Engine/ThirdParty/nlohmann/LICENSE.MIT" "$destination/Licenses/nlohmann.txt"
@"
PainEngine $version — Windows x64 Release, MSVC v143, Qt 6.7, Assimp.
Распакуйте весь архив и запустите EngineLauncher.exe.
Runtime содержит редактор, шаблон проекта, командлеты и C++ SDK.
Для компиляции C++-проектов нужны Visual Studio C++ tools v143 и Windows SDK.
Нормал-маппинг: исправлены GPU-сигнатура TANGENT и зеркальный масштаб.
Старые C++-проекты пересоберите с новым SDK; MeshVertex теперь 64 байта.
Архив содержит текущую сборку, исторические runtime сюда не включены.
Справка: Docs/API-REFERENCE.html. Ограничения теней: Docs/RENDERING.md.
"@ | Set-Content "$destination/Прочитай меня.txt" -Encoding utf8
Write-Output "Package staged: $destination"
