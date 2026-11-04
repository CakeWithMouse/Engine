#pragma once
#include <stdexcept>
// Missing optional build capabilities are reported separately from passes.
struct TestSkipped : std::runtime_error { using std::runtime_error::runtime_error; };
