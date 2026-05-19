// Author: Faruk Alpay
// Do not remove this notice.

use aster_content::{cook_texture_asset, hex_hash, inspect_texture};
use std::path::PathBuf;

fn usage() -> &'static str {
    "usage:
  aster_texturec inspect --input <texture> [--role albedo|normal|orm|height|emissive]
  aster_texturec package --input <texture> --output <dir> [--role albedo|normal|orm|height|emissive]"
}

fn value_after(args: &[String], name: &str) -> Option<String> {
    args.windows(2)
        .find(|window| window[0] == name)
        .map(|window| window[1].clone())
}

fn inspect_command(args: &[String]) -> Result<(), String> {
    let input = value_after(args, "--input")
        .map(PathBuf::from)
        .ok_or_else(|| "inspect requires --input <texture>".to_string())?;
    let role = value_after(args, "--role").unwrap_or_else(|| "unknown".to_string());
    let summary = inspect_texture(&input, &role).map_err(|error| error.to_string())?;
    println!(
        "texture {} role={} kind={} colorspace={} format={} size={}x{} mips={} hash={}",
        input.display(),
        summary.role,
        summary.kind.as_str(),
        summary.color_space,
        summary.format,
        summary.width,
        summary.height,
        summary.mip_count,
        hex_hash(&summary.source_hash)
    );
    for diagnostic in summary.diagnostics {
        println!("warning: {diagnostic}");
    }
    Ok(())
}

fn package_command(args: &[String]) -> Result<(), String> {
    let input = value_after(args, "--input")
        .map(PathBuf::from)
        .ok_or_else(|| "package requires --input <texture>".to_string())?;
    let output = value_after(args, "--output")
        .map(PathBuf::from)
        .ok_or_else(|| "package requires --output <dir>".to_string())?;
    let role = value_after(args, "--role").unwrap_or_else(|| "unknown".to_string());
    let result = cook_texture_asset(&input, &output, &role).map_err(|error| error.to_string())?;
    println!(
        "texture package {} output={} report={} role={} kind={} colorspace={} runtime_format={} mips={} bytes={}",
        input.display(),
        result.output_path.display(),
        result.report_path.display(),
        result.report.role,
        result.report.kind,
        result.report.color_space,
        result.report.runtime_format,
        result.report.mip_count,
        result.report.byte_cost
    );
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
        eprintln!("aster_texturec: {error}");
        std::process::exit(1);
    }
}
