/**
 * @fileoverview Valdi CLI Doctor Command - Environment Health Diagnostics
 *
 * This module implements the `valdi doctor` command, which performs comprehensive
 * health checks on the Valdi development environment. It validates system requirements,
 * tool installations, workspace configuration, and provides actionable feedback
 * for resolving issues.
 *
 * @author rohanjsh
 *
 * @example
 * ```bash
 * # Basic health check (dev environment only)
 * valdi doctor
 *
 * # Include project-specific checks
 * valdi doctor --project
 *
 * # Include framework development checks
 * valdi doctor --framework
 *
 * # Detailed diagnostics with verbose output
 * valdi doctor --verbose
 *
 * # Machine-readable JSON output for CI/CD
 * valdi doctor --json
 *
 * # Attempt automatic fixes where possible
 * valdi doctor --fix
 * ```

 * @see {@link https://bazel.build/install} Bazel Installation Guide
 * @see {@link https://nodejs.org} Node.js Installation
 */

import * as fs from 'fs';
import * as os from 'os';
import path from 'path';
import type { Argv } from 'yargs';
import { ANSI_COLORS } from '../core/constants';

import { ANDROID_NDK_VERSION, ANDROID_PLATFORM_VERSION } from '../setup/versions';
import type { ArgumentsResolver } from '../utils/ArgumentsResolver';
import { BazelClient } from '../utils/BazelClient';
import { checkCommandExists, runCliCommand } from '../utils/cliUtils';
import { makeCommandHandler } from '../utils/errorUtils';
import {
  type LinuxDistroInfo,
  buildInstallCommand,
  detectLinuxDistro,
  getCommonPackageMappings,
  getPackageName,
} from '../utils/linuxDistro';
import { wrapInColor } from '../utils/logUtils';

/** Discord support link for troubleshooting */
const DISCORD_SUPPORT_URL = 'https://discord.gg/uJyNEeYX2U';

/**
 * Command line parameters for the doctor command.
 *
 * @interface CommandParameters
 */
interface CommandParameters {
  /** Enable detailed diagnostic information output */
  verbose: boolean;
  /** Attempt to automatically fix issues where possible */
  fix: boolean;
  /** Output results in JSON format for machine processing */
  json: boolean;
  /** Include framework development checks (git-lfs, temurin, etc.) */
  framework: boolean;
  /** Include project-specific checks (workspace structure, etc.) */
  project: boolean;
}

/**
 * Represents the result of a single diagnostic check.
 *
 * @interface DiagnosticResult
 */
interface DiagnosticResult {
  /** Human-readable name of the diagnostic check */
  name: string;
  /** Status of the check: pass (✓), warn (⚠), or fail (✗) */
  status: 'pass' | 'warn' | 'fail';
  /** Primary message describing the check result */
  message: string;
  /** Optional detailed information about the check */
  details?: string;
  /** Whether this issue can potentially be auto-fixed */
  fixable?: boolean;
  /** Command or instruction to fix the issue */
  fixCommand?: string;
  /** Category for grouping related checks */
  category?: string;
}

/**
 * Represents a group of related diagnostic checks with their overall status.
 *
 * @interface GroupedDiagnosticResult
 */
interface GroupedDiagnosticResult {
  /** Category name for the group */
  category: string;
  /** Overall status of the group (worst status among all checks) */
  status: 'pass' | 'warn' | 'fail';
  /** Summary message for the group */
  message: string;
  /** Individual check results within this group */
  checks: DiagnosticResult[];
  /** Issues found in this group (only checks with warn/fail status) */
  issues: DiagnosticResult[];
}

/**
 * Main class responsible for performing Valdi environment health diagnostics.
 *
 * This class orchestrates various system checks to ensure the development environment
 * is properly configured for Valdi development. It validates:
 * - Node.js version compatibility (≥18.0.0)
 * - Bazel build system installation and functionality (with version validation)
 * - Java JDK installation (Java 17+ required)
 * - Platform-specific development tools (Android SDK, Xcode)
 * - Required development dependencies (git, npm, watchman, ios-webkit-debug-proxy)
 * - Optional project-specific checks (workspace structure)
 * - Optional framework development tools (git-lfs, temurin)
 *
 * @class ValdiDoctor
 */
class ValdiDoctor {
  /** Collection of diagnostic check results */
  private readonly results: DiagnosticResult[] = [];

  /** Whether to show detailed diagnostic information */
  private readonly verbose: boolean;

  /** Whether to attempt automatic fixes for detected issues */
  private readonly autoFix: boolean;

  /** Whether to output results in JSON format */
  private readonly jsonOutput: boolean;

  /** Whether to include framework development checks */
  private readonly frameworkMode: boolean;

  /** Whether to include project-specific checks */
  private readonly projectMode: boolean;

  /** Cached Linux distribution info (only on Linux) */
  private readonly linuxDistro?: LinuxDistroInfo;

  /**
   * Creates a new ValdiDoctor instance.
   *
   * @param verbose - Enable detailed diagnostic output
   * @param autoFix - Attempt to automatically fix issues where possible
   * @param jsonOutput - Output results in JSON format for machine processing
   * @param frameworkMode - Include framework development checks
   * @param projectMode - Include project-specific checks
   */
  constructor(verbose: boolean, autoFix: boolean, jsonOutput: boolean, frameworkMode: boolean, projectMode: boolean) {
    this.verbose = verbose;
    this.autoFix = autoFix;
    this.jsonOutput = jsonOutput;
    this.frameworkMode = frameworkMode;
    this.projectMode = projectMode;

    // Detect Linux distribution if on Linux
    if (os.platform() === 'linux') {
      this.linuxDistro = detectLinuxDistro();
    }
  }

  /**
   * Executes diagnostic checks based on the target audience.
   *
   * **App Development Mode (default):**
   * - Essential tools for building Valdi applications
   * - Node.js, Bazel (with version validation)
   * - Basic Android SDK and Java setup (Java 17+ required)
   * - Core development tools (git, npm, watchman, ios-webkit-debug-proxy)
   *
   * **Project Mode (--project flag):**
   * - All app development checks plus
   * - Workspace structure validation (WORKSPACE file, .bazelrc)
   *
   * **Framework Development Mode (--framework flag):**
   * - All app development checks plus
   * - Advanced development tools (git-lfs, temurin)
   * - Detailed environment variable validation
   * - Platform-specific development packages
   *
   * @returns Promise that resolves when all diagnostics are complete
   *
   * @example
   * ```typescript
   * // App development checks only
   * await doctor.runDiagnostics();
   *
   * // App development + project checks
   * await doctor.runDiagnostics(); // with projectMode = true
   *
   * // App development + framework checks
   * await doctor.runDiagnostics(); // with frameworkMode = true
   * ```
   */
  async runDiagnostics(): Promise<DiagnosticResult['status']> {
    if (!this.jsonOutput) {
      let mode = 'app development';
      if (this.frameworkMode) mode = 'framework development';
      if (this.projectMode) mode += ' + project';
      console.log(wrapInColor(`Running Valdi environment diagnostics (${mode} mode)...`, ANSI_COLORS.BLUE_COLOR));
      console.log();
    }

    // Core checks for all users
    await this.checkNodeVersion();
    await this.checkBazelInstallation();

    // Project-specific checks (only if requested)
    if (this.projectMode) {
      this.checkWorkspaceStructure();
    }

    // Essential platform tools
    await this.checkEssentialPlatformTools();

    // Java for Android development
    await this.checkJavaInstallation();

    // Android SDK basics
    this.checkAndroidSDKBasics();

    // Core development dependencies
    await this.checkCoreDependencies();

    // Git LFS initialization check
    await this.checkGitLfsInitialization();

    // Shell autocomplete configuration
    this.checkShellAutoComplete();

    // VSCode/Cursor extensions check
    await this.checkEditorExtensions();

    // Framework-specific checks (only if requested)
    if (this.frameworkMode) {
      this.checkFrameworkDependencies();
      this.checkAdvancedAndroidSDK();
      this.checkEnvironmentVariables();
    }

    return this.results.some(r => r.status === 'fail') ? 'fail' : 'pass';
  }

  /**
   * Outputs the diagnostic results in the appropriate format.
   *
   * Depending on the configuration, this method will either:
   * - Output structured JSON for machine processing (--json flag)
   * - Display formatted, colored output for human consumption
   *
   
   *
   * @example
   * ```typescript
   * doctor.printResults(); // Human-readable output
   * ```
   */
  printResults(): void {
    if (this.jsonOutput) {
      this.printJsonResults();
    } else {
      this.printFormattedResults();
    }
  }

  /**
   * Adds a diagnostic result to the internal collection.
   *
   * @param result - The diagnostic result to add
   * @private

   */
  private addResult(result: DiagnosticResult): void {
    this.results.push(result);
  }

  /**
   * Groups diagnostic results by category for more concise output.
   *
   * @returns Array of grouped diagnostic results
   * @private
   */
  private groupResultsByCategory(): GroupedDiagnosticResult[] {
    const groups = new Map<string, DiagnosticResult[]>();

    // Group results by category, with fallback for uncategorized results
    for (const result of this.results) {
      const category = result.category || result.name;
      if (!groups.has(category)) {
        groups.set(category, []);
      }
      const categoryGroup = groups.get(category);
      if (categoryGroup) {
        categoryGroup.push(result);
      }
    }

    // Convert to grouped results with overall status
    const groupedResults: GroupedDiagnosticResult[] = [];
    for (const [category, checks] of groups.entries()) {
      // Determine overall status (worst status wins)
      let overallStatus: 'pass' | 'warn' | 'fail' = 'pass';
      for (const check of checks) {
        if (check.status === 'fail') {
          overallStatus = 'fail';
          break;
        } else if (check.status === 'warn' && overallStatus === 'pass') {
          overallStatus = 'warn';
        }
      }

      // Create summary message
      const warnCount = checks.filter(c => c.status === 'warn').length;
      const failCount = checks.filter(c => c.status === 'fail').length;

      let message: string;
      if (overallStatus === 'pass') {
        const firstCheck = checks[0];
        message = checks.length === 1 && firstCheck ? firstCheck.message : `All ${checks.length} checks passed`;
      } else {
        const issues = failCount + warnCount;
        message = `${issues} issue${issues > 1 ? 's' : ''} found`;
      }

      groupedResults.push({
        category,
        status: overallStatus,
        message,
        checks,
        issues: checks.filter(c => c.status !== 'pass'),
      });
    }

    return groupedResults;
  }

  /**
   * Attempts to automatically fix a detected issue.
   *
   * This method executes the provided fix command and reports the outcome.
   * It only runs when auto-fix mode is enabled and provides user feedback
   * about the success or failure of the fix attempt.
   *
   * @param tool - Name of the tool being fixed (for user feedback)
   * @param command - Shell command to execute for the fix
   * @returns Promise that resolves when the fix attempt is complete
   */
  private async attemptAutoFix(tool: string, command: string): Promise<void> {
    if (!this.autoFix) {
      return;
    }

    try {
      console.log(wrapInColor(`Attempting to fix ${tool}...`, ANSI_COLORS.YELLOW_COLOR));
      const { returnCode } = await runCliCommand(command);

      if (returnCode === 0) {
        console.log(wrapInColor(`✓ Successfully fixed ${tool}`, ANSI_COLORS.GREEN_COLOR));
      } else {
        console.log(wrapInColor(`✗ Failed to fix ${tool}`, ANSI_COLORS.RED_COLOR));
      }
    } catch (error) {
      console.log(wrapInColor(`✗ Failed to fix ${tool}: ${error instanceof Error ? error.message : 'Unknown error'}`, ANSI_COLORS.RED_COLOR));
    }
  }

  /**
   * Validates Node.js installation and version compatibility.
   *
   * Valdi requires Node.js version 18.0.0 or higher for optimal compatibility.
   * This method:
   * 1. Checks if Node.js is installed and accessible via PATH
   * 2. Validates the version meets minimum requirements (≥18.0.0)
   * 3. Optionally attempts to upgrade to Node.js 20 if auto-fix is enabled
   * 4. Provides specific installation/upgrade instructions
   *
   * @returns Promise that resolves when the check is complete
   */
  private async checkNodeVersion(): Promise<void> {
    try {
      const { stdout } = await runCliCommand('node --version');
      const version = stdout.trim();
      const versionParts = version.replace('v', '').split('.');
      const majorVersionStr = versionParts[0];

      if (!majorVersionStr) {
        throw new Error('Invalid version format');
      }

      const majorVersion = Number.parseInt(majorVersionStr, 10);

      if (majorVersion >= 18) {
        this.addResult({
          name: 'Node.js version',
          status: 'pass',
          message: `Node.js ${version} is installed`,
          category: 'Node.js installation',
        });

        // Suggest upgrading to Node.js 20 for better performance
        if (this.autoFix && majorVersion < 20) {
          await this.attemptAutoFix('node', 'nvm install 20 && nvm use 20');
        }
      } else {
        this.addResult({
          name: 'Node.js version',
          status: 'fail',
          message: `Node.js ${version} is outdated. Valdi requires Node.js 18 or higher`,
          fixable: true,
          fixCommand: 'nvm install 18 && nvm use 18',
          category: 'Node.js installation',
        });

        if (this.autoFix) {
          await this.attemptAutoFix('node', 'nvm install 18 && nvm use 18');
        }
      }
    } catch {
      this.addResult({
        name: 'Node.js version',
        status: 'fail',
        message: 'Node.js is not installed or not in PATH',
        fixable: true,
        fixCommand: 'Install Node.js from https://nodejs.org or use nvm',
        category: 'Node.js installation',
      });
    }
  }

  /**
   * Validates Bazel build system installation and functionality.
   *
   * Bazel is the core build system for Valdi projects. This method:
   * 1. Attempts to create a BazelClient instance
   * 2. Executes `bazel version` to verify installation and functionality
   * 3. Validates version against .bazelversion file if available
   * 4. Provides installation guidance if Bazel is missing or broken
   *
   * @returns Promise that resolves when the check is complete
   * @see {@link https://bazel.build/install} Bazel Installation Guide
   */
  private async checkBazelInstallation(): Promise<void> {
    try {
      const bazel = new BazelClient();
      const [returnCode, versionInfo, errorInfo] = await bazel.getVersion();

      if (returnCode === 0 && versionInfo) {
        const versionLine = versionInfo.split('\n')[0] || 'Unknown version';

        // Extract version number for comparison
        const versionMatch = versionLine.match(/(\d+\.\d+\.\d+)/);
        const installedVersion = versionMatch?.[1];

        // Check against .bazelversion file
        const bazelVersionFile = path.join(process.cwd(), '.bazelversion');
        let expectedVersion: string | undefined;

        try {
          if (fs.existsSync(bazelVersionFile)) {
            expectedVersion = fs.readFileSync(bazelVersionFile, 'utf8').trim();
          }
        } catch {
          // Ignore file read errors
        }

        if (expectedVersion && installedVersion && installedVersion !== expectedVersion) {
          this.addResult({
            name: 'Bazel version',
            status: 'warn',
            message: `Bazel version mismatch: installed ${installedVersion}, expected ${expectedVersion}`,
            details: 'Version mismatch may cause build issues. Consider updating Bazel.',
            fixable: true,
            fixCommand: `Install Bazel ${expectedVersion} or run a trial bazel command to verify compatibility`,
            category: 'Bazel installation',
          });
        } else {
          this.addResult({
            name: 'Bazel installation',
            status: 'pass',
            message: `Bazel is installed: ${versionLine}${expectedVersion ? ` (matches expected ${expectedVersion})` : ''}`,
            category: 'Bazel installation',
          });
        }
      } else {
        this.addResult({
          name: 'Bazel installation',
          status: 'fail',
          message: 'Bazel is installed but not working correctly',
          details: errorInfo || versionInfo || 'Unknown error',
          category: 'Bazel installation',
        });
      }
    } catch {
      this.addResult({
        name: 'Bazel installation',
        status: 'fail',
        message: 'Bazel is not installed or not in PATH',
        fixable: true,
        fixCommand: 'Install Bazel from https://bazel.build/install',
        category: 'Bazel installation',
      });
    }
  }

  /**
   * Validates Valdi workspace structure and configuration files.
   *
   * **WORKSPACE File Requirement:**
   * Every Valdi application requires a WORKSPACE file at the project root. This file:
   * - Defines the Bazel workspace name
   * - Imports the Valdi framework as an external dependency
   * - Configures build rules and toolchains for Valdi development
   * - Is automatically created by `valdi bootstrap` when starting a new project
   *
   * **Configuration Files:**
   * - WORKSPACE file (required for all Valdi apps)
   * - .bazelrc file (recommended for build optimization and consistency)
   *
   * This method checks the current working directory for these essential files
   * and provides guidance if they're missing.
   *
   * @private
   *
   * @see {@link https://bazel.build/concepts/build-ref#workspace} Bazel Workspace Documentation
   *
   * @example
   * ```typescript
   * this.checkWorkspaceStructure();
   * // Results in diagnostic output like:
   * // ✓ Valid Valdi workspace detected
   * // ✗ Not in a Valdi workspace directory
   * ```
   */
  private checkWorkspaceStructure(): void {
    const workspaceFile = path.join(process.cwd(), 'WORKSPACE');
    const bazelrcFile = path.join(process.cwd(), '.bazelrc');

    if (fs.existsSync(workspaceFile)) {
      this.addResult({
        name: 'Valdi workspace',
        status: 'pass',
        message: 'Valid Valdi workspace detected',
        category: 'Workspace configuration',
      });
    } else {
      this.addResult({
        name: 'Valdi workspace',
        status: 'fail',
        message: 'Not in a Valdi workspace directory',
        details: 'WORKSPACE file is required for all Valdi applications. Run `valdi bootstrap` to create a new project or navigate to an existing Valdi project root.',
        fixable: true,
        fixCommand: 'valdi bootstrap',
        category: 'Workspace configuration',
      });
    }

    if (fs.existsSync(bazelrcFile)) {
      this.addResult({
        name: 'Bazel configuration',
        status: 'pass',
        message: '.bazelrc file found',
        category: 'Workspace configuration',
      });
    } else {
      this.addResult({
        name: 'Bazel configuration',
        status: 'warn',
        message: '.bazelrc file not found',
        details: 'A .bazelrc file provides build optimization and consistency. Consider creating one or use `valdi bootstrap` for new projects.',
        fixable: true,
        fixCommand: 'Create .bazelrc file with Valdi-specific build configurations',
        category: 'Workspace configuration',
      });
    }
  }

  /**
   * Validates essential platform tools needed for app development.
   *
   * Focuses on core tools that app developers need:
   * - Android SDK (basic check)
   * - Xcode (macOS only, for iOS apps)
   *
   * @returns Promise that resolves when essential platform checks are complete
   * @private
   */
  private async checkEssentialPlatformTools(): Promise<void> {
    // Check Android SDK (essential for mobile app development)
    const androidHome = process.env['ANDROID_HOME'] || process.env['ANDROID_SDK_ROOT'];
    if (androidHome && fs.existsSync(androidHome)) {
      this.addResult({
        name: 'Android SDK',
        status: 'pass',
        message: `Android SDK found at ${androidHome}`,
        category: 'Android installation',
      });
    } else {
      this.addResult({
        name: 'Android SDK',
        status: 'warn',
        message: 'Android SDK not found',
        details: 'Required for Android app development. Set ANDROID_HOME environment variable.',
        fixable: true,
        fixCommand: 'Install Android Studio and set ANDROID_HOME',
        category: 'Android installation',
      });
    }

    // Check Xcode (macOS only, essential for iOS app development)
    if (os.platform() === 'darwin') {
      await this.checkXcodeInstallation();
    }
  }

  /**
   * Validates comprehensive Xcode installation and configuration.
   *
   * Performs thorough checks matching dev_setup requirements:
   * - xcode-select command exists
   * - xcode-select path is configured and valid
   * - /Applications/Xcode.app exists
   *
   * @returns Promise that resolves when Xcode checks are complete
   * @private
   */
  private async checkXcodeInstallation(): Promise<void> {
    // Check if xcode-select command exists
    if (!checkCommandExists('xcode-select')) {
      this.addResult({
        name: 'Xcode',
        status: 'fail',
        message: 'Xcode command line tools not installed',
        details: 'Required for iOS app development',
        fixable: true,
        fixCommand: 'Install Xcode from App Store (https://apps.apple.com/us/app/xcode/id497799835)',
        category: 'iOS development',
      });
      return;
    }

    // Check if Xcode is properly configured
    try {
      const { stdout } = await runCliCommand('xcode-select -p');
      const xcodePath = stdout.trim();

      if (!xcodePath || !fs.existsSync(xcodePath)) {
        this.addResult({
          name: 'Xcode configuration',
          status: 'fail',
          message: 'Xcode command line tools path is not configured correctly',
          fixable: true,
          fixCommand: 'Run: sudo xcode-select -s /Applications/Xcode.app (or your Xcode path)',
          category: 'iOS development',
        });
        return;
      }

      // Extract Xcode.app path from the Developer path
      // xcode-select -p typically returns: /Applications/Xcode.app/Contents/Developer
      const xcodeAppPath = xcodePath.replace(/\/Contents\/Developer\/?$/, '');
      
      if (fs.existsSync(xcodeAppPath) && xcodeAppPath.includes('Xcode')) {
        // Xcode is installed and configured
        this.addResult({
          name: 'Xcode',
          status: 'pass',
          message: `Xcode found at ${xcodeAppPath}`,
          category: 'iOS development',
        });
      } else {
        // Path doesn't point to an Xcode installation
        this.addResult({
          name: 'Xcode',
          status: 'fail',
          message: 'Xcode installation not found',
          details: `xcode-select points to ${xcodePath}, but Xcode app not found at ${xcodeAppPath}`,
          fixable: true,
          fixCommand: 'Install Xcode from App Store (https://apps.apple.com/us/app/xcode/id497799835)',
          category: 'iOS development',
        });
      }
    } catch {
      this.addResult({
        name: 'Xcode',
        status: 'fail',
        message: 'Xcode not properly configured',
        fixable: true,
        fixCommand: 'Run: sudo xcode-select -s /Applications/Xcode.app (or your Xcode path)',
        category: 'iOS development',
      });
    }
  }

  /**
   * Validates Java JDK installation and configuration as set up by dev_setup.
   *
   * Checks for Java installation and configuration that dev_setup manages:
   * - Java JDK availability and version (Java 17+)
   * - JAVA_HOME environment variable
   * - Java symlink configuration (macOS)
   * - Java runtime availability (Linux)
   *
   * @returns Promise that resolves when Java checks are complete
   * @private
   */
  private async checkJavaInstallation(): Promise<void> {
    // Check if Java is available
    if (checkCommandExists('java')) {
      try {
        const { stdout, stderr } = await runCliCommand('java -version');
        // java -version typically outputs to stderr, but check both
        const versionInfo = stderr || stdout || '';
        
        // Try multiple version string formats:
        // 1. version "17.0.16" (older format)
        // 2. openjdk 17.0.16 (OpenJDK format)
        // 3. java version "17.0.16" (alternative format)
        let versionMatch = versionInfo.match(/version "([^"]+)"/);
        if (!versionMatch) {
          versionMatch = versionInfo.match(/openjdk\s+(\d+\.\d+\.\d+)/i);
        }
        if (!versionMatch) {
          versionMatch = versionInfo.match(/java\s+(\d+\.\d+\.\d+)/i);
        }
        
        const version = versionMatch?.[1] ?? 'Unknown version';

        // Check if Java version is 17 or higher
        const majorVersionMatch = version.match(/^(\d+)/);
        const majorVersion = majorVersionMatch?.[1] ? Number.parseInt(majorVersionMatch[1], 10) : 0;

        if (majorVersion >= 17) {
          this.addResult({
            name: 'Java Runtime',
            status: 'pass',
            message: `Java is installed: ${version}`,
            category: 'Java installation',
          });
        } else if (majorVersion > 0) {
          this.addResult({
            name: 'Java Runtime',
            status: 'fail',
            message: `Java ${version} is outdated. Java 17+ is required`,
            details: 'dev_setup installs Java 17 for Android development',
            fixable: true,
            fixCommand: this.getJavaInstallCommand(),
            category: 'Java installation',
          });
        } else {
          // Could detect java but not parse version
          this.addResult({
            name: 'Java Runtime',
            status: 'pass',
            message: 'Java is installed',
            details: `Version info: ${versionInfo.split('\n')[0] || 'Unable to parse version'}`,
            category: 'Java installation',
          });
        }
      } catch {
        this.addResult({
          name: 'Java Runtime',
          status: 'pass',
          message: 'Java is installed',
          category: 'Java installation',
        });
      }
    } else {
      this.addResult({
        name: 'Java Runtime',
        status: 'fail',
        message: 'Java not found in PATH',
        details: 'dev_setup installs Java JDK for Android development',
        fixable: true,
        fixCommand: this.getJavaInstallCommand(),
        category: 'Java installation',
      });
    }

    // Check JAVA_HOME environment variable
    const javaHome = process.env['JAVA_HOME'];
    if (javaHome && fs.existsSync(javaHome)) {
      this.addResult({
        name: 'JAVA_HOME',
        status: 'pass',
        message: `JAVA_HOME set to ${javaHome}`,
        category: 'Java installation',
      });
    } else {
      this.addResult({
        name: 'JAVA_HOME',
        status: 'warn',
        message: 'JAVA_HOME not set or invalid',
        details: 'dev_setup configures JAVA_HOME for Android development',
        fixable: true,
        fixCommand: os.platform() === 'darwin' ? 'export JAVA_HOME=`/usr/libexec/java_home -v 17`' : 'Set JAVA_HOME environment variable',
        category: 'Java installation',
      });
    }

    // Check Java tools in PATH
    const pathEnv = process.env['PATH'] || '';
    if (os.platform() === 'darwin') {
      if (pathEnv.includes('/opt/homebrew/opt/openjdk@17/bin')) {
        this.addResult({
          name: 'Java PATH',
          status: 'pass',
          message: 'Java tools in PATH',
          category: 'Java installation',
        });
      } else if (pathEnv.includes('/opt/homebrew/opt/openjdk@11/bin')) {
        this.addResult({
          name: 'Java PATH',
          status: 'fail',
          message: 'Java 11 in PATH, but Java 17 is required',
          details: 'dev_setup installs Java 17 for Android development',
          fixable: true,
          fixCommand: 'export PATH="/opt/homebrew/opt/openjdk@17/bin:$PATH"',
          category: 'Java installation',
        });
      } else {
        this.addResult({
          name: 'Java PATH',
          status: 'warn',
          message: 'Java tools not in PATH',
          details: 'dev_setup adds Java tools to PATH',
          fixable: true,
          fixCommand: 'export PATH="/opt/homebrew/opt/openjdk@17/bin:$PATH"',
          category: 'Java installation',
        });
      }
    }

    // macOS-specific Java JDK symlink check
    if (os.platform() === 'darwin') {
      const jdk17Symlink = '/Library/Java/JavaVirtualMachines/openjdk-17.jdk';
      const jdk11Symlink = '/Library/Java/JavaVirtualMachines/openjdk-11.jdk';

      if (fs.existsSync(jdk17Symlink)) {
        this.addResult({
          name: 'Java JDK symlink',
          status: 'pass',
          message: 'OpenJDK 17 symlink configured',
          category: 'Java installation',
        });
      } else if (fs.existsSync(jdk11Symlink)) {
        this.addResult({
          name: 'Java JDK symlink',
          status: 'warn',
          message: 'OpenJDK 11 symlink found, but Java 17 is required',
          details: 'dev_setup installs Java 17',
          fixable: true,
          fixCommand: 'sudo ln -sfn /opt/homebrew/opt/openjdk@17/libexec/openjdk.jdk /Library/Java/JavaVirtualMachines/openjdk-17.jdk',
          category: 'Java installation',
        });
      } else {
        this.addResult({
          name: 'Java JDK symlink',
          status: 'warn',
          message: 'OpenJDK symlink not found',
          details: 'dev_setup creates symlink for system-wide Java access',
          fixable: true,
          fixCommand: 'sudo ln -sfn /opt/homebrew/opt/openjdk@17/libexec/openjdk.jdk /Library/Java/JavaVirtualMachines/openjdk-17.jdk',
          category: 'Java installation',
        });
      }
    }
  }



  /**
   * Validates basic Android SDK components needed for app development.
   *
   * Checks essential Android SDK components without overwhelming users:
   * - Android Platform (latest)
   * - Build Tools (basic check)
   *
   * @private
   */
  private checkAndroidSDKBasics(): void {
    const androidHome = process.env['ANDROID_HOME'] || process.env['ANDROID_SDK_ROOT'];

    if (!androidHome || !fs.existsSync(androidHome)) {
      this.addResult({
        name: 'Android SDK Components',
        status: 'warn',
        message: 'Cannot check Android SDK components - ANDROID_HOME not set',
        details: 'Set ANDROID_HOME environment variable for Android development',
        fixable: true,
        fixCommand: 'Install Android Studio and set ANDROID_HOME',
        category: 'Android installation',
      });
      return;
    }

    // Check for any Android Platform (not specific version)
    const platformsDir = path.join(androidHome, 'platforms');
    if (fs.existsSync(platformsDir)) {
      const platforms = fs.readdirSync(platformsDir).filter(dir => dir.startsWith('android-'));
      if (platforms.length > 0) {
        this.addResult({
          name: 'Android Platform',
          status: 'pass',
          message: `Android Platform installed (${platforms.length} version${platforms.length > 1 ? 's' : ''})`,
          category: 'Android installation',
        });
      } else {
        this.addResult({
          name: 'Android Platform',
          status: 'warn',
          message: 'No Android Platform found',
          details: 'Install an Android Platform via Android Studio SDK Manager',
          fixable: true,
          fixCommand: 'Open Android Studio > SDK Manager > Install an Android Platform',
          category: 'Android installation',
        });
      }
    } else {
      this.addResult({
        name: 'Android Platform',
        status: 'warn',
        message: 'Android platforms directory not found',
        fixable: true,
        fixCommand: 'Install Android Studio and configure SDK',
        category: 'Android installation',
      });
    }

    // Check for any Build Tools (not specific version)
    const buildToolsDir = path.join(androidHome, 'build-tools');
    if (fs.existsSync(buildToolsDir)) {
      const buildTools = fs.readdirSync(buildToolsDir);
      if (buildTools.length > 0) {
        this.addResult({
          name: 'Android Build Tools',
          status: 'pass',
          message: `Android Build Tools installed (${buildTools.length} version${buildTools.length > 1 ? 's' : ''})`,
          category: 'Android installation',
        });
      } else {
        this.addResult({
          name: 'Android Build Tools',
          status: 'warn',
          message: 'No Android Build Tools found',
          fixable: true,
          fixCommand: 'Install Build Tools via Android Studio SDK Manager',
          category: 'Android installation',
        });
      }
    } else {
      this.addResult({
        name: 'Android Build Tools',
        status: 'warn',
        message: 'Build tools directory not found',
        fixable: true,
        fixCommand: 'Install Android Studio and configure SDK',
        category: 'Android installation',
      });
    }
  }

  /**
   * Validates core development dependencies needed for app development.
   *
   * Focuses on essential tools that app developers need:
   * - git: Version control (essential)
   * - npm: Package management (essential)
   * - watchman: File watching for hot reloader (essential)
   * - ios-webkit-debug-proxy: iOS debugging for hot reloader (macOS only, essential)
   *
   * @returns Promise that resolves when core dependency checks are complete
   * @private
   */
  private async checkCoreDependencies(): Promise<void> {
    const coreDeps = ['git', 'npm', 'watchman'];

    for (const dep of coreDeps) {
      await this.checkSingleDependency(dep, 'fail'); // Core deps are critical
    }

    // Platform-specific core dependencies
    if (os.platform() === 'darwin') {
      await this.checkSingleDependency('ios_webkit_debug_proxy', 'fail'); // Essential for hot reloader
    }
  }

  /**
   * Validates Git LFS initialization status.
   *
   * Checks if git-lfs is installed and properly initialized.
   * dev_setup runs `git lfs install` to configure Git LFS hooks.
   *
   * @returns Promise that resolves when Git LFS check is complete
   * @private
   */
  private async checkGitLfsInitialization(): Promise<void> {
    // First check if git-lfs command exists
    if (!checkCommandExists('git-lfs')) {
      const fixCommand = this.getFixCommandForDependency('git-lfs');
      this.addResult({
        name: 'Git LFS',
        status: 'warn',
        message: 'git-lfs not installed',
        details: 'Required for working with large files in the repository',
        fixable: true,
        fixCommand,
        category: 'Development tools',
      });
      return;
    }

    // Check if git lfs is initialized by checking the config
    try {
      const { stdout, returnCode } = await runCliCommand('git config --global --get filter.lfs.process');
      
      if (returnCode === 0 && stdout.trim()) {
        this.addResult({
          name: 'Git LFS initialization',
          status: 'pass',
          message: 'git-lfs is initialized',
          category: 'Development tools',
        });
      } else {
        this.addResult({
          name: 'Git LFS initialization',
          status: 'warn',
          message: 'git-lfs is installed but not initialized',
          details: 'Run git lfs install to configure Git LFS hooks',
          fixable: true,
          fixCommand: 'git lfs install',
          category: 'Development tools',
        });

        if (this.autoFix) {
          await this.attemptAutoFix('git-lfs', 'git lfs install');
        }
      }
    } catch {
      this.addResult({
        name: 'Git LFS initialization',
        status: 'warn',
        message: 'git-lfs is installed but not initialized',
        details: 'Run git lfs install to configure Git LFS hooks',
        fixable: true,
        fixCommand: 'git lfs install',
        category: 'Development tools',
      });

      if (this.autoFix) {
        await this.attemptAutoFix('git-lfs', 'git lfs install');
      }
    }
  }

  /**
   * Validates shell autocomplete configuration.
   *
   * Checks if shell autocomplete (compinit/bashcompinit) is configured.
   * dev_setup adds these to shell RC files for better CLI experience.
   *
   * @private
   */
  private checkShellAutoComplete(): void {
    const homeDir = process.env['HOME'] ?? '';
    const shell = process.env['SHELL'] ?? '';
    
    let rcFile: string | undefined;
    let requiredLines: string[] = [];

    // Determine shell and required configuration
    if (shell.endsWith('/zsh')) {
      rcFile = path.join(homeDir, '.zshrc');
      requiredLines = ['autoload -U compinit && compinit', 'autoload -U bashcompinit && bashcompinit'];
    } else if (shell.endsWith('/bash')) {
      rcFile = path.join(homeDir, '.bashrc');
      // Bash typically auto-loads completion, so we're more lenient
      requiredLines = [];
    } else {
      // Unknown shell, skip check
      return;
    }

    if (!rcFile || !fs.existsSync(rcFile)) {
      this.addResult({
        name: 'Shell autocomplete',
        status: 'warn',
        message: `Shell configuration file not found (${rcFile || 'unknown'})`,
        details: 'Shell autocomplete improves CLI experience',
        category: 'Development tools',
      });
      return;
    }

    // For bash, we just report success since it typically handles completion automatically
    if (requiredLines.length === 0) {
      this.addResult({
        name: 'Shell autocomplete',
        status: 'pass',
        message: 'Shell autocomplete configured (bash)',
        category: 'Development tools',
      });
      return;
    }

    // Read RC file and check for required lines
    try {
      const rcContent = fs.readFileSync(rcFile, 'utf8');
      const rcLines = rcContent.split('\n');
      
      const missingLines: string[] = [];
      for (const required of requiredLines) {
        if (!rcLines.includes(required)) {
          missingLines.push(required);
        }
      }

      if (missingLines.length === 0) {
        this.addResult({
          name: 'Shell autocomplete',
          status: 'pass',
          message: 'Shell autocomplete configured',
          category: 'Development tools',
        });
      } else {
        this.addResult({
          name: 'Shell autocomplete',
          status: 'warn',
          message: 'Shell autocomplete not fully configured',
          details: `Missing in ${rcFile}: ${missingLines.join(', ')}`,
          fixable: true,
          fixCommand: `Add to ${rcFile}: ${missingLines.join(' && ')}`,
          category: 'Development tools',
        });
      }
    } catch {
      this.addResult({
        name: 'Shell autocomplete',
        status: 'warn',
        message: 'Could not verify shell autocomplete configuration',
        category: 'Development tools',
      });
    }
  }

  /**
   * Validates VSCode/Cursor extensions installation.
   *
   * Checks if Valdi extensions are installed in VSCode or Cursor:
   * - valdi-vivaldi: Device logs and language support
   * - valdi-debug: JavaScript debugger
   *
   * @returns Promise that resolves when extension checks are complete
   * @private
   */
  private async checkEditorExtensions(): Promise<void> {
    const hasCode = checkCommandExists('code');
    const hasCursor = checkCommandExists('cursor');

    if (!hasCode && !hasCursor) {
      // No editor installed, skip check
      return;
    }

    let extensionsInstalled = false;

    // Check VSCode extensions
    if (hasCode) {
      try {
        const { stdout } = await runCliCommand('code --list-extensions');
        const installedExtensions = stdout.toLowerCase();
        
        const hasVivaldi = installedExtensions.includes('valdi-vivaldi');
        const hasDebug = installedExtensions.includes('valdi-debug');

        if (hasVivaldi && hasDebug) {
          extensionsInstalled = true;
          this.addResult({
            name: 'VSCode Extensions',
            status: 'pass',
            message: 'Valdi extensions installed in VSCode',
            category: 'Development tools',
          });
        }
      } catch {
        // Could not check VSCode extensions
      }
    }

    // Check Cursor extensions
    if (hasCursor) {
      try {
        const { stdout } = await runCliCommand('cursor --list-extensions');
        const installedExtensions = stdout.toLowerCase();
        
        const hasVivaldi = installedExtensions.includes('valdi-vivaldi');
        const hasDebug = installedExtensions.includes('valdi-debug');

        if (hasVivaldi && hasDebug) {
          extensionsInstalled = true;
          this.addResult({
            name: 'Cursor Extensions',
            status: 'pass',
            message: 'Valdi extensions installed in Cursor',
            category: 'Development tools',
          });
        }
      } catch {
        // Could not check Cursor extensions
      }
    }

    // If neither editor has extensions, show info
    if (!extensionsInstalled && (hasCode || hasCursor)) {
      const editor = hasCode ? 'VSCode' : 'Cursor';
      this.addResult({
        name: `${editor} Extensions`,
        status: 'warn',
        message: 'Valdi editor extensions not installed',
        details: 'Extensions provide syntax highlighting, debugging, and device logs',
        fixable: true,
        fixCommand: 'See installation instructions: https://github.com/Snapchat/Valdi/blob/main/docs/INSTALL.md#vscodecursor-setup-optional-but-recommended',
        category: 'Development tools',
      });
    }
  }

  /**
   * Validates framework development dependencies.
   *
   * Additional tools needed for framework development:
   * - temurin: Alternative JDK (macOS)
   *
   * Note: git-lfs is now checked in core dependencies with initialization verification
   *
   * @private
   */
  private checkFrameworkDependencies(): void {
    // git-lfs is now checked in core with initialization verification

    // Platform-specific framework dependencies
    if (os.platform() === 'darwin') {
      // Check for temurin package
      const temurinPath = '/opt/homebrew/opt/temurin';
      if (fs.existsSync(temurinPath)) {
        this.addResult({
          name: 'temurin package',
          status: 'pass',
          message: 'temurin installed via Homebrew',
          category: 'Framework tools',
        });
      } else {
        this.addResult({
          name: 'temurin package',
          status: 'warn',
          message: 'temurin not found',
          details: 'Alternative JDK for framework development',
          fixable: true,
          fixCommand: 'brew install temurin',
          category: 'Framework tools',
        });
      }
    }
  }

  /**
   * Validates advanced Android SDK components for framework development.
   *
   * Detailed Android SDK validation including:
   * - Specific platform versions
   * - NDK installation
   * - Command line tools
   *
   * @private
   */
  private checkAdvancedAndroidSDK(): void {
    const androidHome = process.env['ANDROID_HOME'] || process.env['ANDROID_SDK_ROOT'];

    if (!androidHome || !fs.existsSync(androidHome)) {
      return; // Already checked in basics
    }

    // Check specific Android Platform version
    const platformPath = path.join(androidHome, 'platforms', ANDROID_PLATFORM_VERSION);
    if (fs.existsSync(platformPath)) {
      this.addResult({
        name: `Android Platform ${ANDROID_PLATFORM_VERSION}`,
        status: 'pass',
        message: `Android Platform ${ANDROID_PLATFORM_VERSION} installed`,
        category: 'Android installation',
      });
    } else {
      this.addResult({
        name: `Android Platform ${ANDROID_PLATFORM_VERSION}`,
        status: 'warn',
        message: `Android Platform ${ANDROID_PLATFORM_VERSION} not found`,
        details: 'Specific platform version for framework development',
        fixable: true,
        fixCommand: `sdkmanager --install 'platforms;${ANDROID_PLATFORM_VERSION}'`,
        category: 'Android installation',
      });
    }

    // Check Android NDK
    const ndkPath = path.join(androidHome, 'ndk', ANDROID_NDK_VERSION);
    if (fs.existsSync(ndkPath)) {
      this.addResult({
        name: 'Android NDK',
        status: 'pass',
        message: `Android NDK ${ANDROID_NDK_VERSION} installed`,
        category: 'Android installation',
      });
    } else {
      this.addResult({
        name: 'Android NDK',
        status: 'warn',
        message: `Android NDK ${ANDROID_NDK_VERSION} not found`,
        details: 'Required for native development in framework',
        fixable: true,
        fixCommand: `sdkmanager --install 'ndk;${ANDROID_NDK_VERSION}'`,
        category: 'Android installation',
      });
    }
  }





  /**
   * Checks a single dependency and reports its status.
   * @private
   */
  private async checkSingleDependency(dep: string, failureLevel: 'warn' | 'fail'): Promise<void> {
    // Determine category based on dependency type
    let category: string;
    if (['git', 'npm', 'watchman', 'ios_webkit_debug_proxy'].includes(dep)) {
      category = 'Development tools';
    } else if (['git-lfs', 'temurin'].includes(dep)) {
      category = 'Framework tools';
    } else {
      category = 'Development tools';
    }

    if (checkCommandExists(dep)) {
      try {
        const { stdout } = await runCliCommand(`${dep} --version`);
        const versionLine = stdout.split('\n')[0] || 'Unknown version';
        this.addResult({
          name: `${dep} installation`,
          status: 'pass',
          message: `${dep} is installed: ${versionLine}`,
          category,
        });
      } catch {
        this.addResult({
          name: `${dep} installation`,
          status: 'pass',
          message: `${dep} is installed`,
          category,
        });
      }
    } else {
      const fixCommand = this.getFixCommandForDependency(dep);
      this.addResult({
        name: `${dep} installation`,
        status: failureLevel,
        message: `${dep} is not installed or not in PATH`,
        fixable: true,
        fixCommand,
        category,
      });

      if (this.autoFix && failureLevel === 'fail') {
        await this.attemptAutoFix(dep, fixCommand);
      }
    }
  }

  /**
   * Validates environment variables as configured by dev_setup.
   *
   * Checks for essential environment variables that dev_setup configures:
   * - ANDROID_HOME: Android SDK location
   * - ANDROID_NDK_HOME: Android NDK location
   * - JAVA_HOME: Java JDK location
   * - PATH modifications: Java, Android tools, Bazelisk
   *
   * @private
   */
  private checkEnvironmentVariables(): void {
    // Check ANDROID_HOME
    const androidHome = process.env['ANDROID_HOME'];
    if (androidHome && fs.existsSync(androidHome)) {
      this.addResult({
        name: 'ANDROID_HOME',
        status: 'pass',
        message: `ANDROID_HOME set to ${androidHome}`,
        category: 'Android installation',
      });
    } else {
      this.addResult({
        name: 'ANDROID_HOME',
        status: 'fail',
        message: 'ANDROID_HOME not set or invalid',
        details: 'dev_setup configures ANDROID_HOME for Android development',
        fixable: true,
        fixCommand: 'valdi dev_setup',
        category: 'Android installation',
      });
    }

    // Check ANDROID_NDK_HOME
    const androidNdkHome = process.env['ANDROID_NDK_HOME'];
    if (androidNdkHome && fs.existsSync(androidNdkHome)) {
      this.addResult({
        name: 'ANDROID_NDK_HOME',
        status: 'pass',
        message: `ANDROID_NDK_HOME set to ${androidNdkHome}`,
        category: 'Android installation',
      });
    } else {
      this.addResult({
        name: 'ANDROID_NDK_HOME',
        status: 'warn',
        message: 'ANDROID_NDK_HOME not set or invalid',
        details: 'dev_setup configures ANDROID_NDK_HOME for native development',
        fixable: true,
        fixCommand: 'valdi dev_setup',
        category: 'Android installation',
      });
    }

    // Check platform-tools in PATH
    const pathEnvAndroid = process.env['PATH'] || '';
    const androidHomeForPath = process.env['ANDROID_HOME'];
    if (androidHomeForPath) {
      const platformToolsInPath = pathEnvAndroid.includes(`${androidHomeForPath}/platform-tools`) || 
                                   pathEnvAndroid.includes('platform-tools');
      if (platformToolsInPath) {
        this.addResult({
          name: 'Android platform-tools PATH',
          status: 'pass',
          message: 'Android platform-tools in PATH',
          category: 'Android installation',
        });
      } else {
        this.addResult({
          name: 'Android platform-tools PATH',
          status: 'warn',
          message: 'Android platform-tools not in PATH',
          details: 'dev_setup adds platform-tools (containing adb) to PATH',
          fixable: true,
          fixCommand: 'export PATH="$ANDROID_HOME/platform-tools:$PATH"',
          category: 'Android installation',
        });
      }
    }

    // JAVA_HOME is checked in Java installation section to avoid duplication

    // Check PATH modifications
    const pathEnv = process.env['PATH'] || '';

    // Check for Bazelisk in PATH (Linux only - Java PATH checked in Java section)
    if (os.platform() === 'linux') {
      if (pathEnv.includes('/.valdi/bin')) {
        this.addResult({
          name: 'Bazelisk PATH',
          status: 'pass',
          message: 'Bazelisk directory in PATH',
          category: 'Bazel installation',
        });
      } else {
        this.addResult({
          name: 'Bazelisk PATH',
          status: 'warn',
          message: 'Bazelisk directory not in PATH',
          details: 'dev_setup adds ~/.valdi/bin to PATH for Bazelisk',
          fixable: true,
          fixCommand: 'export PATH="$HOME/.valdi/bin:$PATH"',
          category: 'Bazel installation',
        });
      }
    }
  }

  /**
   * Gets the appropriate Java installation command for the current platform
   * @private
   */
  private getJavaInstallCommand(): string {
    if (os.platform() === 'darwin') {
      return 'brew install openjdk@17';
    } else if (os.platform() === 'linux' && this.linuxDistro) {
      const packageMappings = getCommonPackageMappings();
      const javaPackage = getPackageName(packageMappings['openjdk-17']!, this.linuxDistro);
      return buildInstallCommand([javaPackage], this.linuxDistro);
    } else {
      return 'Install Java 17 JDK for your distribution';
    }
  }

  /**
   * Generates platform-specific fix commands for missing dependencies.
   *
   * Provides appropriate installation commands based on the current platform
   * and the specific dependency that's missing. Enhanced to support all
   * dependencies that dev_setup installs.
   *
   * @param dep - The name of the missing dependency
   * @returns Platform-appropriate installation command or instruction
   * @private
   */
  private getFixCommandForDependency(dep: string): string {
    // macOS-specific dependencies
    if (os.platform() === 'darwin') {
      switch (dep) {
        case 'git': {
          return 'brew install git';
        }
        case 'npm': {
          return 'Install Node.js from https://nodejs.org (includes npm)';
        }
        case 'watchman': {
          return 'brew install watchman';
        }
        case 'git-lfs': {
          return 'brew install git-lfs';
        }
        case 'bazelisk': {
          return 'brew install bazelisk';
        }
        case 'ios_webkit_debug_proxy': {
          return 'brew install ios-webkit-debug-proxy';
        }
        default: {
          return `brew install ${dep}`;
        }
      }
    }

    // Linux dependencies with distribution detection
    if (os.platform() === 'linux' && this.linuxDistro) {
      const packageMappings = getCommonPackageMappings();

      switch (dep) {
        case 'git': {
          return buildInstallCommand(['git'], this.linuxDistro);
        }
        case 'npm': {
          return buildInstallCommand([getPackageName(packageMappings['npm']!, this.linuxDistro)], this.linuxDistro);
        }
        case 'watchman': {
          const watchmanPkg = getPackageName(packageMappings['watchman']!, this.linuxDistro);
          const cmd = buildInstallCommand([watchmanPkg], this.linuxDistro);
          // Add note for RHEL-based systems
          if (this.linuxDistro.packageManager.name === 'yum' || this.linuxDistro.packageManager.name === 'dnf') {
            return `${cmd} (may require EPEL repository)`;
          }
          return cmd;
        }
        case 'git-lfs': {
          return buildInstallCommand([getPackageName(packageMappings['git-lfs']!, this.linuxDistro)], this.linuxDistro);
        }
        case 'bazelisk': {
          return 'valdi dev_setup';
        }
        case 'adb': {
          return buildInstallCommand([getPackageName(packageMappings['adb']!, this.linuxDistro)], this.linuxDistro);
        }
        default: {
          return buildInstallCommand([dep], this.linuxDistro);
        }
      }
    }

    // Fallback for unknown platforms or when distro detection fails
    switch (dep) {
      case 'npm': {
        return 'Install Node.js from https://nodejs.org (includes npm)';
      }
      case 'bazelisk': {
        return 'valdi dev_setup';
      }
      default: {
        return `Install ${dep} using your system's package manager`;
      }
    }
  }

  /**
   * Outputs diagnostic results in JSON format for machine processing.
   *
   * Generates a structured JSON report containing:
   * - ISO timestamp of the diagnostic run
   * - Summary statistics (passed, warnings, failed, total)
   * - Complete array of diagnostic results with all details
   *
   * This format is ideal for:
   * - CI/CD pipeline integration
   * - Automated monitoring and alerting
   * - Programmatic analysis of environment health
   * @example
   * ```json
   * {
   *   "timestamp": "2024-01-15T10:30:00.000Z",
   *   "summary": { "passed": 8, "warnings": 0, "failed": 1, "total": 9 },
   *   "results": [...]
   * }
   * ```
   */
  private printJsonResults(): void {
    const passCount = this.results.filter(r => r.status === 'pass').length;
    const warnCount = this.results.filter(r => r.status === 'warn').length;
    const failCount = this.results.filter(r => r.status === 'fail').length;

    const output = {
      timestamp: new Date().toISOString(),
      summary: {
        passed: passCount,
        warnings: warnCount,
        failed: failCount,
        total: this.results.length,
      },
      results: this.results,
    };

    console.log(JSON.stringify(output, null, 2));
  }

  /**
   * Outputs diagnostic results in human-readable format with colors and icons.
   *
   * Generates a formatted report with:
   * - Grouped categories for better readability
   * - Colored status icons (✓ ⚠ ✗) for visual clarity
   * - Detailed issue information only when problems exist
   * - Optional verbose details when requested
   * - Actionable fix commands for failed checks
   * - Summary statistics and overall health assessment
   *
   * The output uses ANSI color codes for enhanced readability:
   * - Green (✓): Successful checks
   * - Yellow (⚠): Warnings that don't block development
   * - Red (✗): Critical failures requiring attention
   * @example
   * ```
   * Valdi Doctor Report
   * ==================================================
   * ✓ Node.js installation
   * ✗ Java installation
   *   • JAVA_HOME not set or invalid
   *   • Java tools not in PATH
   * ==================================================
   * Summary: 1 passed, 0 warnings, 1 failed
   * ```
   */
  private printFormattedResults(): void {
    console.log(wrapInColor('Valdi Doctor Report', ANSI_COLORS.BLUE_COLOR));
    console.log('='.repeat(50));
    console.log();

    const groupedResults = this.groupResultsByCategory();
    let totalPassCount = 0;
    let totalWarnCount = 0;
    let totalFailCount = 0;

    for (const group of groupedResults) {
      const statusIcon = group.status === 'pass' ? '✓' : group.status === 'warn' ? '⚠' : '✗';
      const statusColor = group.status === 'pass' ? ANSI_COLORS.GREEN_COLOR :
                         group.status === 'warn' ? ANSI_COLORS.YELLOW_COLOR : ANSI_COLORS.RED_COLOR;

      console.log(`${wrapInColor(statusIcon, statusColor)} ${group.category}`);

      // Show issues when there are problems
      if (group.issues.length > 0) {
        for (const issue of group.issues) {
          console.log(`   • ${issue.message}`);

          if (this.verbose && issue.details) {
            console.log(`     ${wrapInColor('Details:', ANSI_COLORS.GRAY_COLOR)} ${issue.details}`);
          }

          if (issue.fixable && issue.fixCommand) {
            console.log(`     ${wrapInColor('Fix:', ANSI_COLORS.BLUE_COLOR)} ${issue.fixCommand}`);
          }
        }
      }

      console.log();

      // Count individual check results for summary
      for (const check of group.checks) {
        if (check.status === 'pass') totalPassCount++;
        else if (check.status === 'warn') totalWarnCount++;
        else totalFailCount++;
      }
    }

    console.log('='.repeat(50));
    console.log(`${wrapInColor('Summary:', ANSI_COLORS.BLUE_COLOR)} ${totalPassCount} passed, ${totalWarnCount} warnings, ${totalFailCount} failed`);

    if (totalFailCount > 0) {
      console.log();
      console.log(wrapInColor('Some issues need to be resolved before Valdi can work properly.', ANSI_COLORS.RED_COLOR));
      console.log();
      console.log(wrapInColor('Still having trouble? Come get help on Discord:', ANSI_COLORS.BLUE_COLOR));
      console.log(wrapInColor(DISCORD_SUPPORT_URL, ANSI_COLORS.BLUE_COLOR));
      console.log(wrapInColor('Please paste the entire output of this command when asking for help.', ANSI_COLORS.YELLOW_COLOR));
    } else if (totalWarnCount > 0) {
      console.log();
      console.log(wrapInColor('Your environment is mostly ready, but some optional tools are missing.', ANSI_COLORS.YELLOW_COLOR));
    } else {
      console.log();
      console.log(wrapInColor('Your Valdi development environment is ready! 🎉', ANSI_COLORS.GREEN_COLOR));
    }
  }
}

/**
 * Main entry point for the Valdi doctor command.
 *
 * This function serves as the command handler that:
 * 1. Extracts command line arguments (verbose, fix, json)
 * 2. Creates a new ValdiDoctor instance with the specified configuration
 * 3. Executes the complete diagnostic suite
 * 4. Outputs results in the requested format
 *
 * @param argv - Resolved command line arguments
 * @returns Promise that resolves when the doctor command completes
 * 
 * @example
 * ```bash
 * valdi doctor --verbose --fix --json
 * ```
 */
async function valdiDoctor(argv: ArgumentsResolver<CommandParameters>): Promise<void> {
  const verbose = argv.getArgument('verbose');
  const autoFix = argv.getArgument('fix');
  const jsonOutput = argv.getArgument('json');
  const frameworkMode = argv.getArgument('framework');
  const projectMode = argv.getArgument('project');

  const doctor = new ValdiDoctor(verbose, autoFix, jsonOutput, frameworkMode, projectMode);
  const status = await doctor.runDiagnostics();
  doctor.printResults();

  if (status === 'fail') {
    throw new Error('valdi doctor failed');
  }
}

// ============================================================================
// YARGS COMMAND CONFIGURATION
// ============================================================================

/**
 * The command name as it appears in the CLI.
 
 */
export const command = 'doctor';

/**
 * Human-readable description of the command for help output.
 
 */
export const describe = 'Check your Valdi development environment for common issues';

/**
 * Configures command line options and their validation.
 *
 * Defines three main options:
 * - `--verbose` (-v): Enable detailed diagnostic output
 * - `--fix` (-f): Attempt automatic fixes where possible
 * - `--json` (-j): Output results in JSON format for automation
 *
 * @param yargs - The yargs instance to configure
 
 */
export const builder = (yargs: Argv<CommandParameters>): void => {
  yargs
    .option('verbose', {
      describe: 'Show detailed diagnostic information',
      type: 'boolean',
      default: false,
      alias: 'v',
    })
    .option('fix', {
      describe: 'Attempt to automatically fix issues where possible',
      type: 'boolean',
      default: false,
      alias: 'f',
    })
    .option('json', {
      describe: 'Output results in JSON format',
      type: 'boolean',
      default: false,
      alias: 'j',
    })
    .option('framework', {
      describe: 'Include framework development checks (git-lfs, temurin, etc.)',
      type: 'boolean',
      default: false,
      alias: 'F',
    })
    .option('project', {
      describe: 'Include project-specific checks (workspace structure, etc.)',
      type: 'boolean',
      default: false,
      alias: 'p',
    });
};

/**
 * The command handler wrapped with error handling and logging.
 
 */
export const handler = makeCommandHandler(valdiDoctor);
