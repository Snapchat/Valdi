# Valdi API Design & Extensibility

Valdi is designed as a flexible, high-performance platform that balances developer control with framework-level optimizations.

## Design Philosophy

The core ideology behind Valdi is to **decouple the framework team from feature teams**. By providing a flexible, non-opinionated API, Valdi ensures that feature teams are never blocked by framework limitations.

### The "Highest Level API" Motto
> *"What is the highest level API we can expose that's easy and safe to use while allowing us to achieve a high level of performance?"*

Valdi aims to:
- **Minimize API Surface**: A smaller API surface is easier to learn and maintain.
- **Avoid Domain-Specific APIs**: Instead of solving one specific problem, Valdi designs powerful, flexible APIs that can solve multiple problems.
- **Maintain Under-the-Hood Control**: Valdi provides a sense of control to the user while allowing the framework to make performance improvements without changing the public APIs.

## Extensibility Mechanisms

Valdi provides three primary mechanisms for extending its core functionality:

### 1. Bridge Modules (Global)
Bridge modules expose functionality globally to all TypeScript code. They are best suited for services that are available across the entire application regardless of state (e.g., logging, analytics, global configuration).

### 2. Component Context (Scoped)
The `ComponentContext` exposes host app functionality locally to a particular component and its subtree. This is ideal for services scoped to a specific session or feature (e.g., user profile data, feature-specific data stores).

### 3. Custom View Classes (UI)
Custom view classes allow developers to reuse existing native UI elements (iOS/Android) that would be impractical or non-performant to reimplement in TypeScript (e.g., complex video players, maps, or system-provided pickers).

## Layered Architecture

Valdi is organized into distinct layers to ensure separation of concerns:

| Layer | Language | Responsibility |
| :--- | :--- | :--- |
| **Feature Layer** | TypeScript | Feature-specific business logic and UI. |
| **SIG/CoreUI** | TypeScript | High-level, reusable UI components following design guidelines. |
| **Service Layer** | TS / C++ / Native | Exposes application-specific services (e.g., authentication, data stores) to Valdi. |
| **Framework Layer** | TS / C++ | Manages component lifecycles and the element tree. |
| **Core Runtime** | C++ | JS engine integration, Yoga layout engine, and UI synchronization. |
| **Integration Layer** | C++ / Native | Platform-specific implementation of native elements (e.g., `SCValdiLabel`). |
