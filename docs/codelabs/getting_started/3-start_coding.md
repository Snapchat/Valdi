# Start coding

Now that you have the code and tools installed, you can jump right in. The quickest way to start playing with Valdi code is through the `helloworld` app.

## Valdi "Hello World"

The source for the `helloworld` app is located in `./apps/helloworld` of the [github.com/Snapchat/Valdi](https://github.com/Snapchat/Valdi) repository.

From the root of your Valdi clone, you can build the `helloworld` Valdi app. Start an emulator/simulator/development device of your choice and use the correct install script for iOS and Android

For Android:

```
valdi install android
```

Select the `//apps/helloworld:hello_world_android` target.

For iOS:

```
valdi install ios
```

Select the `//apps/helloworld:hello_world_ios` target.

Once the build is complete it will launch the `helloworld` app.

## Start the hotreloader

The hotreloader tool will watch for changes to the Valdi source code, recompile, and re-render the application UI with the changed Valdi modules.

```
valdi hotreload --module hello_world
```

> **CLI Reference:** For more details on available Valdi CLI commands, see [Command Line References](../../docs/command-line-references.md).

The hotreloader process will print to the console when it is ready to build your changes:

```
[3.203] [INFO] Recompilation pass finished, waiting for file changes...
[3.203] [INFO] --------------------------------------------------------
```

## Make Some Changes

Open [HelloWorldApp.tsx](../../../apps/helloworld/src/valdi/hello_world/src/HelloWorldApp.tsx) in VS Code.

```
code apps/helloworld/src/valdi/hello_world/src/HelloWorldApp.tsx
```

Find the `<view>` that describes the background ([source](../../../apps/helloworld/src/valdi/hello_world/src/HelloWorldApp.tsx#L34)):

```tsx
    <view backgroundColor='white'>
```

Change the `backgroundColor` to `blue` (or any other supported CSS color value):

```tsx
    <view backgroundColor='blue'>
```

The hotreloader process will begin rebuilding the changed module and will update the running application.

```
[387.506] [INFO] --- Files changed - starting recompilation pass
# ... additional logs
[387.823] [INFO] [Hot reloader] sending 2 updated resources to 1 connected client(s)...
[387.823] [INFO] Saving files
[387.823] [INFO] Compilation took 316ms
```

The `helloworld` application running on your development device/emulator/simulator will now reflect the changes you've made to the `<view>`.

## Find the codelab code

This is where the codelab starts in earnest.

We've created a separate component for you to play with. Open [`GettingStartedCodelab.tsx`](../../../apps/helloworld/src/valdi/hello_world/src/GettingStartedCodelab.tsx) in your editor.

It should look something like this:

```tsx
import { Component } from "valdi_core/src/Component";
import { systemBoldFont, systemFont } from "valdi_core/src/SystemFont";

export class GettingStartedCodelab extends Component {
    onRender(){
        <layout padding={24} paddingTop={128}>
            <label value='Getting started codelab!' font={systemBoldFont(16)} />
            <label value='github.com/Snapchat/Valdi' font={systemFont(12)} />
        </layout>
    }
}
```

Inside of `HelloWorldApp.tsx` import `GettingStartedCodelab` then update the `onRender` method to render the `GettingStartedCodelab` component:

```tsx
import { GettingStartedCodelab } from './GettingStartedCodelab';
```

```tsx
  onRender(): void {
    console.log('Hello World onRender!!!');
    <view backgroundColor='white'>
      <GettingStartedCodelab />
    </view>
  }
```

The hotreloader will rebuild the app and the `GettingStartedCodelab` component will be visible on the screen.

If you see your change, you're ready to move on to the next step.

## Finding help
If your UI isn't updating or you're seeing other issues, check out the [How to get help](../how_to_get_help.md) section.


### [Next >](./4-declarative_rendering.md)
