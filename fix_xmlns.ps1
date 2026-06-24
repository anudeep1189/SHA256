$vcxprojPath = "C:\Anudeep\Repo\IITJ_MTech\SHA256\SHA256.vcxproj"
$content = Get-Content $vcxprojPath -Raw

# Remove the xmlns="" attribute from sha256_cpu.cu
$content = $content -replace 'Include="src\\sha256_cpu\.cu" xmlns="" /', 'Include="src\sha256_cpu.cu" /'

Set-Content -Path $vcxprojPath -Value $content -NoNewline
Write-Host "Fixed xmlns attribute in sha256_cpu.cu entry"
