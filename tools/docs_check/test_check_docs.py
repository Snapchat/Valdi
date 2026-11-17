import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import check_docs as cd  # noqa: E402


class SluggerTest(unittest.TestCase):
    def test_matches_github_slugger(self):
        slugger = cd.Slugger()
        self.assertEqual(slugger.slug("Getting Started"), "getting-started")
        self.assertEqual(slugger.slug("What's new?"), "whats-new")
        self.assertEqual(slugger.slug("🚀 Quick start"), "-quick-start")
        self.assertEqual(slugger.slug("snake_case and-dash"), "snake_case-and-dash")

    def test_duplicates_get_suffixes(self):
        slugger = cd.Slugger()
        self.assertEqual([slugger.slug("Setup") for _ in range(3)], ["setup", "setup-1", "setup-2"])


class HeadingIdsTest(unittest.TestCase):
    def test_strips_inline_markdown(self):
        content = "## The `Style<>`\n## [Link](x.md) and **bold**\n"
        self.assertEqual(cd.heading_ids(content), {"the-style", "link-and-bold"})

    def test_title_h1_has_no_id_but_takes_its_slug(self):
        content = "# Full stack Valdi\n## Full-Stack Valdi\n# Later H1\n"
        self.assertEqual(cd.heading_ids(content), {"full-stack-valdi-1", "later-h1"})

    def test_explicit_id(self):
        self.assertEqual(cd.heading_ids("## Events {#events}\n"), {"events"})

    def test_ignores_code_blocks(self):
        content = "```sh\n# not a heading\n```\n> ```\n> # also not\n> ```\n## Real\n"
        self.assertEqual(cd.heading_ids(content), {"real"})

    def test_blockquote_headings(self):
        self.assertEqual(cd.heading_ids("> ## Quoted\n> ### Custom {#custom}\n"), {"quoted", "custom"})

    def test_html_ids_outside_code_only(self):
        content = '<a id="real-anchor"></a>\n```html\n<div id="example"></div>\n```\nUse `<span id="inline">`.\n'
        self.assertEqual(cd.heading_ids(content), {"real-anchor"})


class CheckerTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = os.path.join(self.tmp.name, "repo", "docs")
        os.makedirs(os.path.join(self.root, "website"))
        os.makedirs(os.path.join(self.root, "docs"))
        os.makedirs(os.path.join(self.tmp.name, "repo", "apps"))
        options = {"include": ["**/*.{md,mdx}"], "exclude": ["website/**", "README.md"], "repoLinks": {"rootPath": ".."}}
        with open(os.path.join(self.root, "website", "docs-options.json"), "w") as f:
            json.dump(options, f)
        self.write("docs/target.md", "# Target\n## Some Section\n")
        self.write("README.md", "# Readme\n")

    def tearDown(self):
        self.tmp.cleanup()

    def write(self, rel, content):
        with open(os.path.join(self.root, rel), "w") as f:
            f.write(content)

    def problems(self, content):
        self.write("docs/page.md", content)
        return [(p.check, p.message) for p in cd.DocsChecker(self.root).run(cd.CHECKS) if p.path == "docs/page.md"]

    def test_clean_page(self):
        content = (
            "# Page\n## Intro\n[ok](./target.md#some-section) [web](https://example.com) [self](#intro)\n"
            "[apps](../../apps) <img src=\"./target.md\" width=\"10\" />\n"
            "Inline `Style<T>` and `{x}` are fine, so is a < b.\n"
            "```ts\nconst x: Array<T> = {};\n```\n"
            "<!--\n<Todo> {draft}\n-->\n"
        )
        self.assertEqual(self.problems(content), [])

    def test_broken_link(self):
        self.assertEqual([c for c, _ in self.problems("[x](./missing.md)\n")], ["links"])

    def test_directory_link(self):
        self.assertEqual([c for c, _ in self.problems("[x](../docs)\n")], ["links"])

    def test_excluded_page(self):
        self.assertEqual([c for c, _ in self.problems("[x](../README.md)\n")], ["links"])

    def test_broken_anchor_is_case_sensitive(self):
        self.assertEqual([c for c, _ in self.problems("[x](./target.md#Some-Section)\n")], ["anchors"])

    def test_missing_repo_file(self):
        self.assertEqual([c for c, _ in self.problems("[x](../../apps/nope.ts)\n")], ["repo-links"])

    def test_bare_jsx_and_braces(self):
        checks = [c for c, _ in self.problems("# Using Style<T>\nPass {value} here.\n")]
        self.assertEqual(checks, ["mdx", "mdx", "mdx"])

    def test_bare_tag_in_blockquote(self):
        self.assertEqual([c for c, _ in self.problems("> Use <MyComp> here\n")], ["mdx"])

    def test_any_html_tag_and_attribute_expressions(self):
        self.assertEqual(self.problems('<small>s</small> <abbr title="x">a</abbr> <span style={{color: "red"}}>q</span>\n'), [])

    def test_mdx_comments_and_pointy_link_destinations(self):
        self.assertEqual(self.problems("{/* note */} [x](<https://example.com>) [y][id]\n\n[id]: <https://example.com>\n"), [])

    def test_void_tag_must_self_close(self):
        self.assertEqual([c for c, _ in self.problems('line<br> <img src="https://x.com/a.png"> ok<br/> ok<br></br> <img src="https://x.com/a.png" />\n')], ["mdx", "mdx"])

    def test_escaped_lt(self):
        self.assertEqual(self.problems("Use \\<T> and \\<br> here.\n"), [])

    def test_autolink_is_mdx_error(self):
        self.assertEqual([c for c, _ in self.problems("See <https://example.com>.\n")], ["mdx"])

    def test_unclosed_lowercase_generic_is_mdx(self):
        self.assertEqual([c for c, _ in self.problems("Returns Array<string> here.\n")], ["mdx"])

    def test_explicit_id_on_blockquote_heading_is_not_mdx(self):
        self.assertEqual(self.problems("> ## Quoted {#quoted}\n"), [])

    def test_link_leaving_the_public_repo(self):
        with open(os.path.join(self.tmp.name, "internal.star"), "w") as f:
            f.write("")
        self.assertEqual([c for c, _ in self.problems("[x](../../../internal.star)\n")], ["outside-repo"])

    def test_text_after_multiline_comment_is_checked(self):
        self.assertEqual([c for c, _ in self.problems("<!--\ndraft\n--> see [x](./missing.md)\n")], ["links"])

    def test_indented_list_continuation_is_checked(self):
        self.assertEqual([c for c, _ in self.problems("- item\n\n    more about [x](./missing.md)\n")], ["links"])


if __name__ == "__main__":
    unittest.main()
