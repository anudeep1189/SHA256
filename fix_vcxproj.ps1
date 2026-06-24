$vcxprojPath = "C:\Anudeep\Repo\IITJ_MTech\SHA256\SHA256.vcxproj"
$content = Get-Content $vcxprojPath -Raw

# Remove sha256_cpu.cpp from ClCompile
$content = $content -replace '(\s*)<ClCompile Include="src\\sha256_cpu\.cpp" />\s*', ''

Set-Content -Path $vcxprojPath -Value $content -NoNewline
Write-Host "Removed sha256_cpu.cpp from ClCompile section"
