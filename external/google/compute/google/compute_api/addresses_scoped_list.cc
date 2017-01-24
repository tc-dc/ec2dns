// Copyright 2010 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

// This code was generated by google-apis-code-generator 1.5.1
//   Build date: 2017-01-11 18:31:16 UTC
//   on: 2017-01-18, 05:15:31 UTC
//   C++ generator version: 0.1.4

// ----------------------------------------------------------------------------
// NOTE: This file is generated from Google APIs Discovery Service.
// Service:
//   Compute Engine API (compute/v1)
// Description:
//   Creates and runs virtual machines on Google Cloud Platform.
// Classes:
//   AddressesScopedList
// Documentation:
//   https://developers.google.com/compute/docs/reference/latest/

#include "google/compute_api/addresses_scoped_list.h"
#include <string>
#include "googleapis/client/data/jsoncpp_data.h"
#include "googleapis/strings/stringpiece.h"

#include "google/compute_api/address.h"


#include <string>
#include "googleapis/strings/strcat.h"

namespace google_compute_api {
using namespace googleapis;




// Object factory method (static).
AddressesScopedList::AddressesScopedListWarning::AddressesScopedListWarningData* AddressesScopedList::AddressesScopedListWarning::AddressesScopedListWarningData::New() {
  return new client::JsonCppCapsule<AddressesScopedListWarningData>;
}

// Standard immutable constructor.
AddressesScopedList::AddressesScopedListWarning::AddressesScopedListWarningData::AddressesScopedListWarningData(const Json::Value& storage)
  : client::JsonCppData(storage) {
}

// Standard mutable constructor.
AddressesScopedList::AddressesScopedListWarning::AddressesScopedListWarningData::AddressesScopedListWarningData(Json::Value* storage)
  : client::JsonCppData(storage) {
}

// Standard destructor.
AddressesScopedList::AddressesScopedListWarning::AddressesScopedListWarningData::~AddressesScopedListWarningData() {
}
// Object factory method (static).
AddressesScopedList::AddressesScopedListWarning* AddressesScopedList::AddressesScopedListWarning::New() {
  return new client::JsonCppCapsule<AddressesScopedListWarning>;
}

// Standard immutable constructor.
AddressesScopedList::AddressesScopedListWarning::AddressesScopedListWarning(const Json::Value& storage)
  : client::JsonCppData(storage) {
}

// Standard mutable constructor.
AddressesScopedList::AddressesScopedListWarning::AddressesScopedListWarning(Json::Value* storage)
  : client::JsonCppData(storage) {
}

// Standard destructor.
AddressesScopedList::AddressesScopedListWarning::~AddressesScopedListWarning() {
}
// Object factory method (static).
AddressesScopedList* AddressesScopedList::New() {
  return new client::JsonCppCapsule<AddressesScopedList>;
}

// Standard immutable constructor.
AddressesScopedList::AddressesScopedList(const Json::Value& storage)
  : client::JsonCppData(storage) {
}

// Standard mutable constructor.
AddressesScopedList::AddressesScopedList(Json::Value* storage)
  : client::JsonCppData(storage) {
}

// Standard destructor.
AddressesScopedList::~AddressesScopedList() {
}
}  // namespace google_compute_api