# SectionList
SectionList is a [Valdi_Widgets][] component that is used by many features to display complex lists with headers.

## Render the list

Import the `SectionList` component from the `valdi_widgets` library.

```tsx
import { SectionList } from 'valdi_widgets/src/components/section/SectionList';
```

Update `onRender` to include a `SectionList`.

```tsx
onRender() {
    <view backgroundColor="white">
        <scroll>
            <SectionList />
        </scroll>
    </view>;
}
```

VSCode is going to give you some angry squiggles because `SectionList` is missing some parameters.

## ScrollViewHandler

The `SectionList` needs to share a `ScrollViewHandler` with it's parent `<scroll>` to subscribe to various scroll events.

Import `ScrollViewHandler` from `valdi_widgets`

```tsx
import { ScrollViewHandler } from 'valdi_widgets/src/components/scroll/ScrollViewHandler';
```

Create a `ScrollViewHandler` instance as a property on the .

```tsx
export class App extends Component<ViewModel, ComponentContext> {
    state: PlaygroundState = {};

    private scrollViewHandler = new ScrollViewHandler();
```

Pass it to the parent `<scroll>` and the `SectionList`.

```tsx
<view padding='60 20' backgroundColor={SemanticColor.Background.SUBSCREEN}>
    <scroll ref={this.scrollViewHandler}>
        <SectionList scrollViewHandler={this.scrollViewHandler} />
    </scroll>
</view>;
```

We're getting closer, but VSCode is still unhappy.

## Sections

Sections in the `<SectionList>` are specified as [`SectionModel`s](https://github.com/Snapchat/Valdi_Widgets/blob/main/valdi_modules/widgets/src/components/section/SectionModel.d.ts#L8).

```typescript
export interface SectionModel {
    /**
    * A unique key which identifies this Section.
    */
    key: string;
    /**
    * Render function for injecting anchors right above the section header
    * (used for anchors because it will not be translated by the sticky headers)
    */
    onRenderAnchor?: RenderFunctionAnchor;
    /**
    * Render function for rendering the visual content of the header of the section.
    * (do not use this for anchor because it may get translated by the sticky headers)
    */
    onRenderHeader?: RenderFunctionHeader;
    /**
    * Render function for rendering the body of the section.
    */
    onRenderBody: RenderFunctionBody;
    /**
    * When created, this will make the section roll-in fading
    */
    animated?: boolean;
}
```

The only required variables in this object are the `key` and `onRenderBody`. `key` is a unique identifier, and `onRenderBody` is a render function (like `onRender`) that creates the views in the section.

Let’s create a `SectionModel` for our `SectionList`.

Define a class variable to hold onto our models.

```typescript
export class Playground extends Component<ViewModel, ComponentContext> {
    private sections: SectionModel[] = [];
    
    ...
```

Then implement `onCreate` and use it to create a section.

```typescript
onCreate() {
    this.sections.push({
        key: 'one',
        onRenderBody: () => {
            <label value={'One'}></label>;
        },
    });
}
```

`key` can be whatever you want as long as it’s unique, but `onRenderBody` functions in a similar manner to the Component's `onRender`. This is where you render the UI for this particular section.

Now let’s hook it up to the `SectionList`

```typescript
<SectionList scrollViewHandler={this.scrollViewHandler} sections={this.sections} />
```

## Render a list
`SectionList`s are usually used to render lists so let's create one.

Create a list of `captains` in `onCreate` and then iterate through it.

```typescript
onCreate() {
    const captains = ['Picard', 'Janeway', 'Sisko'];

    this.sections.push({
    key: 'one',
    onRenderBody: () => {
        captains.forEach(captain => {
            <label value={captain}></label>;
            });
        },
    });
}
```

## Lists of lists

If we want to render multiple sections, things are going to get complicated so let’s do some refactoring.

First let’s separate the data from the rendering.

Above the `Playground` component definition, create a new interface to hold on to your data. 

```typescript
interface OfficersData {
  officers: string[];
  title: string;
  id: string;
}
```

When you are working with your own data this might be SnapDoc or some data format from native. Each object will need a unique `id` to identify it to the `SectionList`.

Now let's implement a `getData()` function to create our data structure.

```typescript
private getData(): OfficersData[] {
    return [
        {
            officers: ['Picard', 'Janeway', 'Sisko'],
            title: 'Captains',
            id: 'starTrekCaptains',
        },
        {
            officers: ['Riker', 'Chakotay', 'Kira'],
            title: 'First officers',
            id: 'starTrekFirstOfficers',
        },
    ];
}
```

With your own feature, `getData()` will be a call to native or to the server.

Now our `onCreate()` function becomes: 

```typescript
onCreate() {
    const data = this.getData();

    data.forEach(section => {
        this.sections.push({
            key: section.id,
            onRenderBody: () => {
                section.officers.forEach(officer => {
                    <label value={officer}></label>;
                });
            },
        });
    });
}
```

Fetch the data, iterate through all of the objects, and create a section for each.

We have titles now so let's add those in. Add an `onRenderHeader` function into the sections.

```typescript
import { systemFont } from 'valdi_core/src/SystemFont';
```

```typescript
onRenderHeader: () => {
    <label value={section.title} font={systemFont(20)} />;
},
```

## Fancy rendering

`onRenderBody` and `onRenderHeader` are plain old render functions and you can do the same things with them that you can with a component's `onRender`, but there are a few custom components built to work specifically with the `SectionList`.

### SectionBody

[`<SectionBody>`](https://github.com/Snapchat/Valdi_Widgets/blob/main/valdi_modules/widgets/src/components/section/SectionBody.tsx#L13) is a utility component that applies default padding with a flag to make the section the full width of the parent view.

Let's wrap our `officer` label in a view with a background color so we can see what's up and then put the whole thing in a `<SectionBody>`.

```typescript
section.officers.forEach(officer => {
    <SectionBody>
        <view backgroundColor='lightblue'>
            <label value={officer} />
        </view>
    </SectionBody>;
});
```

Take a look at how your officers are rendering. Then set `fullBleed={true}` on the `<SectionBody>` to see how it changes.

### SectionHeader

[`SectionHeader`](https://github.com/Snapchat/Valdi_Widgets/blob/main/valdi_modules/widgets/src/components/section/SectionHeader.tsx#L50) has a bunch of fancy rendering options.

Let's swap out our `onRenderHeader` implementation with a `<SectionHeader>`.

```typescript
onRenderHeader: () => {
    <SectionHeader title={section.title} />;
},
```

The only required parameter is **`title`** but you can add a **`subTitle`**, **`description`**, and additional config as well.

What’s most interesting, however, is the built in **`actionButton`**. Let’s add one.

For this, let’s add an **`action`** function pointer to our data model.

```typescript
interface OfficersData {
  officers: string[];
  title: string;
  id: string;
  action: () => void;
}
```

Then update our `getData()` function.

```typescript
private getData(): OfficersData[] {
    return [
        {
            officers: ['Picard', 'Janeway', 'Sisko'],
            title: 'Captains',
            id: 'starTrekCaptains',
            action: () => {
                console.log('tap captains');
            },
        },
        {
            officers: ['Riker', 'Chakotay', 'Kira'],
            title: 'First officers',
            id: 'starTrekFirstOfficers',
            action: () => {
                console.log('tap first officers');
            },
        },
    ];
}
```

For now we’re just logging some debug information.

Now we can hook this up to our section header.

```typescript
<SectionHeader
    title={section.title}
    actionButton={{ label: 'Learn more', onTap: section.action, type: 'navigation' }}
/>;
```

Check out the UI, see how it renders, play with the other parameters.

## Full solution

Here's the full solution if you need it.

```tsx
import { StatefulComponent } from 'valdi_core/src/Component';
import { ScrollViewHandler } from 'valdi_widgets/src/components/scroll/ScrollViewHandler';
import { SectionBody } from 'valdi_widgets/src/components/section/SectionBody';
import { SectionHeader } from 'valdi_widgets/src/components/section/SectionHeader';
import { SectionList } from 'valdi_widgets/src/components/section/SectionList';
import { SectionModel } from 'valdi_widgets/src/components/section/SectionModel';


interface OfficersData {
  officers: string[];
  title: string;
  id: string;
  action: () => void;
}

/**
 * @Context
 * @ExportModel
 */
export interface ComponentContext {}

/**
 * @Component
 * @ExportModel
 */
export class App extends Component<ViewModel, ComponentContext> {
  private sections: SectionModel[] = [];

  private scrollViewHandler = new ScrollViewHandler();

  onCreate() {
    const data = this.getData();

    data.forEach(section => {
      this.sections.push({
        key: section.id,
        onRenderBody: () => {
          section.officers.forEach(officer => {
            <SectionBody>
              <view backgroundColor='lightblue'>
                <label value={officer}></label>
              </view>
            </SectionBody>;
          });
        },
        onRenderHeader: () => {
          <SectionHeader
            title={section.title}
            actionButton={{ label: 'Learn more', onTap: section.action, type: 'navigation' }}
          />;
        },
      });
    });
  }

  private getData(): OfficersData[] {
    return [
      {
        officers: ['Picard', 'Janeway', 'Sisko'],
        title: 'Captains',
        id: 'starTrekCaptains',
        action: () => {
          console.log('tap captains');
        },
      },
      {
        officers: ['Riker', 'Chakotay', 'Kira'],
        title: 'First officers',
        id: 'starTrekFirstOfficers',
        action: () => {
          console.log('tap first officers');
        },
      },
    ];
  }

  onRender() {
    <view padding='60 20' backgroundColor="white">
      <scroll>
        <scroll ref={this.scrollViewHandler}>
          <SectionList scrollViewHandler={this.scrollViewHandler} sections={this.sections} />
        </scroll>
      </scroll>
    </view>;
  }
}

```

[Valdi_Widgets]: https://github.com/Snapchat/Valdi_Widgets
