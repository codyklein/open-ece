#include "schematic_preview_cases.hpp"
#include <QApplication>
#include <QElapsedTimer>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QImage>
#include <QPainter>
#include <QScrollBar>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <schematic/qt/canvas.hpp>
#ifdef _WIN32
#include <windows.h>

#include <psapi.h>
#else
#include <sys/resource.h>
#endif
namespace q = openece::schematic::qt;
namespace s = openece::schematic;
static std::uint64_t peak() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters{};
    counters.cb = static_cast<DWORD>(sizeof(counters));
    return GetProcessMemoryInfo(GetCurrentProcess(), &counters, counters.cb)
               ? static_cast<std::uint64_t>(counters.PeakWorkingSetSize)
               : 0;
#else
    rusage usage{};
    return getrusage(RUSAGE_SELF, &usage) == 0 ? static_cast<std::uint64_t>(usage.ru_maxrss) * 1024
                                               : 0;
#endif
}
int main(int argc, char** argv) {
    QApplication app(argc, argv);
    q::SchematicCanvas canvas;
    canvas.resize(1280, 850);
    canvas.show();
    QApplication::processEvents();
    std::cout << "Qt " << QT_VERSION_STR
              << " logical viewport=1280x850 DPR=" << canvas.devicePixelRatioF() << "\n";
    auto cases = q::preview_cases();
    cases.push_back(
        {"Large retained source metadata", s::project_schematic(schematic_cases::large_sources())});
    for (const auto& c : cases) {
        std::vector<double> layout, scene, paint, interact, inspect;
        for (int run = 0; run < 6; ++run) {
            QElapsedTimer clock;
            clock.start();
            const auto measured = q::measure_labels(c.graph, canvas.font());
            const auto result = s::layout_schematic(c.graph, measured.metrics);
            const auto lm = static_cast<double>(clock.nsecsElapsed()) / 1e6;
            if (!std::holds_alternative<s::LayoutDiagram>(result))
                return 1;
            clock.restart();
            if (!canvas.setDiagram(c.graph, std::get<s::LayoutDiagram>(result), measured.metrics))
                return 2;
            const auto sm = static_cast<double>(clock.nsecsElapsed()) / 1e6;
            QImage image(1280, 850, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
            clock.restart();
            {
                QPainter p(&image);
                canvas.view()->render(&p);
            }
            const auto pm = static_cast<double>(clock.nsecsElapsed()) / 1e6;
            QApplication::processEvents();
            clock.restart();
            canvas.zoomIn();
            canvas.view()->verticalScrollBar()->setValue(40);
            {
                QPainter p(&image);
                canvas.view()->render(&p);
            }
            const auto im = static_cast<double>(clock.nsecsElapsed()) / 1e6;
            clock.restart();
            if (!canvas.diagram()->nets.empty())
                canvas.select(canvas.diagram()->nets.front().source.key);
            const auto nm = static_cast<double>(clock.nsecsElapsed()) / 1e6;
            if (run) {
                layout.push_back(lm);
                scene.push_back(sm);
                paint.push_back(pm);
                interact.push_back(im);
                inspect.push_back(nm);
            }
        }
        auto median = [](auto values) {
            std::sort(values.begin(), values.end());
            return values[values.size() / 2];
        };
        std::cout << std::fixed << std::setprecision(3) << c.name.toStdString()
                  << " layout_font_ms=" << median(layout)
                  << " scene_replace_verify_ms=" << median(scene)
                  << " initial_paint_ms=" << median(paint)
                  << " zoom_pan_paint_ms=" << median(interact)
                  << " net_inspect_ms=" << median(inspect)
                  << " items=" << canvas.scene()->items().size()
                  << " crossings=" << canvas.diagram()->crossings.size() << "\n";
    }
    std::cout << "whole_process_peak_bytes=" << peak()
              << " (Qt, fixtures, scene, image, runtime; not per-diagram allocation)\n";
}
