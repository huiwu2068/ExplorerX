#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import shutil
import subprocess
import re


def print_help():
    print("""用法:
  python tools/backup_project.py [备份名称]

创建当前项目的版本化文件备份。备份包含 Git 已跟踪文件，以及未被忽略的未跟踪文件。

参数:
  备份名称        可选。指定后直接创建备份；省略时会交互式询问。
  -h, --help      显示本帮助信息并退出。

备份位置:
  默认在项目目录的同级目录创建 <项目名>_backup/，每次备份使用递增编号，
  例如 01_before_refactor/。使用外置 Git 目录的项目会备份到该 Git 目录的同级位置。
""")


def get_project_root():
    # Prefer Git's own answer.  This also supports repositories whose Git
    # directory is stored outside the working tree (a .git pointer file).
    # Check the launch directory first: this script is intended to be shared
    # from a tools directory rather than stored inside every project.
    for start_dir in (os.getcwd(), os.path.dirname(os.path.abspath(__file__))):
        try:
            return subprocess.check_output(
                ['git', '-C', start_dir, 'rev-parse', '--show-toplevel'],
                stderr=subprocess.DEVNULL,
            ).decode('utf-8', errors='ignore').strip()
        except (subprocess.CalledProcessError, FileNotFoundError):
            pass

    # Fallback for projects not yet initialized as Git repositories.
    current_dir = os.getcwd()
    while current_dir != os.path.dirname(current_dir):
        if os.path.exists(os.path.join(current_dir, '.git')) or os.path.exists(os.path.join(current_dir, '.gitignore')):
            return current_dir
        current_dir = os.path.dirname(current_dir)
    # Fallback to current working directory
    return os.getcwd()

def get_backup_parent_dir(project_root):
    """Return where this project's versioned backups should live.

    A normal repository keeps its Git directory at ``<project>/.git`` and
    backs up beside the project.  If Git reports a directory outside the
    working tree (for example, created with ``git init --separate-git-dir``),
    back up beside that external Git directory instead.  This is based only
    on Git's layout, not on any project-specific name or path.
    """
    default_parent = os.path.dirname(project_root)
    local_git_dir = os.path.realpath(os.path.join(project_root, '.git'))

    try:
        git_dir = subprocess.check_output(
            ['git', '-C', project_root, 'rev-parse', '--path-format=absolute', '--git-dir'],
            stderr=subprocess.DEVNULL,
        ).decode('utf-8', errors='ignore').strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return default_parent

    if git_dir and os.path.realpath(git_dir) != local_git_dir:
        return os.path.dirname(os.path.realpath(git_dir))
    return default_parent

def find_backup_root(project_root, parent_dir):
    proj_name = os.path.basename(project_root)
    # Check parent directory contents
    try:
        entries = os.listdir(parent_dir)
    except Exception:
        entries = []
    
    # Check for existing backup directory (case-insensitive)
    # Matches project_name followed by _backup, _backukp, _bakcup, etc.
    pattern = re.compile(rf"^{re.escape(proj_name)}_(backup|backukp|bakcup)$", re.IGNORECASE)
    for entry in entries:
        if os.path.isdir(os.path.join(parent_dir, entry)) and pattern.match(entry):
            return os.path.join(parent_dir, entry)
            
    # Default to creating project_name_backup
    return os.path.join(parent_dir, f"{proj_name}_backup")

def get_next_index(backup_root):
    if not os.path.exists(backup_root):
        return 1, 2 # next_index, width
        
    try:
        entries = os.listdir(backup_root)
    except Exception:
        entries = []
        
    max_num = 0
    max_num_len = 2 # default width
    
    pattern = re.compile(r'^(\d+)_(.*)$')
    for entry in entries:
        if os.path.isdir(os.path.join(backup_root, entry)):
            match = pattern.match(entry)
            if match:
                num_str = match.group(1)
                num = int(num_str)
                if num > max_num:
                    max_num = num
                    max_num_len = len(num_str)
                    
    if max_num == 0:
        return 1, 2
    else:
        return max_num + 1, max_num_len

def get_git_files(project_root):
    # Run git ls-files to get tracked files
    # Run git ls-files -o --exclude-standard to get untracked, non-ignored files
    files = set()
    try:
        # -z prevents Git from quoting paths with spaces or non-ASCII
        # characters, so filenames can be copied exactly as they are.
        for cmd in (
            ['git', 'ls-files', '-z'],
            ['git', 'ls-files', '-o', '--exclude-standard', '-z'],
        ):
            output = subprocess.check_output(cmd, cwd=project_root, stderr=subprocess.DEVNULL)
            for path_bytes in output.split(b'\0'):
                if path_bytes:
                    files.add(os.fsdecode(path_bytes))
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass
        
    # Filter only files that actually exist (handling deleted tracked files)
    existing_files = []
    for f in sorted(list(files)):
        full_path = os.path.join(project_root, f)
        if os.path.isfile(full_path):
            existing_files.append(f)
            
    return existing_files

def main():
    if any(arg in {"-h", "--help"} for arg in sys.argv[1:]):
        print_help()
        return

    project_root = get_project_root()
    print(f"检测到的项目根目录: {project_root}")
    
    # Get user input for directory name
    user_input = ""
    if len(sys.argv) > 1:
        user_input = " ".join(sys.argv[1:]).strip()
        print(f"已通过命令行入参获取备份名称: {user_input}")
    
    while not user_input:
        try:
            user_input = input("请输入备份目录名称: ").strip()
        except (KeyboardInterrupt, EOFError):
            print("\n操作已取消。")
            sys.exit(1)
            
    # Resolve target paths
    parent_dir = get_backup_parent_dir(project_root)
    backup_root = find_backup_root(project_root, parent_dir)
    
    next_index, width = get_next_index(backup_root)
    index_str = f"{next_index:0{width}d}"
    
    target_dir_name = f"{index_str}_{user_input}"
    target_dir = os.path.join(backup_root, target_dir_name)
    
    print(f"备份根目录: {backup_root}")
    print(f"目标备份路径: {target_dir}")
    
    # Collect files
    print("正在扫描项目文件...")
    files_to_copy = get_git_files(project_root)
    total_files = len(files_to_copy)
    
    if total_files == 0:
        print("未找到需要备份的文件（或者不是有效的Git仓库）。")
        sys.exit(1)
        
    print(f"共发现 {total_files} 个文件需要备份。开始复制...")
    
    # Ensure target directory exists
    try:
        os.makedirs(target_dir, exist_ok=True)
    except Exception as e:
        print(f"无法创建目标备份目录: {e}")
        sys.exit(1)
    
    # Copy files with progress
    for i, rel_path in enumerate(files_to_copy):
        src_file = os.path.join(project_root, rel_path)
        dest_file = os.path.join(target_dir, rel_path)
        
        # Ensure parent directories exist
        os.makedirs(os.path.dirname(dest_file), exist_ok=True)
        
        try:
            shutil.copy2(src_file, dest_file)
        except Exception as e:
            # If a file fails to copy, we print an error on a new line and then continue
            print(f"\n[错误] 无法复制文件 {rel_path}: {e}")
            
        # Update progress bar
        percent = (i + 1) / total_files * 100
        bar_len = 30
        filled_len = int(round(bar_len * (i + 1) / total_files))
        bar = '=' * filled_len + ' ' * (bar_len - filled_len)
        
        # Format filename to fit inside terminal line (truncate if necessary)
        max_file_display_len = 30
        display_name = rel_path
        if len(display_name) > max_file_display_len:
            display_name = "..." + display_name[-(max_file_display_len-3):]
            
        sys.stdout.write(f"\r进度: [{bar}] {percent:6.2f}% ({i+1}/{total_files}) {display_name:<30}")
        sys.stdout.flush()
        
    print(f"\n\n备份完成！备份保存在: {target_dir}")

if __name__ == '__main__':
    main()
