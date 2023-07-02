# Check if the script is run as an administrator
if (-NOT ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Error "You must run this script as an Administrator. Start Windows PowerShell with the 'Run as administrator' option."
    pause
    Break
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
    "--add Microsoft.VisualStudio.ComponentGroup.WebToolsExtensions.CMake"
    "--add Microsoft.VisualStudio.Component.VC.CMake.Project"
    "--add Microsoft.VisualStudio.Component.VC.ASAN"
    "--add Microsoft.VisualStudio.Component.Windows11SDK.22000"
    "--add Microsoft.VisualStudio.Workload.NativeDesktop")
$components = $components -join " "

# Install Visual Studio 2022 Community along with the specified components
Write-Output "Installing Visual Studio 2022 Community..."
Start-Process -FilePath $installerPath -ArgumentList "$components --quiet --norestart --wait" -Wait
Remove-Item $installerPath
Write-Output "Visual Studio 2022 Community has been installed."

Write-Output "Adding path to the address sanitizer dll to the system path..."

# User-specified DLL file
$specifiedDLL = "clang_rt.asan_dynamic-x86_64.dll"
$specifiedFolder = "Hostx64"

# Get the logical disk objects using WMI
$drives = Get-WmiObject -Class Win32_LogicalDisk
$dllPath = $null

foreach ($drive in $drives)
{
    # Only search on drives of type 3, i.e., local disks
    if ($drive.DriveType -ne 3)
    {
        continue
    }

    $pathsToSearch = @("\Program Files\Microsoft Visual Studio\2022\Community", "\Program Files (x86)\Microsoft Visual Studio\2022\Community")

    foreach ($pathToSearch in $pathsToSearch)
    {
        # Try/Catch block for any errors (like access denied)
        try
        {
            $pathToSearch = $drive.DeviceID + $pathToSearch
            # Find the folder
            $folderPath = (Get-ChildItem -Path $pathToSearch -Recurse -ErrorAction Stop -Filter $specifiedFolder -Directory).FullName

            # If the folder exists, search for the DLL within it
            if ($folderPath)
            {
                $dllPath = Get-ChildItem -Path $folderPath -Recurse -ErrorAction Stop -Filter $specifiedDLL
                if ($dllPath)
                {
                    break
                }
            }
        }
        catch
        {
            Write-Host "Error searching drive $( $drive.DeviceID ): $( $_.Exception.Message )"
        }
    }

    # Check if DLL was found
    if ($dllPath -ne $null) {
        break
    }
}

# Check if DLL was found
if ($dllPath -eq $null) {
    Write-Host "Specified DLL not found"
    return
}
$dllPath = (Get-Item $($dllPath.FullName)).DirectoryName
# Get the current system path
$systemPath = [Environment]::GetEnvironmentVariable("Path", "Machine")

# Check if the DLL path is already in the system path
if ($systemPath -notlike "*$dllPath*") {
    # Add the DLL path to the system path
    $newPath = $systemPath + ";" + $dllPath
    [Environment]::SetEnvironmentVariable("Path", $newPath, "Machine")
    Write-Host "Add path $dllPath to system path"
} else {
    Write-Host "Path $dllPath is already in the system path"
}
