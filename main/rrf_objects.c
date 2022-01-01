//
// Copyright (c) 2021 Wolfgang Christl
// Licensed under Apache License, Version 2.0 - https://opensource.org/licenses/Apache-2.0

#include <string.h>
#include "rrf_objects.h"

reprap_model_t reprap_model;

/**
 * Init an empty reprap variable structure
 * @return An empty reprap_model_t structure to be filled by the parser
 */
reprap_model_t init_reprap_model() {
    memset(&reprap_model, 0, sizeof(reprap_model_t));
    return reprap_model;
}
