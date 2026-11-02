param([string]$ToolsScript = '')
$ErrorActionPreference='Stop'
if(!$ToolsScript){$ToolsScript=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../BuildSupport/ProjectTools.ps1'))}
# Load the production commit helper and function, without running a project build.
$tokens=$null;$parseErrors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($ToolsScript,[ref]$tokens,[ref]$parseErrors)
if($parseErrors.Count){throw 'ProjectTools parse failed'}
$bootstrap=$ast.EndBlock.Statements|Where-Object {$_.Extent.Text.StartsWith("if (-not ('ProjectFileCommit'")}
. ([scriptblock]::Create($bootstrap.Extent.Text))
$definition=$ast.Find({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'WriteJson'},$true)
. ([scriptblock]::Create($definition.Extent.Text))
$encoding=[Text.UTF8Encoding]::new($false)
$atomicTestRoot=Join-Path ([IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../build'))) ('metadata-'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($atomicTestRoot)|Out-Null
$atomicTestFile=Join-Path $atomicTestRoot 'Project.json'
Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Threading;
public sealed class MetadataTestLock : IDisposable {
 readonly FileStream stream;
 readonly Timer timer;
 public MetadataTestLock(string path, int milliseconds) {
  stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
  if (milliseconds >= 0) timer = new Timer(_ => stream.Dispose(), null, milliseconds, Timeout.Infinite);
 }
 public void Dispose() { if(timer != null) timer.Dispose(); stream.Dispose(); }
}
'@
$name=-join ([char[]]@(0x41F,0x440,0x430,0x43A,0x442,0x438,0x43A,0x430))
WriteJson $atomicTestFile @{name=$name;module='old.dll'}
$held=[MetadataTestLock]::new($atomicTestFile,300)
try { WriteJson $atomicTestFile @{name=$name;module='new.dll'} } finally {$held.Dispose()}
$value=Get-Content -LiteralPath $atomicTestFile -Raw -Encoding UTF8|ConvertFrom-Json
if($value.name -cne $name -or $value.module -ne 'new.dll'){throw 'Transient lock or UTF8 round trip failed'}
$before=[IO.File]::ReadAllText($atomicTestFile)
$held=[MetadataTestLock]::new($atomicTestFile,-1)
$failure=''
try {try {WriteJson $atomicTestFile @{module='rejected.dll'}}catch{$failure=$_.Exception.Message}} finally {$held.Dispose()}
if(!$failure.Contains($atomicTestFile) -or $failure -notmatch 'Windows error (5|32)'){throw 'Persistent lock lacks path/error diagnostic'}
if([IO.File]::ReadAllText($atomicTestFile) -cne $before){throw 'Persistent lock corrupted previous metadata'}
if(@(Get-ChildItem -LiteralPath $atomicTestRoot -Filter '*.tmp.*').Count){throw 'Commit left temporary files'}
Write-Output 'Metadata checks passed: transient lock retry, bounded failure, preserved previous file, UTF8 and cleanup.'
