# Frequently Asked Questions

## Why did Snap create this?

The Snapchat iOS and Android applications are meant to be basically exactly the same, with identical designs. This is a trend in that has become very common in the industry. Whereas initially many applications had different designs and user experience on each application, over time these differences faded, and nowadays for many companies, difference of application behaviors between iOS and Android are merely just a result of the codebase being implemented by different engineers and technologies rather than conscious design choices. This observation is why code sharing for both business logic and UI is so attractive on paper for many companies, including Snap.

It is often thought that cross-platform UI frameworks come with many trade-offs, and that cross-platform applications tend to perform worse or offer a less polished experience than platform native codebases. This is an unacceptable trade-off for Snap. Any use of cross-platform had to come with minimal or no trade-offs on application quality and performance.

We created Valdi because we believed there was an opportunity to increase developer velocity for mobile developers at Snap without reducing application quality, and that we didn't find, especially at the time, any other open source solutions that could check all our boxes. Some of these boxes were:

- Performance comparable to a native UI in most scenarios.
- Strong consistency between iOS and Android.
- Scalable by design, startup latency doesn't increase as codebase grows.
- Low application size impact, both initially and incrementally as codebase grows.
- Low bar to entry for mobile developers (easy to learn, easy to onboard, hard to use inefficiently).
- Can be efficiently adopted to render full page components as well as tiny components inside an existing native UI.
- Platform native and type safe API generation.

## How much is Snap using Valdi?

Valdi is used on almost every Snapchat screen: sometimes to render some smaller components as part of an otherwise UI implemented in Swift or Objective-C, sometimes to render the entire screen. Some features are just service layers written using Valdi and don't have any UI. Teams at Snap can choose to adopt Valdi or not based on their use cases and whether they believe Valdi is the right solution for them. Throughout the years, the team working on Valdi has worked with its customers to make this technology useable in more and more use cases. There are close to 2 million lines of TypeScript written by Snap engineers using Valdi.

## Why use Valdi instead of React Native?

When we started Valdi, we actually had no intention of making it a React Native competitor. It was initially a small framework, with a clear set of use cases it was meant to help with. We wanted the framework to be really good at solving these use cases knowing that developers would have to initially resort to a large amount of platform native code. Over time, as we kept adding more and more features, Valdi became similar to React Native, although with a different development vision and implementation.

React Native and Valdi are quite similar in how they work, at their core they are each development platforms allowing users to write their UI in TypeScript, with the backing UI system ultimately using platform native UI components. React Native is more web centric and more compatible with web technologies in general. Valdi breaks with web standards when that enables optimizations that can improve performance on mobile, or when it lowers the bar to entry to mobile engineers. For example, the module loader in Valdi is lazy by default, where dependency module files only actually get imported when they get used for the first time, rather than when their dependent modules get loaded. This solves a common pain point that many companies had where as codebase grew, so did the load time for the main application bundle. In Valdi, each module is independent, there is not a `main.jsbundle`. In Valdi, modules build with Bazel, and native code is an integral part of applications that are built using it. The list of differences is very long, and it'd take many pages to list them all out. The sum of these differences all add up to a technology that was ultimately possible to integrate in so many surfaces on Snapchat.

React Native is a fantastic technology and we don't claim Valdi is strictly superior to it. We chose a different set of trade-offs for Valdi to make it work at Snap, and other projects might also find these trade-offs desirable. The main target audience for Valdi are developers that have strong requirements for performance and scalability and/or have an existing native application and want to efficiently use some cross-platform business logic or UI.

## Why using TypeScript instead of my favorite language?

In its early days, the Valdi team experimented with a few different user-facing languages, including Kotlin and Swift. TypeScript was eventually chosen for the following reasons:

- it was easy to integrate at runtime on both iOS and Android, since there are many integrable open source JS engines.
- it was decently easy to implement a responsive hot reloader on top of it. Hot reloading was a killer feature for us that we were not willing to part way with.
- it was easy to learn by mobile developers.
- it was supported equally well by tooling on macOS and Linux.
- it didn't require the use of a massive IDE to edit source code effectively.
- it compiled into modules that use very little app size.
- It is widely adopted, and its adoption kept growing.

We thought that TypeScript provided the best trade-offs for what we were aiming for. We still believe today it was the right choice.

## What are the trade-offs?

You will find below a non exhaustive list of trade-offs.

Compared to platform native (native iOS and Android application):

- Incomplete access to some platform specific features, which will require to write and integrate platform native code
- Harder to make use of the best that each individual platform has to offer
- Not officially supported by Apple and Google
- Potentially harder to hire experienced engineers
- Mobile engineers sometimes prefer Swift/Kotlin
- Debuggability worse in some scenarios
- Lower performance ceiling (how fast the application can run if you have unlimited budget for optimizing the application code)

Compared to React Native:

- While APIs look similar, it's not React
- Not compatible with the vast set of already published libraries
- Not as mature
- Supported by a smaller company than Meta
- Much smaller community

## How does it work?

At build time, the Valdi compiler packages a Valdi module into .valdimodule file, which contains the compiled TS code. An application can contain many .valdimodule files, Snapchat had around 600 in late 2025. Code can be packaged in 3 different ways:

- JS source: TS code compiled with the TS compiler, then minified.
- JS bytecode: TS code compiled with the TS compiler, minified, then compiled as bytecode ahead of time. This is the default output mode.
- Native: TS code compiled to C, then compiled to native with clang. When this mode is used, the .valdimodule doesn't contain the compiled code, the native code is compiled within the application binary itself (or within the main .so dynamic library bundled in the application on Android).

JS source and bytecode gets interpreted by a JS engine at runtime. Supported engines are JavaScriptCore, QuickJS and Hermes. When compiled as native, the code integrates within the QuickJS engine (arrays end up using JS engine arrays). There is complete and "toll-free" interop between modules compiled as JS source, JS bytecode and native.

TSX code gets transformed into TS by a special compiler step. The XML statements (like `<view>`, `<MyComponent>` etc...) get converted into Valdi renderer statements like `jsx.beginElement(__viewNodePrototype)`, `jsx.beginComponent(__myComponentPrototype)`. Unlike in React, XML expressions return `void` in Valdi. This is because they mutate the renderer stack when invoked, as opposed to creating objects that need to be diffed. This difference allows Valdi to better control the render flow which made it possible to implement several optimizations that would have been difficult to do in React, and it also reduces how much work happen during render in the first place especially on incremental renders.

The Valdi runtime is able to process the render output and emit native backing views for each element that need one, that means creating `UIView`, `UILabel` instances on iOS or `ViewGroup`, `TextView` on Android for example. The interaction between the TS code and the runtime is thus highly dynamic, the runtime tries to populate the view hierarchy in the most efficient way based on the TS output. There was extensive work to make this interaction as efficient as possible.
