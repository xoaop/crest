#include "context.hpp"

static Context global_context;

Context *context() {
    return &global_context;
}



PackageRef add_package(Context *ctx, Package pkg) {
    ctx->all_packages.push_back(std::move(pkg));
    return ctx->all_packages.count - 1;
}