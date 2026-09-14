# Requirements

- Visual Studio 2019/2022 (2022 recommended). Install the "Desktop development with C++" workload.
- Windows 10 SDK (most recent version recommended)
- (Optional) To build for ARM64, the following Visual Studio components should be installed:
    - MSVC v143 - VS 2022 C++ ARM64 build tools (Latest)
- (Optional) To build with Clang (using the Debug-Clang/Release-Clang solution configurations), the following Visual Studio components should be installed:
    - C++ Clang tools for Windows
- (Optional) To build the installer, WiX 3.11 and the relevant Visual Studio extension should be installed. Download links for both of those items can be found on the [WiX website](https://wixtoolset.org/docs/wix3/).

# Setup

[vcpkg](https://vcpkg.io/) is used to manage dependencies. Before you can build Explorer++, you'll first need to initialize vcpkg:

- Clone this repo with submodules enabled:
```
git clone --recurse-submodules https://github.com/derceg/explorerplusplus.git
cd explorerplusplus
```
- Run the vcpkg bootstrapper:
    - cmd/PowerShell: `.\Explorer++\ThirdParty\vcpkg\bootstrap-vcpkg.bat`
    - git bash/posix shell: `./Explorer++/ThirdParty/vcpkg/bootstrap-vcpkg.sh`


The relevant packages should then be automatically installed during the first build.

# Compiling

Open `Explorer++\Explorer++.sln`. From within Visual Studio, select `Debug` > `Start Without Debugging` to compile and run the program.

## Command-line build (without the Visual Studio IDE)

The Visual Studio IDE is not required. Install **Visual Studio 2022 Build Tools** with the C++ desktop workload and a Windows SDK. From an elevated PowerShell prompt, this can be done with:

```powershell
winget install --id Microsoft.VisualStudio.2022.BuildTools --exact --source winget --accept-package-agreements --accept-source-agreements --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --add Microsoft.VisualStudio.Component.Windows10SDK.22621"
```

Then use the **x64 Native Tools Command Prompt for VS 2022**, or initialize the build environment manually before calling MSBuild:

```bat
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
cd /d path\to\explorerplusplus
msbuild Explorer++\Explorer++.sln /m /p:Configuration=Debug /p:Platform=x64

msbuild Explorer++\Explorer++.sln /m /p:Configuration=Debug /p:Platform=x64
```

Use `Configuration=Release` to build the release variant. The first build installs the vcpkg manifest dependencies automatically and can take considerably longer than later incremental builds.

### PowerShell

From PowerShell, call `VsDevCmd.bat` and MSBuild in the same `cmd.exe` process. Calling the batch file directly from PowerShell starts a child process, so its MSVC environment variables would not be available to the subsequent MSBuild command.

```powershell
Set-Location 'E:\path\to\explorerplusplus'

# Build the 64-bit Debug configuration.
& cmd.exe /d /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && msbuild ".\Explorer++\Explorer++.sln" /m /p:Configuration=Debug /p:Platform=x64'

# Build the 64-bit Release configuration.
& cmd.exe /d /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && msbuild ".\Explorer++\Explorer++.sln" /m /p:Configuration=Release /p:Platform=x64'
```

After a successful Debug build, run the program from PowerShell with:

```powershell
& .\Explorer++\Explorer++\x64\Debug\Explorer++.exe
```

To force a complete rebuild instead of an incremental build, add `/t:Rebuild` to the MSBuild command. Use `/t:Clean` to remove only the build outputs for the selected configuration.

For the configurations used most often:

| Purpose | MSBuild properties |
| --- | --- |
| Development/debugging | `/p:Configuration=Debug /p:Platform=x64` |
| 64-bit release | `/p:Configuration=Release /p:Platform=x64` |
| 32-bit release | `/p:Configuration=Release /p:Platform=Win32` |

The Debug x64 executable is written to `Explorer++\Explorer++\x64\Debug\Explorer++.exe`; the Release x64 executable is written to `Explorer++\Explorer++\x64\Release\Explorer++.exe`.

## VS Code

The repository includes VS Code tasks in `.vscode/tasks.json`. After Build Tools and vcpkg have been initialized, open the repository root in VS Code:

- Press `Ctrl+Shift+B` to build `Debug | x64`.
- Run the `build: Release x64` task to create a release build.
- Press `F5` to build and launch the Debug x64 executable under the Microsoft C/C++ debugger.

## Code navigation with clangd

For code completion, go-to-definition, find references, and rename outside Visual Studio, install LLVM (which includes `clangd`) and the VS Code `clangd` extension:

```powershell
winget install --id LLVM.LLVM --exact --source winget
code --install-extension llvm-vs-code-extensions.vscode-clangd
```

`clangd` needs a compilation database because it does not read `.sln` or `.vcxproj` files directly. Generate `compile_commands.json` after a clean rebuild using an MSBuild compilation-database extractor, then restart the VS Code window so clangd can index the project. The included VS Code task `index: clangd (Debug x64)` performs this operation when `msbuild-compdb` is installed at the path configured in `.vscode/tasks.json`.

# Translations

Building the program in release mode will also build all of the translations. The resulting DLLs can then be used with Explorer++.

# Tests

The `TestExplorer++` project contains unit tests for the solution as a whole. The GoogleTest package is installed via vcpkg, so provided vcpkg has been initialized and you've been able to build the solution, you should just need to build the `TestExplorer++` project, then run the tests via the Visual Studio Test Explorer.

Note that the `TestHelper` project is older and is in the process of being removed. It doesn't currently compile and shouldn't be used.
