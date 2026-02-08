const Anthropic = require('@anthropic-ai/sdk');
const fs = require('fs');
const { execSync } = require('child_process');

const anthropic = new Anthropic({
  apiKey: process.env.ANTHROPIC_API_KEY,
});

async function analyzeCode() {
  try {
    // Get the diff
    const diff = execSync('git diff HEAD^ HEAD', { encoding: 'utf-8' });
    
    if (!diff.trim()) {
      console.log('No changes detected.');
      fs.writeFileSync('claude-review-output.md', '## 🤖 Claude Code Review\n\nNo changes to review.');
      return;
    }

    // Get changed files
    const changedFiles = fs.readFileSync('changed_files.txt', 'utf-8').trim();

    const prompt = `You are an expert code reviewer. Please analyze the following code changes and provide:

1. **Security Issues** - Any potential security vulnerabilities
2. **Bug Detection** - Potential bugs or logic errors
3. **Code Quality** - Suggestions for improvements
4. **Best Practices** - Violations of coding standards
5. **Documentation** - Missing or inadequate documentation

Format your response in Markdown with clear sections and severity levels (🔴 Critical, 🟡 Warning, 🟢 Info).

Changed files: ${changedFiles}

Git Diff:
\`\`\`diff
${diff}
\`\`\``;

    console.log('Requesting Claude code review...');

    const message = await anthropic.messages.create({
      model: 'claude-sonnet-4-20250514',
      max_tokens: 4096,
      messages: [
        {
          role: 'user',
          content: prompt,
        },
      ],
    });

    const reviewContent = message.content[0].text;

    // Format output
    const output = `## 🤖 Claude Code Review

**Model:** ${message.model}
**Analyzed Files:** ${changedFiles.split(' ').length} file(s)

---

${reviewContent}

---

*Review generated on ${new Date().toISOString()}*
`;

    fs.writeFileSync('claude-review-output.md', output);
    console.log('✅ Claude code review completed successfully!');
    console.log(output);

  } catch (error) {
    console.error('❌ Error during code review:', error.message);
    
    const errorOutput = `## 🤖 Claude Code Review - Error

**Status:** Failed
**Error:** ${error.message}

Please check the workflow logs for more details.
`;
    
    fs.writeFileSync('claude-review-output.md', errorOutput);
    process.exit(1);
  }
}

analyzeCode();
