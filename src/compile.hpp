#pragma once

#include "analyser.hpp"   // PackageRef


// cycle_err：命中 import 循环依赖时写入报错消息并返回 none
xpOption<PackageRef> compile_package_from_path(xpString path, xpString *cycle_err = nullptr);
xpOption<PackageRef> compile_package_from_import(xpString import_path, xpString *cycle_err = nullptr);
