#include "../src/PolygonGeometry.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>

static void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
static double area(const std::vector<PolygonGeometry::Point>& points) {
    double sum = 0;
    for (size_t i = 0; i < points.size(); ++i) {
        const auto a = points[i], b = points[(i + 1) % points.size()];
        sum += double(a.x) * b.y - double(a.y) * b.x;
    }
    return sum / 2;
}
int main() {
    try {
        using namespace PolygonGeometry;
        require(patternCount(100, 10, 28) == 10, "dash count");
        require(patternCount(100, 10, 24, true) == 11, "dot endpoints");
        require(patternCount(1e8, 3, 24, true) == 0, "dotted complexity bypass");
        require(patternCount(100000, 30, 24, true) == 0, "total vertex budget");
        require(patternCount(100, 0, 24) == 0, "zero spacing");
        require(patternCount(100, -1, 24) == 0, "negative spacing");
        require(patternCount(100, 1, 0) == 0, "empty circle");
        require(patternCount(100, std::numeric_limits<double>::quiet_NaN(), 24) == 0, "NaN spacing");
        require(patternCount(1e300, 1e-300, 24) == 0, "count overflow before integer conversion");
        for (int degrees = 0; degrees < 360; degrees += 15) {
            const double a = degrees * std::numbers::pi / 180;
            Point direction{float(std::cos(a)), float(std::sin(a))};
            require(area(arrow({100,100}, direction, 30, 30)) > 0, "end arrow winding");
            require(area(arrow({100,100}, {-direction.x,-direction.y}, 30, 30)) > 0, "start arrow winding");
        }
        for (float radius : {0.5f, 1.0f, 15.0f, 500.0f}) {
            const auto points = circle({0,0}, radius);
            require(points.size() >= 24 && points.size() <= 4096, "circle budget");
            require(area(points) > 0 && hasArea(points), "circle winding");
            const double sagitta = radius * (1 - std::cos(std::numbers::pi / points.size()));
            require(sagitta <= 0.050001, "circle approximation error");
        }
        require(circle({0,0}, 0).empty(), "zero radius");
        require(circle({0,0}, std::numeric_limits<float>::infinity()).empty(), "infinite radius");
        require(circle({0,0}, 1e20f).empty(), "unbounded tessellation");
        require(!hasArea({{0,0},{5,5},{10,10}}), "diagonal collinear lasso");
        require(!hasArea({{1,1},{1,1},{1,1}}), "duplicate lasso");
        require(!hasArea({{0,0},{5,5},{0,std::numeric_limits<float>::quiet_NaN()}}), "nonfinite lasso");
        require(hasArea({{0,0},{10,10},{0,10},{10,0}}), "self-crossing lasso must not be rejected by signed-area cancellation");
        std::cout << "Polygon geometry checks passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
