#pragma once

#include "ref.hpp"

struct CIRPackage;

template<>
CIRPackage* Ref<CIRPackage>::resolve() const;
