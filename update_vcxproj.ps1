$vcxprojPath = "C:\Anudeep\Repo\IITJ_MTech\SHA256\SHA256.vcxproj"
[xml]$xml = Get-Content $vcxprojPath

# Find the CudaCompile ItemGroup and add sha256_cpu.cu if not present
$cudaGroup = $xml.Project.ItemGroup | Where-Object { $_.CudaCompile -ne $null }
$cudaFiles = @($cudaGroup.CudaCompile) | Where-Object { $_.Include -eq "src\sha256_cpu.cu" }

if ($cudaFiles.Count -eq 0) {
	$newItem = $xml.CreateElement("CudaCompile")
	$newItem.SetAttribute("Include", "src\sha256_cpu.cu")
	$cudaGroup.AppendChild($newItem) | Out-Null
	Write-Host "Added sha256_cpu.cu to CudaCompile"
}

# Find the ClCompile ItemGroup and remove sha256_cpu.cpp if present
$clGroup = $xml.Project.ItemGroup | Where-Object { $_.ClCompile -ne $null }
$toRemove = @($clGroup.ClCompile) | Where-Object { $_.Include -eq "src\sha256_cpu.cpp" }
if ($toRemove.Count -gt 0) {
	$clGroup.RemoveChild($toRemove[0]) | Out-Null
	Write-Host "Removed sha256_cpu.cpp from ClCompile"
}

$xml.Save($vcxprojPath)
Write-Host "Project file updated successfully"
