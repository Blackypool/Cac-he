#pragma once

#include <stdexcept>

namespace MyCache {

class cache_error : public std::runtime_error
{
    public:
        using std::runtime_error::runtime_error;
};

class configuration_error : public cache_error
{
    public:
        using cache_error::cache_error;
};

class invalid_operation : public cache_error
{
    public:
        using cache_error::cache_error;
};

} // namespace MyCache
