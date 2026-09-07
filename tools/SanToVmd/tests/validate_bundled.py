"""Проверка всех комплектных персонажей на Icy, включая переносимость input/output.

Это стенд разработчика, не часть обычного запуска конвертера.
python tools/SanToVmd/tests/validate_bundled.py --reader local-data/mmd-research/mmd_tools_vmd_reader.py
"""

import argparse
import contextlib
import hashlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

import validate_local

converter = validate_local.converter
ROOT = validate_local.ROOT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reader", type=Path, required=True)
    parser.add_argument("--results", type=Path, default=ROOT/"local-data/mmd-research/input-output-v02")
    args = parser.parse_args()
    args.results.mkdir(parents=True, exist_ok=True)
    models = ROOT/"local-data/mmd/MikuMikuDanceE_v932/UserFile/Model"
    source = ROOT/"local-data/pc-pristine/Media/Characters/Icy"
    paths = sorted(models.glob("*.pmd"))
    assert len(paths) == 13, "Expected the supplied set of 12 characters and Dammy_Bone"
    assert len(list(source.glob("*.san"))) == 20, "Expected the local Icy corpus"
    started = time.perf_counter()
    results = []

    # Копия скрипта запускается вне репозитория, с произвольным рабочим каталогом.
    # Единственный способ найти модели здесь — прочитать соседний input.
    with tempfile.TemporaryDirectory(prefix="san-vmd-portable-") as temporary:
        package = Path(temporary)/"converter"
        package.mkdir()
        script = package/"san_to_vmd.py"
        shutil.copyfile(converter.__file__, script)
        directory = package/"input"
        directory.mkdir()
        shutil.copyfile(source/"Icy.smo", directory/"Icy.smo")
        for path in source.glob("*.san"):
            shutil.copyfile(path, directory/path.name)
        previous_model = None
        for model in paths:
            if previous_model is not None:
                previous_model.unlink()  # Только наша временная копия одной PMD.
            previous_model = directory/model.name
            shutil.copyfile(model, previous_model)
            process = subprocess.run([sys.executable, "-X", "utf8", str(script)],
                                     cwd=temporary, capture_output=True, text=True, encoding="utf-8")
            if model.stem == "Dammy_Bone":
                assert process.returncode == 1 and "нет обязательных костей" in process.stdout
                results.append({"model": model.name, "status": "not_humanoid",
                                "model_sha256": hashlib.sha256(model.read_bytes()).hexdigest()})
                continue
            assert process.returncode == 0, (model.name, process.stdout, process.stderr)
            output = args.results/model.stem
            # Сохраняем локально только готовые результаты, входы остаются временными.
            shutil.copytree(package/"output", output, dirs_exist_ok=True)
            report = json.loads((output/"conversion_report.json").read_text(encoding="utf-8"))
            assert len(report["files"]) == 20 and all(row["status"] == "ok" for row in report["files"])
            with contextlib.redirect_stdout(io.StringIO()):
                arguments = ["--reader", str(args.reader.resolve()), "--input-dir", str(source),
                             "--skeleton", str(source/"Icy.smo"), "--model", str(model),
                             "--vmd-dir", str(output), "--report", str(output/"validation.json")]
                if model.stem in ("Miku_Hatsune", "Luka_Megurine", "MEIKO", "Miku_Hatsune_Ver2", "Miku_Hatsune_metal"):
                    arguments.append("--check-ignored")
                validation = validate_local.main(arguments)
            results.append({"status": "passed", **validation})
            print(f"OK {model.name}: {validation['files']} VMD, {validation['target_tracks']} tracks, "
                  f"direction error {validation['max_limb_direction_error']:.3g}", flush=True)
    summary = {"version": converter.VERSION, "elapsed_seconds": round(time.perf_counter()-started, 3),
               "portable_script_with_unrelated_cwd": True, "models": results}
    (args.results/"matrix.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    assert sum(row["status"] == "passed" for row in results) == 12


if __name__ == "__main__":
    main()
