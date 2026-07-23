//
//  TransformAttributes.cpp
//  valdi
//

#include "valdi/runtime/Attributes/TransformAttributes.hpp"
#include "valdi/runtime/Attributes/ValueConverters.hpp"
#include "valdi/runtime/Context/ViewNode.hpp"
#include "valdi_core/cpp/Attributes/AttributeUtils.hpp"
#include "valdi_core/cpp/Utils/Format.hpp"
#include "valdi_core/cpp/Utils/StringBox.hpp"
#include "valdi_core/cpp/Utils/ValueArray.hpp"

#include <cmath>
#include <optional>
#include <string_view>

namespace Valdi {

namespace {

constexpr size_t kTransformPartOrigin = 0;
constexpr size_t kTransformPartTransform = 1;
constexpr size_t kTransformPartTranslationX = 2;
constexpr size_t kTransformPartTranslationY = 3;
constexpr size_t kTransformPartScaleX = 4;
constexpr size_t kTransformPartScaleY = 5;
constexpr size_t kTransformPartRotation = 6;
constexpr size_t kTransformPartsSize = 7;

struct TransformOrigin {
    double x = 0;
    double y = 0;
    bool isCenter = false;

    TransformOrigin() = default;
    TransformOrigin(double x, double y, bool isCenter) : x(x), y(y), isCenter(isCenter) {}
};

enum class OriginKeywordAxis { Horizontal, Vertical, Center };

struct OriginKeyword {
    OriginKeywordAxis axis;
    double ratio;
};

struct TransformComponents {
    double translationX = 0.0;
    double translationY = 0.0;
    double scaleX = 1.0;
    double scaleY = 1.0;
    double rotation = 0.0;

    TransformComponents() = default;
    TransformComponents(double translationX, double translationY, double scaleX, double scaleY, double rotation)
        : translationX(translationX),
          translationY(translationY),
          scaleX(scaleX),
          scaleY(scaleY),
          rotation(rotation) {}
};

struct TransformMatrix {
    double a = 1.0;
    double b = 0.0;
    double c = 0.0;
    double d = 1.0;
    double e = 0.0;
    double f = 0.0;

    TransformMatrix() = default;
    TransformMatrix(double a, double b, double c, double d, double e, double f)
        : a(a), b(b), c(c), d(d), e(e), f(f) {}

    static TransformMatrix translate(double x, double y);
    static TransformMatrix scale(double x, double y);
    static TransformMatrix rotate(double radians);

    TransformMatrix concat(const TransformMatrix& other) const;
    Result<TransformComponents> decompose() const;
};

struct ResolvedTransform {
    TransformComponents components;
    TransformOrigin origin;

    ResolvedTransform(TransformComponents components, TransformOrigin origin)
        : components(components), origin(origin) {}
};

Value makeTransformValue(double translationX, double translationY, double scaleX, double scaleY, double rotation);
Result<TransformOrigin> parseTransformOrigin(const Value& value, double width, double height);

std::optional<OriginKeyword> parseOriginKeyword(std::string_view token) {
    if (token == "left") {
        return OriginKeyword{OriginKeywordAxis::Horizontal, 0};
    } else if (token == "right") {
        return OriginKeyword{OriginKeywordAxis::Horizontal, 1};
    } else if (token == "top") {
        return OriginKeyword{OriginKeywordAxis::Vertical, 0};
    } else if (token == "bottom") {
        return OriginKeyword{OriginKeywordAxis::Vertical, 1};
    } else if (token == "center") {
        return OriginKeyword{OriginKeywordAxis::Center, 0.5};
    }

    return std::nullopt;
}

TransformMatrix TransformMatrix::translate(double x, double y) {
    return TransformMatrix(1.0, 0.0, 0.0, 1.0, x, y);
}

TransformMatrix TransformMatrix::scale(double x, double y) {
    return TransformMatrix(x, 0.0, 0.0, y, 0.0, 0.0);
}

TransformMatrix TransformMatrix::rotate(double radians) {
    auto cosValue = std::cos(radians);
    auto sinValue = std::sin(radians);
    return TransformMatrix(cosValue, sinValue, -sinValue, cosValue, 0.0, 0.0);
}

TransformMatrix TransformMatrix::concat(const TransformMatrix& other) const {
    return TransformMatrix(
        a * other.a + c * other.b,
        b * other.a + d * other.b,
        a * other.c + c * other.d,
        b * other.c + d * other.d,
        a * other.e + c * other.f + e,
        b * other.e + d * other.f + f);
}

Result<TransformComponents> TransformMatrix::decompose() const {
    constexpr auto epsilon = 0.00001;
    auto resolvedScaleX = std::hypot(a, b);
    if (std::abs(resolvedScaleX) < epsilon) {
        return Error("transform scaleX must not resolve to zero");
    }

    auto shear = a * c + b * d;
    if (std::abs(shear) > epsilon) {
        return Error("transform strings cannot resolve to skewed matrices");
    }

    auto determinant = a * d - b * c;
    auto resolvedScaleY = determinant / resolvedScaleX;
    auto resolvedRotation = std::atan2(b, a);

    return TransformComponents(e, f, resolvedScaleX, resolvedScaleY, resolvedRotation);
}

Result<Dimension> parseOriginDimension(AttributeParser& parser) {
    auto dimension = parser.parseDimension();
    if (!dimension) {
        return parser.getError().rethrow(STRING_LITERAL("Invalid transformOrigin dimension"));
    }

    if (dimension->unit == Dimension::Unit::None) {
        return Error("transformOrigin dimensions require px, pt, or percent units");
    }

    return *dimension;
}

double resolveDimension(const Dimension& dimension, double referenceLength) {
    if (dimension.unit == Dimension::Unit::Percent) {
        return referenceLength * dimension.value / 100.0;
    }

    return dimension.value;
}

Result<double> parseTransformLength(AttributeParser& parser, double referenceLength) {
    auto dimension = parser.parseDimension();
    if (!dimension) {
        return parser.getError().rethrow(STRING_LITERAL("Invalid transform length"));
    }

    return resolveDimension(dimension.value(), referenceLength);
}

Result<double> parseTransformNumber(AttributeParser& parser) {
    auto number = parser.parseDouble();
    if (!number) {
        return parser.getError().rethrow(STRING_LITERAL("Invalid transform number"));
    }

    return number.value();
}

Result<double> parseTransformAngle(AttributeParser& parser) {
    auto angle = parser.parseAngle();
    if (!angle) {
        return parser.getError().rethrow(STRING_LITERAL("Invalid transform angle"));
    }

    return angle.value();
}

bool parseOptionalArgumentSeparator(AttributeParser& parser) {
    auto hadWhitespace = parser.tryParseWhitespaces();
    if (parser.tryParse(',')) {
        parser.tryParseWhitespaces();
        return true;
    }
    return hadWhitespace && !parser.peek(')');
}

Result<TransformMatrix> parseTranslateFunction(AttributeParser& parser, double width, double height) {
    auto x = parseTransformLength(parser, width);
    if (!x) {
        return x.moveError();
    }

    auto y = 0.0;
    if (parseOptionalArgumentSeparator(parser)) {
        auto parsedY = parseTransformLength(parser, height);
        if (!parsedY) {
            return parsedY.moveError();
        }
        y = parsedY.value();
    }

    return TransformMatrix::translate(x.value(), y);
}

Result<TransformMatrix> parseTranslateXFunction(AttributeParser& parser, double width) {
    auto x = parseTransformLength(parser, width);
    if (!x) {
        return x.moveError();
    }

    return TransformMatrix::translate(x.value(), 0.0);
}

Result<TransformMatrix> parseTranslateYFunction(AttributeParser& parser, double height) {
    auto y = parseTransformLength(parser, height);
    if (!y) {
        return y.moveError();
    }

    return TransformMatrix::translate(0.0, y.value());
}

Result<TransformMatrix> parseScaleFunction(AttributeParser& parser) {
    auto x = parseTransformNumber(parser);
    if (!x) {
        return x.moveError();
    }

    auto y = x.value();
    if (parseOptionalArgumentSeparator(parser)) {
        auto parsedY = parseTransformNumber(parser);
        if (!parsedY) {
            return parsedY.moveError();
        }
        y = parsedY.value();
    }

    return TransformMatrix::scale(x.value(), y);
}

Result<TransformMatrix> parseScaleXFunction(AttributeParser& parser) {
    auto x = parseTransformNumber(parser);
    if (!x) {
        return x.moveError();
    }

    return TransformMatrix::scale(x.value(), 1.0);
}

Result<TransformMatrix> parseScaleYFunction(AttributeParser& parser) {
    auto y = parseTransformNumber(parser);
    if (!y) {
        return y.moveError();
    }

    return TransformMatrix::scale(1.0, y.value());
}

Result<TransformMatrix> parseRotateFunction(AttributeParser& parser) {
    auto angle = parseTransformAngle(parser);
    if (!angle) {
        return angle.moveError();
    }

    return TransformMatrix::rotate(angle.value());
}

Result<TransformMatrix> parseTransformFunction(AttributeParser& parser,
                                               std::string_view functionName,
                                               double width,
                                               double height) {
    if (functionName == "translate") {
        return parseTranslateFunction(parser, width, height);
    } else if (functionName == "translateX") {
        return parseTranslateXFunction(parser, width);
    } else if (functionName == "translateY") {
        return parseTranslateYFunction(parser, height);
    } else if (functionName == "scale") {
        return parseScaleFunction(parser);
    } else if (functionName == "scaleX") {
        return parseScaleXFunction(parser);
    } else if (functionName == "scaleY") {
        return parseScaleYFunction(parser);
    } else if (functionName == "rotate") {
        return parseRotateFunction(parser);
    } else if (functionName == "rotateZ") {
        return parseRotateFunction(parser);
    }

    return Error(STRING_FORMAT("Unsupported transform function '{}'", functionName));
}

Result<ResolvedTransform> resolveTransformFromString(const Value& transformValue,
                                                     const TransformOrigin& origin,
                                                     double width,
                                                     double height) {
    auto transformString = transformValue.toStringBox();
    AttributeParser parser(transformString.toStringView());
    parser.tryParseWhitespaces();

    if (parser.tryParse("none")) {
        parser.tryParseWhitespaces();
        if (!parser.ensureIsAtEnd()) {
            return parser.getError();
        }
        return ResolvedTransform(TransformComponents(), origin);
    }

    auto matrix = TransformMatrix();
    auto parsedFunctionCount = 0;
    while (!parser.isAtEnd()) {
        auto functionName = parser.parseIdentifier();
        if (!functionName) {
            return parser.getError().rethrow(STRING_LITERAL("Invalid transform function"));
        }

        if (!parser.parse('(')) {
            return parser.getError().rethrow(STRING_LITERAL("Invalid transform function"));
        }
        parser.tryParseWhitespaces();

        auto parsedMatrix = parseTransformFunction(parser, functionName.value(), width, height);
        if (!parsedMatrix) {
            return parsedMatrix.moveError();
        }

        parser.tryParseWhitespaces();
        if (!parser.parse(')')) {
            return parser.getError().rethrow(STRING_LITERAL("Invalid transform function arguments"));
        }

        matrix = matrix.concat(parsedMatrix.value());
        parsedFunctionCount++;
        parser.tryParseWhitespaces();
    }

    if (parsedFunctionCount == 0) {
        return Error("transform string must contain at least one function");
    }

    auto components = matrix.decompose();
    if (!components) {
        return components.moveError();
    }

    return ResolvedTransform(components.moveValue(), origin);
}

Result<TransformOrigin> parseDimensionOrigin(AttributeParser& parser, double width, double height) {
    auto xDimension = parseOriginDimension(parser);
    if (!xDimension) {
        return xDimension.moveError();
    }

    if (!parser.tryParseWhitespaces()) {
        return Error("transformOrigin length and percent values require exactly two components");
    }

    auto yDimension = parseOriginDimension(parser);
    if (!yDimension) {
        return yDimension.moveError();
    }

    parser.tryParseWhitespaces();
    if (!parser.ensureIsAtEnd()) {
        return parser.getError().rethrow(STRING_LITERAL("Invalid transformOrigin dimension pair"));
    }

    auto x = resolveDimension(xDimension.value(), width);
    auto y = resolveDimension(yDimension.value(), height);
    return TransformOrigin(x, y, x == width / 2.0 && y == height / 2.0);
}

Result<TransformOrigin> parseKeywordOrigin(AttributeParser& parser, double width, double height) {
    auto xRatio = 0.5;
    auto yRatio = 0.5;
    auto hasHorizontal = false;
    auto hasVertical = false;
    auto parsedKeywords = 0;

    while (!parser.isAtEnd()) {
        auto token = parser.parseIdentifier();
        if (!token) {
            return parser.getError().rethrow(STRING_LITERAL("Invalid transformOrigin keyword"));
        }

        auto keyword = parseOriginKeyword(token.value());
        if (!keyword) {
            return Error(STRING_FORMAT("Invalid transformOrigin keyword '{}'", token.value()));
        }

        parsedKeywords++;
        if (parsedKeywords > 2) {
            return Error("transformOrigin keyword values require one or two components");
        }

        switch (keyword->axis) {
            case OriginKeywordAxis::Horizontal:
                if (hasHorizontal) {
                    return Error("transformOrigin cannot contain two horizontal keywords");
                }
                xRatio = keyword->ratio;
                hasHorizontal = true;
                break;
            case OriginKeywordAxis::Vertical:
                if (hasVertical) {
                    return Error("transformOrigin cannot contain two vertical keywords");
                }
                yRatio = keyword->ratio;
                hasVertical = true;
                break;
            case OriginKeywordAxis::Center:
                break;
        }

        if (!parser.tryParseWhitespaces()) {
            break;
        }
    }

    if (parsedKeywords == 0) {
        return Error("transformOrigin keyword values require one or two components");
    }

    if (!parser.ensureIsAtEnd()) {
        return parser.getError().rethrow(STRING_LITERAL("Invalid transformOrigin keyword value"));
    }

    return TransformOrigin(width * xRatio, height * yRatio, xRatio == 0.5 && yRatio == 0.5);
}

Result<double> resolveTranslation(const Value& value, double referenceLength, bool flip) {
    if (value.isNullOrUndefined()) {
        return 0.0;
    }

    auto percent = ValueConverter::toPercent(value);
    if (!percent) {
        return percent.moveError();
    }

    auto resolvedValue = percent.value().isPercent ? referenceLength * percent.value().value / 100.0
                                                   : percent.value().value;
    return flip ? -resolvedValue : resolvedValue;
}

Result<double> resolveDouble(const Value& value, double defaultValue) {
    if (value.isNullOrUndefined()) {
        return defaultValue;
    }

    return ValueConverter::toDouble(value);
}

Result<TransformOrigin> parseTransformOrigin(const Value& value, double width, double height) {
    if (value.isNullOrUndefined()) {
        return TransformOrigin(width / 2.0, height / 2.0, true);
    }

    if (!value.isString()) {
        return Error("transformOrigin must be a string");
    }

    auto originString = value.toStringBox();
    AttributeParser parser(originString.toStringView());
    parser.tryParseWhitespaces();
    if (parser.isAtEnd()) {
        return Error("transformOrigin cannot be empty");
    }

    if (parser.peekPredicate([](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); })) {
        return parseKeywordOrigin(parser, width, height);
    }

    return parseDimensionOrigin(parser, width, height);
}

Value makeTransformValue(double translationX, double translationY, double scaleX, double scaleY, double rotation) {
    auto out = ValueArray::make(5);
    (*out)[0] = Value(translationX);
    (*out)[1] = Value(translationY);
    (*out)[2] = Value(scaleX);
    (*out)[3] = Value(scaleY);
    (*out)[4] = Value(rotation);
    return Value(out);
}

Result<ResolvedTransform> resolveTransformFromAttributeParts(const Value& value,
                                                             const TransformOrigin& origin,
                                                             double width,
                                                             double height) {
    const auto* array = value.getArray();
    if (array == nullptr || array->size() != kTransformPartsSize) {
        return Error("Expected 7 transform components");
    }

    TransformComponents components;

    auto translationX = resolveTranslation((*array)[kTransformPartTranslationX], width, false);
    if (!translationX) {
        return translationX.moveError();
    }
    components.translationX = translationX.value();

    auto translationY = resolveTranslation((*array)[kTransformPartTranslationY], height, false);
    if (!translationY) {
        return translationY.moveError();
    }
    components.translationY = translationY.value();

    auto scaleX = resolveDouble((*array)[kTransformPartScaleX], 1.0);
    if (!scaleX) {
        return scaleX.moveError();
    }
    components.scaleX = scaleX.value();

    auto scaleY = resolveDouble((*array)[kTransformPartScaleY], 1.0);
    if (!scaleY) {
        return scaleY.moveError();
    }
    components.scaleY = scaleY.value();

    auto rotation = resolveDouble((*array)[kTransformPartRotation], 0.0);
    if (!rotation) {
        return rotation.moveError();
    }
    components.rotation = rotation.value();

    return ResolvedTransform(components, origin);
}

Value postprocessResolvedTransform(ResolvedTransform resolvedTransform,
                                   double width,
                                   double height,
                                   bool isRightToLeft) {
    auto components = resolvedTransform.components;
    auto origin = resolvedTransform.origin;

    if (isRightToLeft) {
        components.translationX *= -1.0;
        components.rotation *= -1.0;
    }

    if (origin.isCenter) {
        return makeTransformValue(components.translationX,
                                  components.translationY,
                                  components.scaleX,
                                  components.scaleY,
                                  components.rotation);
    }

    auto centerX = width / 2.0;
    auto centerY = height / 2.0;
    auto deltaX = centerX - origin.x;
    auto deltaY = centerY - origin.y;

    auto cosRotation = std::cos(components.rotation);
    auto sinRotation = std::sin(components.rotation);
    auto transformedDeltaX = cosRotation * components.scaleX * deltaX - sinRotation * components.scaleY * deltaY;
    auto transformedDeltaY = sinRotation * components.scaleX * deltaX + cosRotation * components.scaleY * deltaY;

    auto adjustedTranslationX = components.translationX + origin.x - centerX + transformedDeltaX;
    auto adjustedTranslationY = components.translationY + origin.y - centerY + transformedDeltaY;

    return makeTransformValue(adjustedTranslationX,
                              adjustedTranslationY,
                              components.scaleX,
                              components.scaleY,
                              components.rotation);
}

} // namespace

Result<Value> TransformAttributes::postprocess(float width, float height, bool isRightToLeft, const Value& value) {
    auto resolvedWidth = static_cast<double>(width);
    auto resolvedHeight = static_cast<double>(height);

    const auto* array = value.getArray();
    if (array == nullptr || array->size() != kTransformPartsSize) {
        return Error("Expected 7 transform components");
    }

    auto origin = parseTransformOrigin((*array)[kTransformPartOrigin], resolvedWidth, resolvedHeight);
    if (!origin) {
        return origin.moveError();
    }

    auto resolvedTransform = (*array)[kTransformPartTransform].isString()
                                 ? resolveTransformFromString(
                                       (*array)[kTransformPartTransform], origin.value(), resolvedWidth, resolvedHeight)
                                 : resolveTransformFromAttributeParts(value, origin.value(), resolvedWidth, resolvedHeight);

    if (!resolvedTransform) {
        return resolvedTransform.moveError();
    }

    return postprocessResolvedTransform(resolvedTransform.moveValue(), resolvedWidth, resolvedHeight, isRightToLeft);
}

Result<Value> TransformAttributes::postprocessViewNode(ViewNode& viewNode, const Value& value) {
    auto frame = viewNode.getCalculatedFrame();
    return postprocess(frame.width, frame.height, viewNode.isRightToLeft(), value);
}

} // namespace Valdi
