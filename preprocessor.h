#pragma once
#include <string>
#include <vector>

struct PreprocessResult {
    std::string code;
    std::vector<std::string> errors;
    bool ok() const { return errors.empty(); }
};

PreprocessResult preprocess(const std::string& source);