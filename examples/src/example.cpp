// Copyright (c) 2019 Michael Vilim
//
// This file is part of the pbd library. It is currently hosted at
// https://github.com/mvilim/pbd
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <pbd/pbd.hpp>

#include <example.pb.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/dynamic_message.h>

#include <iostream>
#include <fstream>

namespace pbde = pbd::example;
namespace pbdde = pbd::dep_example;

namespace pb = google::protobuf;

using std::string;

void test_simple_message()
{
    pbde::SimpleMessage message;
    message.set_a(13);
    message.mutable_m()->set_b(23);
    message.mutable_d()->set_c(33);
    message.mutable_d()->set_d(-1.3);
    message.set_e(pbde::SimpleEnum::B);
    message.add_f(2.3);
    message.add_f(3.3);
    message.set_s("test");
    message.set_de(pbdde::DependencyEnum::DE1);
    message.add_rm()->set_b(11);
    message.add_rm()->set_b(22);

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
}

void test_empty_message()
{
    pbde::EmptyMessage message;

    std::ostringstream oss;
    pbd::PBDWriter pw(oss);
    pw.write(message);
    pw.write(message);
    string ss = oss.str();
    std::istringstream iss(ss);
    pbd::PBDReader pr(iss);
    unique_ptr<pb::Message> m = pr.readMessage();
    m = pr.readMessage();
    if (!m) {
        throw std::runtime_error("Second message should be available");
    }
    pr.readMessage(m);
    if (m) {
        throw std::runtime_error("End of stream did not return empty message pointer");
    }
}

int main() {
    test_simple_message();
    test_empty_message();

    return 0;
}
