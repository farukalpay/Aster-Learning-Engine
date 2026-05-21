// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

use serde_json::{json, Value};
use std::collections::{BTreeMap, BTreeSet};
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

const CORE_SPDX: &str = "Apache-2.0";
const CONTENT_SPDX: &str = "LicenseRef-Aster-Content";
const COPYRIGHT: &str = "Copyright (c) 2026 Faruk Alpay";

#[derive(Clone, Debug)]
struct SourceFile {
    path: PathBuf,
    relative: String,
    comment: &'static str,
    license: &'static str,
}

#[derive(Clone, Debug)]
struct HeaderFinding {
    severity: &'static str,
    code: &'static str,
    path: String,
    message: String,
}

#[derive(Clone, Debug, Default)]
struct HeaderSummary {
    checked_files: usize,
    compliant_files: usize,
    changed_files: usize,
    old_notice_files: usize,
    missing_spdx_files: usize,
    wrong_license_files: usize,
    error_count: usize,
}

#[derive(Clone, Debug)]
struct HeaderReport {
    repo: PathBuf,
    write: bool,
    summary: HeaderSummary,
    changed_files: Vec<String>,
    findings: Vec<HeaderFinding>,
}

pub fn agent_audit_report_json(repo: &Path) -> Result<String, String> {
    let root = canonical_repo(repo)?;
    let files = collect_repo_files(&root)?;
    let source_files = header_source_files(&root, &files);
    let header_report = inspect_headers(&root, &source_files, false)?;
    let dirty = git_status_short(&root);
    let mut by_license = BTreeMap::<String, usize>::new();
    for file in &source_files {
        *by_license.entry(file.license.to_string()).or_default() += 1;
    }
    let report = json!({
        "schema_version": 1,
        "kind": "aster_agent_audit",
        "repo": normalize_path(&root),
        "summary": {
            "repo_files": files.len(),
            "source_files": source_files.len(),
            "header_errors": header_report.summary.error_count,
            "dirty_entries": dirty.lines().filter(|line| !line.trim().is_empty()).count(),
            "license_partitions": by_license,
        },
        "header_policy": header_policy_json(),
        "dirty_worktree": dirty,
        "ownership_boundaries": ownership_boundaries(),
        "findings": header_findings_json(&header_report.findings),
    });
    serde_json::to_string_pretty(&report).map_err(|error| error.to_string())
}

pub fn agent_audit_report_markdown(repo: &Path) -> Result<String, String> {
    let root = canonical_repo(repo)?;
    let files = collect_repo_files(&root)?;
    let source_files = header_source_files(&root, &files);
    let header_report = inspect_headers(&root, &source_files, false)?;
    let dirty = git_status_short(&root);
    let mut out = String::new();
    out.push_str("# Aster Agent Audit\n\n");
    out.push_str(&format!("- Repo: `{}`\n", normalize_path(&root)));
    out.push_str(&format!("- Files: {}\n", files.len()));
    out.push_str(&format!("- Source files: {}\n", source_files.len()));
    out.push_str(&format!(
        "- Header errors: {}\n",
        header_report.summary.error_count
    ));
    out.push_str(&format!(
        "- Dirty entries: {}\n\n",
        dirty.lines().filter(|line| !line.trim().is_empty()).count()
    ));
    out.push_str("## Header Policy\n\n");
    out.push_str("- Core source: `Apache-2.0`\n");
    out.push_str("- Content/brand: `LicenseRef-Aster-Content`\n");
    out.push_str("- Required source header: SPDX plus copyright\n\n");
    if !dirty.trim().is_empty() {
        out.push_str("## Dirty Worktree\n\n```text\n");
        out.push_str(&dirty);
        out.push_str("\n```\n\n");
    }
    append_findings_markdown(&mut out, &header_report.findings);
    Ok(out)
}

pub fn agent_fix_headers_report_json(repo: &Path, write: bool) -> Result<String, String> {
    let root = canonical_repo(repo)?;
    let files = collect_repo_files(&root)?;
    let source_files = header_source_files(&root, &files);
    let report = inspect_headers(&root, &source_files, write)?;
    let output = json!({
        "schema_version": 1,
        "kind": "aster_agent_fix_headers",
        "repo": normalize_path(&report.repo),
        "write": report.write,
        "summary": {
            "checked_files": report.summary.checked_files,
            "compliant_files": report.summary.compliant_files,
            "changed_files": report.summary.changed_files,
            "old_notice_files": report.summary.old_notice_files,
            "missing_spdx_files": report.summary.missing_spdx_files,
            "wrong_license_files": report.summary.wrong_license_files,
            "error_count": report.summary.error_count,
        },
        "changed_files": report.changed_files,
        "findings": header_findings_json(&report.findings),
    });
    if !write && report.summary.error_count > 0 {
        return Err(serde_json::to_string_pretty(&output).map_err(|error| error.to_string())?);
    }
    serde_json::to_string_pretty(&output).map_err(|error| error.to_string())
}

pub fn agent_native_audit_report_json(repo: &Path) -> Result<String, String> {
    let root = canonical_repo(repo)?;
    let files = collect_repo_files(&root)?;
    let mut dialects = BTreeMap::<String, usize>::new();
    let mut include_edges = 0usize;
    let mut public_symbols = 0usize;
    let mut exported_symbols = 0usize;
    let mut old_notice_files = Vec::<String>::new();
    let mut modules = BTreeMap::<String, usize>::new();
    for path in files.iter().filter(|path| native_dialect(path).is_some()) {
        let dialect = native_dialect(path).unwrap_or("unknown");
        *dialects.entry(dialect.to_string()).or_default() += 1;
        let rel = normalize_relative(&root, path);
        if let Some(first) = rel.split('/').next() {
            *modules.entry(first.to_string()).or_default() += 1;
        }
        let Ok(text) = fs::read_to_string(path) else {
            continue;
        };
        if contains_legacy_notice(&text) {
            old_notice_files.push(rel);
        }
        include_edges += text
            .lines()
            .filter(|line| line.trim_start().starts_with("#include "))
            .count();
        public_symbols += text
            .lines()
            .filter(|line| {
                let trimmed = line.trim_start();
                trimmed.starts_with("pub ")
                    || trimmed.starts_with("class ")
                    || trimmed.starts_with("struct ")
                    || trimmed.starts_with("enum class ")
            })
            .count();
        exported_symbols += text.matches("ASTER_KERNEL_API").count();
    }
    let report = json!({
        "schema_version": 1,
        "kind": "aster_agent_native_audit",
        "repo": normalize_path(&root),
        "summary": {
            "native_files": dialects.values().sum::<usize>(),
            "dialects": dialects,
            "modules": modules,
            "include_edges": include_edges,
            "public_symbol_candidates": public_symbols,
            "exported_symbol_candidates": exported_symbols,
            "old_notice_files": old_notice_files.len(),
        },
        "findings": old_notice_files.into_iter().map(|path| {
            json!({
                "severity": "error",
                "code": "old-author-notice",
                "path": path,
                "message": "file still contains the legacy author notice"
            })
        }).collect::<Vec<_>>(),
    });
    serde_json::to_string_pretty(&report).map_err(|error| error.to_string())
}

pub fn agent_native_audit_report_markdown(repo: &Path) -> Result<String, String> {
    let json_text = agent_native_audit_report_json(repo)?;
    let value: Value = serde_json::from_str(&json_text).map_err(|error| error.to_string())?;
    let summary = &value["summary"];
    let mut out = String::new();
    out.push_str("# Aster Native Audit\n\n");
    out.push_str(&format!(
        "- Repo: `{}`\n",
        value["repo"].as_str().unwrap_or("")
    ));
    out.push_str(&format!(
        "- Native files: {}\n",
        summary["native_files"].as_u64().unwrap_or(0)
    ));
    out.push_str(&format!(
        "- Include edges: {}\n",
        summary["include_edges"].as_u64().unwrap_or(0)
    ));
    out.push_str(&format!(
        "- Public symbol candidates: {}\n",
        summary["public_symbol_candidates"].as_u64().unwrap_or(0)
    ));
    out.push_str(&format!(
        "- Exported symbol candidates: {}\n\n",
        summary["exported_symbol_candidates"].as_u64().unwrap_or(0)
    ));
    out.push_str("## Dialects\n\n");
    if let Some(dialects) = summary["dialects"].as_object() {
        for (name, count) in dialects {
            out.push_str(&format!("- `{}`: {}\n", name, count.as_u64().unwrap_or(0)));
        }
    }
    out.push('\n');
    append_json_findings_markdown(&mut out, &value["findings"]);
    Ok(out)
}

pub fn agent_runtime_audit_report_json(repo: &Path) -> Result<String, String> {
    let root = canonical_repo(repo)?;
    let files = collect_repo_files(&root)?;
    let mut components = BTreeMap::<String, BTreeSet<String>>::new();
    let signals = [
        ("agent-contract", ["agent", "handoff", "batch", "runbook"]),
        ("tool-surface", ["command", "assetc", "tool", "policy"]),
        (
            "evidence-ledger",
            ["forensics", "audit", "report", "evidence"],
        ),
        (
            "review-gate",
            ["review", "validation", "gate", "diagnostic"],
        ),
        (
            "patch-engine",
            ["patch", "transform", "diff", "changed_files"],
        ),
    ];
    for path in files.iter().filter(|path| text_extension(path)) {
        let Ok(text) = fs::read_to_string(path) else {
            continue;
        };
        let lower = text.to_ascii_lowercase();
        let rel = normalize_relative(&root, path);
        for (component, words) in signals {
            if words.iter().any(|word| lower.contains(word)) {
                components
                    .entry(component.to_string())
                    .or_default()
                    .insert(rel.clone());
            }
        }
    }
    let report = json!({
        "schema_version": 1,
        "kind": "aster_agent_runtime_audit",
        "repo": normalize_path(&root),
        "summary": {
            "components": components.len(),
            "component_files": components.values().map(BTreeSet::len).sum::<usize>(),
        },
        "components": components.into_iter().map(|(id, files)| {
            json!({
                "id": id,
                "owner": "aster",
                "files": files.into_iter().collect::<Vec<_>>(),
            })
        }).collect::<Vec<_>>(),
    });
    serde_json::to_string_pretty(&report).map_err(|error| error.to_string())
}

pub fn agent_runtime_audit_report_markdown(repo: &Path) -> Result<String, String> {
    let json_text = agent_runtime_audit_report_json(repo)?;
    let value: Value = serde_json::from_str(&json_text).map_err(|error| error.to_string())?;
    let mut out = String::new();
    out.push_str("# Aster Runtime Audit\n\n");
    out.push_str(&format!(
        "- Repo: `{}`\n",
        value["repo"].as_str().unwrap_or("")
    ));
    out.push_str(&format!(
        "- Components: {}\n\n",
        value["summary"]["components"].as_u64().unwrap_or(0)
    ));
    if let Some(components) = value["components"].as_array() {
        for component in components {
            out.push_str(&format!(
                "## {}\n\n",
                component["id"].as_str().unwrap_or("component")
            ));
            if let Some(files) = component["files"].as_array() {
                for path in files.iter().take(12) {
                    out.push_str(&format!("- `{}`\n", path.as_str().unwrap_or("")));
                }
                if files.len() > 12 {
                    out.push_str(&format!("- ... {} more\n", files.len() - 12));
                }
            }
            out.push('\n');
        }
    }
    Ok(out)
}

pub fn agent_review_report_json(plan: &Path, repo: &Path) -> Result<String, String> {
    let root = canonical_repo(repo)?;
    let plan_text = fs::read_to_string(plan).map_err(|error| error.to_string())?;
    let audit_text = agent_audit_report_json(&root)?;
    let audit: Value = serde_json::from_str(&audit_text).map_err(|error| error.to_string())?;
    let parsed_plan = serde_json::from_str::<Value>(&plan_text).ok();
    let report = json!({
        "schema_version": 1,
        "kind": "aster_agent_review",
        "repo": normalize_path(&root),
        "plan": {
            "path": normalize_path(plan),
            "bytes": plan_text.len(),
            "hash": hex_u64(hash_bytes(plan_text.as_bytes())),
            "json": parsed_plan.is_some(),
        },
        "audit_summary": audit["summary"].clone(),
        "readiness": if audit["summary"]["header_errors"].as_u64().unwrap_or(0) == 0 {
            "ready"
        } else {
            "needs_attention"
        },
        "notes": [
            "Review packet is dry-run only; it does not write repository files.",
            "Use agent-fix-headers --write for mechanical header remediation.",
            "Use targeted CMake/Cargo validation before handoff."
        ],
    });
    serde_json::to_string_pretty(&report).map_err(|error| error.to_string())
}

pub fn agent_review_report_markdown(plan: &Path, repo: &Path) -> Result<String, String> {
    let json_text = agent_review_report_json(plan, repo)?;
    let value: Value = serde_json::from_str(&json_text).map_err(|error| error.to_string())?;
    let mut out = String::new();
    out.push_str("# Aster Agent Review\n\n");
    out.push_str(&format!(
        "- Repo: `{}`\n",
        value["repo"].as_str().unwrap_or("")
    ));
    out.push_str(&format!(
        "- Plan: `{}`\n",
        value["plan"]["path"].as_str().unwrap_or("")
    ));
    out.push_str(&format!(
        "- Plan hash: `{}`\n",
        value["plan"]["hash"].as_str().unwrap_or("")
    ));
    out.push_str(&format!(
        "- Readiness: `{}`\n",
        value["readiness"].as_str().unwrap_or("unknown")
    ));
    out.push_str(&format!(
        "- Header errors: {}\n",
        value["audit_summary"]["header_errors"]
            .as_u64()
            .unwrap_or(0)
    ));
    Ok(out)
}

fn inspect_headers(root: &Path, files: &[SourceFile], write: bool) -> Result<HeaderReport, String> {
    let mut summary = HeaderSummary {
        checked_files: files.len(),
        ..HeaderSummary::default()
    };
    let mut findings = Vec::<HeaderFinding>::new();
    let mut changed_files = Vec::<String>::new();
    for file in files {
        let Ok(original) = fs::read_to_string(&file.path) else {
            findings.push(HeaderFinding {
                severity: "warning",
                code: "read-failed",
                path: file.relative.clone(),
                message: "could not read source as UTF-8".to_string(),
            });
            continue;
        };
        let legacy = contains_legacy_notice(&original);
        if legacy {
            summary.old_notice_files += 1;
        }
        let expected = expected_header(file.comment, file.license);
        let rewritten = rewrite_header(&original, file.comment, file.license);
        if rewritten == original
            && !legacy
            && starts_with_expected_header(&original, file.comment, file.license)
        {
            summary.compliant_files += 1;
            continue;
        }
        let missing_spdx = !original.contains("SPDX-License-Identifier:");
        let wrong_license = original.contains("SPDX-License-Identifier:")
            && !original.contains(&format!("SPDX-License-Identifier: {}", file.license));
        if missing_spdx {
            summary.missing_spdx_files += 1;
        }
        if wrong_license {
            summary.wrong_license_files += 1;
        }
        summary.error_count += 1;
        findings.push(HeaderFinding {
            severity: "error",
            code: if legacy {
                "legacy-author-notice"
            } else if wrong_license {
                "wrong-spdx-license"
            } else {
                "missing-header"
            },
            path: file.relative.clone(),
            message: format!("expected header `{}`", expected.replace('\n', "\\n")),
        });
        if write {
            fs::write(&file.path, rewritten).map_err(|error| error.to_string())?;
            summary.changed_files += 1;
            changed_files.push(file.relative.clone());
        }
    }
    Ok(HeaderReport {
        repo: root.to_path_buf(),
        write,
        summary,
        changed_files,
        findings,
    })
}

fn rewrite_header(text: &str, comment: &str, license: &str) -> String {
    let mut lines = text.lines().collect::<Vec<_>>();
    let had_trailing_newline = text.ends_with('\n');
    let mut prefix = Vec::<&str>::new();
    if lines.first().is_some_and(|line| line.starts_with("#!")) {
        prefix.push(lines.remove(0));
    }
    while lines.first().is_some_and(|line| {
        let trimmed = line.trim();
        is_legacy_notice_line(trimmed, comment)
            || trimmed.starts_with(&format!("{comment} SPDX-License-Identifier:"))
            || trimmed.starts_with(&format!("{comment} Copyright "))
            || trimmed.is_empty()
    }) {
        if lines.first().is_some_and(|line| {
            let trimmed = line.trim();
            !trimmed.is_empty()
                && !is_legacy_notice_line(trimmed, comment)
                && !trimmed.contains("SPDX-License-Identifier:")
                && !trimmed.contains("Copyright ")
        }) {
            break;
        }
        lines.remove(0);
    }
    let mut out = String::new();
    for line in prefix {
        out.push_str(line);
        out.push('\n');
    }
    out.push_str(&expected_header(comment, license));
    out.push('\n');
    if !lines.is_empty() {
        out.push('\n');
        out.push_str(&lines.join("\n"));
    }
    if had_trailing_newline || !lines.is_empty() {
        out.push('\n');
    }
    out
}

fn starts_with_expected_header(text: &str, comment: &str, license: &str) -> bool {
    let expected = expected_header(comment, license);
    if text.starts_with(&expected) {
        return true;
    }
    let mut lines = text.lines();
    if lines.next().is_some_and(|line| line.starts_with("#!")) {
        return lines.collect::<Vec<_>>().join("\n").starts_with(&expected);
    }
    false
}

fn expected_header(comment: &str, license: &str) -> String {
    format!("{comment} SPDX-License-Identifier: {license}\n{comment} {COPYRIGHT}")
}

fn header_source_files(root: &Path, files: &[PathBuf]) -> Vec<SourceFile> {
    let mut out = Vec::new();
    for path in files {
        let rel = normalize_relative(root, path);
        let license = if is_core_source_path(&rel) {
            CORE_SPDX
        } else if is_content_path(&rel) {
            CONTENT_SPDX
        } else {
            continue;
        };
        let Some(comment) = comment_prefix(path) else {
            continue;
        };
        out.push(SourceFile {
            path: path.clone(),
            relative: rel,
            comment,
            license,
        });
    }
    out
}

fn comment_prefix(path: &Path) -> Option<&'static str> {
    let name = path
        .file_name()
        .and_then(|value| value.to_str())
        .unwrap_or("");
    if name == "CMakeLists.txt" || name.ends_with(".cmake.in") {
        return Some("#");
    }
    match path
        .extension()
        .and_then(|value| value.to_str())
        .unwrap_or("")
    {
        "c" | "cc" | "cpp" | "cxx" | "h" | "hh" | "hpp" | "hxx" | "m" | "mm" | "rs" | "astsl"
        | "astermat" | "swift" => Some("//"),
        "cmake" | "symbols" | "sh" | "ps1" | "toml" => Some("#"),
        _ => None,
    }
}

fn native_dialect(path: &Path) -> Option<&'static str> {
    match path
        .extension()
        .and_then(|value| value.to_str())
        .unwrap_or("")
    {
        "rs" => Some("rust"),
        "c" => Some("c"),
        "cc" | "cpp" | "cxx" | "h" | "hh" | "hpp" | "hxx" | "m" | "mm" => Some("cpp"),
        _ => None,
    }
}

fn text_extension(path: &Path) -> bool {
    matches!(
        path.extension()
            .and_then(|value| value.to_str())
            .unwrap_or(""),
        "rs" | "c"
            | "cc"
            | "cpp"
            | "cxx"
            | "h"
            | "hpp"
            | "mm"
            | "md"
            | "toml"
            | "json"
            | "cmake"
            | "txt"
    ) || path.file_name().and_then(|value| value.to_str()) == Some("CMakeLists.txt")
}

fn is_core_source_path(rel: &str) -> bool {
    rel.starts_with("include/")
        || rel.starts_with("src/")
        || rel.starts_with("apps/")
        || rel.starts_with("tests/")
        || rel.starts_with("crates/")
        || rel.starts_with("shaders/lib/")
        || rel.starts_with("external_app_minimal/")
        || rel.starts_with("abi/")
        || rel.starts_with("cmake/")
        || rel.starts_with("tools/")
        || rel == "Cargo.toml"
        || rel == "CMakeLists.txt"
}

fn is_content_path(rel: &str) -> bool {
    rel.starts_with("projects/")
        || rel.starts_with("showcases/")
        || rel.starts_with("assets/")
        || rel.starts_with("tests/artifacts/")
}

fn collect_repo_files(root: &Path) -> Result<Vec<PathBuf>, String> {
    let mut files = Vec::new();
    collect_repo_files_into(root, root, &mut files)?;
    files.sort();
    Ok(files)
}

fn collect_repo_files_into(
    root: &Path,
    dir: &Path,
    files: &mut Vec<PathBuf>,
) -> Result<(), String> {
    for entry in fs::read_dir(dir).map_err(|error| error.to_string())? {
        let entry = entry.map_err(|error| error.to_string())?;
        let path = entry.path();
        let file_name = path
            .file_name()
            .and_then(|value| value.to_str())
            .unwrap_or("");
        if path.is_dir() {
            if skip_dir(file_name)
                || normalize_relative(root, &path).starts_with("showcases/material_lab/cooked/")
            {
                continue;
            }
            collect_repo_files_into(root, &path, files)?;
        } else if path.is_file() && !skip_file(file_name) {
            files.push(path);
        }
    }
    Ok(())
}

fn skip_dir(name: &str) -> bool {
    name == ".git"
        || name == "target"
        || name == "CMakeFiles"
        || name == ".build"
        || name == "build"
        || name == "cooked"
        || name.starts_with("build-")
}

fn skip_file(name: &str) -> bool {
    name == ".DS_Store"
}

fn contains_legacy_notice(text: &str) -> bool {
    text.lines().any(|line| {
        let trimmed = line.trim_start();
        trimmed.starts_with("// Author:")
            || trimmed.starts_with("# Author:")
            || trimmed.starts_with("// Do not remove")
            || trimmed.starts_with("# Do not remove")
    })
}

fn is_legacy_notice_line(line: &str, comment: &str) -> bool {
    let Some(rest) = line.strip_prefix(comment) else {
        return false;
    };
    let rest = rest.trim();
    (rest.starts_with("Author:") && rest.contains("Faruk Alpay"))
        || (rest.contains("Do not remove") && rest.contains("this notice"))
}

fn canonical_repo(repo: &Path) -> Result<PathBuf, String> {
    repo.canonicalize().map_err(|error| error.to_string())
}

fn git_status_short(root: &Path) -> String {
    Command::new("git")
        .arg("-C")
        .arg(root)
        .arg("status")
        .arg("--short")
        .output()
        .ok()
        .map(|output| {
            String::from_utf8_lossy(&output.stdout)
                .trim_end()
                .to_string()
        })
        .unwrap_or_default()
}

fn normalize_path(path: &Path) -> String {
    path.to_string_lossy().replace('\\', "/")
}

fn normalize_relative(root: &Path, path: &Path) -> String {
    path.strip_prefix(root)
        .unwrap_or(path)
        .to_string_lossy()
        .replace('\\', "/")
}

fn hash_bytes(bytes: &[u8]) -> u64 {
    let mut hash = 1469598103934665603u64;
    for byte in bytes {
        hash ^= u64::from(*byte);
        hash = hash.wrapping_mul(1099511628211u64);
    }
    hash
}

fn hex_u64(value: u64) -> String {
    format!("0x{value:016x}")
}

fn header_policy_json() -> Value {
    json!({
        "core_license": CORE_SPDX,
        "content_license": CONTENT_SPDX,
        "copyright": COPYRIGHT,
        "core_prefixes": ["include", "src", "apps", "tests", "crates", "shaders/lib", "external_app_minimal", "abi", "cmake", "tools", "Cargo.toml"],
        "content_prefixes": ["projects", "showcases", "assets", "tests/artifacts"],
    })
}

fn ownership_boundaries() -> Vec<Value> {
    vec![
        json!({"path": "include/aster/kernel", "owner": "stable public C ABI"}),
        json!({"path": "include/aster/game_sdk", "owner": "source Game SDK and agent authoring"}),
        json!({"path": "src", "owner": "internal engine modules"}),
        json!({"path": "projects/lumen_run", "owner": "sample content"}),
        json!({"path": "crates/aster_assetc", "owner": "asset compiler and agent reports"}),
    ]
}

fn header_findings_json(findings: &[HeaderFinding]) -> Vec<Value> {
    findings
        .iter()
        .map(|finding| {
            json!({
                "severity": finding.severity,
                "code": finding.code,
                "path": finding.path,
                "message": finding.message,
            })
        })
        .collect()
}

fn append_findings_markdown(out: &mut String, findings: &[HeaderFinding]) {
    if findings.is_empty() {
        out.push_str("## Findings\n\nNo findings.\n");
        return;
    }
    out.push_str("## Findings\n\n");
    for finding in findings.iter().take(80) {
        out.push_str(&format!(
            "- [{}] `{}` {}: {}\n",
            finding.severity, finding.code, finding.path, finding.message
        ));
    }
    if findings.len() > 80 {
        out.push_str(&format!("- ... {} more\n", findings.len() - 80));
    }
}

fn append_json_findings_markdown(out: &mut String, findings: &Value) {
    out.push_str("## Findings\n\n");
    let Some(items) = findings.as_array() else {
        out.push_str("No findings.\n");
        return;
    };
    if items.is_empty() {
        out.push_str("No findings.\n");
        return;
    }
    for item in items.iter().take(80) {
        out.push_str(&format!(
            "- [{}] `{}` {}: {}\n",
            item["severity"].as_str().unwrap_or("info"),
            item["code"].as_str().unwrap_or("finding"),
            item["path"].as_str().unwrap_or(""),
            item["message"].as_str().unwrap_or("")
        ));
    }
    if items.len() > 80 {
        out.push_str(&format!("- ... {} more\n", items.len() - 80));
    }
}
