import XCTest
@testable import Compiler

// A ViewModel bound by more than one Component is attached to every consuming
// Component's document, so the dedup that emits the model once must pick the same
// attribution document across runs regardless of item iteration order (which is not
// deterministic across clean builds). See COMPOSER-6163. attributionRepresentatives
// is that deterministic choice: lexicographically smallest (fileName, exportedMember).
final class GenerateModelsProcessorTests: XCTestCase {

    private let identity = "SCCMusicPickerViewModel|MusicPickerViewModel|"

    private func path(_ fileName: String, _ member: String) -> ComponentPath {
        return ComponentPath(fileName: fileName, exportedMember: member)
    }

    func testSharedViewModelAttributionIsOrderIndependent() {
        let picker = path("music/src/components/Picker", "Picker")
        let pickerV2 = path("music/src/components/PickerV2", "PickerV2")

        let forward = GenerateModelsProcessor.attributionRepresentatives(
            identityAndPaths: [(identity, picker), (identity, pickerV2)])
        let reversed = GenerateModelsProcessor.attributionRepresentatives(
            identityAndPaths: [(identity, pickerV2), (identity, picker)])

        XCTAssertEqual(forward[identity], reversed[identity],
                       "attribution must not depend on input order")
        XCTAssertEqual(forward[identity], picker,
                       "winner must be the lexicographically smallest componentPath")
    }

    func testTiebreakFallsBackToExportedMember() {
        // Same fileName: the exportedMember breaks the tie.
        let alpha = path("shared/vm", "Alpha")
        let beta = path("shared/vm", "Beta")

        let chosen = GenerateModelsProcessor.attributionRepresentatives(
            identityAndPaths: [(identity, beta), (identity, alpha)])

        XCTAssertEqual(chosen[identity], alpha)
    }

    func testDistinctIdentitiesKeptSeparate() {
        let otherIdentity = "SCCOtherViewModel|OtherViewModel|"
        let vmPath = path("a/b", "A")
        let otherPath = path("c/d", "C")

        let chosen = GenerateModelsProcessor.attributionRepresentatives(
            identityAndPaths: [(identity, vmPath), (otherIdentity, otherPath)])

        XCTAssertEqual(chosen.count, 2)
        XCTAssertEqual(chosen[identity], vmPath)
        XCTAssertEqual(chosen[otherIdentity], otherPath)
    }
}
