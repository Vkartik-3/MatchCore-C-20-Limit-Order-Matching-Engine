#pragma once

#include <vector>
#include "execution_report.hpp"

namespace trading {

// Represents a matched trade between two orders
class Execution {
public:
    Execution(const ExecutionReport& bid, const ExecutionReport& ask)
        : bidExec(bid)
        , askExec(ask)
    {}

    const ExecutionReport& bidExecution() const { return bidExec; }
    const ExecutionReport& askExecution() const { return askExec; }

private:
    ExecutionReport bidExec;
    ExecutionReport askExec;
};

using Executions = std::vector<Execution>;

} // namespace trading
