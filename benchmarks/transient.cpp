#include <algorithm>
#include <chrono>
#include <iostream>
#include <openece/circuits/transient/analysis.hpp>
#ifdef OPENECE_BENCH_GUI
#include <QApplication>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#endif
namespace tr = openece::circuits::transient;
using namespace openece::circuits;
using Clock = std::chrono::steady_clock;
double ms(Clock::time_point t) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t).count();
}
int main(int argc, char** argv) {
#ifdef OPENECE_BENCH_GUI
    QApplication app(argc, argv);
#else
    (void)argc;
    (void)argv;
#endif
    for (unsigned count : {2U, 32U, 128U}) {
        tr::CircuitDefinition d;
        d.ground = NodeId{0};
        d.nodes.push_back({{0}, "ground"});
        for (unsigned i = 1; i < count; ++i) {
            d.nodes.push_back({{i}, "N" + std::to_string(i)});
            d.components.emplace_back(Resistor{{i * 2}, "R" + std::to_string(i), {i}, {0}, 1000});
            d.components.emplace_back(
                tr::Capacitor{{i * 2 + 1}, "C" + std::to_string(i), {i}, {0}, 1e-6});
        }
        d.components.emplace_back(tr::CurrentSource{{1000}, "I", {0}, {1}, {0.001, {}}});
        tr::Request r;
        r.stop_seconds = .1;
        r.maximum_step_seconds = count == 128 ? .001 : .00001;
        r.voltages.push_back({{1}, {0}});
        auto start = Clock::now();
        tr::Simulation sim(tr::Circuit(std::move(d)), r);
        const double initialization = ms(start);
        std::vector<double> steps;
        tr::Status status;
        do {
            start = Clock::now();
            status = sim.step();
            steps.push_back(ms(start));
        } while (status == tr::Status::running);
        start = Clock::now();
        const auto result = sim.snapshot();
        const double copy = ms(start);
        std::sort(steps.begin(), steps.end());
        std::cout << "nodes=" << count << " init_ms=" << initialization << " steps=" << steps.size()
                  << " p95_ms=" << steps[steps.size() * 95 / 100] << " max_ms=" << steps.back()
                  << " snapshot_ms=" << copy << " samples=" << result.times.size() << '\n';
    }
    {
        tr::CircuitDefinition d;
        d.ground = NodeId{0};
        for (unsigned i = 0; i < 128; ++i)
            d.nodes.push_back({{i}, "N" + std::to_string(i)});
        tr::Request r;
        r.stop_seconds = .1;
        r.maximum_step_seconds = .002;
        r.initial.mode = tr::Initialization::specified_storage;
        for (unsigned i = 1; i <= 64; ++i)
            d.components.emplace_back(
                tr::VoltageSource{{i}, "V" + std::to_string(i), {i}, {0}, {1, {}}});
        for (unsigned i = 65; i < 128; ++i) {
            d.components.emplace_back(Resistor{{i}, "R" + std::to_string(i), {i}, {0}, 1000});
            const unsigned id = i + 100;
            d.components.emplace_back(tr::Inductor{{id}, "L" + std::to_string(i), {i}, {0}, .01});
            r.initial.inductor_currents.push_back({{id}, 0});
        }
        d.components.emplace_back(tr::Inductor{{300}, "L extra", {1}, {0}, .01});
        r.initial.inductor_currents.push_back({{300}, 0});
        r.voltages.push_back({{1}, {0}});
        auto start = Clock::now();
        tr::Simulation sim(tr::Circuit(std::move(d)), r);
        const auto init = ms(start);
        std::vector<double> steps;
        tr::Status status;
        do {
            start = Clock::now();
            status = sim.step();
            steps.push_back(ms(start));
        } while (status == tr::Status::running);
        std::sort(steps.begin(), steps.end());
        std::cout << "unknowns=255 init_ms=" << init << " steps=" << steps.size()
                  << " p95_ms=" << steps[steps.size() * 95 / 100] << " max_ms=" << steps.back()
                  << '\n';
    }
    tr::Result large{};
    large.times.resize(31250);
    large.voltages.resize(64, std::vector<double>(31250, 1));
    auto start = Clock::now();
    const auto copy = large;
    std::cout << "2m_values_copy_ms=" << ms(start) << " copied_probes=" << copy.voltages.size()
              << " trace_payload_bytes=" << 2000000 * sizeof(double) + 31250 * sizeof(tr::TimePoint)
              << '\n';
#ifdef OPENECE_BENCH_GUI
    QwtPlot plot;
    plot.resize(1000, 500);
    auto* curve = new QwtPlotCurve;
    curve->attach(&plot);
    plot.show();
    app.processEvents();
    for (int n : {4096, 16384}) {
        QVector<QPointF> points;
        for (int i = 0; i < n; ++i)
            points.push_back({double(i), double(i % 17)});
        start = Clock::now();
        curve->setSamples(points);
        plot.replot();
        app.processEvents();
        std::cout << "render_points=" << n << " render_ms=" << ms(start) << '\n';
    }
#endif
}
