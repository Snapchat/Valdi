# Security Policy

The security of Valdi and the apps built with it is important to us. Thank you for helping keep the project and its users safe.

## Reporting a vulnerability

Please report security vulnerabilities privately through Snap's bug bounty program on HackerOne:

**[https://hackerone.com/snapchat](https://hackerone.com/snapchat)**

Please **do not** report security vulnerabilities through public GitHub issues, discussions, or pull requests. Public reports put users at risk before a fix is available. If you open a public issue by mistake, a maintainer will close and lock it and ask you to resubmit through HackerOne.

Reports are handled under the terms of Snap's HackerOne program, including its rules on disclosure and eligibility for rewards.

## What to include

A report we can act on quickly includes:

- The affected component (for example the runtime, compiler, CLI, or a specific module) and the Valdi version or commit.
- The target platform(s) affected (iOS, Android, web, macOS).
- Step-by-step instructions to reproduce the issue, ideally with a minimal proof of concept.
- The impact: what an attacker could achieve, and under what conditions.

## Supported versions

Security fixes are made on the `main` branch and included in the next release. Valdi is in beta, and fixes are not backported to older releases.

## Scope

### In scope

Vulnerabilities in the code in this repository, including:

- The Valdi runtime on iOS, Android, web, and macOS.
- The Valdi compiler and build rules.
- The `valdi` command-line tool.
- Modules and libraries published from this repository.

### Out of scope

The following are generally **not** considered security vulnerabilities in Valdi. Reports about them will usually be closed without further action:

- **Vulnerabilities in apps built with Valdi** that are caused by the app's own code or configuration. Please report these to the app's developer.
- **Behavior that requires a malicious Valdi module or app code.** Valdi executes the TypeScript and native code that an app ships, by design. Code that is already part of the app running with the app's privileges is not a vulnerability in Valdi.
- **Attacks that require a compromised device or development machine**, such as a rooted or jailbroken device, physical access, malware already present on the machine, or an attacker who controls the build environment.
- **Known vulnerabilities in third-party dependencies** without a demonstrated, exploitable impact on Valdi. Please report the underlying issue to the upstream project.
- **Output from automated scanners or AI tools** that has not been verified to be exploitable. Given the volume of automated findings we receive, reports must include a working reproduction and a description of real-world impact.
- **Missing hardening or best-practice suggestions** without a demonstrated exploit, such as missing compiler flags or security headers.
- **Example and demo apps** under `apps/`, which are not intended for production use.
- **Denial of service** caused by resource exhaustion on a developer's own machine, for example by feeding very large inputs to the compiler or CLI.
- **Social engineering**, phishing, or physical attacks against Snap employees, offices, or infrastructure.

If you are unsure whether something is in scope, please report it through HackerOne anyway. We would rather review a report than miss a real issue.
