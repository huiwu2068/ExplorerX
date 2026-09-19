# ExplorerX

ExplorerX is a fast, portable Windows file manager that preserves the native Shell experience while improving multi-folder work, fast discovery, and large-directory browsing.

[简体中文](README.zh-CN.md)

![ExplorerX overview: tabs, dual-pane workspace, Everything search, and native Details view](assets/screenshots/explorerx-overview.png)

## What ExplorerX adds

| Scenario | ExplorerX improvement |
| --- | --- |
| Find files quickly | Integrated [Everything](https://www.voidtools.com/) search opens results in a dedicated tab, with global and current-folder scopes. |
| Work across folders | Dual panes and tabs keep multiple locations available for copy, comparison, and organization work. |
| Browse folder structure | Details view supports Directory Opus-style in-place folder expansion: click the chevron or use the Left/Right keys without leaving the list. |
| Browse large folders | A fast path for physical directories, responsive sorting, extension-icon caching, and lazy tab restoration reduce unnecessary waits. |
| Command-line workflow | Open Windows Terminal at the active folder. |
| Keep the Windows experience | Reuses the native Windows context menu and Shell extensions, so installed extensions such as 7-Zip, Git, VS Code, WinRAR, and security tools continue to work through Windows. |

## Key features

### Everything search

- Queries Everything through IPC and displays results in a reusable results tab.
- Choose a global search or limit it to the current folder while retaining common matching options.
- Search, loading, and sorting retain native Windows file operations instead of replacing them with a custom context menu.

### Dual panes and tabs

- View two locations side by side and use tabs to switch working sets quickly.
- Well suited to frequent copy, comparison, and archive workflows.

### Opus-style folder expansion

- Expand folders into a hierarchy directly within the Details view.
- Use the expand chevron, or `Right` to expand and `Left` to collapse, without repeatedly entering and leaving folders.
- Child items load through an asynchronous fast path; hierarchical sorting, selection, drag and drop, and the native Shell context menu remain available.

### Native, lightweight performance improvements

- Physical local folders use batch reads based on `GetFileInformationByHandleEx`; virtual folders, network locations, and MTP devices continue through a compatible Shell path.
- Zero-copy sorting, extension-icon caching, lazy tab restoration, and configuration dirty tracking reduce redundant startup and large-folder work.
- No resident service is required; slow or failed external calls do not block the UI thread.

### Portable use

ExplorerX supports both registry-based configuration and a configuration file alongside the executable, making it suitable for a USB drive or a self-contained work directory.

## Screenshot

The image above is a real ExplorerX runtime capture. It shows the Everything search entry at the upper right, the tabbed workspace, and the Windows Shell Details view with native item type information. See the feature descriptions above for dual-pane operation and in-place folder expansion.

## Build

ExplorerX is built with Visual Studio 2022 Build Tools and the Windows SDK. See
[BUILDING.md](BUILDING.md) for the portable command-line build instructions.

## Upstream and license

ExplorerX is an independently maintained, modified derivative of
[Explorer++](https://github.com/derceg/explorerplusplus), originally created by
David Erceg. ExplorerX-specific modifications and maintenance are by Hank Li.

ExplorerX is licensed under the GNU General Public License, version 3.0. Original
copyright notices and license terms are retained throughout the source tree. When
distributing a binary, provide the corresponding source code under GPL-3.0.
