// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 Faruk Alpay

use serde_json::{json, Value};
use std::collections::{BTreeMap, BTreeSet};
use std::env;
use std::fs;
use std::path::{Path, PathBuf};

fn value_str<'a>(value: &'a Value, key: &str) -> Option<&'a str> {
    value.get(key).and_then(Value::as_str)
}

fn asset_domain(kind: &str) -> Vec<&'static str> {
    match kind {
        "scene" => vec!["project", "scene", "physics"],
        "prefab" => vec!["prefab", "systems", "physics"],
        "cave" | "mesh" | "asset_graph" => vec!["geometry", "scene", "physics"],
        "material" | "texture" => vec!["material", "rendering"],
        "item" => vec!["item", "systems"],
        "action_graph" => vec!["action_graph", "systems"],
        "lesson" => vec!["learning", "systems"],
        "input" | "input_map" => vec!["systems", "ui"],
        "ui" => vec!["ui"],
        _ => vec!["unknown"],
    }
}

fn normalize_path(path: &Path) -> String {
    path.components()
        .as_path()
        .to_string_lossy()
        .replace('\\', "/")
}

fn trim_rule_prefix(line: &str) -> Option<String> {
    let trimmed = line.trim();
    if let Some(rest) = trimmed
        .strip_prefix("- ")
        .or_else(|| trimmed.strip_prefix("* "))
    {
        let rule = rest.trim();
        return (!rule.is_empty()).then(|| rule.to_string());
    }
    let dot = trimmed.find('.')?;
    if trimmed[..dot].chars().all(|ch| ch.is_ascii_digit()) {
        let rule = trimmed[dot + 1..].trim();
        return (!rule.is_empty()).then(|| rule.to_string());
    }
    None
}

fn instruction_rules(text: &str) -> Vec<String> {
    text.lines()
        .filter_map(trim_rule_prefix)
        .map(|rule| {
            if rule.len() > 240 {
                format!("{}...", rule.chars().take(240).collect::<String>())
            } else {
                rule
            }
        })
        .collect()
}

fn ancestor_chain(root: &Path, leaf: &Path) -> Vec<PathBuf> {
    let mut chain = Vec::new();
    let mut current = leaf.to_path_buf();
    loop {
        chain.push(current.clone());
        if current == root {
            break;
        }
        let Some(parent) = current.parent() else {
            break;
        };
        if parent == current {
            break;
        }
        current = parent.to_path_buf();
    }
    chain.reverse();
    chain
}

fn workspace_root_for(project: &Path) -> PathBuf {
    let project_abs = fs::canonicalize(project).unwrap_or_else(|_| project.to_path_buf());
    let project_root = project_abs
        .parent()
        .unwrap_or_else(|| Path::new("."))
        .to_path_buf();
    let cwd = env::current_dir().unwrap_or_else(|_| project_root.clone());
    if project_abs.starts_with(&cwd) {
        cwd
    } else {
        project_root
    }
}

fn instruction_stack_rows(project: &Path) -> Vec<Value> {
    let project_abs = fs::canonicalize(project).unwrap_or_else(|_| project.to_path_buf());
    let project_root = project_abs.parent().unwrap_or_else(|| Path::new("."));
    let workspace_root = workspace_root_for(project);
    ancestor_chain(&workspace_root, project_root)
        .into_iter()
        .enumerate()
        .filter_map(|(depth, scope_root)| {
            let path = scope_root.join("AGENTS.md");
            let text = fs::read_to_string(&path).ok()?;
            Some(json!({
                "path": normalize_path(&path),
                "scope_root": normalize_path(&scope_root),
                "depth": depth,
                "rules": instruction_rules(&text),
            }))
        })
        .collect()
}

fn command_policy_rows() -> Vec<Value> {
    vec![
        json!({
            "id": "aster.allow.git_status",
            "decision": "allow",
            "prefix": ["git", "status"],
            "rationale": "Read-only git state keeps batches from colliding."
        }),
        json!({
            "id": "aster.allow.git_diff",
            "decision": "allow",
            "prefix": ["git", "diff"],
            "rationale": "Diff inspection protects unrelated local edits."
        }),
        json!({
            "id": "aster.allow.search",
            "decision": "allow",
            "prefix": ["rg"],
            "rationale": "Fast repository search is expected for Aster agents."
        }),
        json!({
            "id": "aster.allow.cmake_build",
            "decision": "allow",
            "prefix": ["cmake", "--build"],
            "rationale": "Targeted CMake proof validates the public contracts."
        }),
        json!({
            "id": "aster.allow.ctest",
            "decision": "allow",
            "prefix": ["ctest"],
            "rationale": "Targeted test proof is part of handoff quality."
        }),
        json!({
            "id": "aster.allow.assetc",
            "decision": "allow",
            "prefix": ["cargo", "run", "-p", "aster_assetc"],
            "rationale": "The asset compiler owns machine-readable project context."
        }),
        json!({
            "id": "aster.review.git_stage",
            "decision": "review",
            "prefix": ["git", "add"],
            "rationale": "Staging is a durable handoff action."
        }),
        json!({
            "id": "aster.review.git_commit",
            "decision": "review",
            "prefix": ["git", "commit"],
            "rationale": "Commits should follow an explicit user request."
        }),
        json!({
            "id": "aster.deny.git_reset_hard",
            "decision": "deny",
            "prefix": ["git", "reset", "--hard"],
            "rationale": "This can erase another agent's work."
        }),
        json!({
            "id": "aster.deny.git_checkout_paths",
            "decision": "deny",
            "prefix": ["git", "checkout", "--"],
            "rationale": "Path checkout can silently revert unrelated edits."
        }),
        json!({
            "id": "aster.deny.third_party_notice",
            "decision": "deny",
            "prefix": ["touch", "THIRD_PARTY_NOTICES"],
            "rationale": "Aster-owned batches do not add third-party notice files."
        }),
    ]
}

fn handoff_policy() -> Value {
    json!({
        "summary_budget_chars": 1400,
        "required_sections": [
            "changed_paths",
            "decisions",
            "validation",
            "remaining_tasks",
            "collision_notes"
        ],
        "rules": [
            "Name any touched files that were already dirty before the batch.",
            "Keep continuation notes small enough for another agent to resume without rereading the whole repo.",
            "Record skipped validation with the concrete reason."
        ]
    })
}

fn header_policy() -> Value {
    json!({
        "core_license": "Apache-2.0",
        "content_license": "LicenseRef-Aster-Content",
        "allowed_license_ids": [
            "Apache-2.0",
            "LicenseRef-Aster-Content"
        ],
        "copyright": "Copyright (c) 2026 Faruk Alpay",
        "source_header": {
            "slash_comment": [
                "// SPDX-License-Identifier: Apache-2.0",
                "// Copyright (c) 2026 Faruk Alpay"
            ],
            "hash_comment": [
                "# SPDX-License-Identifier: Apache-2.0",
                "# Copyright (c) 2026 Faruk Alpay"
            ]
        },
        "rules": [
            "Add the SPDX/copyright header to new engine source and Cargo/CMake metadata files.",
            "Do not add source headers to binary, cooked, or commentless content formats.",
            "Aster sample content and branding remain under the content license unless a file says otherwise."
        ]
    })
}

fn ownership_boundaries() -> Vec<Value> {
    vec![
        json!({
            "path": "include/aster/kernel",
            "owner": "stable C ABI",
            "policy": "do not edit unless the task explicitly asks for a kernel contract update"
        }),
        json!({
            "path": "include/aster/game_sdk",
            "owner": "source Game SDK and agent authoring contracts",
            "policy": "keep schema and batch reports stable for external consumers"
        }),
        json!({
            "path": "src",
            "owner": "internal reusable engine modules",
            "policy": "keep sample content out of engine defaults"
        }),
        json!({
            "path": "projects/lumen_run",
            "owner": "sample content",
            "policy": "prove features without defining engine architecture by accident"
        }),
        json!({
            "path": "crates/aster_assetc",
            "owner": "asset compiler and agent reports",
            "policy": "prefer machine-readable reports for agent context"
        }),
    ]
}

fn physics_performance_contract(assets: &[Value]) -> Value {
    let physics_asset_count = assets
        .iter()
        .filter(|asset| {
            asset
                .get("domains")
                .and_then(Value::as_array)
                .into_iter()
                .flatten()
                .any(|domain| domain.as_str() == Some("physics"))
        })
        .count();
    json!({
        "target_frame_seconds": 1.0 / 60.0,
        "target_frame_hz": 60,
        "physics_asset_count": physics_asset_count,
        "required_kernel_stats": [
            "broadphase_rebuild_count",
            "narrowphase_pair_tests",
            "mesh_accelerated_body_count",
            "mesh_acceleration_cell_visits",
            "mesh_triangle_candidate_count",
            "contact_island_count",
            "warm_started_contacts"
        ],
        "collision_cook_policy": "static mesh collision must use cooked acceleration before sample runtime shortcuts",
        "risk_rules": [
            "large cave, terrain, and castle meshes must report triangle candidate counts",
            "frame governor decisions must be driven by kernel ABI stats",
            "sample content may prove engine behavior but must not define engine architecture"
        ]
    })
}

fn dirty_worktree(project: &Path) -> String {
    let repo = workspace_root_for(project);
    std::process::Command::new("git")
        .arg("-C")
        .arg(repo)
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

fn project_asset_rows(root: &Value) -> Vec<Value> {
    let startup_scene = value_str(root, "startup_scene").unwrap_or_default();
    let mut rows = root
        .get("assets")
        .and_then(Value::as_array)
        .into_iter()
        .flatten()
        .map(|asset| {
            let kind = value_str(asset, "kind").unwrap_or("unknown");
            let id = value_str(asset, "id").unwrap_or_default();
            json!({
                "id": id,
                "kind": kind,
                "path": value_str(asset, "path").unwrap_or_default(),
                "domains": asset_domain(kind),
                "startup": id == startup_scene,
            })
        })
        .collect::<Vec<_>>();
    rows.sort_by(|lhs, rhs| {
        value_str(lhs, "id")
            .unwrap_or_default()
            .cmp(value_str(rhs, "id").unwrap_or_default())
    });
    rows
}

fn scope_rows(project_root: &Path, assets: &[Value]) -> Vec<Value> {
    let mut by_path = BTreeMap::<String, BTreeSet<String>>::new();
    for asset in assets {
        let Some(path) = value_str(asset, "path") else {
            continue;
        };
        let parent = Path::new(path).parent().unwrap_or_else(|| Path::new("."));
        let key = normalize_path(&project_root.join(parent));
        let domains = by_path.entry(key).or_default();
        if let Some(domain_values) = asset.get("domains").and_then(Value::as_array) {
            for domain in domain_values {
                if let Some(domain) = domain.as_str() {
                    domains.insert(domain.to_string());
                }
            }
        }
    }
    by_path
        .into_iter()
        .map(|(path, domains)| {
            json!({
                "path": path,
                "owner": "aster-game-sdk",
                "writable": true,
                "domains": domains.into_iter().collect::<Vec<_>>(),
            })
        })
        .collect()
}

fn output_schema() -> Value {
    json!({
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "title": "Aster Agent Batch Report",
        "type": "object",
        "required": ["schema_version", "batch_id", "summary", "changed_files", "validation", "handoff"],
        "properties": {
            "schema_version": { "const": 1 },
            "batch_id": { "type": "string", "minLength": 1 },
            "summary": { "type": "string", "minLength": 1 },
            "changed_files": {
                "type": "array",
                "items": { "type": "string", "minLength": 1 }
            },
            "validation": {
                "type": "array",
                "items": {
                    "type": "object",
                    "required": ["command", "status"],
                    "properties": {
                        "command": { "type": "string" },
                        "status": { "enum": ["passed", "failed", "skipped"] },
                        "notes": { "type": "string" }
                    }
                }
            },
            "handoff": {
                "type": "object",
                "required": ["session_id", "decisions", "remaining_tasks", "risk_notes"],
                "properties": {
                    "session_id": { "type": "string" },
                    "decisions": { "type": "array", "items": { "type": "string" } },
                    "remaining_tasks": { "type": "array", "items": { "type": "string" } },
                    "risk_notes": { "type": "array", "items": { "type": "string" } }
                }
            }
        }
    })
}

fn asset_output_schema() -> Value {
    json!({
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "title": "Aster Agent Asset Iteration Report",
        "type": "object",
        "required": [
            "schema_version",
            "asset_id",
            "reference_images_used",
            "changed_files",
            "candidate_artifacts",
            "claimed_signals",
            "rejected_signals",
            "presentation_quality",
            "surface_stack",
            "self_review"
        ],
        "properties": {
            "schema_version": { "const": 1 },
            "asset_id": { "type": "string", "minLength": 1 },
            "reference_images_used": {
                "type": "array",
                "items": { "type": "string", "minLength": 1 }
            },
            "changed_files": {
                "type": "array",
                "items": { "type": "string", "minLength": 1 }
            },
            "candidate_artifacts": {
                "type": "array",
                "items": { "type": "string", "minLength": 1 }
            },
            "claimed_signals": {
                "type": "array",
                "items": { "type": "string", "minLength": 1 }
            },
            "rejected_signals": {
                "type": "array",
                "items": { "type": "string", "minLength": 1 }
            },
            "presentation_quality": {
                "type": "object",
                "required": [
                    "scale_cues",
                    "contact_shadows",
                    "surface_occlusion",
                    "volumetric_depth",
                    "camera_language",
                    "preview_artifact"
                ],
                "properties": {
                    "scale_cues": { "type": "string" },
                    "contact_shadows": { "type": "string" },
                    "surface_occlusion": { "type": "string" },
                    "volumetric_depth": { "type": "string" },
                    "camera_language": { "type": "string" },
                    "preview_artifact": { "type": "string", "minLength": 1 }
                }
            },
            "surface_stack": {
                "type": "object",
                "required": [
                    "physical_texel_density",
                    "height_normal_coupling",
                    "roughness_height_coupling",
                    "macro_frequency_breakup",
                    "micro_frequency_breakup"
                ],
                "properties": {
                    "physical_texel_density": { "type": "number", "exclusiveMinimum": 0 },
                    "height_normal_coupling": { "type": "number", "minimum": 0 },
                    "roughness_height_coupling": { "type": "number", "minimum": 0 },
                    "macro_frequency_breakup": { "type": "number", "minimum": 0 },
                    "micro_frequency_breakup": { "type": "number", "minimum": 0 }
                }
            },
            "self_review": {
                "type": "object",
                "required": ["status", "score", "missing_required", "present_forbidden", "next_actions"],
                "properties": {
                    "status": { "enum": ["passed", "needs_work", "blocked"] },
                    "score": { "type": "number", "minimum": 0, "maximum": 1 },
                    "missing_required": {
                        "type": "array",
                        "items": { "type": "string" }
                    },
                    "present_forbidden": {
                        "type": "array",
                        "items": { "type": "string" }
                    },
                    "next_actions": {
                        "type": "array",
                        "items": { "type": "string" }
                    }
                }
            }
        }
    })
}

fn industrial_pipe_required_signals() -> Vec<Value> {
    vec![
        json!({
            "id": "corroded_orange_brown_rust",
            "description": "Orange-brown rust must be the dominant material signal.",
            "weight": 1.0
        }),
        json!({
            "id": "dark_oxide_cavities",
            "description": "Dark oxide and grime collect around cavities, inner rim, underside, and weld shadows.",
            "weight": 0.85
        }),
        json!({
            "id": "raised_weld_rings",
            "description": "Circumferential raised weld bands stay visible and integrated into the body.",
            "weight": 1.0
        }),
        json!({
            "id": "open_hollow_rims",
            "description": "Pipe ends read as hollow, thick, dark inner metal with worn rim edges.",
            "weight": 0.95
        }),
        json!({
            "id": "uneven_pitting",
            "description": "Irregular pitting and blotchy oxidation replace smooth uniform noise.",
            "weight": 0.85
        }),
        json!({
            "id": "axial_scratches",
            "description": "Long scratches and wear follow the pipe axis.",
            "weight": 0.65
        }),
        json!({
            "id": "reference_silhouette",
            "description": "The simple reference pipe silhouette is preserved unless the brief asks for variants.",
            "weight": 1.0
        }),
    ]
}

fn industrial_pipe_forbidden_signals() -> Vec<Value> {
    vec![
        json!({
            "id": "smooth_black_pipe",
            "description": "A mostly smooth black or charcoal pipe misses the reference.",
            "weight": 1.0
        }),
        json!({
            "id": "decorative_bolts_without_reference",
            "description": "Bolts, clamp ears, or extra flange hardware must not appear unless explicitly requested.",
            "weight": 0.9
        }),
        json!({
            "id": "clean_plastic_surface",
            "description": "Avoid plastic-like smoothness or toy material response.",
            "weight": 0.85
        }),
        json!({
            "id": "monochrome_material",
            "description": "Avoid one-color rust/noise with no oxide, wear, or depth variation.",
            "weight": 0.8
        }),
        json!({
            "id": "missing_weld_rings",
            "description": "Do not remove or hide the reference weld bands.",
            "weight": 1.0
        }),
    ]
}

fn custom_signal_rows(values: &[String]) -> Vec<Value> {
    values
        .iter()
        .map(|value| {
            let id = value
                .trim()
                .to_ascii_lowercase()
                .replace(' ', "_")
                .replace('-', "_");
            json!({
                "id": id,
                "description": value,
                "weight": 1.0
            })
        })
        .collect()
}

fn has_domain(assets: &[Value], domain: &str) -> bool {
    assets.iter().any(|asset| {
        asset
            .get("domains")
            .and_then(Value::as_array)
            .into_iter()
            .flatten()
            .any(|value| value.as_str() == Some(domain))
    })
}

fn batch_rows(assets: &[Value]) -> Vec<Value> {
    let mut batches = Vec::new();
    batches.push(json!({
        "id": "batch.foundation",
        "policy": "review_gate",
        "tasks": [
            {
                "id": "agent.map_project_contracts",
                "status": "ready",
                "brief": "Map the Aster project manifest, startup scene, asset ids, and writable scopes."
            },
            {
                "id": "agent.normalize_authoring_surface",
                "status": "planned",
                "depends_on": ["agent.map_project_contracts"],
                "brief": "Group schema documents into content, systems, visual proof, and validation batches."
            }
        ]
    }));

    let mut content_tasks = Vec::new();
    if has_domain(assets, "scene") || has_domain(assets, "geometry") {
        content_tasks.push(json!({
            "id": "agent.scene_foundation",
            "status": "planned",
            "depends_on": ["agent.normalize_authoring_surface"],
            "brief": "Make scene, cave, mesh, and placement contracts easy to extend."
        }));
    }
    if has_domain(assets, "prefab") {
        content_tasks.push(json!({
            "id": "agent.prefab_contracts",
            "status": "planned",
            "depends_on": ["agent.normalize_authoring_surface"],
            "brief": "Keep prefab roots, sockets, and component ownership explicit."
        }));
    }
    if has_domain(assets, "material") || has_domain(assets, "rendering") {
        content_tasks.push(json!({
            "id": "agent.visual_proof_surface",
            "status": "planned",
            "depends_on": ["agent.normalize_authoring_surface"],
            "brief": "Connect materials, textures, and graphs to renderer proof commands."
        }));
    }
    if !content_tasks.is_empty() {
        batches.push(json!({
            "id": "batch.content_surface",
            "policy": "parallel_safe",
            "tasks": content_tasks,
        }));
    }

    if has_domain(assets, "action_graph") || has_domain(assets, "item") || has_domain(assets, "ui")
    {
        batches.push(json!({
            "id": "batch.gameplay_systems",
            "policy": "serial",
            "tasks": [{
                "id": "agent.gameplay_loop_contracts",
                "status": "planned",
                "depends_on": ["agent.scene_foundation"],
                "brief": "Bind items, action graphs, input maps, and UI files to entity/component documents."
            }]
        }));
    }

    if has_domain(assets, "learning") {
        batches.push(json!({
            "id": "batch.learning_contracts",
            "policy": "review_gate",
            "tasks": [{
                "id": "agent.learning_proof_contracts",
                "status": "planned",
                "depends_on": ["agent.normalize_authoring_surface"],
                "brief": "Bind lesson objectives, learner-state hypotheses, scaffolds, and trace proof to project assets."
            }]
        }));
    }

    batches.push(json!({
        "id": "batch.proof",
        "policy": "review_gate",
        "tasks": [{
            "id": "agent.validation_handoff",
            "status": "planned",
            "depends_on": ["agent.normalize_authoring_surface"],
            "brief": "Run targeted Aster validation and leave a continuation-safe handoff."
        }]
    }));
    batches
}

fn diagnostics(root: &Value, assets: &[Value]) -> Vec<Value> {
    let mut diagnostics = Vec::new();
    if value_str(root, "name").unwrap_or_default().is_empty() {
        diagnostics.push(json!({
            "severity": "error",
            "path": "$.name",
            "message": "project name is required"
        }));
    }
    let startup_scene = value_str(root, "startup_scene").unwrap_or_default();
    if !startup_scene.is_empty()
        && !assets
            .iter()
            .any(|asset| value_str(asset, "id") == Some(startup_scene))
    {
        diagnostics.push(json!({
            "severity": "error",
            "path": "$.startup_scene",
            "message": "startup_scene does not match an asset id"
        }));
    }
    let mut ids = BTreeSet::new();
    for asset in assets {
        let id = value_str(asset, "id").unwrap_or_default();
        if id.is_empty() {
            diagnostics.push(json!({
                "severity": "error",
                "path": "$.assets[].id",
                "message": "asset id is required"
            }));
        } else if !ids.insert(id.to_string()) {
            diagnostics.push(json!({
                "severity": "error",
                "path": format!("$.assets.{id}"),
                "message": "duplicate asset id"
            }));
        }
    }
    diagnostics
}

pub fn agent_plan_report_json(
    project: &Path,
    objective: Option<&str>,
    include_schema: bool,
) -> Result<String, String> {
    let bytes = fs::read(project).map_err(|error| error.to_string())?;
    let root: Value = serde_json::from_slice(&bytes).map_err(|error| error.to_string())?;
    let project_root = project.parent().unwrap_or_else(|| Path::new("."));
    let assets = project_asset_rows(&root);
    let scopes = scope_rows(project_root, &assets);
    let batches = batch_rows(&assets);
    let diagnostics = diagnostics(&root, &assets);
    let instruction_stack = instruction_stack_rows(project);
    let mut validation = vec![
        json!({
            "id": "build.game_sdk_public_consumer",
            "command": "cmake --build <build-dir> --target aster_game_sdk_public_consumer",
            "required": true
        }),
        json!({
            "id": "test.game_sdk_public_consumer",
            "command": "ctest --test-dir <build-dir> --output-on-failure -R aster_game_sdk_public_consumer",
            "required": true
        }),
    ];
    validation.push(json!({
        "id": "cook.project",
        "command": format!(
            "cargo run -p aster_assetc --bin aster_assetc -- cook --project {} --platform desktop --output <cook-output-dir>",
            normalize_path(project)
        ),
        "required": false
    }));
    let mut report = json!({
        "schema_version": 1,
        "kind": "aster_agent_plan",
        "project": {
            "name": value_str(&root, "name").unwrap_or("Aster Project"),
            "path": normalize_path(project),
            "root": normalize_path(project_root),
            "startup_scene": value_str(&root, "startup_scene").unwrap_or_default(),
        },
        "objective": objective.unwrap_or("Grow this Aster project through schema-first agent batches."),
        "assets": assets,
        "scopes": scopes,
        "batches": batches,
        "instruction_stack": instruction_stack,
        "command_policy": command_policy_rows(),
        "handoff_policy": handoff_policy(),
        "header_policy": header_policy(),
        "ownership_boundaries": ownership_boundaries(),
        "physics_performance_contract": physics_performance_contract(&assets),
        "dirty_worktree": dirty_worktree(project),
        "validation": validation,
        "diagnostics": diagnostics,
        "rules": [
            "Keep ownership in Aster names, schemas, and modules.",
            "Use the SPDX/copyright header for new Apache-2.0 engine source files.",
            "Prefer data documents before sample-specific runtime shortcuts.",
            "Do not add third-party notice files as part of an Aster agent batch.",
            "Leave a compact handoff with changed paths, decisions, validation, and remaining tasks."
        ],
    });
    if include_schema {
        report["output_schema"] = output_schema();
    }
    serde_json::to_string_pretty(&report).map_err(|error| error.to_string())
}

pub fn asset_brief_report_json(
    project: &Path,
    asset_id: &str,
    references: &[PathBuf],
    target: Option<&str>,
    required: &[String],
    forbidden: &[String],
    include_schema: bool,
) -> Result<String, String> {
    let bytes = fs::read(project).map_err(|error| error.to_string())?;
    let root: Value = serde_json::from_slice(&bytes).map_err(|error| error.to_string())?;
    let assets = project_asset_rows(&root);
    let asset = assets
        .iter()
        .find(|row| value_str(row, "id") == Some(asset_id))
        .cloned();
    let default_pipe_profile = asset_id.contains("pipe") || asset_id.contains("rusted");
    let required_signals = if required.is_empty() && default_pipe_profile {
        industrial_pipe_required_signals()
    } else {
        custom_signal_rows(required)
    };
    let forbidden_signals = if forbidden.is_empty() && default_pipe_profile {
        industrial_pipe_forbidden_signals()
    } else {
        custom_signal_rows(forbidden)
    };
    let reference_rows = references
        .iter()
        .enumerate()
        .map(|(index, path)| {
            json!({
                "path": normalize_path(path),
                "role": if index == 0 { "visual_target" } else { "material_target" },
                "weight": if index == 0 { 1.0 } else { 0.75 },
                "note": "Use this image as a visual contract for the asset iteration."
            })
        })
        .collect::<Vec<_>>();
    let prompt = format!(
        "Produce an Aster-owned asset iteration for {asset_id}. Use the reference images as hard visual contracts, preserve every required signal, reject every forbidden signal, and return the structured asset iteration report."
    );
    let mut report = json!({
        "schema_version": 1,
        "kind": "aster_agent_asset_brief",
        "project": {
            "name": value_str(&root, "name").unwrap_or("Aster Project"),
            "path": normalize_path(project),
        },
        "asset": asset.unwrap_or_else(|| {
            json!({
                "id": asset_id,
                "kind": "unknown",
                "path": "",
                "domains": ["unknown"],
                "startup": false
            })
        }),
        "target": target.unwrap_or("Match the reference image before adding creative variation."),
        "reference_images": reference_rows,
        "required_signals": required_signals,
        "forbidden_signals": forbidden_signals,
        "presentation_quality": {
            "scale_cues": "Include authored scale references or context geometry; the asset must not read as a tiny isolated viewport demo.",
            "contact_shadows": "Show grounded contact with hardening near the receiver and soft falloff at the edge.",
            "surface_occlusion": "Claim cavity/horizon occlusion at seams, rims, undercuts, and surface intersections.",
            "volumetric_depth": "Use fog or depth layering when the asset has interior darkness, hollow volume, or recesses.",
            "camera_language": "Use a production preview lens/framing that shows weight, silhouette, and floor contact.",
            "preview_artifact_required": true
        },
        "surface_stack": {
            "physical_texel_density": {
                "minimum": if default_pipe_profile { 768 } else { 512 },
                "unit": "texels_per_meter",
                "note": "Texture detail must read at physical scale, not as same-frequency procedural noise."
            },
            "height_normal_coupling": "Height, normal, and roughness responses must be authored together.",
            "roughness_height_coupling": "Cavity roughness, wetness, grime, and edge polish must respond to height/cavity fields.",
            "macro_frequency_breakup": "Use macro variation to break repetition across the asset body.",
            "micro_frequency_breakup": "Use micro variation for pitting, scratches, and material grain without overpowering form."
        },
        "visual_proof_expectations": [
            "scale cues visible",
            "contact shadows visible",
            "surface occlusion visible",
            "physical texel density declared",
            "height normal roughness coupling declared",
            "preview artifact listed"
        ],
        "minimum_score": if default_pipe_profile { 0.88 } else { 0.82 },
        "iteration_budget": if default_pipe_profile { 4 } else { 3 },
        "expected_artifacts": [
            "candidate preview image",
            "source asset or graph file",
            "asset iteration report"
        ],
        "rules": [
            "Reference images are visual contracts, not mood boards.",
            "A passed build is not enough; the visual brief must pass too.",
            "Do not invent forbidden hardware, silhouettes, or material simplifications.",
            "Return needs_work when required signals are missing."
        ],
        "agent_prompt": prompt,
    });
    if include_schema {
        report["output_schema"] = asset_output_schema();
    }
    serde_json::to_string_pretty(&report).map_err(|error| error.to_string())
}
