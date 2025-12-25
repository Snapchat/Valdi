module.exports = function formatClaudeOutput(rawOutput) {
    // Assuming rawOutput is a string containing the output from Claude Code
    const lines = rawOutput.split('\n');
    const formattedOutput = lines.map(line => {
        // Example formatting: trim whitespace and add a bullet point
        return line.trim() ? `• ${line.trim()}` : '';
    }).filter(line => line !== '').join('\n');

    return formattedOutput;
};