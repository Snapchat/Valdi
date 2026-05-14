# valdi-mock-iterate

Compare a design mock against a rendered Valdi screenshot and produce specific, actionable code fixes. This is the "critic" in the mock-to-module pipeline — it only looks at pixels, never at code.

## When to use

- You have both a design mock/screenshot AND a rendered Valdi screenshot
- You need to evaluate visual fidelity between a target design and generated output
- You're the "critic" agent in a generator+critic iteration loop
- User asks "does this match?" or "what's different?"

## Overview

1. Compare the two images side by side
2. List every visual difference — be specific and quantitative
3. Prioritize fixes by visual impact
4. Output structured feedback the generator can act on

---

## Step 1: Compare the images

You will receive two images:

- **Mock** — the target design (screenshot, Figma export, or photo)
- **Rendered** — the Valdi app screenshot (from `valdi inspect snapshot`)

Compare them systematically by scanning these dimensions:

### Layout and structure
- Are all elements present? Any missing or extra elements?
- Is the overall structure correct? (vertical vs horizontal, scroll vs fixed)
- Is the element ordering correct?

### Spacing and sizing
- Are margins and padding correct? Compare gaps between elements.
- Are element sizes correct? (width, height, aspect ratios)
- Is the overall content centered/aligned as expected?

### Typography
- Are font sizes correct? (compare relative sizes: title vs body vs caption)
- Are font weights correct? (bold vs regular)
- Is text color correct?
- Is text alignment correct? (left, center, right)

### Colors and backgrounds
- Is the background color correct?
- Are element background colors correct?
- Are accent/highlight colors correct?
- Are borders and dividers the right color and thickness?

### Components
- Do interactive components look correct? (buttons, toggles, checkboxes)
- Are icons/images the right size and position?
- Are rounded corners the right radius?

---

## Step 2: Produce structured feedback

For each difference found, output a structured entry:

```
ISSUE: <what's wrong>
ELEMENT: <which element in the layout — be specific>
EXPECTED: <what the mock shows>
ACTUAL: <what the rendered output shows>
FIX: <specific code change to make>
SEVERITY: critical | major | minor
```

### Severity levels

- **critical** — Missing elements, wrong structure, completely wrong layout
- **major** — Wrong colors, wrong font sizes, significantly wrong spacing
- **minor** — Slightly off spacing (2-4px), minor color shade differences, minor radius differences

### Example output

```
ISSUE: Title font too small
ELEMENT: Header title label ("Settings")
EXPECTED: Bold, approximately 24px
ACTUAL: Regular weight, approximately 16px
FIX: Change font from 'system 16' to 'system-bold 24'
SEVERITY: major

ISSUE: Missing divider between rows
ELEMENT: Between row 2 ("Dark Mode") and row 3 ("Notifications")
EXPECTED: Thin gray horizontal line
ACTUAL: No divider present
FIX: Add <view height={1} backgroundColor="#E0E0E0" margin="0 16" /> between rows
SEVERITY: major

ISSUE: Bottom button slightly too narrow
ELEMENT: "Save Changes" button at bottom
EXPECTED: Full width with 16px side margins
ACTUAL: Appears to have 24px side margins
FIX: Change button container padding from '0 24' to '0 16'
SEVERITY: minor
```

---

## Step 3: Summarize and prioritize

After listing all issues, provide:

1. **Match score** — rough percentage (e.g., "~70% match")
2. **Top 3 fixes** — the changes that will have the biggest visual impact
3. **Verdict** — one of:
   - `MATCH` — close enough, no more iteration needed (>95% match)
   - `ITERATE` — specific fixes needed, list them
   - `RESTRUCTURE` — fundamental layout is wrong, generator should start over with different structure

---

## Guidelines for the critic

### Be specific, not vague
```
// BAD: "The text looks different"
// GOOD: "The title label is regular weight ~16px but should be bold ~24px"

// BAD: "Spacing is off"  
// GOOD: "Gap between the header and first row is ~8px but should be ~24px"

// BAD: "Colors are wrong"
// GOOD: "Button background is #007AFF but should be #FFFC00 (yellow)"
```

### Estimate in points, not pixels
Valdi uses point-based sizing. Screenshots from `valdi inspect snapshot` are in points (1pt = 1 unit in the output). When estimating dimensions:
- Standard spacing increments: 4, 8, 12, 16, 24, 32, 48
- Expanded font size reference:

| Visual role | Typical range |
|------------|---------------|
| Hero / display numbers | 64–80pt bold |
| Section headings | 20–24pt bold |
| Primary body text | 20–22pt |
| Secondary labels | 17–19pt |
| Captions / axis labels | 15–17pt |
| Small metadata | 12–14pt |

- Round to the nearest standard value

### Common first-pass mistakes to watch for

These are the most frequent issues on the **first iteration** — catch them early for the biggest visual improvement:

1. **Font sizes 30-50% too small** — the #1 error. If all text looks "a size too small" compared to the mock, recommend bumping every font size up by 2-6pt.
2. **Vertical spacing too tight** — sections feel cramped compared to the mock. Gaps between major sections should typically be 24-44pt, not 8-16pt.
3. **Text colors too dark** — body text using `#000000` when it should be `#444444-#555555`. Secondary labels using `#666666` when they should be `#999999-#BBBBBB`.
4. **Card padding too small** — cards typically need 28-36pt padding, not 12-16pt.
5. **Card border radius too sharp** — modern designs use 16-20pt radius.
6. **Chart elements too thin/small** — data lines should be 4-5pt width, dots 10-12pt diameter.

When these issues appear together (they usually do), recommend **all of them at once** rather than fixing one at a time. This dramatically reduces iteration count.

7. **Gradient/area fills look like bar charts** — when using overlapping `<view>` columns to simulate area fill under a curve, you need 60-80+ columns with slight width overlap (`width: ${100/STEPS + 2}%`) and uniform low opacity (~0.10-0.20) for a smooth continuous appearance.
8. **Selected state not visible enough** — highlight backgrounds need sufficient contrast from the card background. Use at least `#F5E6D0` level warmth for beige/tan selection pills on white backgrounds, not `#F0EAE0` which is barely visible.
9. **Emoji instead of real icons** — emoji characters look wrong at any size. Recommend replacing with icon assets from the project's icon library if one exists.
10. **Raw primitives instead of widget components** — hand-built list rows using `<view>` + `<label>` miss standard spacing and styling. Check the workspace for a shared widget library and use those components instead.

### Don't nitpick platform differences
Some things will always look slightly different:
- System font rendering varies by platform
- Shadow rendering is approximate
- Scroll indicator visibility
- Status bar / window chrome

These are NOT issues to report.

### Focus on what the generator can fix
Only report issues that can be fixed by editing the .tsx file:
- Layout, spacing, sizing, colors, fonts, element ordering, missing elements
- Do NOT report: animation timing, gesture behavior, dynamic data differences, system UI differences

---

## Pixel diff (preferred over eyeballing)

When available, use the bundled pixel diff scripts for objective comparison:

```bash
# ALWAYS use --auto-mask to exclude avatar/thumbnail regions from the match score
python3 <skill_dir>/scripts/diff.py /path/to/mock.png /path/to/render.png --auto-mask

# For iOS simulator screenshots, also mask the status bar:
python3 <skill_dir>/scripts/diff.py /path/to/mock.png /path/to/render.png --auto-mask --mask "0,0,400,30"
```

**ALWAYS use `--auto-mask`** on every diff. This auto-detects avatar/thumbnail/Bitmoji regions and excludes them from the match score — these render as colored placeholder circles that unfairly penalize the score.

Output: `/tmp/mock-vs-render-diff.png` — side-by-side: mock | render | amplified diff. Bright spots in the third panel = mismatches. Also prints a match percentage.

**Always open the diff image** so humans can see it: `open /tmp/mock-vs-render-diff.png`

This catches subtle differences that eyeballing misses: background color mismatches (#F2F2F2 vs #FFFFFF), missing drop shadows, and small spacing differences.

---

## Integration with the pipeline

When used as part of `/valdi-mock-to-module`:

1. Receive mock image, rendered screenshot, and pixel diff image
2. Produce the structured feedback above
3. The diff coordinator applies fixes one at a time via hot-reload
4. After a batch of fixes, re-diff and compare again
5. Repeat until `MATCH` or max iterations reached

In the two-phase pipeline, this critic role is distributed across three specialist agents (Text, Icons, Widgets) that run in parallel. Each focuses on its domain and reports structured fix recommendations.

**Key principle**: You never see code. You only see images. This keeps your feedback honest — you can't be biased by "the code looks right". You evaluate purely on visual output.
