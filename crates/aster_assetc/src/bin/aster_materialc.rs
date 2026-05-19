// Author: Faruk Alpay
// Do not remove this notice.

use aster_content::{cook_material_asset, material_inspect_report_json};
use std::path::{Path, PathBuf};

fn usage() -> &'static str {
    "usage:
  aster_materialc inspect --input <file.astermat> [--asset-root <dir>]
  aster_materialc package --input <file.astermat> --output <dir> [--asset-root <dir>] [--platform desktop]"
}

fn value_after(args: &[String], name: &str) -> Option<String> {
    args.windows(2)
        .find(|window| window[0] == name)
        .map(|window| window[1].clone())
}

fn fallback_id(input: &Path) -> String {
    input
        .file_stem()
        .and_then(|value| value.to_str())
        .unwrap_or("material")
        .to_string()
}

fn default_asset_root(input: &Path) -> PathBuf {
    input
        .parent()
        .and_then(Path::parent)
        .map(Path::to_path_buf)
        .unwrap_or_default()
}

fn inspect_command(args: &[String]) -> Result<(), String> {
    let input = value_after(args, "--input")
        .map(PathBuf::from)
        .ok_or_else(|| "inspect requires --input <file.astermat>".to_string())?;
    let asset_root = value_after(args, "--asset-root")
        .map(PathBuf::from)
        .unwrap_or_else(|| default_asset_root(&input));
    let report =
        material_inspect_report_json(&input, &asset_root).map_err(|error| error.to_string())?;
    println!("{report}");
    Ok(())
}

fn package_command(args: &[String]) -> Result<(), String> {
    let input = value_after(args, "--input")
        .map(PathBuf::from)
        .ok_or_else(|| "package requires --input <file.astermat>".to_string())?;
    let output = value_after(args, "--output")
        .map(PathBuf::from)
        .ok_or_else(|| "package requires --output <dir>".to_string())?;
    let asset_root = value_after(args, "--asset-root")
        .map(PathBuf::from)
        .unwrap_or_else(|| default_asset_root(&input));
    let platform = value_after(args, "--platform").unwrap_or_else(|| "desktop".to_string());
    let result = cook_material_asset(
        &input,
        &asset_root,
        &output,
        &fallback_id(&input),
        &platform,
        None,
    )
    .map_err(|error| error.to_string())?;
    let error_count = result
        .material_bin
        .diagnostics
        .iter()
        .filter(|diagnostic| diagnostic.severity == "error")
        .count();
    println!(
        "material package {} report={} runtime_outputs={} textures={} bindings={} diagnostics={}",
        input.display(),
        result.report_path.display(),
        result.emitted_runtime_outputs,
        result.material_bin.textures.len(),
        result.material_bin.binding_layout.len(),
        result.material_bin.diagnostics.len()
    );
    if let Some(path) = &result.material_bin_path {
        println!("materialbin={}", path.display());
    }
    if let Some(path) = &result.preview_path {
        println!("preview={}", path.display());
    }
    if error_count > 0 {
        return Err(format!(
            "strict material package failed with {error_count} error(s)"
        ));
    }
    Ok(())
}

fn run() -> Result<(), String> {
    let args: Vec<String> = std::env::args().collect();
    match args.get(1).map(String::as_str) {
        Some("inspect") => inspect_command(&args[2..]),
        Some("package") => package_command(&args[2..]),
        Some("--help") | Some("-h") | None => {
            println!("{}", usage());
            Ok(())
        }
        Some(command) => Err(format!("unknown command '{command}'\n{}", usage())),
    }
}

fn main() {
    if let Err(error) = run() {
        eprintln!("aster_materialc: {error}");
        std::process::exit(1);
    }
}
