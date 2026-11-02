#pragma once

#include <nlohmann/json.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace JsonConfig {

using json = nlohmann::json;

inline json read_json_file(const std::filesystem::path& path)
{
    std::ifstream file{path};

    if (!file)
    {
        throw std::runtime_error(
            "Cannot open cache config: " + path.string()
        );
    }

    try
    {
        json root;
        file >> root;
        return root;
    }
    catch (const json::parse_error& error)
    {
        throw std::invalid_argument(
            "Invalid JSON in '" + path.string() + "': " + error.what()
        );
    }
}

inline const json& require_levels(const json& root)
{
    if (!root.is_object())
    {
        throw std::invalid_argument(
            "Cache config root must be a JSON object"
        );
    }

    if (!root.contains("levels"))
    {
        throw std::invalid_argument(
            "Cache config must contain 'levels'"
        );
    }

    const json& levels = root.at("levels");

    if (!levels.is_array())
    {
        throw std::invalid_argument(
            "Cache config field 'levels' must be an array"
        );
    }

    if (levels.empty())
    {
        throw std::invalid_argument(
            "Cache must contain at least one level"
        );
    }

    return levels;
}

inline std::string require_policy_name(
    const json& level,
    std::size_t level_index
)
{
    const std::string prefix =
        "Cache level " + std::to_string(level_index) + ": ";

    if (!level.is_object())
    {
        throw std::invalid_argument(
            prefix + "level description must be an object"
        );
    }

    if (!level.contains("policy") || !level.at("policy").is_string())
    {
        throw std::invalid_argument(
            prefix + "field 'policy' must be a string"
        );
    }

    return level.at("policy").get<std::string>();
}

inline const json& require_options(
    const json& level,
    std::size_t level_index
)
{
    const std::string prefix =
        "Cache level " + std::to_string(level_index) + ": ";

    if (!level.contains("options") || !level.at("options").is_object())
    {
        throw std::invalid_argument(
            prefix + "field 'options' must be an object"
        );
    }

    return level.at("options");
}

inline std::size_t require_capacity(
    const json& options,
    std::size_t level_index
)
{
    const std::string prefix =
        "Cache level " + std::to_string(level_index) + ": ";

    if (!options.contains("capacity") ||
        !options.at("capacity").is_number_unsigned())
    {
        throw std::invalid_argument(
            prefix + "field 'capacity' must be a positive integer"
        );
    }

    const std::size_t capacity =
        options.at("capacity").get<std::size_t>();

    if (capacity == 0)
    {
        throw std::invalid_argument(
            prefix + "field 'capacity' must be greater than zero"
        );
    }

    return capacity;
}

} // namespace JsonConfig
