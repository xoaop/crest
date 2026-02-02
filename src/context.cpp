#include "context.hpp"

static Context global_context;

Context *context() {
    return &global_context;
}