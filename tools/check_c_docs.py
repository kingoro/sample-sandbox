#!/usr/bin/env python3
"""C headerの公開宣言にDoxygenコメントがあることを検査する。"""

from __future__ import annotations

import re
import sys
from pathlib import Path


DECLARATION_PATTERNS = (
    re.compile(r"^\s*#define\s+[A-Z][A-Z0-9_]*(?:\s|\()"),
    re.compile(r"^\s*typedef\s+"),
    re.compile(r"^\s*[A-Za-z_][A-Za-z0-9_\s\*]*\([^;]*$"),
)


def has_doc_comment(lines: list[str], index: int) -> bool:
    cursor = index - 1
    while cursor >= 0 and not lines[cursor].strip():
        cursor -= 1
    if cursor < 0:
        return False
    if lines[cursor].strip().endswith("*/"):
        while cursor >= 0:
            stripped = lines[cursor].strip()
            if stripped.startswith("/**") or stripped.startswith("/*!"):
                return True
            if stripped.startswith("/*") and not stripped.startswith(("/**", "/*!")):
                return False
            cursor -= 1
    return False


def declarations(path: Path) -> list[tuple[int, str]]:
    lines = path.read_text(encoding="utf-8").splitlines()
    failures = []
    if not any("@file" in line for line in lines):
        failures.append((1, "file-level @file comment"))
    in_enum = False
    in_struct = False
    in_macro = False
    for index, line in enumerate(lines):
        stripped = line.strip()
        if in_macro:
            in_macro = line.rstrip().endswith("\\")
            continue
        if stripped.startswith("#define ") and line.rstrip().endswith("\\"):
            in_macro = True

        if stripped.startswith("typedef enum") or stripped == "enum {":
            in_enum = True
        if stripped.startswith("typedef struct"):
            in_struct = True

        is_include_guard = stripped.startswith("#define ") and stripped.endswith("_H")
        is_generated_enum_alias = (
            path.name.endswith("_generated.h")
            and stripped.startswith(("typedef enum ", "typedef uint32_t "))
        )
        is_declaration = (
            not is_include_guard
            and not is_generated_enum_alias
            and any(pattern.match(line) for pattern in DECLARATION_PATTERNS)
        )
        is_member = (
            not is_generated_enum_alias
            and
            (in_enum or in_struct)
            and stripped
            and not stripped.startswith(("/", "*", "{", "}"))
            and stripped.endswith((",", ";"))
        )
        if (is_declaration or is_member) and not has_doc_comment(lines, index):
            failures.append((index + 1, stripped))

        if in_enum and stripped == "};":
            in_enum = False
        if in_struct and stripped.startswith("} "):
            in_struct = False
    return failures


def main() -> int:
    failed = False
    for path_text in sys.argv[1:]:
        path = Path(path_text)
        for line, declaration in declarations(path):
            print(f"{path}:{line}: Doxygenコメントがありません: {declaration}")
            failed = True
    return int(failed)


if __name__ == "__main__":
    raise SystemExit(main())
