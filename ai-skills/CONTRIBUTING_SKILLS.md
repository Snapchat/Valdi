# Contributing Skills

Skills are neutral, agent-agnostic markdown files that teach AI assistants how to work with the Valdi framework. They are installed into your AI tool of choice using `valdi skills install`.

## What is a skill?

A skill is a single `skill.md` file containing focused guidance for a specific area of Valdi development. Skills are:

- **Agent-agnostic** — plain markdown with no tool-specific frontmatter or directives
- **Self-contained** — each skill covers one topic without assuming other skills are loaded
- **Code-first** — concrete examples with correct and incorrect patterns labeled clearly
- **Accurate** — verified against the actual Valdi source and documentation

## skill.md format

```markdown
# Skill Title

Brief description of what this skill covers.

## When to use

Optional section: describe the file types or scenarios where this skill applies.

## Key concepts

Prose or list explaining important ideas.

## Correct patterns

\`\`\`typescript
// ✅ Correct example
\`\`\`

## Common mistakes

\`\`\`typescript
// ❌ Wrong example
\`\`\`
```

Rules:
- Start with an H1 title
- Include code examples — skills without examples are less useful
- Label examples as `// ✅` (correct) or `// ❌` (wrong)
- No agent-specific frontmatter (no `alwaysApply`, no YAML blocks)
- No secrets, API keys, or proprietary Snap information

## Scaffolding a new skill

Run the interactive scaffold command from within the Valdi framework checkout:

```bash
valdi skills create
```

You will be prompted for a name, description, and category. The command creates `ai-skills/skills/<name>/skill.md` in the correct location and registers the entry in `ai-skills/registry.json` automatically.

## Submitting a skill to the public registry

1. Fork [github.com/Snapchat/Valdi](https://github.com/Snapchat/Valdi)
2. Run `valdi skills create` from the repo root — it creates the skill file and registers it in `registry.json` automatically
3. Fill in the `skill.md` content
4. Open a pull request — title it `[skill] Add <your-skill-name>`

## Review criteria

Pull requests adding or updating skills are reviewed for:

- **Accuracy** — does the code compile and run correctly against current Valdi?
- **Code examples** — are there concrete `✅` / `❌` examples?
- **Breadth** — does it cover the common cases a developer would encounter?
- **No hallucinations** — especially for TSX skills, ensure no React patterns are presented as valid Valdi
- **Conciseness** — focused guidance is more useful than exhaustive reference dumps
