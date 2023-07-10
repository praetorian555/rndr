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

# Will search under X:\Program Files\Microsoft Visual Studio and X:\Program Files (x86)\Microsoft Visual Studio for
# the specified file. If found, the folder containing the file will be added to the system path if its not already
# there. It will only search local drives.
function AddToSystemPath{
    param(
        [Parameter(Mandatory=$true)]
        [string]$FileToFind,

        [Parameter(Mandatory=$true)]
        [string]$FolderRegex
    )

    Write-Output "Adding folder that contains $FileToFind to the system path, trying to match $FolderRegex pattern..."

    # Get the logical disk objects using WMI
    $drives = Get-WmiObject -Class Win32_LogicalDisk
    $filePath = $null

    foreach ($drive in $drives)
    {
        # Only search on drives of type 3, i.e., local disks
        if ($drive.DriveType -ne 3)
        {
            continue
        }

        $pathsToSearch = @("\Program Files\Microsoft Visual Studio", "\Program Files (x86)\Microsoft Visual Studio")

        foreach ($pathToSearch in $pathsToSearch)
        {
            # Try/Catch block for any errors (like access denied)
            try
            {
                $pathToSearch = $drive.DeviceID + $pathToSearch
                $filePath = Get-ChildItem -Path $pathToSearch -Recurse -ErrorAction Stop -File | Where-Object { ($_.Name -eq $FileToFind) -and ($_.Directory -match $FolderRegex) }
                if ($filePath)
                {
                    break
                }
            }
            catch
            {
                Write-Host "Error searching drive $( $drive.DeviceID ): $( $_.Exception.Message )"
            }
        }

        # Check if file was found
        if ($filePath -ne $null) {
            break
        }
    }

    # Check if file was found
    if ($filePath -eq $null) {
        Write-Host "File $FileToFind not found!"
        return
    }
    $filePath = (Get-Item $($filePath.FullName)).DirectoryName
    # Get the current system path
    $systemPath = [Environment]::GetEnvironmentVariable("Path", "Machine")

    # Check if the exe path is already in the system path
    if ($systemPath -notlike "*$filePath*") {
        # Add the DLL path to the system path
        $newPath = $systemPath + ";" + $filePath
        [Environment]::SetEnvironmentVariable("Path", $newPath, "Machine")
        Write-Host "Add path $filePath to system path"
    } else {
        Write-Host "Path $filePath is already in the system path"
    }
}

AddToSystemPath -FileToFind "clang_rt.asan_dynamic-x86_64.dll" -FolderRegex ".*bin.*Hostx64.*x64.*"
AddToSystemPath -FileToFind "clang-format.exe" -FolderRegex ".*Llvm.*x64.*bin.*"
