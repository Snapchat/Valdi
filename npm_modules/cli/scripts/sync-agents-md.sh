#!/bin/bash
# Sync AGENTS.md content between root and bootstrap templates
# This script extracts common sections from the root AGENTS.md and updates bootstrap templates

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CLI_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OPEN_SOURCE_ROOT="$(cd "$CLI_ROOT/../.." && pwd)"

ROOT_AGENTS="$OPEN_SOURCE_ROOT/AGENTS.md"
UI_AGENTS_TEMPLATE="$CLI_ROOT/.bootstrap/apps/ui_application/AGENTS.md.template"
CLI_AGENTS_TEMPLATE="$CLI_ROOT/.bootstrap/apps/cli_application/AGENTS.md.template"

echo "Syncing AGENTS.md patterns from root to bootstrap templates..."
echo "Root AGENTS.md: $ROOT_AGENTS"
echo "UI Template: $UI_AGENTS_TEMPLATE"
echo "CLI Template: $CLI_AGENTS_TEMPLATE"

# Extract the Anti-Hallucination section from root AGENTS.md
# This ensures patterns stay consistent
ANTI_HALLUCINATION_START="## 🚨 AI Anti-Hallucination: This is NOT React!"
ANTI_HALLUCINATION_END="## Directory Structure"

# For now, just validate that the files exist
if [ ! -f "$ROOT_AGENTS" ]; then
    echo "Error: Root AGENTS.md not found at $ROOT_AGENTS"
    exit 1
fi

if [ ! -f "$UI_AGENTS_TEMPLATE" ]; then
    echo "Error: UI AGENTS.md template not found at $UI_AGENTS_TEMPLATE"
    exit 1
fi

if [ ! -f "$CLI_AGENTS_TEMPLATE" ]; then
    echo "Error: CLI AGENTS.md template not found at $CLI_AGENTS_TEMPLATE"
    exit 1
fi

echo "✅ All AGENTS.md files found"
echo ""
echo "Note: This script currently validates file existence."
echo "To fully implement sync, run: npm run update-agents-templates"
