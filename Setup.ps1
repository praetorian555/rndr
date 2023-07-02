# Check if the script is run as an administrator
if (-NOT ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Error "You must run this script as an Administrator. Start Windows PowerShell with the 'Run as administrator' option."
    pause
    Break
}

# Specify your desired version here
$desiredVersion = [Version]"3.26.4"
$installed = $false

# Check if cmake is installed
try {
    $output = cmake --version
    $installed = $true
}
catch {
    Write-Output "CMake not found. Will proceed with the installation."
}

# Check cmake version if it's installed
if ($installed) {
    $output = $output -split "`n"
    $installedVersion = $output[0] -replace "cmake version ", ""
    if ([Version]$installedVersion -lt $desiredVersion) {
        Write-Output "CMake version is lower than desired. Will proceed with the installation of the desired version."
        $installed = $false
    }
    else {
        Write-Output "CMake of version $installedVersion or higher is already installed."
    }
}

# Install cmake if not installed or the installed version is lower than desired
if (-NOT $installed) {
    $installerName = "cmake-$($desiredVersion.Major).$($desiredVersion.Minor).$($desiredVersion.Build)-windows-x86_64.msi"
    $url = "https://cmake.org/files/v$($desiredVersion.Major).$($desiredVersion.Minor)/$installerName"
    $outputDir = "$env:TEMP"

    # Download the installer
    Write-Output "Downloading CMake installer from $url"
    Invoke-WebRequest -Uri $url -OutFile "$outputDir\$installerName"
    Write-Output "Download complete."

    # Compute the SHA-256 hash of the downloaded file
    Write-Output "Validating the downloaded file..."
    $hash = (Get-FileHash -Path "$outputDir\$installerName" -Algorithm SHA256).Hash
    $knownGoodHash = "566fed3fa51a274609677717d0a0f38ba69ab0b3d987df53c05392a13fc211c4"
    if ($hash -ne $knownGoodHash) {
        Write-Error "SHA-256 hash does not match the known good hash. The file might be corrupted or tampered with."
        Break
    }
    Write-Output "Validation complete."

    # Install cmake
    Write-Output "Installing CMake..."
    Start-Process -FilePath "msiexec.exe" -ArgumentList "/i `"$outputDir\$installerName`" /quiet /norestart ADD_CMAKE_TO_PATH=System" -Wait
    Write-Output "CMake of version $desiredVersion has been installed."

    # Clean up
    Remove-Item -Path "$outputDir\$installerName"
    Write-Output "Clean up complete."
}

# Define the URL to download the installer and the known SHA-256 checksum for that installer
$installerUrl = "https://aka.ms/vs/17/release/vs_Community.exe"

# Define the path where to download the installer
$installerPath = "${Env:TEMP}\vs_Community.exe"

# Download the installer
Write-Output "Downloading Visual Studio 2022 Community installer from $installerUrl"
Invoke-WebRequest -Uri $installerUrl -OutFile $installerPath
Write-Output "Download complete."


# Define the components to install
# Replace these component IDs with the ones you want to install
$components = @(
    "--add Microsoft.VisualStudio.Component.CoreEditor"
    "--add Microsoft.VisualStudio.Workload.CoreEditor"
    "--add Microsoft.VisualStudio.Component.Roslyn.Compiler"
    "--add Microsoft.Component.MSBuild"
    "--add Microsoft.VisualStudio.Component.TextTemplating"
    "--add Microsoft.VisualStudio.Component.VC.CoreIde"
    "--add Microsoft.VisualStudio.Component.VC.Tools.x86.x64"
    "--add Microsoft.VisualStudio.Component.VC.ATL"
    "--add Microsoft.VisualStudio.Component.VC.Redist.14.Latest"
    "--add Microsoft.VisualStudio.ComponentGroup.NativeDesktop.Core"
    "--add Microsoft.VisualStudio.Component.VC.ASAN"
    "--add Microsoft.VisualStudio.Component.Windows10SDK.20348"
    "--add Microsoft.VisualStudio.Workload.NativeDesktop")
$components = $components -join " "

# Install Visual Studio 2022 Community along with the specified components
Write-Output "Installing Visual Studio 2022 Community..."
Start-Process -FilePath $installerPath -ArgumentList "$components --quiet --norestart --wait" -Wait
Write-Output "Visual Studio 2022 Community has been installed."
