#pragma once
namespace NativeNameThread {
// Pending, deliberately false. This input must be generated from an accepted
// exact postpackage import/provider receipt; source/NID correlation alone is
// not final PRX provider admission. Candidate startup explicitly refuses while
// this control is unset. No selected/frozen source is changed.
inline constexpr bool kExactPostpackageImportAdmitted=false;
}
