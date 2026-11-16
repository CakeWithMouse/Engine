"""Reject fractional and out-of-range integer parameters before narrowing conversions."""
import argparse
import json
from pathlib import Path
import subprocess


def suite(tool: Path, output: Path) -> None:
    output.mkdir(parents=True, exist_ok=False)
    cases = [
        ("simplify-fractional-slices", "mesh.simplify", {"slices": 24.5}),
        ("simplify-huge-slices", "mesh.simplify", {"slices": 1e30}),
        ("simplify-fractional-stacks", "mesh.simplify", {"stacks": 2.5}),
        ("particles-huge-width", "render.particles", {"width": 1e30}),
        ("particles-fractional-width", "render.particles", {"width": 480.5}),
        ("particles-huge-count", "render.particles", {"particleCount": 1e30}),
        ("particles-fractional-count", "render.particles", {"particleCount": 64.5}),
        ("particles-fractional-render-mode", "render.particles", {"rendering": 0.5}),
        ("particles-huge-steps", "render.particles", {"frames": [{"steps": 1e30}]}),
        ("particles-fractional-steps", "render.particles", {"frames": [{"steps": 1.5}]}),
        ("mesh-huge-width", "render.mesh", {"width": 1e30}),
        ("shadow-huge-width", "render.point-shadows", {"width": 1e30}),
        ("shadow-fractional-render-mode", "render.point-shadows", {"rendering": 0.5}),
    ]
    results = []
    for name, command, request in cases:
        request_path = output / f"{name}.request.json"
        result_path = output / f"{name}.result.json"
        request_path.write_text(json.dumps(request), encoding="utf-8")
        process = subprocess.run(
            [str(tool), command, str(request_path), str(result_path)],
            capture_output=True, timeout=120,
        )
        (output / f"{name}.log").write_bytes(process.stdout + process.stderr)
        result = json.loads(result_path.read_text(encoding="utf-8-sig"))
        assert process.returncode == 2 and not result.get("ok", False), (name, process.returncode, result)
        assert "integer field" in result.get("error", ""), (name, result)
        results.append({"name": name, "command": command, "error": result["error"]})

    summary = {"passed": len(results), "cases": results,
               "result": "Fractional and huge integer inputs are rejected before int conversion."}
    (output / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(json.dumps({"passed": len(results), "result": summary["result"]}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tool", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    suite(args.tool.resolve(strict=True), args.output.resolve())
