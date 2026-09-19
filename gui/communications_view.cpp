#include "communications_view.hpp"
#include "communications_plot.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
namespace openece::gui {
namespace {
namespace comm = communications;
double number(QLineEdit* edit) {
    QLocale locale = QLocale::c();
    locale.setNumberOptions(QLocale::RejectGroupSeparator);
    bool ok = false;
    const double value = locale.toDouble(edit->text().trimmed(), &ok);
    if (!ok || !std::isfinite(value))
        throw std::invalid_argument("Invalid numeric text; use a finite dot-decimal value.");
    return value;
}
std::uint64_t seed(QLineEdit* edit) {
    const auto text = edit->text().trimmed();
    bool ok = false;
    const auto value = text.toULongLong(&ok);
    if (!ok || !QRegularExpression("^[0-9]+$").match(text).hasMatch())
        throw std::invalid_argument("Seed must be an unsigned 64-bit decimal integer.");
    return value;
}
QString format(double value) { return QString::number(value, 'g', 12); }
QTableWidgetItem* item(const QString& text) {
    auto* result = new QTableWidgetItem(text);
    result->setFlags(result->flags() & ~Qt::ItemIsEditable);
    return result;
}
QTableWidget* table(const QStringList& labels, const char* name) {
    auto* result = new QTableWidget(0, static_cast<int>(labels.size()));
    result->setObjectName(name);
    result->setHorizontalHeaderLabels(labels);
    result->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    result->horizontalHeader()->setStretchLastSection(true);
    return result;
}
} // namespace
CommunicationsView::CommunicationsView(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    auto* title = new QLabel("Digital Communications — ideal coherent BPSK / QPSK");
    layout->addWidget(title);
    auto* form = new QGridLayout;
    layout->addLayout(form);
    auto field = [&](const QString& label, QWidget* widget, const char* name, int row, int col) {
        widget->setObjectName(name);
        widget->setAccessibleName(label);
        auto* caption = new QLabel(label);
        caption->setBuddy(widget);
        form->addWidget(caption, row, col * 2);
        form->addWidget(widget, row, col * 2 + 1);
    };
    modulation_ = new QComboBox;
    modulation_->addItems({"BPSK", "QPSK (Gray)"});
    modulation_->setCurrentIndex(1);
    source_ = new QComboBox;
    source_->addItems({"Seeded random", "Manual bits"});
    count_ = new QSpinBox;
    count_->setRange(1, communications_gui_limits::bits);
    count_->setValue(256);
    samples_ = new QSpinBox;
    samples_->setRange(1, 64);
    samples_->setValue(16);
    rate_ = new QLineEdit("1");
    rate_unit_ = new QComboBox;
    rate_unit_->addItems({"symbols/s", "ksymbols/s", "Msymbols/s"});
    rate_unit_->setCurrentIndex(1);
    bit_seed_ = new QLineEdit("1");
    noise_seed_ = new QLineEdit("2");
    bit_seed_->setMaxLength(64);
    noise_seed_->setMaxLength(64);
    eb_ = new QLineEdit("6");
    noise_ = new QCheckBox("AWGN enabled");
    noise_->setChecked(true);
    manual_ = new QLineEdit("00011110");
    manual_->setMaxLength(2 * communications_gui_limits::bits);
    manual_->setEnabled(false);
    field("Modulation", modulation_, "comm_modulation", 0, 0);
    field("Waveform source", source_, "comm_source", 0, 1);
    field("Waveform bits", count_, "comm_bit_count", 0, 2);
    field("Symbol rate", rate_, "comm_rate", 1, 0);
    field("Rate unit", rate_unit_, "comm_rate_unit", 1, 1);
    field("Samples/symbol", samples_, "comm_samples", 1, 2);
    field("Bit seed", bit_seed_, "comm_bit_seed", 2, 0);
    field("Noise seed", noise_seed_, "comm_noise_seed", 2, 1);
    field("Link Eb/N0 (dB)", eb_, "comm_eb", 2, 2);
    field("Manual 0/1 bits", manual_, "comm_manual", 3, 0);
    field("Link channel", noise_, "comm_noise", 3, 1);
    auto* simulate_button = new QPushButton("Simulate link");
    simulate_button->setObjectName("comm_simulate");
    form->addWidget(simulate_button, 3, 5);
    tabs_ = new QTabWidget;
    tabs_->setObjectName("comm_tabs");
    layout->addWidget(tabs_, 1);
    auto* wave = new QWidget;
    auto* wave_layout = new QVBoxLayout(wave);
    link_summary_ = new QLabel;
    link_summary_->setObjectName("comm_link_summary");
    link_summary_->setWordWrap(true);
    wave_layout->addWidget(link_summary_);
    i_plot_ = new CommunicationsPlot;
    q_plot_ = new CommunicationsPlot;
    i_plot_->setObjectName("comm_i_plot");
    q_plot_->setObjectName("comm_q_plot");
    wave_layout->addWidget(i_plot_, 1);
    wave_layout->addWidget(q_plot_, 1);
    tabs_->addTab(wave, "I/Q waveforms");
    auto* decisions = new QWidget;
    auto* decisions_layout = new QHBoxLayout(decisions);
    constellation_ = new CommunicationsPlot;
    constellation_->setObjectName("comm_constellation");
    decisions_layout->addWidget(constellation_, 2);
    bits_ = table({"Bit index", "Transmitted", "Recovered", "Error"}, "comm_bits");
    decisions_layout->addWidget(bits_, 1);
    tabs_->addTab(decisions, "Constellation / bits");
    auto* ber_page = new QWidget;
    auto* ber_layout = new QVBoxLayout(ber_page);
    auto* ber_form = new QGridLayout;
    ber_layout->addWidget(new QLabel("Independent seeded Monte Carlo experiment at decision rate; "
                                     "waveform controls do not set its bit budget."));
    ber_layout->addLayout(ber_form);
    start_ = new QLineEdit("-2");
    stop_ = new QLineEdit("10");
    points_ = new QSpinBox;
    points_->setRange(1, communications_gui_limits::points);
    points_->setValue(7);
    budget_ = new QSpinBox;
    budget_->setRange(1, communications_gui_limits::bits_per_point);
    budget_->setValue(100000);
    auto ber_field = [&](const QString& label, QWidget* widget, const char* name, int col) {
        widget->setObjectName(name);
        widget->setAccessibleName(label);
        ber_form->addWidget(new QLabel(label), 0, col);
        ber_form->addWidget(widget, 1, col);
    };
    ber_field("Start Eb/N0 (dB)", start_, "comm_start", 0);
    ber_field("Stop Eb/N0 (dB)", stop_, "comm_stop", 1);
    ber_field("Points", points_, "comm_points", 2);
    ber_field("Bits per point", budget_, "comm_budget", 3);
    auto* run = new QPushButton("Run / resume BER");
    run->setObjectName("comm_run");
    auto* step = new QPushButton("Step BER");
    step->setObjectName("comm_step");
    cancel_ = new QPushButton("Cancel");
    cancel_->setObjectName("comm_cancel");
    cancel_->setEnabled(false);
    auto* reset = new QPushButton("Clear results");
    reset->setObjectName("comm_clear");
    ber_form->addWidget(run, 2, 0);
    ber_form->addWidget(step, 2, 1);
    ber_form->addWidget(cancel_, 2, 2);
    ber_form->addWidget(reset, 2, 3);
    ber_plot_ = new CommunicationsPlot;
    ber_plot_->setObjectName("comm_ber_plot");
    ber_layout->addWidget(ber_plot_, 1);
    ber_table_ = table(
        {"Eb/N0 (dB)", "Bit errors", "Bits tested / requested", "Measured BER", "Theory", "Status"},
        "comm_ber_results");
    ber_layout->addWidget(ber_table_, 1);
    tabs_->addTab(ber_page, "BER experiment");
    auto* help = new QLabel(
        "<h3>Ideal coherent complex baseband</h3><p>BPSK: 0 → +1, 1 → −1. QPSK: 00 → (+I,+Q), 01 → "
        "(+I,−Q), 11 → (−I,−Q), 10 → (−I,+Q), divided by √2. First bit controls I. Exact zero "
        "decisions map to 0. Odd QPSK bit counts are rejected.</p><p>Rectangular pulse: L samples "
        "of s/√L. Matched filter sums samples/√L. Symbol energy is one; amplitudes are normalized, "
        "not physical RMS volts. Decision m is available at (m+1)/Rs. Perfect phase and timing are "
        "assumed.</p><p>Eb/N0 controls independent Gaussian I/Q variance 1/(2k·10^(dB/10)). Both "
        "mappings have theoretical BER ½ erfc(√(10^(dB/10))). More samples/symbol do not improve "
        "BER.</p><p>Waveforms and constellations show the first 2048 samples/decisions; the bit "
        "table shows the first 256 bits. Error counts use the full record. Manual input accepts "
        "0/1 and whitespace only. Numeric input uses dot decimals.</p><p>BER uses separate indexed "
        "bit/noise streams and fixed bit budgets, without storing waveforms. Run/resume and Step "
        "give the same completed counts. Cancel keeps counts and marks unfinished points. Changes "
        "discard old results. Domain switching preserves this workspace; no files are "
        "saved.</p><p>Zero errors means 0 errors in N bits, not proven zero BER. Downward "
        "triangles mark the fixed-N one-sided 95% upper bound 1−0.05^(1/N), not measured BER. "
        "Partial results remain incomplete; adaptively chosen stopping does not retain the fixed-N "
        "coverage guarantee.</p><p>Randomness: specified mt19937_64 streams, open uniforms, "
        "Box–Muller with cached state. Repeatable on one build; Gaussian rounding can vary by "
        "platform. No RF carrier, coding, pulse-shaping options, recovery, or SDR hardware.</p>");
    help->setWordWrap(true);
    help->setTextFormat(Qt::RichText);
    auto* scroll = new QScrollArea;
    scroll->setWidget(help);
    scroll->setWidgetResizable(true);
    tabs_->addTab(scroll, "Conventions");
    status_ = new QLabel;
    status_->setObjectName("comm_status");
    status_->setWordWrap(true);
    layout->addWidget(status_);
    timer_ = new QTimer(this);
    timer_->setInterval(0);
    connect(timer_, &QTimer::timeout, this, &CommunicationsView::advance);
    for (auto* edit : {manual_, rate_, eb_, bit_seed_, noise_seed_, start_, stop_})
        connect(edit, &QLineEdit::textChanged, this, [this] { invalidate(); });
    for (auto* spin : {count_, samples_, points_, budget_})
        connect(spin, &QSpinBox::valueChanged, this, [this] { invalidate(); });
    connect(modulation_, &QComboBox::currentIndexChanged, this, [this] { invalidate(); });
    connect(source_, &QComboBox::currentIndexChanged, this, [this] {
        manual_->setEnabled(source_->currentIndex() == 1);
        count_->setEnabled(source_->currentIndex() == 0);
        invalidate();
    });
    connect(noise_, &QCheckBox::toggled, this, [this](bool on) {
        eb_->setEnabled(on);
        invalidate();
    });
    connect(rate_unit_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (loading_)
            return;
        try {
            const double physical = number(rate_) * std::pow(1000., previous_unit_);
            if (!std::isfinite(physical))
                throw std::invalid_argument("Rate conversion is nonfinite.");
            QSignalBlocker block(rate_);
            rate_->setText(QString::number(physical / std::pow(1000., index), 'g', 17));
            previous_unit_ = index;
            invalidate();
        } catch (const std::exception& e) {
            QSignalBlocker block(rate_unit_);
            rate_unit_->setCurrentIndex(previous_unit_);
            status_->setText("Invalid pending rate: " + QString::fromUtf8(e.what()));
        }
    });
    connect(simulate_button, &QPushButton::clicked, this, &CommunicationsView::simulate);
    connect(run, &QPushButton::clicked, this, [this] {
        try {
            if (experiment_ && experiment_->complete())
                experiment_.reset();
            ensure_experiment();
            cancelled_ = false;
            cancel_->setEnabled(true);
            timer_->start();
        } catch (const std::exception& e) {
            status_->setText(QString::fromUtf8(e.what()));
        }
    });
    connect(step, &QPushButton::clicked, this, [this] {
        timer_->stop();
        cancel_->setEnabled(false);
        try {
            ensure_experiment();
            cancelled_ = false;
            advance();
        } catch (const std::exception& e) {
            status_->setText(QString::fromUtf8(e.what()));
        }
    });
    connect(cancel_, &QPushButton::clicked, this, &CommunicationsView::cancel);
    connect(reset, &QPushButton::clicked, this, [this] { invalidate(); });
    loading_ = false;
    simulate();
}
communications::Modulation CommunicationsView::modulation() const {
    return modulation_->currentIndex() == 0 ? communications::Modulation::bpsk
                                            : communications::Modulation::qpsk;
}
void CommunicationsView::invalidate() {
    if (loading_)
        return;
    timer_->stop();
    cancel_->setEnabled(false);
    cancelled_ = false;
    experiment_.reset();
    link_.reset();
    bits_->setRowCount(0);
    ber_table_->setRowCount(0);
    i_plot_->clear();
    q_plot_->clear();
    constellation_->clear();
    ber_plot_->clear();
    link_summary_->clear();
    status_->setText("Inputs changed; results cleared.");
}
void CommunicationsView::simulate() {
    invalidate();
    try {
        const auto bit_seed = seed(bit_seed_), noise_seed = seed(noise_seed_);
        auto bits = [&] {
            if (source_->currentIndex() == 0)
                return communications::generate_bits(static_cast<std::size_t>(count_->value()),
                                                     bit_seed);
            std::vector<std::uint8_t> values;
            for (auto ch : manual_->text()) {
                if (ch.isSpace())
                    continue;
                if (ch != '0' && ch != '1')
                    throw std::invalid_argument(
                        "Manual bits must contain only 0, 1 and whitespace.");
                if (values.size() >= communications_gui_limits::bits)
                    throw std::invalid_argument("GUI bit limit exceeded.");
                values.push_back(static_cast<std::uint8_t>(ch == '1'));
            }
            return communications::BitSequence(std::move(values));
        }();
        communications::FrameSpec frame{modulation(),
                                        number(rate_) * std::pow(1000., rate_unit_->currentIndex()),
                                        static_cast<std::size_t>(samples_->value())};
        if (communications::validate_frame(bits.size(), frame) > communications_gui_limits::samples)
            throw std::invalid_argument(
                "GUI waveform limit is 65536 complex samples; reduce bits or samples/symbol.");
        link_.emplace(communications::simulate_link(
            {bits, frame, noise_->isChecked() ? std::optional<double>(number(eb_)) : std::nullopt,
             noise_seed}));
        const auto& result = *link_;
        i_plot_->waveform(result, false);
        q_plot_->waveform(result, true);
        constellation_->constellation(result);
        bits_->setRowCount(static_cast<int>(std::min<std::size_t>(256, bits.size())));
        for (int row = 0; row < bits_->rowCount(); ++row) {
            auto i = static_cast<std::size_t>(row);
            bits_->setItem(row, 0, item(QString::number(row)));
            bits_->setItem(row, 1, item(QString::number(bits.bits()[i])));
            bits_->setItem(row, 2, item(QString::number(result.recovered_bits.bits()[i])));
            bits_->setItem(row, 3,
                           item(bits.bits()[i] == result.recovered_bits.bits()[i] ? "" : "Error"));
        }
        QString report =
            QString(
                "Link complete: %1 errors in %2 bits; %3 symbols. Rs=%4 symbols/s; Rb=%5 bits/s; "
                "fs=%6 Hz. Plots show first %7 samples / %8 decisions; bit table first %9 bits.")
                .arg(static_cast<qulonglong>(result.bit_errors))
                .arg(static_cast<qulonglong>(result.bits_tested))
                .arg(result.decisions.size())
                .arg(format(frame.symbol_rate_hz))
                .arg(format(frame.symbol_rate_hz *
                            static_cast<double>(communications::bits_per_symbol(frame.modulation))))
                .arg(format(result.transmitted.sample_rate_hz()))
                .arg(std::min(communications_plot_points, result.transmitted.size()))
                .arg(std::min(communications_plot_points, result.decisions.size()))
                .arg(bits_->rowCount());
        if (!result.bit_errors && noise_->isChecked())
            report += " 95% fixed-N upper bound: " +
                      format(communications::zero_error_upper_bound95(result.bits_tested)) + ".";
        link_summary_->setText(report);
        status_->setText(report);
        tabs_->setCurrentIndex(0);
    } catch (const std::exception& e) {
        status_->setText(QString::fromUtf8(e.what()));
    }
}
void CommunicationsView::ensure_experiment() {
    if (experiment_)
        return;
    const auto count = static_cast<std::size_t>(points_->value());
    const auto budget = static_cast<std::uint64_t>(budget_->value());
    if (budget > communications_gui_limits::aggregate_bits / count)
        throw std::invalid_argument(
            "GUI aggregate BER limit is 10000000 bits; reduce points or bit budget.");
    const double first = number(start_), last = number(stop_);
    if ((count == 1 && first != last) || (count > 1 && first >= last))
        throw std::invalid_argument(
            "Use equal endpoints for one BER point, or increasing endpoints for a sweep.");
    std::vector<double> grid(count, first);
    if (count > 1) {
        grid.back() = last;
        for (std::size_t i = 1; i + 1 < count; ++i)
            grid[i] =
                first + (last - first) * static_cast<double>(i) / static_cast<double>(count - 1);
        for (std::size_t i = 1; i < count; ++i)
            if (!(grid[i] > grid[i - 1]))
                throw std::invalid_argument("BER grid is not strictly increasing.");
    }
    experiment_modulation_ = modulation();
    experiment_ = std::make_unique<communications::BerExperiment>(communications::BerRequest{
        experiment_modulation_, std::move(grid), budget, seed(bit_seed_), seed(noise_seed_)});
    render_ber();
    tabs_->setCurrentIndex(2);
}
void CommunicationsView::advance() {
    if (!experiment_)
        return;
    try {
        const auto results = experiment_->results();
        for (std::size_t i = 0; i < results.size(); ++i)
            if (!results[i].complete()) {
                experiment_->advance(i, communications_gui_limits::chunk_bits);
                break;
            }
        render_ber();
        if (experiment_->complete()) {
            timer_->stop();
            cancel_->setEnabled(false);
        }
    } catch (const std::exception& e) {
        timer_->stop();
        cancel_->setEnabled(false);
        cancelled_ = true;
        render_ber();
        status_->setText("BER interrupted: " + QString::fromUtf8(e.what()));
    }
}
void CommunicationsView::render_ber() {
    if (!experiment_)
        return;
    const auto results = experiment_->results();
    ber_table_->setRowCount(static_cast<int>(results.size()));
    std::uint64_t total = 0, requested = 0;
    for (std::size_t i = 0; i < results.size(); ++i) {
        const auto& p = results[i];
        const int row = static_cast<int>(i);
        total += p.bits_tested;
        requested += p.requested_bits;
        auto* frequency = item(QString::number(p.eb_n0_db, 'g', 17));
        frequency->setData(Qt::UserRole, p.eb_n0_db);
        ber_table_->setItem(row, 0, frequency);
        auto* errors = item(QString::number(static_cast<qulonglong>(p.bit_errors)));
        errors->setData(Qt::UserRole, QVariant::fromValue(static_cast<qulonglong>(p.bit_errors)));
        ber_table_->setItem(row, 1, errors);
        auto* tested = item(QString("%1 / %2")
                                .arg(static_cast<qulonglong>(p.bits_tested))
                                .arg(static_cast<qulonglong>(p.requested_bits)));
        tested->setData(Qt::UserRole, QVariant::fromValue(static_cast<qulonglong>(p.bits_tested)));
        ber_table_->setItem(row, 2, tested);
        QString estimate = "Not evaluated";
        if (p.bits_tested)
            estimate =
                p.bit_errors
                    ? format(*communications::measured_ber(p))
                    : QString("0 errors in %1 bits; 95% upper ≤ %2")
                          .arg(static_cast<qulonglong>(p.bits_tested))
                          .arg(format(communications::zero_error_upper_bound95(p.bits_tested)));
        ber_table_->setItem(row, 3, item(estimate));
        ber_table_->setItem(
            row, 4,
            item(format(communications::theoretical_ber(experiment_modulation_, p.eb_n0_db))));
        ber_table_->setItem(row, 5,
                            item(p.complete()    ? "Complete"
                                 : cancelled_    ? (p.bits_tested ? "Cancelled (partial)"
                                                                  : "Not evaluated (cancelled)")
                                 : p.bits_tested ? "Partial"
                                                 : "Pending"));
    }
    ber_plot_->ber(results, experiment_modulation_);
    status_->setText(QString(experiment_->complete() ? "BER complete: %1 / %2 bits tested."
                             : cancelled_
                                 ? "BER cancelled: %1 / %2 bits tested; unfinished points retained."
                                 : "BER in progress: %1 / %2 bits tested.")
                         .arg(static_cast<qulonglong>(total))
                         .arg(static_cast<qulonglong>(requested)));
}
void CommunicationsView::cancel() {
    timer_->stop();
    cancel_->setEnabled(false);
    cancelled_ = true;
    render_ber();
}
} // namespace openece::gui
