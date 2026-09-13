#include "main_window.hpp"
#include "phase_input.hpp"
#include "plot_widget.hpp"

#include <openece/dsp/fft.hpp>
#include <openece/dsp/fir.hpp>
#include <openece/signals/sine.hpp>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace openece::gui {
namespace {
QDoubleSpinBox* field(QWidget* parent, const char* name, double minimum, double maximum,
                      double value, int decimals, const QString& suffix = {}) {
    auto* spin = new QDoubleSpinBox(parent);
    spin->setObjectName(name);
    spin->setDecimals(decimals);
    spin->setRange(minimum, maximum);
    spin->setValue(value);
    spin->setSuffix(suffix);
    spin->setKeyboardTracking(false);
    return spin;
}
} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("OpenECE — Signals / FIR / FFT");
    resize(1160, 820);
    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* layout = new QHBoxLayout(central);

    auto* controls = new QGroupBox("Signal and filter", central);
    controls->setMaximumWidth(400);
    auto* left = new QVBoxLayout(controls);
    auto* form = new QFormLayout;
    amplitude_ = field(controls, "amplitude", 0.0, 1e6, 1.0, 4);
    frequency_ = field(controls, "frequency", 0.0, 5e8, 8.0, 4, " Hz");
    phase_ = new QLineEdit("0", controls);
    phase_->setObjectName("phase");
    phase_->setToolTip(
        "Degrees: decimal within ±360. Radians: decimal or pi/2, 3*pi/4, etc., within ±2*pi.");
    pi_button_ = new QPushButton("π", controls);
    pi_button_->setObjectName("insert_pi");
    pi_button_->setAccessibleName("Insert pi");
    pi_button_->setToolTip("Insert π at the cursor, replacing selected text (Radians only).");
    pi_button_->setFixedWidth(28);
    pi_button_->setEnabled(false);
    phase_unit_ = new QComboBox(controls);
    phase_unit_->setObjectName("phase_unit");
    phase_unit_->setAccessibleName("Phase unit");
    phase_unit_->addItems({"Degrees", "Radians"});
    phase_unit_->setToolTip(
        "Changing units converts the displayed phase; Generate updates the plots.");
    auto* phase_row = new QWidget(controls);
    auto* phase_layout = new QHBoxLayout(phase_row);
    phase_layout->setContentsMargins(0, 0, 0, 0);
    phase_layout->addWidget(phase_, 1);
    phase_layout->addWidget(pi_button_);
    phase_layout->addWidget(phase_unit_);
    phase_row->setFocusProxy(phase_);
    window_ = new QComboBox(controls);
    window_->setObjectName("spectral_window");
    window_->addItem("Rectangular", static_cast<int>(dsp::Window::Rectangular));
    window_->addItem("Hann (periodic)", static_cast<int>(dsp::Window::Hann));
    window_->setToolTip(
        "Hann reduces sidelobes at the cost of a wider main lobe. Only the spectrum is windowed.");
    sample_rate_ = field(controls, "sample_rate", 1.0, 1e9, 1024.0, 4, " Hz");
    duration_ = field(controls, "duration", 0.000001, 1e6, 1.0, 6, " s");
    form->addRow("&Amplitude", amplitude_);
    form->addRow("&Frequency", frequency_);
    form->addRow("&Phase", phase_row);
    form->addRow("Sample &rate", sample_rate_);
    form->addRow("&Duration", duration_);
    form->addRow("Spectral &window", window_);
    filter_ = new QComboBox(controls);
    filter_->setObjectName("filter_type");
    filter_->addItems({"Off", "FIR low-pass (Hamming)"});
    cutoff_ = field(controls, "filter_cutoff", 0.000001, 5e8, 64.0, 6, " Hz");
    cutoff_->setToolTip(
        "Ideal sinc cutoff: strictly between DC and Nyquist; not an exact -3 dB frequency.");
    tap_count_ = new QSpinBox(controls);
    tap_count_->setObjectName("filter_taps");
    tap_count_->setRange(3, 511);
    tap_count_->setSingleStep(2);
    tap_count_->setValue(63);
    tap_count_->setToolTip(
        "Odd tap count. More taps narrow the transition but increase delay and work.");
    cutoff_->setEnabled(false);
    tap_count_->setEnabled(false);
    form->addRow("&Filter", filter_);
    form->addRow("&Cutoff", cutoff_);
    form->addRow("&Taps (odd)", tap_count_);
    left->addLayout(form);
    auto* button = new QPushButton("&Generate and analyze", controls);
    button->setObjectName("generate");
    left->addWidget(button);
    status_ = new QLabel(controls);
    status_->setObjectName("status");
    status_->setWordWrap(true);
    left->addWidget(status_);
    summary_ = new QLabel(controls);
    summary_->setObjectName("summary");
    summary_->setWordWrap(true);
    summary_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    left->addWidget(summary_);
    left->addStretch();
    auto* hint =
        new QLabel("Amplitude is unitless. Frequency must be at or below half the sample rate. "
                   "Up to 65,536 samples per desktop analysis.\n\n"
                   "Drag either plot to zoom; right-click to zoom out.",
                   controls);
    hint->setWordWrap(true);
    left->addWidget(hint);
    auto* control_scroll = new QScrollArea(central);
    control_scroll->setWidgetResizable(true);
    control_scroll->setFrameShape(QFrame::NoFrame);
    control_scroll->setMinimumWidth(310);
    control_scroll->setMaximumWidth(400);
    control_scroll->setWidget(controls);
    layout->addWidget(control_scroll);

    auto* tabs = new QTabWidget(central);
    auto* plots = new QSplitter(Qt::Vertical, tabs);
    time_plot_ = new PlotWidget("Time domain", "Time (s)", "Sample value", false, plots);
    time_plot_->setObjectName("time_plot");
    spectrum_plot_ = new PlotWidget("One-sided amplitude spectrum", "Frequency (Hz)",
                                    "Peak amplitude", true, plots);
    spectrum_plot_->setObjectName("spectrum_plot");
    plots->addWidget(time_plot_);
    plots->addWidget(spectrum_plot_);
    tabs->addTab(plots, "Signals / FFT");
    auto* help = new QTextBrowser(tabs);
    help->setHtml(
        "<h2>Reading the plots</h2>"
        "<p>The generator samples x[n] = A sin(2π f n / fs + φ), starting at t = 0. "
        "Phase defaults to Degrees (decimal, ±360). Radians accepts decimals or a signed "
        "multiple of pi/π optionally divided by a positive decimal: pi, pi/2, 3*pi/4, "
        "3π/4, -pi/2, 2*pi. The range is ±2π. Whitespace between tokens is allowed; "
        "pi is case-insensitive. No sums, parentheses, or general expressions are supported. "
        "The π button inserts at the cursor or replaces selected text.</p>"
        "<p>Generate and unit switching parse the current text. Invalid text is retained with "
        "an error; a failed unit switch keeps the previous unit. Valid switching converts "
        "to decimal text with up to 17 significant digits. The library always receives radians.</p>"
        "<p>The sample count L is fs × requested duration rounded to the nearest integer "
        "(half upward). The actual record duration is L/fs; its last sample is at (L−1)/fs. "
        "Lines connect discrete samples; they are not an analog reconstruction.</p>"
        "<p>The forward FFT uses exp(−j2πkn/N) and no normalization. Records are "
        "windowed before zero-padding to the next power of two, N. The plot shows "
        "|Xw[k]|/sum(w), doubled "
        "for positive-frequency bins except Nyquist. DC and Nyquist are not doubled.</p>"
        "<p>Bins lie at k fs/N, from DC to fs/2 (DC only for one sample). "
        "This is a peak-amplitude spectrum, not RMS, power, PSD, or dB.</p>"
        "<p>Rectangular uses unit weights. Periodic Hann uses w[n] = 0.5 − 0.5 cos(2πn/L). "
        "For one sample both windows use weight 1. Coherent gain is sum(w)/L: 1 for "
        "Rectangular and 0.5 for Hann with L &gt; 1. Dividing by sum(w) compensates tone "
        "amplitude.</p>"
        "<p>Hann reduces distant sidelobes but broadens the main lobe. It changes only spectral "
        "analysis; the time plot shows the original samples. Coherent-gain correction does not "
        "remove off-bin amplitude error or overlap near DC/Nyquist. Very short records are "
        "especially limited. Windowing can spread DC/Nyquist into adjacent bins.</p>"
        "<p>Frequencies that do not complete an integer number of cycles in the record leak "
        "across bins. Zero-padding provides denser "
        "frequency sampling; it does not increase the record's resolving power. "
        "The tallest bin is not always the input amplitude.</p>"
        "<p>At DC and Nyquist, the sampled sine depends on phase: a zero-phase sine "
        "at Nyquist produces (approximately) zero samples. Frequencies above Nyquist "
        "are rejected in this milestone.</p>");
    help->append(
        "<h2>FIR filtering</h2><p>The low-pass uses an odd-length symmetric Hamming-windowed sinc, "
        "normalized to unity DC gain. Its cutoff is the ideal sinc cutoff, not an exact -3 dB "
        "point. "
        "This symmetric design window is separate from the periodic Hann spectrum window.</p>"
        "<p>Full causal convolution uses zero outside the finite input: N+M-1 output samples. "
        "Both records start at t=0 with the same rate. The output duration extends by (M-1)/fs. "
        "Group delay is (M-1)/2 samples, or (M-1)/(2fs) seconds, and remains visible: "
        "no cropping, wrapping, reflection, or delay compensation.</p>"
        "<p>Startup occupies the first M-1 output samples; the last M-1 samples are the tail. "
        "Only indices M-1 through N-1 are fully immersed when N is at least M. "
        "Short records can be dominated by boundaries.</p>"
        "<p>Original and filtered spectra each use their own record length, FFT grid and window "
        "normalization. Their peak ratio is not necessarily steady-state filter gain. "
        "The response tab evaluates H(f)=sum h[k] exp(-j2πfk/fs) directly, without windowing, "
        "normalization or one-sided doubling. It shows 20 log10(|H|), floored at -120 dB "
        "for magnitudes at or below 10^-6, on 1025 points including DC and Nyquist.</p>");
    tabs->addTab(help, "Conventions");
    auto* response_page = new QWidget(tabs);
    auto* response_layout = new QVBoxLayout(response_page);
    response_summary_ = new QLabel(
        "Filter off. Select FIR low-pass and Generate to inspect its response.", response_page);
    response_summary_->setObjectName("response_summary");
    response_summary_->setWordWrap(true);
    response_layout->addWidget(response_summary_);
    response_plot_ = new PlotWidget("FIR magnitude response", "Frequency (Hz)", "Gain (dB)", false,
                                    response_page);
    response_plot_->setObjectName("filter_response_plot");
    response_layout->addWidget(response_plot_, 1);
    tabs->addTab(response_page, "Filter response");
    layout->addWidget(tabs, 1);

    connect(button, &QPushButton::clicked, this, [this] { generate(); });
    for (auto* spin : {amplitude_, frequency_, sample_rate_, duration_, cutoff_}) {
        connect(spin, &QDoubleSpinBox::valueChanged, this, [this] {
            status_->setText("Parameters changed. Generate to update the displayed result.");
        });
    }
    connect(phase_, &QLineEdit::textChanged, this, [this] {
        status_->setText("Parameters changed. Generate to update the displayed result.");
    });
    connect(phase_, &QLineEdit::returnPressed, this, [this] { generate(); });
    connect(pi_button_, &QPushButton::clicked, this, [this] {
        phase_->insert("π");
        phase_->setFocus();
    });
    connect(window_, &QComboBox::currentIndexChanged, this, [this] {
        status_->setText("Parameters changed. Generate to update the displayed result.");
    });
    connect(filter_, &QComboBox::currentIndexChanged, this, [this] {
        cutoff_->setEnabled(filter_->currentIndex() != 0);
        tap_count_->setEnabled(filter_->currentIndex() != 0);
        status_->setText("Parameters changed. Generate to update the displayed result.");
    });
    connect(tap_count_, &QSpinBox::valueChanged, this, [this] {
        status_->setText("Parameters changed. Generate to update the displayed result.");
    });
    connect(phase_unit_, &QComboBox::currentIndexChanged, this, [this] { change_phase_unit(); });
    generate();
}

void MainWindow::change_phase_unit() {
    try {
        const double radians = parse_phase(phase_->text(), phase_in_radians_);
        const bool new_unit = phase_unit_->currentIndex() == 1;
        const QString converted = format_phase(radians, new_unit);
        phase_in_radians_ = new_unit;
        const QSignalBlocker blocker(phase_);
        phase_->setText(converted);
        pi_button_->setEnabled(new_unit);
        status_->setText(
            "Parameters changed. Phase converted to selected units; Generate to update.");
    } catch (const std::exception& error) {
        const QSignalBlocker blocker(phase_unit_);
        phase_unit_->setCurrentIndex(phase_in_radians_ ? 1 : 0);
        clear_results();
        status_->setText(
            QString("Cannot change phase units: %1").arg(QString::fromUtf8(error.what())));
    }
}

void MainWindow::clear_results() {
    time_plot_->clear();
    spectrum_plot_->clear();
    response_plot_->clear();
    summary_->clear();
    response_summary_->clear();
}

void MainWindow::generate() {
    try {
        const signals::SineParameters parameters{amplitude_->value(), frequency_->value(),
                                                 parse_phase(phase_->text(), phase_in_radians_),
                                                 sample_rate_->value(), duration_->value()};
        const double count =
            std::floor(parameters.sample_rate_hz * parameters.duration_seconds + 0.5);
        if (count > 65536.0) {
            throw std::invalid_argument(
                "Use a shorter duration or lower sample rate (maximum 65,536 samples).");
        }
        const auto signal = signals::generate_sine(parameters);
        const auto spectrum = dsp::amplitude_spectrum(
            signal, static_cast<dsp::Window>(window_->currentData().toInt()));
        std::vector<double> times(signal.size());
        for (std::size_t i = 0; i < times.size(); ++i) {
            times[i] = signal.time_seconds(i);
        }
        std::vector<double> frequencies(spectrum.amplitudes.size());
        for (std::size_t i = 0; i < frequencies.size(); ++i) {
            frequencies[i] = static_cast<double>(i) * spectrum.bin_width_hz();
        }
        const double time_limit = parameters.amplitude > 0.0 ? parameters.amplitude * 1.1 : 1.0;
        time_plot_->set_samples(times, signal.samples(), signal.duration_seconds(), -time_limit,
                                time_limit);
        const double peak = *std::ranges::max_element(spectrum.amplitudes);
        spectrum_plot_->set_samples(frequencies, spectrum.amplitudes, signal.sample_rate_hz() / 2.0,
                                    0.0, peak > 1e-12 ? peak * 1.1 : 1.0);
        summary_->setText(
            QString("Displayed record\n%1 samples · %2 s\nFFT length: %3\n"
                    "Bin spacing: %4 Hz\nNyquist: %5 Hz\nWindow: %6\nCoherent gain: %7")
                .arg(static_cast<qulonglong>(signal.size()))
                .arg(signal.duration_seconds(), 0, 'g', 8)
                .arg(static_cast<qulonglong>(spectrum.fft_size))
                .arg(spectrum.bin_width_hz(), 0, 'g', 8)
                .arg(signal.sample_rate_hz() / 2.0, 0, 'g', 8)
                .arg(window_->currentText())
                .arg(spectrum.coherent_gain, 0, 'g', 8));
        if (filter_->currentIndex() != 0) {
            const auto taps = static_cast<std::size_t>(tap_count_->value());
            if (signal.size() > 65536 - (taps - 1)) {
                throw std::invalid_argument(
                    "Full FIR output exceeds 65,536 samples. Reduce duration or tap count.");
            }
            const auto coefficients =
                dsp::design_lowpass(taps, cutoff_->value(), signal.sample_rate_hz());
            const auto filtered = dsp::apply_fir_full(signal, coefficients);
            const auto filtered_spectrum = dsp::amplitude_spectrum(
                filtered, static_cast<dsp::Window>(window_->currentData().toInt()));
            std::vector<double> filtered_times(filtered.size()),
                filtered_frequencies(filtered_spectrum.amplitudes.size());
            double comparison_limit = time_limit;
            for (std::size_t i = 0; i < filtered.size(); ++i) {
                filtered_times[i] = filtered.time_seconds(i);
                comparison_limit =
                    std::max(comparison_limit, 1.1 * std::abs(filtered.samples()[i]));
            }
            for (std::size_t i = 0; i < filtered_frequencies.size(); ++i) {
                filtered_frequencies[i] = static_cast<double>(i) * filtered_spectrum.bin_width_hz();
            }
            time_plot_->set_comparison(times, signal.samples(), filtered_times, filtered.samples(),
                                       filtered.duration_seconds(), -comparison_limit,
                                       comparison_limit);
            const double comparison_peak =
                std::max(peak, *std::ranges::max_element(filtered_spectrum.amplitudes));
            spectrum_plot_->set_comparison(frequencies, spectrum.amplitudes, filtered_frequencies,
                                           filtered_spectrum.amplitudes,
                                           signal.sample_rate_hz() / 2.0, 0.0,
                                           comparison_peak > 1e-12 ? comparison_peak * 1.1 : 1.0);
            const auto response =
                dsp::frequency_response(coefficients, signal.sample_rate_hz(), 1025);
            std::vector<double> response_frequencies(response.values.size()),
                response_db(response.values.size());
            for (std::size_t i = 0; i < response.values.size(); ++i) {
                response_frequencies[i] =
                    (signal.sample_rate_hz() / 2.0) *
                    (static_cast<double>(i) / static_cast<double>(response.values.size() - 1));
                response_db[i] = 20.0 * std::log10(std::max(1e-6, std::abs(response.values[i])));
            }
            response_plot_->set_samples(response_frequencies, response_db,
                                        signal.sample_rate_hz() / 2.0, -120.,
                                        std::max(3., *std::ranges::max_element(response_db) + 3.));
            const double delay = static_cast<double>((taps - 1) / 2);
            const QString delay_text = QString("Group delay: %1 samples · %2 s (uncompensated)")
                                           .arg(delay)
                                           .arg(delay / signal.sample_rate_hz(), 0, 'g', 8);
            summary_->setText(
                summary_->text() +
                QString("\n\nFiltered record: %1 samples · %2 s\nFFT length: %3 · Bin spacing: %4 "
                        "Hz\nCoherent gain: %5\n%6\nDuration extension: %7 s\nZero extension; full "
                        "output, including tail.")
                    .arg(static_cast<qulonglong>(filtered.size()))
                    .arg(filtered.duration_seconds(), 0, 'g', 8)
                    .arg(static_cast<qulonglong>(filtered_spectrum.fft_size))
                    .arg(filtered_spectrum.bin_width_hz(), 0, 'g', 8)
                    .arg(filtered_spectrum.coherent_gain, 0, 'g', 8)
                    .arg(delay_text)
                    .arg(static_cast<double>(taps - 1) / signal.sample_rate_hz(), 0, 'g', 8));
            if (signal.size() < taps)
                summary_->setText(summary_->text() + "\nShort record: no fully immersed interval.");
            response_summary_->setText(
                QString("%1 taps · Hamming-windowed sinc · Ideal cutoff %2 Hz (not an exact -3 dB "
                        "point).\n%3\n20 log10(|H|); display floor -120 dB for |H| ≤ 10^-6. DC and "
                        "Nyquist included.\nSignal spectra use different record lengths and "
                        "normalization; their peak ratio need not equal this gain.")
                    .arg(static_cast<qulonglong>(taps))
                    .arg(cutoff_->value(), 0, 'g', 8)
                    .arg(delay_text));
        } else {
            response_plot_->clear();
            response_summary_->setText(
                "Filter off. Select FIR low-pass and Generate to inspect its response.");
        }
        status_->setText("Ready — plots match the current parameters.");
    } catch (const std::exception& error) {
        clear_results();
        status_->setText(QString("Cannot generate: %1").arg(QString::fromUtf8(error.what())));
    }
}

} // namespace openece::gui
