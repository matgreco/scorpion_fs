#include "priorities_strategy.h"

#include <iostream>

using namespace std;

namespace OpPriorities {

static class OpPrioritiesStrategyCategoryPlugin : public plugins::TypedCategoryPlugin<PrioritiesStrategy> {
public:
    OpPrioritiesStrategyCategoryPlugin() : TypedCategoryPlugin("OpPrioritiesStrategy") {
        document_synopsis(
            "OpPriorities"
            );
    }
}
_category_plugin;
}
