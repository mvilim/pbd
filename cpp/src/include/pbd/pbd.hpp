// Copyright (c) 2019 Michael Vilim
//
// This file is part of the pbd library. It is currently hosted at
// https://github.com/mvilim/pbd
//
// pbd is licensed under the GPL3, a copy of which can be
// found here: https://github.com/mvilim/pbd/blob/master/LICENSE

#pragma once

#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/message.h>
#include <array>
#include <iostream>
#include <map>
#include <ostream>
#include <set>
#include <sstream>

using std::array;
using std::istream;
using std::map;
using std::ostream;
using std::set;
using std::string;
using std::unique_ptr;
using std::vector;

const unsigned char MAGIC_PBD_BYTES[] = {0x00, 0x00, 0x10, 0xBD};
const uint64_t PBD_VERSION = 0;

namespace pbd {

namespace pb = google::protobuf;

using Limit = pb::io::CodedInputStream::Limit;

struct CompareFileDescriptorName {
    bool operator()(const pb::FileDescriptor* a, const pb::FileDescriptor* b) const {
        bool name_comparison = a->name() < b->name();
        if (name_comparison) {
            return true;
        } else {
            if (b->name() < a->name()) {
                return false;
            }
            return a->package() < b->package();
        }
    }
};

static bool fileEquals(const pb::FileDescriptor* a, const pb::FileDescriptor* b) {
    CompareFileDescriptorName c;
    return c(a, b) == false && c(b, a) == false;
}

class FileBuilder {
    struct CompareDescriptors {
        bool operator()(const pb::Descriptor* a, const pb::Descriptor* b) const {
            return a->full_name() < b->full_name();
        }
    };

    struct CompareEnumDescriptors {
        bool operator()(const pb::EnumDescriptor* a, const pb::EnumDescriptor* b) const {
            return a->full_name() < b->full_name();
        }
    };

    struct CompareFileDescriptors {
        bool operator()(const pb::Descriptor* a, const pb::Descriptor* b) const {
            return a->full_name() < b->full_name();
        }
    };

    pb::FileDescriptorProto fileProto;
    set<const pb::Descriptor*, CompareDescriptors> addedDescriptors;
    set<const pb::EnumDescriptor*, CompareEnumDescriptors> addedEnumDescriptors;
    set<const pb::FileDescriptor*, CompareFileDescriptorName> addedDependencies;

   public:
    FileBuilder(const pb::FileDescriptor* fileDesc) {
        // alternatively, we could copy the entirety of the file descriptor into the proto (instead
        // of trying to carefully extract just the bits we need)
        fileProto.set_name(fileDesc->name());
        fileProto.set_package(fileDesc->package());
        fileProto.set_syntax(fileDesc->SyntaxName(fileDesc->syntax()));
    }

    void addDependency(const pb::FileDescriptor* file) {
        if (!addedDependencies.count(file)) {
            addedDependencies.insert(file);
            fileProto.add_dependency(file->name());
        }
    }

    void addMessage(const pb::Descriptor* messageDesc) {
        addedDescriptors.insert(messageDesc);
        messageDesc->CopyTo(fileProto.add_message_type());
    }

    void addEnum(const pb::EnumDescriptor* enumDesc) {
        addedEnumDescriptors.insert(enumDesc);
        enumDesc->CopyTo(fileProto.add_enum_type());
    }

    const pb::FileDescriptorProto& file() const {
        return fileProto;
    }
};

class ProtoDependencyResolver {
    map<const pb::FileDescriptor*, FileBuilder, CompareFileDescriptorName> fileMap;

   public:
    void addMessage(const pb::Descriptor* message) {
        const pb::FileDescriptor* fileDesc = message->file();
        if (!fileMap.count(fileDesc)) {
            fileMap.emplace(fileDesc, fileDesc);
        }
        FileBuilder& fb = fileMap.at(fileDesc);
        fb.addMessage(message);

        for (size_t i = 0; i < message->field_count(); i++) {
            const pb::FieldDescriptor* field = message->field(i);
            if (field->type() == pb::FieldDescriptor::Type::TYPE_MESSAGE) {
                addMessage(field->message_type());
                const pb::FileDescriptor* fieldFile = field->message_type()->file();
                if (!fileEquals(field->message_type()->file(), message->file())) {
                    fb.addDependency(fieldFile);
                }
            } else if (field->type() == pb::FieldDescriptor::Type::TYPE_ENUM) {
                addEnum(field->enum_type());
            }
        }
    }

    void addEnum(const pb::EnumDescriptor* enumDesc) {
        const pb::FileDescriptor* fileDesc = enumDesc->file();
        if (!fileMap.count(fileDesc)) {
            fileMap.emplace(fileDesc, fileDesc);
        }
        FileBuilder& fb = fileMap.at(fileDesc);
        fb.addEnum(enumDesc);
    }

    vector<const pb::FileDescriptorProto*> files() {
        vector<const pb::FileDescriptorProto*> v;
        for (const auto& fb : fileMap) {
            v.push_back(&fb.second.file());
        }

        return v;
    }
};

static void writeLEB128(uint64_t val, ostream& s) {
    while (val >= 128) {
        uint8_t c = val | 0x80;
        s.put(c);
        val >>= 7;
    }
    s.put(val);
}

static uint64_t readLEB128(istream& s) {
    uint64_t val = 0;
    uint16_t counter = 0;
    unsigned char c;
    do {
        c = s.get();
        uint64_t sec = c & 0x7F;
        bool b = val += (sec << (7 * counter++));
    } while ((c & 0x80) > 0);
    return val;
}

class PBDWriter {
    bool headerWritten = false;
    ostream& out;

   public:
    PBDWriter(ostream& out) : out(out) {}

    void write(pb::Message& message) {
        if (!headerWritten) {
            pb::SourceLocation loc;
            message.GetDescriptor()->GetSourceLocation(&loc);
            ProtoDependencyResolver resolver;
            resolver.addMessage(message.GetDescriptor());
            writeHeader(*message.GetDescriptor());
            headerWritten = true;
        }

        writeMessage(message);
    }

   private:
    void writeHeader(const pb::Descriptor& desc) {
        write(MAGIC_PBD_BYTES, sizeof(MAGIC_PBD_BYTES));
        write(PBD_VERSION);

        pb::SourceLocation loc;
        desc.GetSourceLocation(&loc);
        ProtoDependencyResolver resolver;
        resolver.addMessage(&desc);

        vector<const pb::FileDescriptorProto*> files = resolver.files();
        write(files.size());
        for (const pb::FileDescriptorProto* file : files) {
            string s = file->SerializeAsString();
            write(s.length());
            write(s.data(), s.length());
        }

        string name = desc.full_name();
        write(name.length());
        write(name.data(), name.length());
    }

    void writeMessage(pb::Message& message) {
        string s = message.SerializeAsString();
        write(s.length());
        write(s.data(), s.length());
    }

    void write(const char* s, size_t n) {
        out.write(s, n);
    }

    void write(const unsigned char* s, size_t n) {
        out.write((char*)s, n);
    }

    void write(uint64_t i) {
        writeLEB128(i, out);
    }
};

class PBDReader {
    bool headerRead = false;
    pb::io::IstreamInputStream iis;
    pb::io::CodedInputStream cis;
    pb::DescriptorPool pool;
    pb::DynamicMessageFactory fact;
    const pb::Descriptor* desc;
    const pb::Message* prototype;

   public:
    PBDReader(istream& in) : iis(&in), cis(&iis) {}

    // need to add a note that the reader must be kept alive while using the messages from it, as
    // they share some info in the dynamic message factory)
    unique_ptr<pb::Message> readMessage() {
        checkHeader();

        unique_ptr<pb::Message> m = unique_ptr<pb::Message>(prototype->New());
        readMessageAssumeHeader(m);
        return m;
    }

    // this is if we want to simply re-read into the same message
    //
    // perhaps we should refactor so that the same call is used for both cases? pointer to pointer?
    void readMessage(unique_ptr<pb::Message>& message) {
        checkHeader();

        readMessageAssumeHeader(message);
    }

    const pb::Descriptor* descriptor() {
        return desc;
    }

    const pb::io::CodedInputStream* stream() {
        return &cis;
    }

   private:
    void checkHeader() {
        if (!headerRead) {
            readHeader();
            headerRead = true;
        }
    }

    void readMessageAssumeHeader(unique_ptr<pb::Message>& message) {
        uint64_t protoMessageSize;
        if (!cis.ReadVarint64(&protoMessageSize)) {
            message.reset();
            return;
        }

        Limit limit = cis.PushLimit(protoMessageSize);
        message->ParseFromCodedStream(&cis);
        cis.CheckEntireMessageConsumedAndPopLimit(limit);
    }

    void readHeader() {
        unsigned char magic[sizeof(MAGIC_PBD_BYTES)];
        cis.ReadRaw(&magic, sizeof(MAGIC_PBD_BYTES));
        if (!std::equal(std::begin(magic), std::end(magic), std::begin(MAGIC_PBD_BYTES))) {
            throw std::runtime_error(
                "Magic PBD bytes do not match -- data is either corrupt or not a PBD file");
        }

        uint64_t pbdVersion;
        cis.ReadVarint64(&pbdVersion);
        if (pbdVersion != PBD_VERSION) {
            throw std::runtime_error("PBD file version not supported");
        }

        uint64_t numProto;
        cis.ReadVarint64(&numProto);
        for (size_t i = 0; i < numProto; i++) {
            uint64_t protoFileSize;
            cis.ReadVarint64(&protoFileSize);
            pb::FileDescriptorProto parsedProtoFile;
            Limit limit = cis.PushLimit(protoFileSize);
            parsedProtoFile.ParseFromCodedStream(&cis);
            cis.CheckEntireMessageConsumedAndPopLimit(limit);
            pool.BuildFile(parsedProtoFile);
        }

        uint64_t messageNameLength;
        cis.ReadVarint64(&messageNameLength);
        string name(messageNameLength, 0);
        cis.ReadRaw(&name[0], messageNameLength);
        desc = pool.FindMessageTypeByName(name);

        prototype = fact.GetPrototype(desc);
    }
};

}  // namespace pbd
