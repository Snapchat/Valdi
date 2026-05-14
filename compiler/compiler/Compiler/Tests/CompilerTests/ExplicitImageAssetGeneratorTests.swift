import XCTest
@testable import Compiler

final class ExplicitImageAssetGeneratorTests: XCTestCase {

    private func makeInput(filenamePattern: String, scale: Double, platform: Platform?) -> ExplicitImageAssetGenerator.ResolvedInput {
        let specs = ImageVariantSpecs(filenamePattern: filenamePattern, scale: scale, platform: platform)
        return ExplicitImageAssetGenerator.ResolvedInput(specs: specs, fileURL: URL(fileURLWithPath: "/tmp/\(filenamePattern)"))
    }

    func testBestInputPrefersSVG() {
        let svg = makeInput(filenamePattern: "$file.svg", scale: 1.0, platform: nil)
        let png3x = makeInput(filenamePattern: "$file@3x.png", scale: 3.0, platform: .ios)

        let best = ExplicitImageAssetGenerator.bestInput(among: [png3x, svg])

        XCTAssertEqual(best.specs.filenamePattern, "$file.svg")
    }

    func testBestInputPicksHighestScaleAmongNonSVG() {
        let png2x = makeInput(filenamePattern: "$file@2x.png", scale: 2.0, platform: .ios)
        let png3x = makeInput(filenamePattern: "$file@3x.png", scale: 3.0, platform: .ios)
        let webp = makeInput(filenamePattern: "drawable-mdpi/$file.webp", scale: 1.0, platform: .android)

        let best = ExplicitImageAssetGenerator.bestInput(among: [png2x, webp, png3x])

        XCTAssertEqual(best.specs.scale, 3.0)
        XCTAssertEqual(best.specs.platform, .ios)
    }

    func testBestInputBreaksScaleTieByPreferringIOSOverAndroid() {
        let ios3x = makeInput(filenamePattern: "$file@3x.png", scale: 3.0, platform: .ios)
        let androidXxxhdpi = makeInput(filenamePattern: "drawable-xxxhdpi/$file.webp", scale: 3.0, platform: .android)

        XCTAssertEqual(ExplicitImageAssetGenerator.bestInput(among: [ios3x, androidXxxhdpi]).specs.platform, .ios)
        XCTAssertEqual(ExplicitImageAssetGenerator.bestInput(among: [androidXxxhdpi, ios3x]).specs.platform, .ios)
    }

    // Regression: the old closure violated strict weak ordering for any non-(iOS, Android) tie,
    // which made `inputs.max` produce indeterminate results. These cases must now resolve
    // deterministically without trapping.
    func testBestInputHandlesSamePlatformSameScaleTie() {
        let webA = makeInput(filenamePattern: "$file.png", scale: 3.0, platform: .web)
        let webB = makeInput(filenamePattern: "$file.jpg", scale: 3.0, platform: .web)

        let best = ExplicitImageAssetGenerator.bestInput(among: [webA, webB])

        XCTAssertEqual(best.specs.platform, .web)
        XCTAssertEqual(best.specs.scale, 3.0)
    }

    func testBestInputHandlesWebVsIOSTieByPreferringIOS() {
        let web = makeInput(filenamePattern: "$file.png", scale: 3.0, platform: .web)
        let ios = makeInput(filenamePattern: "$file@3x.png", scale: 3.0, platform: .ios)

        XCTAssertEqual(ExplicitImageAssetGenerator.bestInput(among: [web, ios]).specs.platform, .ios)
        XCTAssertEqual(ExplicitImageAssetGenerator.bestInput(among: [ios, web]).specs.platform, .ios)
    }
}
