#pragma once

#include "node/node.hpp"
#include "node/composite.hpp"
#include "lib/types.hpp"

#include <fstream>

namespace Config {

void loadSchema(std::ifstream& config_file, SharedPtr<SchemaComposite> root_config);
void load(std::ifstream& config_file, SharedPtr<Composite> root_config, SharedPtr<SchemaComposite> root_schema);

} // namespace Config