#include "../src/LineGeometry.hpp"
#include "../src/CanvasComponents/MeshPathSerialization.hpp"
#include <cereal/archives/portable_binary.hpp>
#include <iostream>
#include <sstream>
#include <stdexcept>

struct Config {
    bool hasRoundCaps = true;
    int lineStyle = 0;
    float dashLength = 3;
    float dashGap = 2;
    int arrowMode = 0;
};
static void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
static std::string encode(const SkPath& path) {
    std::ostringstream stream(std::ios::out | std::ios::binary);
    { cereal::PortableBinaryOutputArchive archive(stream); skpath_write(path, archive); }
    return stream.str();
}
static SkPath decode(const std::string& bytes) {
    std::istringstream stream(bytes, std::ios::in | std::ios::binary);
    cereal::PortableBinaryInputArchive archive(stream);
    return skpath_read(archive);
}
static void checkRoundTrip(const SkPath& path) {
    require(!path.isEmpty(), "unexpected empty generated geometry");
    const auto bytes = encode(path);
    const auto loaded = decode(bytes);
    require(encode(loaded) == bytes, "polygon serialization is not stable");
    // Same serializer is used by document and network mesh updates. Compare
    // filled regions, not Skia's internal path allocation/generation IDs.
    for (float y = -30; y <= 30; y += 2)
        for (float x = -30; x <= 220; x += 2)
            require(path.contains(x, y) == loaded.contains(x, y), "round-trip changed fill");
}
int main() {
    try {
        using LineGeometry::generate_line_path;
        for (int style = 0; style <= 4; ++style)
            for (int arrows = 0; arrows <= 3; ++arrows)
                for (bool caps : {false, true}) {
                    Config config;
                    config.lineStyle = style; config.arrowMode = arrows; config.hasRoundCaps = caps;
                    const auto path = generate_line_path(Eigen::Vector2f{0,0}, Eigen::Vector2f{200,0}, 10, config);
                    checkRoundTrip(path);
                    const auto finalPath = Simplify(path);
                    require(finalPath.has_value(), "final mesh simplify failed");
                    checkRoundTrip(*finalPath);
                    const auto fallback = generate_line_path(Eigen::Vector2f{0,0}, Eigen::Vector2f{200,0}, 10, config, false);
                    checkRoundTrip(fallback);
                    if (style == 0 && arrows) {
                        for (float x = 1; x < 200; x += 1)
                            require(fallback.contains(x, 0), "arrow fallback hole on shaft centerline");
                    }
                }
        Config config;
        checkRoundTrip(generate_line_path(Eigen::Vector2f{0,0}, Eigen::Vector2f{0.1f,0}, 10, config));
        require(generate_line_path(Eigen::Vector2f{0,0}, Eigen::Vector2f{200,0}, 0, config).isEmpty(), "zero width");
        config.lineStyle = 2; config.dashLength = 1e9f; config.dashGap = 0.5f;
        require(generate_line_path(Eigen::Vector2f{0,0}, Eigen::Vector2f{1e8f,0}, 1, config).isEmpty(), "dotted count bypass");
        SkPathBuilder curved;
        curved.addCircle(0, 0, 10);
        bool rejected = false;
        try { encode(curved.detach()); } catch (const std::runtime_error&) { rejected = true; }
        require(rejected, "serializer accepted unsupported curve verbs");

        // Boolean union and detached path snapshots used by flat-overlap edits.
        Config solid;
        const auto before = generate_line_path(Eigen::Vector2f{0,0}, Eigen::Vector2f{100,0}, 10, solid);
        const auto incoming = generate_line_path(Eigen::Vector2f{80,0}, Eigen::Vector2f{180,0}, 10, solid);
        const auto merged = Op(before, incoming, SkPathOp::kUnion_SkPathOp);
        require(merged.has_value() && merged->contains(170,0), "flat union lost new stroke");
        require(!before.contains(170,0), "union mutated saved undo path");
        checkRoundTrip(*merged);
        std::cout << "Production Skia line and mesh serialization checks passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
