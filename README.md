<!---
Copyright (c) 2019 Michael Vilim

This file is part of the pbd library. It is currently hosted at
https://github.com/mvilim/pbd

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
-->

# PBD data format

PBD is a data (file and stream) format for transmitting self-describing [Protobuf](https://developers.google.com/protocol-buffers/) messages. Each PBD stream encodes a sequence of messages of the same Protobuf message type.

## Format

All numbers (outside of the serialized Protobuf messages) in the PBD format are encoded using LEB128 variable length encoding.

The PBD format definition:
```
    - PBD magic bytes [0x00, 0x00, 0x10, 0xBD]
    - PBD version number (LEB128)
    - Number of descriptor files that will follow (LEB128)
    - Repeated:
        - Byte length of next descriptor file (LEB128)
        - Serialized FileDescriptorProto
    - Byte length of the full name of the main message type (LEB128)
    - Full name of the main message type (UTF-8 encoded)
    - Repeated:
        - Byte length of next Proto message (LEB128)
        - Serialized message
```

## Usage

This library provides a C++ implementation of the PBD format. It is capable of writing and reading PBD streams. To write the stream, the library accesses the necessary metadata (Proto message definitions) from the first message written and uses it to write the header (containing all necessary proto definitions).

## Building

Requires Protobuf v3 library.

To build:

```
mkdir build
cd build
cmake ..
make
```

You can test by running the `example` binary produced (which will write an example PBD file);

## License

This project is licensed under the [Apache License 2.0](https://github.com/mvilim/pbd/blob/master/LICENSE). The Protobuf license can be found [here](https://github.com/mvilim/pbd/blob/master/thirdparty/protobuf/LICENSE).
