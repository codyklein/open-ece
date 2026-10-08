#pragma once
#include <QString>
#include <openece/circuits/transient/analysis.hpp>
#include <openece/project/model.hpp>
namespace openece::gui {
namespace transient_core = circuits::transient;
struct TransientProbeLabel {
    QString label;
    bool voltage;
    std::size_t index;
};
// Immutable run inputs/metadata, separate from the borrowed editable draft.
struct TransientExecution {
    transient_core::Circuit circuit;
    transient_core::Request request;
    std::vector<TransientProbeLabel> probes;
    QString time_unit;
    double time_scale;
};
// Parsing is an explicit execution action only. Inactive fields are never parsed.
TransientExecution transient_execution(const project::TransientDraft&);
// Raw projection only: no numeric parsing/repair or persistence mutation.
project::TransientDraft transient_active_draft(project::TransientDraft);
} // namespace openece::gui
