#!/usr/bin/env python3
"""品質メトリクスの閾値検証とHTMLレポート生成。"""

from __future__ import annotations

import argparse
import html
import json
import subprocess
import sys
from pathlib import Path

UNIT_BRANCH_MIN = 80.0
INTEGRATION_BRANCH_MIN = 50.0
CC_MAX = 15.0
MI_MIN = 35.0


def load_json(path: Path) -> dict:
    with path.open(encoding="utf-8") as stream:
        return json.load(stream)


def branch_percent(path: Path) -> float:
    return float(load_json(path)["data"][0]["totals"]["branches"]["percent"])


def check_coverage(unit: Path, integration: Path) -> int:
    values = [
        ("単体テスト C1", branch_percent(unit), UNIT_BRANCH_MIN),
        ("結合テスト C1", branch_percent(integration), INTEGRATION_BRANCH_MIN),
    ]
    failed = False
    for name, actual, minimum in values:
        status = "OK" if actual >= minimum else "NG"
        print(f"{name}: {actual:.2f}% / 基準 {minimum:.2f}% [{status}]")
        failed |= actual < minimum
    return int(failed)


def analysis_json(path: Path) -> dict:
    result = subprocess.run(
        [
            "rust-code-analysis-cli",
            "--metrics",
            "--paths",
            str(path),
            "--output-format",
            "json",
        ],
        check=True,
        capture_output=True,
        text=True,
    )
    return json.loads(result.stdout)


def test_boundary(path: Path) -> int:
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if line.strip() == "#[cfg(test)]":
            return number
    return sys.maxsize


def functions(node: dict, file_name: str, end_line: int) -> list[dict]:
    rows: list[dict] = []
    for space in node.get("spaces", []):
        if space.get("kind") == "function" and int(space["start_line"]) < end_line:
            metrics = space["metrics"]
            rows.append(
                {
                    "file": file_name,
                    "name": space["name"],
                    "line": int(space["start_line"]),
                    "cc": float(metrics["cyclomatic"]["max"]),
                    "cognitive": float(metrics["cognitive"]["max"]),
                    "mi": float(metrics["mi"]["mi_visual_studio"]),
                }
            )
        rows.extend(functions(space, file_name, end_line))
    return rows


def generate_metrics(output: Path, sources: list[Path]) -> int:
    rows: list[dict] = []
    for source in sources:
        rows.extend(functions(analysis_json(source), str(source), test_boundary(source)))

    failed = any(row["cc"] > CC_MAX or row["mi"] < MI_MIN for row in rows)
    output.parent.mkdir(parents=True, exist_ok=True)
    table_rows = []
    for row in sorted(rows, key=lambda value: (-value["cc"], value["file"], value["line"])):
        ok = row["cc"] <= CC_MAX and row["mi"] >= MI_MIN
        css_class = "ok" if ok else "ng"
        table_rows.append(
            "<tr class='{css}'><td>{file}:{line}</td><td>{name}</td>"
            "<td>{cc:.1f}</td><td>{cognitive:.1f}</td><td>{mi:.1f}</td>"
            "<td>{status}</td></tr>".format(
                css=css_class,
                file=html.escape(row["file"]),
                line=row["line"],
                name=html.escape(row["name"]),
                cc=row["cc"],
                cognitive=row["cognitive"],
                mi=row["mi"],
                status="OK" if ok else "NG",
            )
        )

    output.write_text(
        """<!doctype html>
<html lang="ja"><head><meta charset="utf-8">
<title>CC・MIレポート</title>
<style>
body{font-family:sans-serif;margin:2rem;color:#222}table{border-collapse:collapse;width:100%}
th,td{border:1px solid #bbb;padding:.45rem;text-align:left}th{background:#eee}
.ok td:last-child{color:#176b2c;font-weight:bold}.ng{background:#ffe2e2}.ng td:last-child{color:#a40000;font-weight:bold}
code{background:#eee;padding:.15rem .3rem}
</style></head><body>
<h1>循環的複雑度（CC）・保守容易性指数（MI）</h1>
<p>品質ゲート: 関数CC <code>&lt;= 15</code>、Visual Studio式MI <code>&gt;= 35</code>。</p>
<table><thead><tr><th>場所</th><th>関数</th><th>CC</th><th>認知的複雑度</th><th>MI</th><th>判定</th></tr></thead>
<tbody>"""
        + "\n".join(table_rows)
        + "</tbody></table></body></html>\n",
        encoding="utf-8",
    )

    for row in rows:
        print(
            f"{row['file']}:{row['line']} {row['name']}: "
            f"CC={row['cc']:.1f}, MI={row['mi']:.1f}"
        )
    if failed:
        print(f"品質基準違反: CC > {CC_MAX:.0f} または MI < {MI_MIN:.0f}", file=sys.stderr)
    return int(failed)


def generate_index(output: Path, unit: Path, integration: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    unit_value = branch_percent(unit)
    integration_value = branch_percent(integration)
    output.write_text(
        f"""<!doctype html>
<html lang="ja"><head><meta charset="utf-8"><title>品質レポート</title>
<style>
body{{font-family:sans-serif;margin:2rem;color:#222}}.cards{{display:flex;gap:1rem;flex-wrap:wrap}}
.card{{border:1px solid #bbb;border-radius:.5rem;padding:1rem;min-width:15rem}}
.value{{font-size:2rem;font-weight:bold}}a{{color:#075ea8}}
</style></head><body>
<h1>memory-buffer 品質レポート</h1>
<div class="cards">
<section class="card"><h2>単体テスト C1</h2><div class="value">{unit_value:.2f}%</div>
<p>基準: {UNIT_BRANCH_MIN:.0f}%以上</p><a href="coverage/unit/html/index.html">詳細を見る</a></section>
<section class="card"><h2>結合テスト C1</h2><div class="value">{integration_value:.2f}%</div>
<p>基準: {INTEGRATION_BRANCH_MIN:.0f}%以上</p><a href="coverage/integration/html/index.html">詳細を見る</a></section>
<section class="card"><h2>静的メトリクス</h2><p>CC上限: {CC_MAX:.0f}<br>MI下限: {MI_MIN:.0f}</p>
<a href="metrics/index.html">CC・MI一覧を見る</a></section>
</div></body></html>
""",
        encoding="utf-8",
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)

    coverage = subparsers.add_parser("check-coverage")
    coverage.add_argument("--unit", type=Path, required=True)
    coverage.add_argument("--integration", type=Path, required=True)

    metrics = subparsers.add_parser("metrics")
    metrics.add_argument("--output", type=Path, required=True)
    metrics.add_argument("sources", nargs="+", type=Path)

    index = subparsers.add_parser("index")
    index.add_argument("--output", type=Path, required=True)
    index.add_argument("--unit", type=Path, required=True)
    index.add_argument("--integration", type=Path, required=True)

    args = parser.parse_args()
    if args.command == "check-coverage":
        return check_coverage(args.unit, args.integration)
    if args.command == "metrics":
        return generate_metrics(args.output, args.sources)
    generate_index(args.output, args.unit, args.integration)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

