#!/usr/bin/env python3
"""品質メトリクスの閾値検証とHTMLレポート生成。"""

from __future__ import annotations

import argparse
import gzip
import html
import json
import subprocess
import sys
from pathlib import Path

QUALITY_POLICY = json.loads(
    (Path(__file__).with_name("quality-policy.json")).read_text(encoding="utf-8")
)
UNIT_BRANCH_MIN = float(QUALITY_POLICY["unit_branch_min"])
INTEGRATION_BRANCH_MIN = float(QUALITY_POLICY["integration_branch_min"])
CC_MAX = float(QUALITY_POLICY["cc_max"])
MI_MIN = float(QUALITY_POLICY["mi_min"])


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


def percent(covered: int, total: int) -> float:
    return 100.0 if total == 0 else (covered * 100.0) / total


def c_module_name(file_name: str) -> str:
    parts = Path(file_name).parts
    if len(parts) >= 2 and parts[0] == "Utility":
        return parts[1]
    return parts[0] if parts else "unknown"


def c_module_rows(rows: list[dict]) -> list[dict]:
    modules: dict[str, dict] = {}
    for row in rows:
        name = c_module_name(row["file"])
        module = modules.setdefault(
            name,
            {
                "module": name,
                "lines_covered": 0,
                "lines_total": 0,
                "branches_covered": 0,
                "branches_total": 0,
            },
        )
        for key in (
            "lines_covered",
            "lines_total",
            "branches_covered",
            "branches_total",
        ):
            module[key] += row[key]
    for module in modules.values():
        module["line_percent"] = percent(
            module["lines_covered"], module["lines_total"]
        )
        module["branch_percent"] = percent(
            module["branches_covered"], module["branches_total"]
        )
    return sorted(modules.values(), key=lambda module: module["module"])


def c_source_rows(paths: list[Path]) -> tuple[list[dict], dict]:
    rows: list[dict] = []
    totals = {
        "lines_covered": 0,
        "lines_total": 0,
        "branches_covered": 0,
        "branches_total": 0,
    }
    for path in paths:
        with gzip.open(path, "rt", encoding="utf-8") as stream:
            report = json.load(stream)
        for source in report.get("files", []):
            line_total = 0
            line_covered = 0
            branch_total = 0
            branch_covered = 0
            line_details = {}
            for line in source.get("lines", []):
                line_total += 1
                line_covered += int(line.get("count", 0) > 0)
                branches = line.get("branches", [])
                branch_total += len(branches)
                covered_branches = sum(
                    int(branch.get("count", 0) > 0) for branch in branches
                )
                branch_covered += covered_branches
                line_details[int(line["line_number"])] = {
                    "count": int(line.get("count", 0)),
                    "branches_covered": covered_branches,
                    "branches_total": len(branches),
                }
            row = {
                "file": source["file"],
                "detail_file": f"{Path(source['file']).name}.html",
                "lines_covered": line_covered,
                "lines_total": line_total,
                "line_percent": percent(line_covered, line_total),
                "branches_covered": branch_covered,
                "branches_total": branch_total,
                "branch_percent": percent(branch_covered, branch_total),
                "line_details": line_details,
            }
            rows.append(row)
            for key in totals:
                totals[key] += row[key]
    totals["line_percent"] = percent(
        totals["lines_covered"], totals["lines_total"]
    )
    totals["branch_percent"] = percent(
        totals["branches_covered"], totals["branches_total"]
    )
    return rows, totals


def generate_c_source_html(output: Path, row: dict) -> None:
    source_path = Path(row["file"])
    source_lines = source_path.read_text(encoding="utf-8").splitlines()
    output.parent.mkdir(parents=True, exist_ok=True)
    table_rows = []
    for number, source_line in enumerate(source_lines, 1):
        detail = row["line_details"].get(number)
        if detail is None:
            css_class = "neutral"
            count = "-"
            branches = "-"
        else:
            css_class = "covered" if detail["count"] > 0 else "uncovered"
            count = str(detail["count"])
            branches = (
                f"{detail['branches_covered']}/{detail['branches_total']}"
                if detail["branches_total"] > 0
                else "-"
            )
        table_rows.append(
            "<tr class='{css}'><td class='number'>{number}</td>"
            "<td class='count'>{count}</td><td class='branches'>{branches}</td>"
            "<td><pre>{source}</pre></td></tr>".format(
                css=css_class,
                number=number,
                count=count,
                branches=branches,
                source=html.escape(source_line),
            )
        )

    output.write_text(
        f"""<!doctype html>
<html lang="ja"><head><meta charset="utf-8">
<title>{html.escape(row["file"])} Coverage</title>
<style>
body{{font-family:sans-serif;margin:2rem;color:#222}}table{{border-collapse:collapse;width:100%}}
th,td{{border:1px solid #ccc;padding:.2rem .45rem;text-align:left;vertical-align:top}}
th{{background:#eee}}pre{{font-family:monospace;margin:0;white-space:pre-wrap}}
.number,.count,.branches{{width:5rem;text-align:right}}.covered{{background:#e4f6e8}}
.uncovered{{background:#ffdede}}.neutral{{background:#f5f5f5;color:#666}}
</style></head><body>
<h1>{html.escape(row["file"])}</h1>
<p><a href="index.html">C Coverage一覧へ戻る</a></p>
<p>Line: {row["line_percent"]:.2f}% ({row["lines_covered"]}/{row["lines_total"]}) /
Branch: {row["branch_percent"]:.2f}% ({row["branches_covered"]}/{row["branches_total"]})</p>
<table><thead><tr><th>Line</th><th>実行回数</th><th>分岐</th><th>Source</th></tr></thead>
<tbody>{"".join(table_rows)}</tbody></table>
</body></html>
""",
        encoding="utf-8",
    )


def generate_c_coverage(
    output: Path, html_output: Path, test_status: str, inputs: list[Path]
) -> int:
    rows, totals = c_source_rows(inputs)
    modules = c_module_rows(rows)
    summary_rows = [
        {key: value for key, value in row.items() if key != "line_details"}
        for row in rows
    ]
    summary = {
        "test_status": test_status,
        "tests_total": 1,
        "tests_passed": int(test_status == "passed"),
        **totals,
        "modules": modules,
        "files": summary_rows,
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(summary, indent=2), encoding="utf-8")

    table_rows = []
    for row in rows:
        generate_c_source_html(html_output.parent / row["detail_file"], row)
        table_rows.append(
            "<tr><td><a href='{detail}'>{file}</a></td>"
            "<td>{lc}/{lt}</td><td>{lp:.2f}%</td>"
            "<td>{bc}/{bt}</td><td>{bp:.2f}%</td></tr>".format(
                detail=html.escape(row["detail_file"]),
                file=html.escape(row["file"]),
                lc=row["lines_covered"],
                lt=row["lines_total"],
                lp=row["line_percent"],
                bc=row["branches_covered"],
                bt=row["branches_total"],
                bp=row["branch_percent"],
            )
        )
    module_table_rows = []
    for module in modules:
        module_ok = module["branch_percent"] >= UNIT_BRANCH_MIN
        module_table_rows.append(
            "<tr><td>{module}</td><td>{lc}/{lt}</td><td>{lp:.2f}%</td>"
            "<td>{bc}/{bt}</td><td class='{css}'>{bp:.2f}%</td>"
            "<td class='{css}'>{status}</td></tr>".format(
                module=html.escape(module["module"]),
                lc=module["lines_covered"],
                lt=module["lines_total"],
                lp=module["line_percent"],
                bc=module["branches_covered"],
                bt=module["branches_total"],
                bp=module["branch_percent"],
                css="ok" if module_ok else "ng",
                status="OK" if module_ok else "NG",
            )
        )
    branch_ok = all(
        module["branch_percent"] >= UNIT_BRANCH_MIN for module in modules
    )
    test_ok = test_status == "passed"
    html_output.write_text(
        f"""<!doctype html>
<html lang="ja"><head><meta charset="utf-8"><title>C Utility Coverage</title>
<style>
body{{font-family:sans-serif;margin:2rem;color:#222}}table{{border-collapse:collapse;width:100%}}
th,td{{border:1px solid #bbb;padding:.45rem;text-align:left}}th{{background:#eee}}
.ok{{color:#176b2c;font-weight:bold}}.ng{{color:#a40000;font-weight:bold}}
</style></head><body>
<h1>C Utility単体テスト・Coverage</h1>
<p>テスト: <span class="{"ok" if test_ok else "ng"}">{html.escape(test_status.upper())}</span></p>
<p>全体Line coverage: {totals["line_percent"]:.2f}%（参考値）</p>
<p>全体Branch coverage: {totals["branch_percent"]:.2f}%（参考値）</p>
<p>合否はUtility module単位で判定する。各moduleのBranch coverageが
{UNIT_BRANCH_MIN:.0f}%以上であること。</p>
<table><thead><tr><th>Module</th><th>Lines</th><th>Line %</th>
<th>Branches</th><th>Branch %</th><th>判定</th></tr></thead><tbody>
{"".join(module_table_rows)}
</tbody></table>
<h2>Source別</h2>
<table><thead><tr><th>Source</th><th>Lines</th><th>Line %</th>
<th>Branches</th><th>Branch %</th></tr></thead><tbody>
{"".join(table_rows)}
</tbody></table></body></html>
""",
        encoding="utf-8",
    )
    print(
        f"C Utility line: {totals['line_percent']:.2f}% [参考値]"
    )
    print(f"C Utility branch total: {totals['branch_percent']:.2f}% [参考値]")
    for module in modules:
        module_ok = module["branch_percent"] >= UNIT_BRANCH_MIN
        print(
            f"C Utility {module['module']} branch: "
            f"{module['branch_percent']:.2f}% / "
            f"単体共通基準 {UNIT_BRANCH_MIN:.2f}% "
            f"[{'OK' if module_ok else 'NG'}]"
        )
    print(f"C Utility test: {test_status.upper()}")
    return int(not (branch_ok and test_ok))


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


def generate_index(
    output: Path, unit: Path, integration: Path, c_coverage: Path
) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    unit_value = branch_percent(unit)
    integration_value = branch_percent(integration)
    c_summary = load_json(c_coverage)
    c_test_status = str(c_summary["test_status"]).upper()
    c_line_value = float(c_summary["line_percent"])
    c_branch_value = float(c_summary["branch_percent"])
    c_module_lines = "<br>".join(
        "{module}: {branch:.2f}%（基準 {minimum:.0f}%）".format(
            module=html.escape(str(module["module"])),
            branch=float(module["branch_percent"]),
            minimum=UNIT_BRANCH_MIN,
        )
        for module in c_summary.get("modules", [])
    )
    output.write_text(
        f"""<!doctype html>
<html lang="ja"><head><meta charset="utf-8"><title>品質レポート</title>
<style>
body{{font-family:sans-serif;margin:2rem;color:#222}}.cards{{display:flex;gap:1rem;flex-wrap:wrap}}
.card{{border:1px solid #bbb;border-radius:.5rem;padding:1rem;min-width:15rem}}
.value{{font-size:2rem;font-weight:bold}}a{{color:#075ea8}}
</style></head><body>
<h1>共通Utility 品質レポート</h1>
<div class="cards">
<section class="card"><h2>単体テスト C1</h2><div class="value">{unit_value:.2f}%</div>
<p>基準: {UNIT_BRANCH_MIN:.0f}%以上</p><a href="coverage/unit/html/index.html">詳細を見る</a></section>
<section class="card"><h2>結合テスト C1</h2><div class="value">{integration_value:.2f}%</div>
<p>基準: {INTEGRATION_BRANCH_MIN:.0f}%以上</p><a href="coverage/integration/html/index.html">詳細を見る</a></section>
<section class="card"><h2>C Utility単体テスト</h2><div class="value">{c_test_status}</div>
<p>全体Line: {c_line_value:.2f}%（参考値）<br>
全体Branch: {c_branch_value:.2f}%（参考値）</p>
<p>{c_module_lines}</p>
<a href="coverage/utility-c/html/index.html">Cテスト・Coverageを見る</a></section>
<section class="card"><h2>静的メトリクス</h2><p>CC上限: {CC_MAX:.0f}<br>MI下限: {MI_MIN:.0f}</p>
<a href="metrics/index.html">CC・MI一覧を見る</a></section>
<section class="card"><h2>C API・Test仕様書</h2>
<p>Header、production、単体テスト、fuzzから自動生成</p>
<a href="../docs/c-api/html/index.html">Doxygen仕様書を見る</a></section>
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

    c_coverage = subparsers.add_parser("c-coverage")
    c_coverage.add_argument("--output", type=Path, required=True)
    c_coverage.add_argument("--html", type=Path, required=True)
    c_coverage.add_argument(
        "--test-status", choices=("passed", "failed"), required=True
    )
    c_coverage.add_argument("inputs", nargs="+", type=Path)

    index = subparsers.add_parser("index")
    index.add_argument("--output", type=Path, required=True)
    index.add_argument("--unit", type=Path, required=True)
    index.add_argument("--integration", type=Path, required=True)
    index.add_argument("--c-coverage", type=Path, required=True)

    args = parser.parse_args()
    if args.command == "check-coverage":
        return check_coverage(args.unit, args.integration)
    if args.command == "metrics":
        return generate_metrics(args.output, args.sources)
    if args.command == "c-coverage":
        return generate_c_coverage(
            args.output, args.html, args.test_status, args.inputs
        )
    generate_index(
        args.output, args.unit, args.integration, args.c_coverage
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
