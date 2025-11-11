Copyright 2025 Vladyslav Kolodii. All rights reserved.

# BEJ-to-JSON Converter

A C-based utility that decodes **Binary Encoded JSON (BEJ)** data into standard **JSON** format.  
Implements the **DSP0218 Redfish "Binary Encoded JSON (BEJ)"** specification, including schema and annotation dictionary handling.

---

## Overview

BEJ (Binary Encoded JSON) is a compact binary format defined by the DMTF in the [DSP0218](https://www.dmtf.org/standards/redfish) specification.  
This project provides a **reference decoder** that reads BEJ-encoded files and converts them into human-readable JSON using schema and annotation dictionaries.

The program:
- Loads the **Schema Dictionary** and **Annotation Dictionary**
- Parses BEJ tuples (`SFLV` — Sequence, Format, Length, Value)
- Resolves names and formats using dictionary entries
- Outputs a formatted JSON file

---

## Features

- Full parsing of BEJ SFLV tuples  
- Dictionary-based name resolution  
- Support for multiple BEJ formats (SET, ARRAY, INTEGER, STRING, ENUM, REAL, BOOLEAN, NULL)  
- Buffer-based decoding (supports reading from both files and memory)  
- Verbose debug output for tracing BEJ parsing steps  
- Implemented using only standard C (no external dependencies)

---

## Build Instructions

### Requirements
- **CMake ≥ 3.23**
- **GCC / Clang / MinGW / MSVC**

## Usage

### Command Syntax
```
BEJ-to-JSON <command> [options] <filename> [options] <filename> [options] <filename>
```

### Decode BEJ File
```
BEJ-to-JSON decode -s <schema_dictionary.bin> -a <annotation_dictionary.bin> -b <bej_encoded_file.bin> [options]
```

### Options
| Option            | Description                            |
|-------------------|----------------------------------------|
| `-s <file>`       | Path to the Schema Dictionary file     |
| `-a <file>`       | Path to the Annotation Dictionary file |
| `-b <file>`       | Path to the BEJ-encoded binary file    |
| `-v`, `--verbose` | Enable verbose output for debugging    |

Example:
```bash
BEJ-to-JSON decode -s schema.bin -a annotation.bin -b example.bin -v
```

Output JSON will be created with the same base name as the input BEJ file, e.g.:
```
record.json
```

---

## Implementation Notes

- **Dictionaries** are loaded and parsed according to section **7.2.3.2** of the DSP0218 specification.
- Each dictionary entry contains:
  - Format
  - Sequence number
  - Child offsets/counts
  - Name length and offset
  - Entry name (resolved dynamically)
- The decoder reconstructs the JSON hierarchy recursively using the **SFLV** structure.
- **Non-Negative Integers (NNINT)** are implemented per **5.3.6**, with variable byte-length encoding.

---

##  Key Source Files

| File | Description |
|------|--------------|
| `main.c` | CLI argument parser, command handler, and entry point |
| `decode.c` | Core decoding logic (dictionary loading, SFLV parsing, BEJ-to-JSON conversion) |
| `decode.h` | Structure and function declarations (DSP0218-aligned types) |
| `CMakeLists.txt` | Build configuration |

---

## References

- **DSP0218 Redfish Specification:**  
  [https://www.dmtf.org/sites/default/files/standards/documents/DSP0218_1.2.0.pdf](https://www.dmtf.org/sites/default/files/standards/documents/DSP0218_1.2.0.pdf)
- **DMTF BEJ Format Overview** — DSP0218_1.2.0  
  (Sections 5.3–7.2)

---

## Author

Developed by **[Vladyslav Kolodii]**  
For educational and research purposes on BEJ decoding and DSP0218 compliance.

---

## License

This project is released under the **BSD 3-Clause License**.  

---

