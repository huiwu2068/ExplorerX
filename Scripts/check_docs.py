#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import sys
from datetime import datetime
from pathlib import Path

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")


ROOT = Path(__file__).resolve().parent.parent
TEXT_EXTENSIONS = {".md", ".yml", ".yaml"}
TODO_PATTERNS = ("REQUIRED_TODO", "OPTIONAL_TODO", "TODO")
DEPRECATED_PATTERN = re.compile(
    r"^(status:\s*(deprecated|superseded)|still_relevant:\s*false|still_live:\s*false)"
)
TIMESTAMP_PATTERN = re.compile(r"(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})")
UPDATED_PATTERN = re.compile(r"^updated:\s*(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})\s*$")
MAX_LINES_PATTERN = re.compile(r"^max_lines:\s*(\d+)\s*$")
DATE_ONLY_FIELD_PATTERN = re.compile(
    r"^(date|created|updated|last_verified|review_date):\s*\d{4}-\d{2}-\d{2}\s*$"
)
DOC_FILENAME_PATTERN = re.compile(
    r"^([A-Z]+)-\d{3}_.+\.md$|^\d{4}-\d{2}-\d{2}_\d{6}_.+\.md$"
)
DOC_TYPE_PREFIXES = {
    "00_overview": "Overview",
    "01_architecture": "Architecture",
    "02_contracts": "Contracts",
    "03_modules": "Modules",
    "04_features": "Features",
    "05_tasks": "Tasks",
    "06_lld": "LLD",
    "07_decisions": "Decisions",
    "08_runbooks": "Runbooks",
    "09_note": "Note",
    "10_references": "References",
}
CONTEXT_SOURCE_DIRS = (
    ROOT / "doc" / "05_tasks",
    ROOT / "memory",
    ROOT / "doc" / "07_decisions",
    ROOT / "doc" / "06_lld",
    ROOT / "doc" / "03_modules",
    ROOT / "doc" / "04_features",
    ROOT / "doc" / "09_note",
)
RISK_KEYWORDS = (
    "高风险",
    "风险",
    "breaking",
    "兼容",
    "迁移",
    "回滚",
    "安全",
    "权限",
    "数据",
    "schema",
    "blocked",
    "blocked_at",
    "confirm_before",
)
PITFALL_KEYWORDS = (
    "已知坑",
    "避坑",
    "注意",
    "根因",
    "误判",
    "不采用",
    "否决",
    "失败原因",
    "临时",
    "workaround",
    "still_live: true",
)
DESIGN_KEYWORDS = (
    "架构",
    "模块",
    "特性",
    "设计",
    "状态机",
    "并发",
    "异常路径",
    "接口",
    "契约",
    "ADR",
    "LLD",
)


def iter_text_files() -> list[Path]:
    return [
        path
        for path in ROOT.rglob("*")
        if path.is_file()
        and path.suffix in TEXT_EXTENSIONS
        and ".git" not in path.parts
    ]


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def infer_context_use(path: Path, text: str) -> str:
    parts = path.parts
    if "07_decisions" in parts or "ADR-" in path.name:
        return "architecture/design"
    if "06_lld" in parts or "LLD-" in path.name:
        return "implementation/design"
    if "05_tasks" in parts or "TASK-" in path.name:
        return "problem/task recovery"
    if "memory" in parts or "MEM-" in path.name:
        return "problem/history"
    if "09_note" in parts or "NOTE-" in path.name:
        return "project note"
    if "03_modules" in parts or "MOD-" in path.name:
        return "module design"
    if "04_features" in parts or "FEAT-" in path.name:
        return "feature design"
    if any(keyword in text for keyword in DESIGN_KEYWORDS):
        return "design"
    return "context"


def first_timestamp(text: str) -> datetime | None:
    match = TIMESTAMP_PATTERN.search(text)
    if not match:
        return None
    return datetime.strptime(match.group(1), "%Y-%m-%d %H:%M:%S")


def collect_context_candidates(limit: int) -> list[dict[str, object]]:
    candidates: list[dict[str, object]] = []
    for directory in CONTEXT_SOURCE_DIRS:
        if not directory.exists():
            continue
        for path in directory.rglob("*.md"):
            text = read_text(path)
            risk_hits = [keyword for keyword in RISK_KEYWORDS if keyword.lower() in text.lower()]
            pitfall_hits = [keyword for keyword in PITFALL_KEYWORDS if keyword.lower() in text.lower()]
            if not risk_hits and not pitfall_hits:
                continue

            timestamp = first_timestamp(text)
            priority_score = 0
            priority_score += 3 if pitfall_hits else 0
            priority_score += 3 if risk_hits else 0
            priority_score += 3 if "still_live: true" in text else 0
            priority_score += 2 if "type: breaking" in text or "type: hotfix" in text else 0
            priority_score += 2 if "blocked" in text or "blocked_at" in text else 0
            priority = "P0" if priority_score >= 6 else "P1" if priority_score >= 3 else "P2"

            candidates.append(
                {
                    "path": path.relative_to(ROOT),
                    "priority": priority,
                    "timestamp": timestamp,
                    "use": infer_context_use(path, text),
                    "risk_hits": risk_hits[:5],
                    "pitfall_hits": pitfall_hits[:5],
                    "score": priority_score,
                }
            )

    return sorted(
        candidates,
        key=lambda item: (
            {"P0": 0, "P1": 1, "P2": 2}[str(item["priority"])],
            item["timestamp"] is None,
            -(item["timestamp"].timestamp() if item["timestamp"] else 0),
            str(item["path"]),
        ),
    )[:limit]


def report_context_candidates(limit: int) -> None:
    print("== Context candidates ==")
    print("Use these to locate problems, architecture decisions, module design, or feature design.")
    print("This command does not write to .ai/project_state.md.")
    candidates = collect_context_candidates(limit)
    if not candidates:
        print("No risk/pitfall candidates found.")
        return

    for item in candidates:
        timestamp = item["timestamp"].strftime("%Y-%m-%d %H:%M:%S") if item["timestamp"] else "N/A"
        print(f"- [{item['priority']}] {item['path']}")
        print(f"  use: {item['use']}")
        print(f"  timestamp: {timestamp}")
        if item["risk_hits"]:
            print(f"  risk signals: {', '.join(item['risk_hits'])}")
        if item["pitfall_hits"]:
            print(f"  pitfall signals: {', '.join(item['pitfall_hits'])}")


def count_markers(files: list[Path]) -> None:
    for pattern in TODO_PATTERNS:
        count = 0
        for path in files:
            count += sum(1 for line in read_text(path).splitlines() if pattern in line)
        print(f"{pattern}: {count}")


def report_deprecated_docs() -> None:
    print()
    print("Deprecated / superseded docs:")
    scan_roots = [ROOT / "doc", ROOT / "memory"]
    matches: list[str] = []
    for scan_root in scan_roots:
        if scan_root.exists():
            for path in scan_root.rglob("*.md"):
                if ".git" in path.parts:
                    continue
                for index, line in enumerate(read_text(path).splitlines(), start=1):
                    if DEPRECATED_PATTERN.search(line):
                        matches.append(f"{path}:{index}: {line.strip()}")

    if matches:
        for item in matches:
            print(item)
    else:
        print("None")


def report_project_state_freshness() -> None:
    print()
    print("Project state freshness:")
    project_state_path = ROOT / ".ai" / "project_state.md"
    if not project_state_path.exists():
        print(".ai/project_state.md missing.")
        return

    lines = read_text(project_state_path).splitlines()
    updated: datetime | None = None
    max_lines: int | None = None
    for line in lines:
        match = UPDATED_PATTERN.match(line)
        if match:
            updated = datetime.strptime(match.group(1), "%Y-%m-%d %H:%M:%S")
        max_lines_match = MAX_LINES_PATTERN.match(line)
        if max_lines_match:
            max_lines = int(max_lines_match.group(1))

    if updated is None:
        print(".ai/project_state.md has no concrete updated timestamp yet.")
    else:
        age_days = (datetime.now() - updated).days
        if age_days > 14:
            print(f".ai/project_state.md updated {age_days} days ago; review current project state.")
        else:
            print(f".ai/project_state.md fresh ({age_days} days old).")

    if max_lines is None:
        print(".ai/project_state.md has no max_lines field.")
    else:
        line_count = len(lines)
        if line_count > max_lines:
            print(f".ai/project_state.md has {line_count} lines; exceeds max_lines={max_lines}.")
        else:
            print(f".ai/project_state.md size OK ({line_count}/{max_lines} lines).")


def report_doc_filename_numbering() -> None:
    print()
    print("Doc and memory filename numbering:")
    doc_dir = ROOT / "doc"
    scan_roots = []
    if doc_dir.exists():
        scan_roots.append(doc_dir)
    else:
        print("doc/ missing.")

    memory_dir = ROOT / "memory"
    if memory_dir.exists():
        scan_roots.append(memory_dir)
    else:
        print("memory/ missing.")

    bad_files = [
        path.relative_to(ROOT)
        for root in scan_roots
        for path in root.rglob("*.md")
        if not DOC_FILENAME_PATTERN.match(path.name)
    ]
    if bad_files:
        for path in bad_files:
            print(f"Missing numbered or timestamp prefix: {path}")
    else:
        print("All doc/*.md and memory/*.md files have numbered or timestamp prefixes.")


def report_timestamp_precision(files: list[Path]) -> None:
    print()
    print("Timestamp precision:")
    matches: list[str] = []
    for path in files:
        for index, line in enumerate(read_text(path).splitlines(), start=1):
            if DATE_ONLY_FIELD_PATTERN.search(line):
                matches.append(f"{path.relative_to(ROOT)}:{index}: {line.strip()}")

    if matches:
        for item in matches:
            print(f"Date-only frontmatter field: {item}")
    else:
        print("No date-only frontmatter fields found.")


def report_doc_tree() -> None:
    print()
    print("Document tree:")
    doc_dir = ROOT / "doc"
    if not doc_dir.exists():
        print("doc/ missing.")
        return

    for directory in sorted(path for path in doc_dir.iterdir() if path.is_dir()):
        files = sorted(directory.rglob("*.md"))
        label = DOC_TYPE_PREFIXES.get(directory.name, directory.name)
        print(f"{directory.relative_to(ROOT)}/ ({label}, {len(files)} files)")
        for path in files[:8]:
            print(f"  - {path.relative_to(ROOT)}")
        if len(files) > 8:
            print(f"  - ... {len(files) - 8} more")


def report_memory_tree() -> None:
    print()
    print("Memory tree:")
    memory_dir = ROOT / "memory"
    if not memory_dir.exists():
        print("memory/ missing.")
        return

    files = sorted(memory_dir.rglob("*.md"))
    print(f"memory/ (Memory, {len(files)} files)")
    for path in files[:8]:
        print(f"  - {path.relative_to(ROOT)}")
    if len(files) > 8:
        print(f"  - ... {len(files) - 8} more")


def main() -> None:
    parser = argparse.ArgumentParser(description="Check AI project documentation health.")
    parser.add_argument(
        "--suggest-context",
        action="store_true",
        help="Suggest risk and pitfall context candidates from TASK, Memory, ADR, LLD, Module, Feature, and Note docs.",
    )
    parser.add_argument("--limit", type=int, default=20, help="Max suggestions for --suggest-context.")
    args = parser.parse_args()

    if args.suggest_context:
        report_context_candidates(args.limit)
        return

    files = iter_text_files()
    print("== AI doc template health check ==")
    print(f"Root: {ROOT}")
    count_markers(files)
    report_deprecated_docs()
    report_project_state_freshness()
    report_doc_filename_numbering()
    report_timestamp_precision(files)
    report_doc_tree()
    report_memory_tree()
    print()
    print("Done.")


if __name__ == "__main__":
    main()
