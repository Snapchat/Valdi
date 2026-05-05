# The Style<>

## What is styling?

You may have noticed that most other examples we are using attributes to impact the rendering of specific components:

```tsx
<label
  color='black' // We are using the attribute color to change the rendering of the label
  value="HelloWorld"
/>
```

Any attribute applied on a native view is effectively styling that native view.

## The Problem

As the complexity of our feature grow we might encounter things like this:

```tsx
<view
  backgroundColor="white"
  flexDirection="row"
  justifyContent="center"
  alignItems="center"
  padding={10}
  margin={10}
  borderRadius={10}
  boxShadow="0 8 11 rgba(0, 0, 0, 0.1)"
  // That's a lot of attributes
  // ... There can even be more
>
</view>
```

Or obvious duplication of attributes group is also very common

```tsx
<view
  // This view has a few attributes
  backgroundColor="lightblue"
  boxShadow="0 8 11 rgba(0, 0, 0, 0.1)"
  padding={10}
  margin={10}
  borderRadius={10}
>
  <image /* ... *//>
</view>
<view
  // This view is part of a different branch of the render tree
  // It has different contents
  // so we have to re-declare all our attributes
  backgroundColor="lightred"
  // Even tho this view shares a lot of attributes
  boxShadow="0 8 11 rgba(0, 0, 0, 0.1)"
  padding={10}
  margin={10}
  borderRadius={10}
>
  <label /* ... *//>
</view>
```

## The `Style<>` Object to the rescue

Fortunately we have access to the `Style<T>` object the style object allows applying many attributes at once on a single view.

So:

```tsx
const myStyle = new Style<View>({
  backgroundColor: "lightblue",
  width: 100,
  height: 100,
});
<view style={myStyle}/>
```

Is equivalent to:
```tsx
<view
  backgroundColor="lightblue"
  width={100}
  height={100}
/>
```

So this will be useful for styling large amount of elements and improve reuseability

## The `Style<>` In Example

```tsx
// Let's import the attribute interfaces for each type of
import { Layout, View } from 'NativeTemplateElements';
import { Style } from 'valdi_core/src/Style';
// Let's declare our groups of attributes that we know will be used together
const styles = {
  // This is a Style<View> so it can only be applied on <view>
  container: new Style<View>({
    backgroundColor: 'lightgrey',
    flexDirection: 'row',
    flexWrap: 'wrap',
  }),
  // This is a Style<Layout> so it can only be applied on <layout>
  // (note that a <view> is also a <layout>)
  square: new Style<Layout>({
    height: 100,
    width: 100,
  }),
  // We can make simple styles that we can combine later
  withMargin: new Style<Layout>({
    margin: 10,
  }),
  withPadding: new Style<Layout>({
    padding: 10,
  }),
  withRounded: new Style<View>({
    borderRadius: 10,
  }),
};

// Note that Styles<> can be combined!

// Using Style.merge
const itemStyle = Style.merge(styles.square, styles.withMargin, styles.withPadding, styles.withRounded);
// Using Style.extend
const imageStyle = styles.withRounded.extend({
  height: '100%',
  width: '100%',
});

// We can then build a component that can uses those group of attributes (called Style(s))
export class HelloWorld extends Component {
  onRender() {
    // And now we can re-use those styles on many different elements in the same template!
    <view style={styles.container}>
      <view style={itemStyle} backgroundColor='lightgreen'>
        <image style={imageStyle} src='https://placedog.net/500' />
      </view>
      <view style={itemStyle} backgroundColor='lightpink'>
        <image style={imageStyle} src='https://placedog.net/500' />
      </view>
      <view style={itemStyle} backgroundColor='lightblue'>
        <image style={imageStyle} src='https://placedog.net/500' />
      </view>
    </view>;
  }
}
```
<img src="assets/core-styling/IMG_1469.jpg" align="right" width="250" border="5" />

## The Good, the Style and the Attribute

An important property of the Style object is that it will always be overrwritten by any attributes set on the element:

```tsx
const myStyle = new Style<View>({
  height: 100,
  width: 100,
  backgroundColor: 'red',
});
// height: 100, width: 100, backgroundColor: red
<view style={myStyle}/>;
// height: 100, width: 100, backgroundColor: blue
<view style={myStyle} backgroundColor='blue'>;
```

That way you can define any immutable values in your style and then dynamically override any dynamic value using a regular attribute.

## Performance considerations

Note that styles are the perfect usecase for static and invariant attributes but should generally be avoided to be instantiated during rendering, for example:

```tsx
// GOOD (at init-time)
const myCheapStyle = new Style<View>({
  width: 100,
  height: 100,
  backgroundColor: 'red',
});
// GOOD also (at init-time)
const myCheapStyle2 = myCheapStyle.extend({})
const myCheapStyle3 = Style.merge(myCheapStyle, myCheapStyle2);

/**
 * Here is what to avoid
 */
class MySlowComponent {
  onRender() {
    // BAD - this will be an expensive call (with memory allocation and everything)
    const myExpensiveStyle = new Style<View>({
      width: 100,
      height: 100,
      backgroundColor: 'blue',
    });
    // BAD - merging or extending styles is also equivalent to instantiating a new Style
    const myExpensiveStyle2 = myExpensiveStyle.extend({})
    const myExpensiveStyle3 = Style.merge(myExpensiveStyle, myExpensiveStyle2);
    // height: 100, width: 100, backgroundColor: red
    <view style={myCheapStyle}/>;
    // height: 100, width: 100, backgroundColor: blue
    <view style={myExpensiveStyle}/>;
  }
}

/**
 * Here is what to do instead
 */
class MyFastComponent {
  onRender() {
    // height: 100, width: 100, backgroundColor: red
    <view style={myCheapStyle}/>;
    // height: 100, width: 100, backgroundColor: blue
    <view style={myCheapStyle} backgroundColor='blue'/>;
  }
}
```

Note that managing and manipulating styles is fine as long as we're not creating any new one:

```tsx
// Statically, we'll create all the styles we need at initialization
const item = new Style<View>({
  width: 100,
  height: 100,
});
const styles = {
  itemSelected: item.extend<View>({
    backgroundColor: 'blue'
  }),
  itemDeselected: item.extend<View>({
    backgroundColor: 'white'
  }),
}
// Dynamically we'll use our existing styles
interface State {
  selected: boolean;
}
class MyFastComponentWithLogic extends StatefulComponent<State> {
  onRender() {
    // this is fast, no style creation here
    <view style={this.state.selected ? styles.itemSelected : styles.itemDeselected}/>
  }
}
```

## TLDR

- DO use Style for batches of static attributes
- using Style is extremely fast (faster than attributes)
- **Style Interning**: The first time a style instance is sent to the native runtime, it is assigned a unique integer identifier. Subsequent uses of the same style object only require passing this ID, drastically reducing marshalling overhead. This is why you should reuse style objects instead of creating them on the fly.
- instantiating/merging/extending Style is costly, don't do it at render-time
- instantiating/merging/extending Style should be done once, at initialization or lazy loaded
- use attributes for any dynamic value
- you can also swap styles for dynamic values with limited granularity

## Complete Style Attributes Reference

For a comprehensive list of all attributes that can be used in styles for each element type, including Yoga flexbox properties, see the [Style Attributes Reference](../api/api-style-attributes.md).
