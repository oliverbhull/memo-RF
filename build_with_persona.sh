#!/bin/bash
# Build script demonstrating persona and translation language options

# Example 1: Build with manufacturing persona
# cmake -B build -DAGENT_PERSONA=manufacturing && cmake --build build

# Example 2: Build with translator persona and French target
# cmake -B build -DAGENT_PERSONA=translator -DTRANSLATE_LANGUAGE=French && cmake --build build

# Example 3: Build with translator persona and Spanish target
# cmake -B build -DAGENT_PERSONA=translator -DTRANSLATE_LANGUAGE=Spanish && cmake --build build

# Example 4: Build with translator persona and German target
# cmake -B build -DAGENT_PERSONA=translator -DTRANSLATE_LANGUAGE=German && cmake --build build

# Example 5: Build with custom language (e.g., Italian, Japanese, etc.)
# cmake -B build -DAGENT_PERSONA=translator -DTRANSLATE_LANGUAGE=Italian && cmake --build build

# Default: manufacturing persona
echo "Building with default settings..."
echo "To customize, run one of the examples above"
cmake -B build -DAGENT_PERSONA=manufacturing && cmake --build build
