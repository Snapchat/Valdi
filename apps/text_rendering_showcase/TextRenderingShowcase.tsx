import { Component, StatefulComponent } from 'valdi_core/src/Component';
import { Device } from 'valdi_core/src/Device';
import { Style } from 'valdi_core/src/Style';
import { systemBoldFont, systemFont } from 'valdi_core/src/SystemFont';
import { AttributedTextBuilder } from 'valdi_core/src/utils/AttributedTextBuilder';
import { makeAssetFromBytes } from 'valdi_core/src/Asset';
import { Asset } from 'valdi_tsx/src/Asset';
import { AttributedTextInlineViewVerticalAlignment } from 'valdi_tsx/src/AttributedTextInlineViewAttachment';
import {
  ImageView,
  Label,
  ScrollView,
  TextAnimationGroup,
  TextField,
  TextView,
  View,
} from 'valdi_tsx/src/NativeTemplateElements';
import { createManagedContext, EmbeddedPlatformViewRasterMethod } from 'drawing/src/ManagedContextFactory';
import { BitmapAlphaType, BitmapColorType, ImageEncoding } from 'drawing/src/IBitmap';
import { createBitmap } from 'drawing/src/BitmapFactory';

const SNAPSHOT_WIDTH = 330;
const SNAPSHOT_HEIGHT = 230;

type CategoryId = 'label' | 'inputs' | 'selection' | 'animation' | 'inline' | 'drawing';

interface Fixture {
  id: string;
  category: CategoryId;
  title: string;
  description: string;
}

interface Category {
  id: CategoryId;
  title: string;
}

interface State {
  selectedCategory: CategoryId;
  selectedFixtureId: string;
  eventLog: string;
  fieldValue: string;
  textViewValue: string;
  animationRun: number;
  snapAsset?: Asset;
  snapStatus: string;
}

/**
 * @ViewModel
 * @ExportModel
 */
export interface ViewModel {}

const categories: Category[] = [
  { id: 'label', title: 'Labels' },
  { id: 'inputs', title: 'Inputs' },
  { id: 'selection', title: 'Selection' },
  { id: 'animation', title: 'Animation' },
  { id: 'inline', title: 'Inline' },
  { id: 'drawing', title: 'Drawing' },
];

const fixtures: Fixture[] = [
  {
    id: 'label-styling',
    category: 'label',
    title: 'Label styling',
    description:
      'Shows system fonts, italic/bold descriptors, text gradients, shadows, custom dashed and dotted underlines, lineHeight, lineHeightMultiple, text overflow, and attributed text backgrounds.',
  },
  {
    id: 'attributed-parts',
    category: 'label',
    title: 'Attributed text parts',
    description:
      'Shows per-run font/color/background/outline/tap/onLayout attributes plus an inline image attachment inside one label value.',
  },
  {
    id: 'textfield',
    category: 'inputs',
    title: 'TextField parity',
    description:
      'Shows placeholder color, tint color, alignment, content type, return key, selection, select-on-focus, character limit, and edit callbacks.',
  },
  {
    id: 'textview',
    category: 'inputs',
    title: 'TextView parity',
    description:
      'Shows multiline TextView line height, gravity, decoration, text overflow, noneditable rendering, and the new background effect attributes.',
  },
  {
    id: 'selection-menu',
    category: 'selection',
    title: 'Selection and menu',
    description:
      'Shows selectable label and TextView content, initial selection ranges, selection callbacks, and iOS custom selection menu hooks.',
  },
  {
    id: 'text-animation',
    category: 'animation',
    title: 'Text animations',
    description:
      'Shows attributed text animation transforms, regex part splitting, restart keys, and persistent animation identities.',
  },
  {
    id: 'character-animation',
    category: 'animation',
    title: 'Character reveal',
    description:
      'Shows a progressive character-by-character reveal using a partPattern that splits the animated run at every character.',
  },
  {
    id: 'animation-group',
    category: 'animation',
    title: 'Animation group',
    description:
      'Shows textanimationgroup coordinating a progressive animation across labels, nested descendants, and a TextView.',
  },
  {
    id: 'inline-children',
    category: 'inline',
    title: 'Inline children',
    description:
      'Shows label and TextView inline child attachments with top, center, bottom, and baseline vertical alignment.',
  },
  {
    id: 'inline-animation',
    category: 'inline',
    title: 'Inline child animation',
    description:
      'Shows a real inline child view changing size and animated text around it, exercising inline child frame updates and animation preservation.',
  },
  {
    id: 'snapdrawing',
    category: 'drawing',
    title: 'SnapDrawing text',
    description:
      'Shows Valdi text rendered through a managed SnapDrawing context. Tap Rasterize to produce a bitmap from Label and TextView layers with text-field chrome in the same scene.',
  },
];

const richAttributedText = new AttributedTextBuilder()
  .append('Rich ', { color: '#1D4ED8', font: 'system-bold 20' })
  .append('background ', {
    backgroundBorderRadius: 6,
    backgroundColor: '#DBEAFE',
    backgroundPadding: { bottom: 2, left: 6, right: 6, top: 2 },
    color: '#1E3A8A',
    font: 'system 20',
  })
  .append('dotted underline ', { color: '#BE123C', font: 'system-italic 19', textDecoration: 'dotted-underline' })
  .append('outlined', {
    color: '#FFFFFF',
    font: 'system-bold 20',
    outlineColor: '#0F172A',
    outlineWidth: 1,
  })
  .build();

const inlineImageText = new AttributedTextBuilder()
  .append('Text before inline image ', { color: '#334155', font: 'system 18' })
  .appendInlineImage({
    attachmentId: 'blue-dot',
    height: 20,
    imageData: new Uint8Array([
      0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00,
      0x14, 0x00, 0x00, 0x00, 0x14, 0x08, 0x06, 0x00, 0x00, 0x00, 0x8d, 0x89, 0x1d, 0x0d, 0x00, 0x00, 0x00, 0x1e, 0x49,
      0x44, 0x41, 0x54, 0x78, 0xda, 0x63, 0x50, 0x4d, 0x7e, 0xfd, 0x9f, 0x9a, 0x98, 0x61, 0xd4, 0xc0, 0x51, 0x03, 0x47,
      0x0d, 0x1c, 0x35, 0x70, 0xd4, 0xc0, 0x91, 0x6a, 0x20, 0x00, 0x2b, 0x9f, 0xd2, 0x4e, 0x5b, 0x3b, 0xbe, 0xf8, 0x00,
      0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
    ]),
    width: 20,
  })
  .append(' after it.', { color: '#047857', font: 'system-bold 18' })
  .build();

const labelInlineText = new AttributedTextBuilder()
  .append('Top ')
  .appendInlineView(0, AttributedTextInlineViewVerticalAlignment.Top)
  .append(' Center ')
  .appendInlineView(1, AttributedTextInlineViewVerticalAlignment.Center)
  .append(' Bottom ')
  .appendInlineView(2, AttributedTextInlineViewVerticalAlignment.Bottom)
  .append(' Baseline ')
  .appendInlineView(3, AttributedTextInlineViewVerticalAlignment.Baseline)
  .append(' attachments wrap with surrounding label text.')
  .build();

const textViewInlineText = new AttributedTextBuilder()
  .append('TextView inline markers: ')
  .appendInlineView(0, AttributedTextInlineViewVerticalAlignment.Top)
  .append(' ')
  .appendInlineView(1, AttributedTextInlineViewVerticalAlignment.Center)
  .append(' ')
  .appendInlineView(2, AttributedTextInlineViewVerticalAlignment.Bottom)
  .append(' ')
  .appendInlineView(3, AttributedTextInlineViewVerticalAlignment.Baseline)
  .append(' placed by native text layout.')
  .build();

interface MarkerViewModel {
  color: string;
  height: number;
  title: string;
  width: number;
}

class InlineMarker extends Component<MarkerViewModel> {
  onRender(): void {
    <view
      style={styles.inlineMarker.extend({
        backgroundColor: this.viewModel.color,
        height: this.viewModel.height,
        width: this.viewModel.width,
      })}
    >
      <label style={styles.inlineMarkerLabel} value={this.viewModel.title} />
    </view>;
  }
}

interface ExpandingInlineState {
  expanded: boolean;
}

class ExpandingInlineButton extends StatefulComponent<{}, ExpandingInlineState> {
  state: ExpandingInlineState = {
    expanded: false,
  };

  onRender(): void {
    const expanded = this.state.expanded;
    <view
      style={expanded ? styles.expandingInlineButtonExpanded : styles.expandingInlineButtonCollapsed}
      onTap={this.toggle}
    >
      <label
        style={expanded ? styles.expandingInlineButtonLabelExpanded : styles.expandingInlineButtonLabelCollapsed}
        value={expanded ? 'Contract inline' : 'Expand'}
      />
    </view>;
  }

  private toggle = () => {
    this.setState({ expanded: !this.state.expanded });
  };
}

class SnapDrawingTextScene extends Component {
  onRender(): void {
    <view style={styles.snapScene}>
      <label style={styles.snapSceneTitle} value="SnapDrawing Label" />
      <view style={styles.snapSceneChrome}>
        <label style={styles.snapSceneChromeLabel} value="TextField-style chrome" />
      </view>
      <textview style={styles.snapSceneTextView} value={richAttributedText} />
    </view>;
  }
}

/**
 * @Component
 * @ExportModel
 */
export class App extends StatefulComponent<ViewModel, State> {
  state: State = {
    animationRun: 0,
    eventLog: 'Tap a fixture card to render it here.',
    fieldValue: 'user@example.com',
    selectedCategory: 'label',
    selectedFixtureId: 'label-styling',
    snapStatus: 'Tap Rasterize to draw the scene through SnapDrawing.',
    textViewValue: 'Editable multiline text\nwith line returns.',
  };

  onRender(): void {
    const fixture = this.selectedFixture();

    <view style={styles.screen}>
      <scroll style={styles.screenScroll}>
        <view style={styles.screenContent.extend({ paddingTop: 16 + Device.getDisplayTopInset() })}>
          <label style={styles.appTitle} value="Text Rendering Showcase" />
          <label
            style={styles.appSubtitle}
            value="Tap a category, then tap a fixture. Each fixture names the backported feature, explains what to inspect, and renders a focused scene."
          />

          {this.renderCategoryTabs()}
          {this.renderFixtureTabs()}

          <view style={styles.detailCard}>
            <label style={styles.detailTitle} value={fixture.title} />
            <label style={styles.detailDescription} value={fixture.description} />
            {this.renderSelectedFixture(fixture.id)}
          </view>

          <view style={styles.eventCard}>
            <label style={styles.eventTitle} value="Interaction log" />
            <label style={styles.eventText} value={this.state.eventLog} />
          </view>
        </view>
      </scroll>
    </view>;
  }

  private renderCategoryTabs(): void {
    <scroll style={styles.categoryScroll}>
      <view style={styles.categoryRow}>
        {categories.forEach(category => {
          const selected = this.state.selectedCategory === category.id;
          <view
            style={selected ? styles.categoryTabSelected : styles.categoryTab}
            onTap={() => this.selectCategory(category.id)}
          >
            <label
              style={selected ? styles.categoryTabLabelSelected : styles.categoryTabLabel}
              value={category.title}
            />
          </view>;
        })}
      </view>
    </scroll>;
  }

  private renderFixtureTabs(): void {
    <view style={styles.fixtureList}>
      {fixtures
        .filter(fixture => fixture.category === this.state.selectedCategory)
        .forEach(fixture => {
          const selected = fixture.id === this.state.selectedFixtureId;
          <view
            style={selected ? styles.fixtureCardSelected : styles.fixtureCard}
            onTap={() => this.selectFixture(fixture.id)}
          >
            <label style={styles.fixtureTitle} value={fixture.title} />
            <label style={styles.fixtureDescription} value={fixture.description} />
          </view>;
        })}
    </view>;
  }

  private renderSelectedFixture(id: string): void {
    switch (id) {
      case 'label-styling':
        this.renderLabelStylingFixture();
        return;
      case 'attributed-parts':
        this.renderAttributedPartsFixture();
        return;
      case 'textfield':
        this.renderTextFieldFixture();
        return;
      case 'textview':
        this.renderTextViewFixture();
        return;
      case 'selection-menu':
        this.renderSelectionFixture();
        return;
      case 'text-animation':
        this.renderTextAnimationFixture();
        return;
      case 'character-animation':
        this.renderCharacterAnimationFixture();
        return;
      case 'animation-group':
        this.renderAnimationGroupFixture();
        return;
      case 'inline-children':
        this.renderInlineChildrenFixture();
        return;
      case 'inline-animation':
        this.renderInlineAnimationFixture();
        return;
      case 'snapdrawing':
        this.renderSnapDrawingFixture();
        return;
      default:
        <label style={styles.errorLabel} value={`Unknown fixture: ${id}`} />;
    }
  }

  private renderLabelStylingFixture(): void {
    <view style={styles.fixtureRoot}>
      <label style={styles.labelUnscaled} value="system-bold 24 unscaled" />
      <label style={styles.labelItalicSpacing} value="system italic with letter spacing" />
      <label style={styles.labelDashedUnderline} value="custom dashed underline" />
      <label style={styles.labelDottedUnderline} value="custom dotted underline" />
      <label style={styles.labelGradient} value="gradient text" />
      <label style={styles.labelShadow} value="shadowed text" />
      <label
        style={styles.labelJustified}
        value="Justified multiline label uses explicit lineHeight. This wraps across several lines so spacing and paragraph alignment are visible."
      />
      <label style={styles.labelAutoshrink} value="autoshrink plus ellipsis for a deliberately long one-line label" />
      <label style={styles.richLabel} value={richAttributedText} />
    </view>;
  }

  private renderAttributedPartsFixture(): void {
    <view style={styles.fixtureRoot}>
      <label
        style={styles.attributedIntro}
        value="Tap the blue phrase below to verify attributed onTap. The inline image should sit on the text baseline."
      />
      <label
        style={styles.attributedLabel}
        value={new AttributedTextBuilder()
          .append('Tap ')
          .append('this attributed span', {
            backgroundBorderRadius: 5,
            backgroundColor: '#DBEAFE',
            backgroundPadding: { bottom: 2, left: 5, right: 5, top: 2 },
            color: '#1D4ED8',
            font: 'system-bold 18',
            onLayout: (x, y, width, height) =>
              this.record(
                `onLayout span ${Math.round(width)}x${Math.round(height)} at ${Math.round(x)},${Math.round(y)}`,
              ),
            onTap: () => this.record('Tapped attributed text span'),
          })
          .append(' and inspect outline + image. ', { color: '#334155' })
          .build()}
      />
      <label style={styles.inlineImageLabel} value={inlineImageText} />
      <label
        style={styles.attributedOutlineLabel}
        value={new AttributedTextBuilder()
          .append('Attributed outline', {
            color: '#FFFFFF',
            font: 'system-bold 22',
            outlineColor: '#0F172A',
            outlineWidth: 1,
          })
          .build()}
      />
    </view>;
  }

  private renderTextFieldFixture(): void {
    <view style={styles.fixtureRoot}>
      <textfield
        style={styles.primaryTextField}
        value={this.state.fieldValue}
        onChange={event => this.updateField(event.text)}
        onEditBegin={event => this.record(`textfield begin ${event.text.length}`)}
        onEditEnd={event => this.record(`textfield end reason ${event.reason}`)}
        onReturn={event => this.record(`textfield return ${event.text}`)}
        onSelectionChange={event => this.record(`textfield selection ${event.selectionStart}-${event.selectionEnd}`)}
        onWillChange={event => {
          this.record(`textfield willChange ${event.text.length}`);
          return event;
        }}
        onWillDelete={event => this.record(`textfield willDelete ${event.text.length}`)}
      />
      <view style={styles.textFieldRow}>
        <textfield style={styles.phoneTextField} value="+1 555 0100" />
        <textfield style={styles.passwordTextField} value="visible-secret" />
      </view>
      <textfield style={styles.disabledTextField} value="-123.45 disabled selectable" />
    </view>;
  }

  private renderTextViewFixture(): void {
    <view style={styles.fixtureRoot}>
      <textview
        style={styles.primaryTextView}
        value={this.state.textViewValue}
        onChange={event => this.updateTextView(event.text)}
        onEditBegin={event => this.record(`textview begin ${event.text.length}`)}
        onEditEnd={event => this.record(`textview end reason ${event.reason}`)}
        onReturn={event => this.record(`textview return ${event.text.length}`)}
        onSelectionChange={event => this.record(`textview selection ${event.selectionStart}-${event.selectionEnd}`)}
      />
      <textview
        style={styles.disabledTextView}
        value="Disabled TextView renders styled static content, clamps to two lines, and should not scroll while still allowing native text selection when selectable is true."
      />
    </view>;
  }

  private renderSelectionFixture(): void {
    <view style={styles.fixtureRoot}>
      <label
        style={styles.selectableLabel}
        value="Selectable label: long-press or drag handles, then try the custom iOS action menu."
        onSelectionChange={event => this.record(`label selection ${event.selectionStart}-${event.selectionEnd}`)}
        onTextSelectionMenu={event => [
          {
            id: 'log-label',
            title: `Log ${event.selectedText.length}`,
          },
        ]}
        onTextSelectionMenuAction={event => this.record(`label menu ${event.id}: ${event.selectedText}`)}
      />
      <textview
        style={styles.selectableTextView}
        value="Selectable TextView: verify disabled selection, copy handles, and the iOS custom menu action."
        onSelectionChange={event => this.record(`textview selection ${event.selectionStart}-${event.selectionEnd}`)}
        onTextSelectionMenu={event => [
          {
            id: 'log-textview',
            title: `Log ${event.selectedText.length}`,
          },
        ]}
        onTextSelectionMenuAction={event => this.record(`textview menu ${event.id}: ${event.selectedText}`)}
      />
    </view>;
  }

  private renderTextAnimationFixture(): void {
    <view style={styles.fixtureRoot}>
      <view style={styles.primaryButton} onTap={this.restartAnimation}>
        <label style={styles.primaryButtonLabel} value="Restart animation" />
      </view>
      <label
        style={styles.textAnimationLabel}
        value={this.animatedText('Words animate as persistent attributed text parts.', {
          key: `persistent-label-${this.state.animationRun}`,
          partPattern: '\\w+',
        })}
      />
      <textview
        style={styles.textAnimationTextView}
        value={this.animatedText('A disabled TextView can animate attributed text too.', {
          key: `persistent-textview-${this.state.animationRun}`,
          partPattern: '\\w+',
          translationY: 14,
        })}
      />
    </view>;
  }

  private renderCharacterAnimationFixture(): void {
    <view style={styles.fixtureRoot}>
      <view style={styles.primaryButton} onTap={this.restartAnimation}>
        <label style={styles.primaryButtonLabel} value="Restart character reveal" />
      </view>
      <label
        style={styles.characterRevealLabel}
        value={this.animatedText('Every character appears progressively.', {
          key: `character-reveal-${this.state.animationRun}`,
          partPattern: '.',
          timeOffsetBetweenParts: 0.035,
          translationY: 6,
        })}
      />
      <label
        style={styles.helperNote}
        value="This fixture uses partPattern='.' so native text layout creates one animated part per character."
      />
    </view>;
  }

  private renderAnimationGroupFixture(): void {
    <view style={styles.fixtureRoot}>
      <view style={styles.primaryButton} onTap={this.restartAnimation}>
        <label style={styles.primaryButtonLabel} value="Start grouped fade" />
      </view>
      <textanimationgroup style={styles.group}>
        <view style={styles.groupLabelBlock}>{this.renderAnimatedLabel('One shared timeline starts here.')}</view>
        <view style={styles.groupNestedBlock}>
          {this.renderAnimatedLabel('Nested labels join the same sequence.')}
          <view style={styles.groupNestedChild}>
            {this.renderAnimatedLabel('This descendant runs later in the group.')}
          </view>
        </view>
        <textview
          style={styles.groupTextView}
          value={this.animatedText('TextView participates after labels.', {
            key: `group-textview-${this.state.animationRun}`,
            timeOffsetBetweenParts: 0.1,
          })}
        />
      </textanimationgroup>
    </view>;
  }

  private renderInlineChildrenFixture(): void {
    <view style={styles.fixtureRoot}>
      <label style={styles.inlineLabel} value={labelInlineText}>
        <InlineMarker color="#DC2626" height={14} title="TOP" width={48} />
        <InlineMarker color="#7C3AED" height={18} title="MID" width={48} />
        <InlineMarker color="#047857" height={22} title="BOT" width={48} />
        <InlineMarker color="#0F766E" height={18} title="BASE" width={52} />
      </label>
      <textview style={styles.inlineTextView} value={textViewInlineText}>
        <InlineMarker color="#EA580C" height={18} title="TOP" width={48} />
        <InlineMarker color="#2563EB" height={18} title="MID" width={48} />
        <InlineMarker color="#16A34A" height={18} title="BOT" width={48} />
        <InlineMarker color="#0F766E" height={18} title="BASE" width={52} />
      </textview>
    </view>;
  }

  private renderInlineAnimationFixture(): void {
    const value = new AttributedTextBuilder()
      .withStyle(
        {
          animationTransform: {
            duration: 0.45,
            key: `inline-whole-${this.state.animationRun}`,
            opacity: 0,
            scale: 0.96,
            translationY: 8,
          },
        },
        builder => {
          builder
            .append('Animated text before ')
            .appendInlineView(0, AttributedTextInlineViewVerticalAlignment.Center)
            .append(' and after the child view.');
        },
      )
      .build();

    <view style={styles.fixtureRoot}>
      <view style={styles.primaryButton} onTap={this.restartAnimation}>
        <label style={styles.primaryButtonLabel} value="Restart inline text animation" />
      </view>
      <label style={styles.inlineLabel} value={value}>
        <ExpandingInlineButton />
      </label>
      <label
        style={styles.helperNote}
        value="Tap the inline pill to resize it. Tap the blue button to restart text animation around the same inline child."
      />
    </view>;
  }

  private renderSnapDrawingFixture(): void {
    <view style={styles.fixtureRoot}>
      <view style={styles.primaryButton} onTap={this.rasterizeSnapDrawing}>
        <label style={styles.primaryButtonLabel} value="Rasterize SnapDrawing text" />
      </view>
      <label style={styles.snapStatus} value={this.state.snapStatus} />
      <view style={styles.snapshotContainer}>
        <SnapDrawingTextScene />
      </view>
      {this.state.snapAsset ? (
        <view style={styles.rasterizedOutput}>
          <label style={styles.rasterizedTitle} value="Rasterized output" />
          <image style={styles.rasterizedImage} src={this.state.snapAsset} />
        </view>
      ) : (
        <label style={styles.emptyRasterizedText} value="No rasterized bitmap yet." />
      )}
    </view>;
  }

  private renderAnimatedLabel(text: string): void {
    <label
      style={styles.groupLabel}
      value={this.animatedText(text, {
        key: `group-label-${this.state.animationRun}-${text}`,
        timeOffsetBetweenParts: 0.08,
      })}
    />;
  }

  private animatedText(
    text: string,
    overrides: { key: string; partPattern?: string; timeOffsetBetweenParts?: number; translationY?: number },
  ) {
    return new AttributedTextBuilder()
      .append(text, {
        animationTransform: {
          duration: 0.45,
          opacity: 0,
          scale: 0.96,
          timeOffsetBetweenParts: overrides.timeOffsetBetweenParts ?? 0.06,
          translationY: overrides.translationY ?? 10,
          ...overrides,
        },
      })
      .build();
  }

  private selectedFixture(): Fixture {
    return fixtures.find(fixture => fixture.id === this.state.selectedFixtureId) ?? fixtures[0];
  }

  private selectCategory = (category: CategoryId) => {
    const firstFixture = fixtures.find(fixture => fixture.category === category) ?? fixtures[0];
    this.setState({
      eventLog: `Selected category ${category}.`,
      selectedCategory: category,
      selectedFixtureId: firstFixture.id,
    });
  };

  private selectFixture = (fixtureId: string) => {
    const fixture = fixtures.find(candidate => candidate.id === fixtureId) ?? fixtures[0];
    this.setState({
      eventLog: `Selected fixture ${fixture.title}.`,
      selectedCategory: fixture.category,
      selectedFixtureId: fixture.id,
    });
  };

  private updateField(text: string): void {
    this.setState({
      eventLog: `textfield change "${text}"`,
      fieldValue: text,
    });
  }

  private updateTextView(text: string): void {
    this.setState({
      eventLog: `textview change length ${text.length}`,
      textViewValue: text,
    });
  }

  private restartAnimation = () => {
    this.setState({
      animationRun: this.state.animationRun + 1,
      eventLog: `Restarted text animations (${this.state.animationRun + 1}).`,
    });
  };

  private record(message: string): void {
    this.setState({ eventLog: message });
  }

  private rasterizeSnapDrawing = () => {
    this.setState({ snapStatus: 'Rasterizing SnapDrawing text...' });
    this.doRasterizeSnapDrawing()
      .then(asset => {
        this.setState({
          eventLog: 'SnapDrawing rasterization complete.',
          snapAsset: asset,
          snapStatus: 'Rasterized successfully.',
        });
      })
      .catch(error => {
        const message = error instanceof Error ? error.message : String(error);
        this.setState({
          eventLog: `SnapDrawing rasterization failed: ${message}`,
          snapStatus: `Rasterization failed: ${message}`,
        });
      });
  };

  private async doRasterizeSnapDrawing(): Promise<Asset> {
    const context = createManagedContext({
      deltaRasterization: true,
      embeddedPlatformViewRasterMethod: EmbeddedPlatformViewRasterMethod.ACCURATE,
    });
    context.render(() => {
      <SnapDrawingTextScene />;
    });

    await context.onAllAssetsLoaded();
    await context.layout(SNAPSHOT_WIDTH, SNAPSHOT_HEIGHT, false);

    const { frame } = await context.draw();
    const scale = Device.getDisplayScale();
    const width = Math.floor(SNAPSHOT_WIDTH * scale);
    const height = Math.floor(SNAPSHOT_HEIGHT * scale);
    const bytesPerPixel = 4;
    const bitmap = createBitmap({
      alphaType: BitmapAlphaType.OPAQUE,
      colorType: BitmapColorType.RGBA8888,
      height,
      rowBytes: width * bytesPerPixel,
      width,
    });

    frame.rasterInto(bitmap, true);
    const buffer = bitmap.encode(ImageEncoding.PNG, 1.0);

    frame.dispose();
    bitmap.dispose();
    context.dispose();

    return makeAssetFromBytes(buffer);
  }
}

const styles = {
  appSubtitle: new Style<Label>({
    color: '#475569',
    font: systemFont(15),
    lineHeightMultiple: 1.25,
    marginBottom: 14,
    numberOfLines: 0,
    width: '100%',
  }),

  appTitle: new Style<Label>({
    color: '#0F172A',
    font: systemBoldFont(27),
    marginBottom: 5,
    width: '100%',
  }),

  attributedIntro: new Style<Label>({
    color: '#334155',
    font: systemFont(14),
    lineHeightMultiple: 1.25,
    marginBottom: 10,
    numberOfLines: 0,
    width: '100%',
  }),

  attributedLabel: new Style<Label>({
    color: '#0F172A',
    font: systemFont(18),
    lineHeightMultiple: 1.35,
    numberOfLines: 0,
    width: '100%',
  }),

  attributedOutlineLabel: new Style<Label>({
    color: '#FFFFFF',
    font: systemBoldFont(22),
    marginTop: 10,
    numberOfLines: 0,
    width: '100%',
  }),

  categoryRow: new Style<View>({
    flexDirection: 'row',
    height: 48,
    width: 620,
  }),

  categoryScroll: new Style<ScrollView>({
    height: 48,
    horizontal: true,
    showsHorizontalScrollIndicator: false,
    width: '100%',
  }),

  categoryTab: new Style<View>({
    alignItems: 'center',
    backgroundColor: '#E2E8F0',
    borderRadius: 999,
    height: 38,
    justifyContent: 'center',
    marginRight: 8,
    paddingLeft: 14,
    paddingRight: 14,
  }),

  categoryTabLabel: new Style<Label>({
    color: '#334155',
    font: systemBoldFont(13),
    textAlign: 'center',
    width: '100%',
  }),

  categoryTabLabelSelected: new Style<Label>({
    color: '#FFFFFF',
    font: systemBoldFont(13),
    textAlign: 'center',
    width: '100%',
  }),

  categoryTabSelected: new Style<View>({
    alignItems: 'center',
    backgroundColor: '#0F172A',
    borderRadius: 999,
    height: 38,
    justifyContent: 'center',
    marginRight: 8,
    paddingLeft: 14,
    paddingRight: 14,
  }),

  characterRevealLabel: new Style<Label>({
    color: '#0F172A',
    font: systemBoldFont(24),
    lineHeightMultiple: 1.25,
    numberOfLines: 0,
    width: '100%',
  }),

  detailCard: new Style<View>({
    backgroundColor: '#FFFFFF',
    border: '1 solid #CBD5E1',
    borderRadius: 12,
    marginTop: 12,
    padding: 14,
    width: '100%',
  }),

  detailDescription: new Style<Label>({
    color: '#475569',
    font: systemFont(14),
    lineHeightMultiple: 1.25,
    marginBottom: 14,
    numberOfLines: 0,
    width: '100%',
  }),

  detailTitle: new Style<Label>({
    color: '#0F172A',
    font: systemBoldFont(22),
    marginBottom: 6,
    width: '100%',
  }),

  disabledTextField: new Style<TextField>({
    color: '#64748B',
    contentType: 'numberDecimalSigned',
    enabled: false,
    font: systemFont(16),
    height: 42,
    marginTop: 10,
    selectable: true,
    textAlign: 'right',
    width: '100%',
  }),

  disabledTextView: new Style<TextView>({
    backgroundColor: '#FFFFFF',
    color: '#334155',
    enabled: false,
    font: systemFont(17),
    height: 96,
    lineHeightMultiple: 1.3,
    marginTop: 10,
    numberOfLines: 2,
    textGravity: 'bottom',
    textOverflow: 'ellipsis',
    width: '100%',
  }),

  emptyRasterizedText: new Style<Label>({
    color: '#94A3B8',
    font: systemFont(13),
    marginTop: 12,
    width: '100%',
  }),

  errorLabel: new Style<Label>({
    color: '#BE123C',
    font: systemBoldFont(15),
    width: '100%',
  }),

  eventCard: new Style<View>({
    backgroundColor: '#F1F5F9',
    border: '1 solid #CBD5E1',
    borderRadius: 10,
    marginBottom: 28,
    marginTop: 12,
    padding: 12,
    width: '100%',
  }),

  eventText: new Style<Label>({
    color: '#475569',
    font: systemFont(13),
    lineHeightMultiple: 1.25,
    numberOfLines: 0,
    width: '100%',
  }),

  eventTitle: new Style<Label>({
    color: '#334155',
    font: systemBoldFont(13),
    marginBottom: 5,
    width: '100%',
  }),

  expandingInlineButtonCollapsed: new Style<View>({
    accessibilityCategory: 'button',
    alignItems: 'center',
    backgroundColor: '#2563EB',
    border: '1 solid #0F172A',
    borderRadius: 7,
    height: 28,
    justifyContent: 'center',
    paddingLeft: 8,
    paddingRight: 8,
    width: 84,
  }),

  expandingInlineButtonExpanded: new Style<View>({
    accessibilityCategory: 'button',
    alignItems: 'center',
    backgroundColor: '#0F766E',
    border: '1 solid #0F172A',
    borderRadius: 7,
    height: 28,
    justifyContent: 'center',
    paddingLeft: 12,
    paddingRight: 12,
    width: 156,
  }),

  expandingInlineButtonLabelCollapsed: new Style<Label>({
    color: '#FFFFFF',
    font: systemBoldFont(12),
    textAlign: 'center',
    width: '100%',
  }),

  expandingInlineButtonLabelExpanded: new Style<Label>({
    color: '#FFFFFF',
    font: systemBoldFont(13),
    textAlign: 'center',
    width: '100%',
  }),

  fixtureCard: new Style<View>({
    backgroundColor: '#FFFFFF',
    border: '1 solid #CBD5E1',
    borderRadius: 10,
    marginBottom: 8,
    padding: 12,
    width: '100%',
  }),

  fixtureCardSelected: new Style<View>({
    backgroundColor: '#DBEAFE',
    border: '1 solid #2563EB',
    borderRadius: 10,
    marginBottom: 8,
    padding: 12,
    width: '100%',
  }),

  fixtureDescription: new Style<Label>({
    color: '#64748B',
    font: systemFont(12),
    lineHeightMultiple: 1.2,
    marginTop: 3,
    numberOfLines: 2,
    width: '100%',
  }),

  fixtureList: new Style<View>({
    marginTop: 8,
    width: '100%',
  }),

  fixtureRoot: new Style<View>({
    width: '100%',
  }),

  fixtureTitle: new Style<Label>({
    color: '#0F172A',
    font: systemBoldFont(15),
    width: '100%',
  }),

  group: new Style<TextAnimationGroup>({
    backgroundColor: '#EFF6FF',
    borderRadius: 14,
    padding: 14,
    width: '100%',
  }),

  groupLabel: new Style<Label>({
    color: '#1E293B',
    font: systemFont(18),
    lineHeightMultiple: 1.2,
    numberOfLines: 0,
    width: '100%',
  }),

  groupLabelBlock: new Style<View>({
    marginBottom: 12,
  }),

  groupNestedBlock: new Style<View>({
    backgroundColor: '#E0F2FE',
    borderRadius: 12,
    marginBottom: 12,
    padding: 12,
  }),

  groupNestedChild: new Style<View>({
    marginTop: 8,
    paddingLeft: 16,
  }),

  groupTextView: new Style<TextView>({
    backgroundColor: '#FFFFFF',
    color: '#334155',
    enabled: false,
    font: systemFont(17),
    height: 72,
    numberOfLines: 0,
    width: '100%',
  }),

  helperNote: new Style<Label>({
    color: '#64748B',
    font: systemFont(13),
    lineHeightMultiple: 1.25,
    marginTop: 10,
    numberOfLines: 0,
    width: '100%',
  }),

  inlineImageLabel: new Style<Label>({
    color: '#0F172A',
    font: systemFont(18),
    lineHeightMultiple: 1.35,
    marginTop: 10,
    numberOfLines: 0,
    width: '100%',
  }),

  inlineLabel: new Style<Label>({
    color: '#0F172A',
    font: systemFont(19),
    lineHeightMultiple: 1.45,
    numberOfLines: 0,
    width: '100%',
  }),

  inlineMarker: new Style<View>({
    alignItems: 'center',
    borderRadius: 5,
    justifyContent: 'center',
  }),

  inlineMarkerLabel: new Style<Label>({
    color: '#FFFFFF',
    font: systemBoldFont(9),
    textAlign: 'center',
    width: '100%',
  }),

  inlineTextView: new Style<TextView>({
    backgroundColor: '#FFFFFF',
    color: '#0F172A',
    enabled: false,
    font: systemFont(18),
    height: 118,
    lineHeightMultiple: 1.45,
    marginTop: 12,
    numberOfLines: 0,
    width: '100%',
  }),

  labelAutoshrink: new Style<Label>({
    adjustsFontSizeToFitWidth: true,
    color: '#BE123C',
    font: systemBoldFont(22),
    marginTop: 10,
    minimumScaleFactor: 0.55,
    numberOfLines: 1,
    textOverflow: 'ellipsis',
    width: 270,
  }),

  labelDashedUnderline: new Style<Label>({
    color: '#0F172A',
    customUnderlineStyle: '1 4 2 -2',
    font: systemFont(18),
    marginTop: 8,
    textDecoration: 'dashed-underline',
    width: '100%',
  }),

  labelDottedUnderline: new Style<Label>({
    color: '#0F172A',
    customUnderlineStyle: '2 1 3 -3',
    font: systemFont(18),
    marginTop: 8,
    textDecoration: 'dotted-underline',
    width: '100%',
  }),

  labelGradient: new Style<Label>({
    font: systemBoldFont(24),
    marginTop: 8,
    textGradient: 'linear-gradient(#DC2626, #7C3AED, #2563EB)',
    width: '100%',
  }),

  labelItalicSpacing: new Style<Label>({
    color: '#7C2D12',
    font: 'system-italic 21',
    letterSpacing: 1.8,
    marginTop: 8,
    width: '100%',
  }),

  labelJustified: new Style<Label>({
    color: '#0F172A',
    font: systemFont(17),
    lineHeight: 26,
    lineHeightMultiple: 1.8,
    marginTop: 10,
    numberOfLines: 0,
    textAlign: 'justified',
    width: '100%',
  }),

  labelShadow: new Style<Label>({
    color: '#78350F',
    font: systemBoldFont(19),
    marginTop: 8,
    textShadow: 'rgba(120, 53, 15, 0.45) 2 0.75 0 2',
    width: '100%',
  }),

  labelUnscaled: new Style<Label>({
    color: '#1D4ED8',
    font: 'system-bold 24 unscaled 24',
    width: '100%',
  }),

  passwordTextField: new Style<TextField>({
    color: '#0F172A',
    contentType: 'passwordVisible',
    font: systemFont(16),
    height: 42,
    placeholder: 'Password',
    returnKeyText: 'go',
    width: '48%',
  }),

  phoneTextField: new Style<TextField>({
    color: '#0F172A',
    contentType: 'phoneNumber',
    font: systemFont(16),
    height: 42,
    marginRight: 8,
    placeholder: 'Phone',
    returnKeyText: 'next',
    textAlign: 'center',
    width: '48%',
  }),

  primaryButton: new Style<View>({
    alignItems: 'center',
    backgroundColor: '#2563EB',
    borderRadius: 10,
    justifyContent: 'center',
    marginBottom: 12,
    padding: 12,
    width: '100%',
  }),

  primaryButtonLabel: new Style<Label>({
    color: '#FFFFFF',
    font: systemBoldFont(15),
    textAlign: 'center',
    width: '100%',
  }),

  primaryTextField: new Style<TextField>({
    autocapitalization: 'words',
    autocorrection: 'none',
    characterLimit: 32,
    closesWhenReturnKeyPressed: true,
    color: '#0F172A',
    contentType: 'email',
    enabled: true,
    enableInlinePredictions: true,
    font: systemFont(17),
    height: 46,
    keyboardAppearance: 'dark',
    placeholder: 'Email',
    placeholderColor: '#94A3B8',
    returnKeyText: 'send',
    selectTextOnFocus: true,
    selectable: true,
    selection: [0, 4],
    textAlign: 'left',
    tintColor: '#2563EB',
    width: '100%',
  }),

  primaryTextView: new Style<TextView>({
    backgroundColor: '#FFFFFF',
    backgroundEffectBorderRadius: 8,
    backgroundEffectColor: '#DBEAFE',
    backgroundEffectPadding: 4,
    color: '#0F172A',
    customUnderlineStyle: '1 0 0 -2',
    enabled: true,
    font: systemFont(17),
    height: 124,
    lineHeight: 25,
    numberOfLines: 0,
    placeholder: 'Type multiline text',
    placeholderColor: '#94A3B8',
    returnType: 'linereturn',
    selectable: true,
    selection: [0, 4],
    textAlign: 'left',
    textDecoration: 'underline',
    textGravity: 'top',
    textOverflow: 'ellipsis',
    width: '100%',
  }),

  rasterizedImage: new Style<ImageView>({
    height: SNAPSHOT_HEIGHT,
    width: '100%',
  }),

  rasterizedOutput: new Style<View>({
    marginTop: 12,
    width: '100%',
  }),

  rasterizedTitle: new Style<Label>({
    color: '#334155',
    font: systemBoldFont(13),
    marginBottom: 6,
    width: '100%',
  }),

  richLabel: new Style<Label>({
    color: '#0F172A',
    font: systemFont(18),
    lineHeightMultiple: 1.35,
    marginTop: 10,
    numberOfLines: 0,
    width: '100%',
  }),

  screen: new Style<View>({
    backgroundColor: '#F8FAFC',
    height: '100%',
    width: '100%',
  }),

  screenContent: new Style<View>({
    padding: 16,
    width: '100%',
  }),

  screenScroll: new Style<ScrollView>({
    height: '100%',
    width: '100%',
  }),

  selectableLabel: new Style<Label>({
    color: '#0F172A',
    font: systemFont(18),
    lineHeightMultiple: 1.3,
    numberOfLines: 0,
    selectable: true,
    selection: [0, 10],
    width: '100%',
  }),

  selectableTextView: new Style<TextView>({
    backgroundColor: '#FFFFFF',
    color: '#334155',
    enabled: false,
    font: systemFont(17),
    height: 118,
    lineHeightMultiple: 1.3,
    marginTop: 12,
    numberOfLines: 0,
    selectable: true,
    selection: [11, 19],
    width: '100%',
  }),

  snapScene: new Style<View>({
    backgroundColor: '#EFF6FF',
    borderRadius: 18,
    height: '100%',
    padding: 16,
    width: '100%',
  }),

  snapSceneChrome: new Style<View>({
    alignItems: 'center',
    backgroundColor: '#FFFFFF',
    border: '1 solid #93C5FD',
    borderRadius: 9,
    height: 44,
    justifyContent: 'center',
    marginBottom: 10,
    width: '100%',
  }),

  snapSceneChromeLabel: new Style<Label>({
    color: '#1D4ED8',
    font: 'system-bold 18',
    textAlign: 'center',
    width: '100%',
  }),

  snapSceneTextView: new Style<TextView>({
    backgroundColor: '#FFFFFF',
    backgroundEffectBorderRadius: 8,
    backgroundEffectColor: '#DBEAFE',
    backgroundEffectPadding: 4,
    border: '1 solid #CBD5E1',
    borderRadius: 9,
    color: '#334155',
    enabled: false,
    font: 'system 17',
    height: 86,
    lineHeightMultiple: 1.3,
    numberOfLines: 0,
    width: '100%',
  }),

  snapSceneTitle: new Style<Label>({
    color: '#0F172A',
    font: 'system-bold 24',
    lineHeight: 29,
    marginBottom: 10,
    numberOfLines: 0,
    textDecoration: 'dashed-underline',
    width: '100%',
  }),

  snapStatus: new Style<Label>({
    color: '#64748B',
    font: systemFont(13),
    marginBottom: 10,
    numberOfLines: 0,
    width: '100%',
  }),

  snapshotContainer: new Style<View>({
    height: SNAPSHOT_HEIGHT,
    width: '100%',
  }),

  textAnimationLabel: new Style<Label>({
    color: '#0F172A',
    font: systemFont(22),
    lineHeightMultiple: 1.25,
    numberOfLines: 0,
    width: '100%',
  }),

  textAnimationTextView: new Style<TextView>({
    backgroundColor: '#FFFFFF',
    color: '#334155',
    enabled: false,
    font: systemFont(18),
    height: 86,
    lineHeightMultiple: 1.25,
    marginTop: 12,
    numberOfLines: 0,
    width: '100%',
  }),

  textFieldRow: new Style<View>({
    flexDirection: 'row',
    marginTop: 10,
    width: '100%',
  }),
};
