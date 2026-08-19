#pragma once

#include "analyser.hpp"


// cycle_err：命中 import 循环依赖时写入报错消息并返回 none
xpOption<Ref<Package>> compile_package_from_path(xpString path, xpString *cycle_err = nullptr);
xpOption<Ref<Package>> compile_package_from_import(xpString import_path, xpString *cycle_err = nullptr);
