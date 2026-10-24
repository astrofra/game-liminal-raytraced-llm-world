"""Generate the native profile table from the same validated download manifest."""
import argparse
import json
from pathlib import Path
from model_profiles import load_profiles


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    manifest = load_profiles()
    lines = ["// Generated from scripts/ministral_models.json. Do not edit.", "#pragma once",
             "struct LiminalModelProfile { const char* name; const char* path; const char* revision; const char* sha256; bool reasoning; float temperature; int top_k; float top_p; };",
             "static const LiminalModelProfile kLiminalModelProfiles[] = {"]
    for name, profile in manifest["profiles"].items():
        fields = [json.dumps(value) for value in (name, profile["relative_path"], profile["revision"], profile["sha256"])]
        fields += [str(profile["reasoning"]).lower(), f"{float(profile['temperature'])}f",
                   str(profile["top_k"]), f"{float(profile['top_p'])}f"]
        lines.append("    {" + ", ".join(fields) + "},")
    lines += ["};", "static const char* kLiminalDefaultProfile = " + json.dumps(manifest["default_profile"]) + ";"]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
