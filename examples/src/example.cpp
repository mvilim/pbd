// Copyright (c) 2019 Michael Vilim
//
// This file is part of the pbd library. It is currently hosted at
// https://github.com/mvilim/pbd
//
// pbd is licensed under the GPL3, a copy of which can be
// found here: https://github.com/mvilim/pbd/blob/master/LICENSE

#include <pbd/pbd.hpp>

#include <example.pb.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/dynamic_message.h>

#include <iostream>
#include <fstream>

namespace pbde = pbd::example;
namespace pb = google::protobuf;

using std::string;

int main() {
    pbde::SimpleMessage message;
    message.set_a(13);
    message.mutable_m()->set_b(23);
    message.mutable_d()->set_c(33);
    message.mutable_d()->set_d(-1.3);
    message.set_e(pbde::SimpleEnum::B);
    message.add_f(2.3);
    message.add_f(3.3);

    std::ostringstream oss;
    std::ofstream ofs("example.pbd");
    pbd::PBDWriter pw(oss);
    pw.write(message);
    string ss = oss.str();
    std::istringstream iss(ss);
    pbd::PBDReader pr(iss);
    ofs.write(ss.data(), ss.length());
    unique_ptr<pb::Message> m = pr.readMessage();
    std::cout << m->DebugString() << std::endl;
    std::cout << message.DebugString() << std::endl;
    if (m->DebugString() != message.DebugString() ||
        m->GetDescriptor()->full_name() != message.GetDescriptor()->full_name()) {
        throw std::runtime_error("Converted message does not match");
    }
    pr.readMessage(m);
    if (m) {
        throw std::runtime_error("End of stream did not return empty message pointer");
    }

    return 0;
}
