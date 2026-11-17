# docs_check

Catches problems in `docs/` that break, or degrade, the Valdi section of developers.snap.com.

That site pulls `docs/` into a strict Docusaurus build. A broken link or a bare `<T>` in one page fails the build and blocks the deploy for every section of the site, so it's cheaper to catch it here.

```bash
python3 tools/docs_check/check_docs.py
python3 tools/docs_check/test_check_docs.py
```

Python 3.8+, standard library only. The list of published pages comes from `docs/website/docs-options.json`.

## Fixing what it reports

| Check | Blocks the site build? | Fix |
|---|---|---|
| `links` | Yes | Point the link at a page that exists. Link to a page, not a directory, and use a relative path, not one starting with `/`. |
| `mdx` | Yes | Docusaurus reads every `.md` file as MDX, so a bare `<` starts a JSX tag and `{` starts an expression. Wrap the text in backticks (`` `Style<T>` ``) or escape it (`\<`, `\{`). Raw HTML tags like `<img>` and `<p>` are fine. MDX also has no `<https://...>` autolinks; write `[text](https://...)`. |
| `anchors` | No, the build only warns | Make the `#fragment` match the heading's id: lowercase, punctuation and emoji dropped, spaces become `-`. `## The \`Style<>\`` becomes `#the-style`. To keep a link stable when a heading changes, give the heading an explicit id: `## Events {#events}`. |
| `repo-links` | No | The link leaves `docs/` and is published as a github.com/Snapchat/Valdi link. Point it at a path that exists in the repo. |
| `outside-repo` | No, but the link is always broken | The link leaves the public repo (the folder `docs/` is mirrored from), so it 404s on GitHub even if the file exists here. Link to something inside the public repo, or to an absolute URL. |

CI fails on every check except `repo-links`. That includes `anchors`: the site still builds with a broken anchor, but the link quietly lands at the top of the page, so CI catches it.

GitHub resolves anchors case-insensitively and Docusaurus doesn't, so `#Events` works on github.com but not on developers.snap.com.

## Limits

It approximates Docusaurus with regexes instead of running it, so the site build has the final word. If it flags something the real build accepts, fix the check here.
