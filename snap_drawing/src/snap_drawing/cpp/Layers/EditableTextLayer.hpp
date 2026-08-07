//
//  EditableTextLayer.hpp
//  valdi-skia
//

#pragma once

#include "snap_drawing/cpp/Layers/Interfaces/IChildInsertionLayerProvider.hpp"
#include "snap_drawing/cpp/Layers/TextLayer.hpp"

namespace snap::drawing {

class EditableTextLayer : public Layer, public IChildInsertionLayerProvider {
public:
    explicit EditableTextLayer(const Ref<Resources>& resources);
    ~EditableTextLayer() override;

    void onInitialize() override;
    Size sizeThatFits(Size maxSize) override;

    void setText(const Valdi::StringBox& text);
    void setAttributedText(const Valdi::Ref<AttributedText>& attributedText);

    TextLayer& getTextLayer();
    Layer& getChildInsertionLayer() override;

    void setTextColor(Color textColor);

    void setPlaceholder(const Valdi::StringBox& placeholder);

    void setPlaceholderColor(Color placeholderColor);

protected:
    void onBoundsChanged() override;
    void onLayout() override;
    void onRightToLeftChanged() override;

private:
    Ref<TextLayer> _textLayer;
    Valdi::StringBox _value;
    Valdi::Ref<AttributedText> _attributedValue;
    Valdi::StringBox _placeholder;
    Color _textColor = Color::black();
    Color _placeholderColor = Color::makeARGB(0xFF, 0xB3, 0xB3, 0xB3);
    bool _showingPlaceholder = false;

    bool hasValue() const;
    void updateDisplayedText();
    void layoutTextLayer();
};

} // namespace snap::drawing
