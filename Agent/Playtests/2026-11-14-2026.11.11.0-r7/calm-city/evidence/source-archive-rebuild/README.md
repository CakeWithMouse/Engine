Fresh extraction check: the archive was extracted to Engine/build/playtests/20261114-r7/calm-city/ArchiveRebuildCheck. The first attempt exposed omitted Game.sln/Game.vcxproj; these are now included. The regenerated archive built successfully with the explicit .11.11.0 Runtime and ran standalone for 20 frames. See build.log/build.exit.txt and run.log/run.exit.txt.

Reproduce from E:\Engine:
```powershell
$zip = 'E:\Engine\Agent\Playtests\2026-11-14-2026.11.11.0-r7\calm-city\calm-city-r7-source.zip'
$dest = 'E:\Engine\Engine\build\playtests\20261114-r7\calm-city\FreshArchive'
Expand-Archive -LiteralPath $zip -DestinationPath $dest
$runtime = 'E:\Engine\Game\build\x64\Release\Runtime'
& "$runtime\SDK\ProjectTools.ps1" -Action Build -Project "$dest\CalmCityUpgraded\Project.json" -Runtime $runtime -Rebuild
& "$runtime\EngineRuntime.exe" --project "$dest\CalmCityUpgraded\Project.json" --mode game --frames 20
```

