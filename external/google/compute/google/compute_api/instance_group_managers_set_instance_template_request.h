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
// Generated from:
//   Version: v1
//   Revision: 133
// Generated by:
//    Tool: google-apis-code-generator 1.5.1
//     C++: 0.1.4
#ifndef  GOOGLE_COMPUTE_API_INSTANCE_GROUP_MANAGERS_SET_INSTANCE_TEMPLATE_REQUEST_H_
#define  GOOGLE_COMPUTE_API_INSTANCE_GROUP_MANAGERS_SET_INSTANCE_TEMPLATE_REQUEST_H_

#include <string>
#include "googleapis/base/macros.h"
#include "googleapis/client/data/jsoncpp_data.h"
#include "googleapis/strings/stringpiece.h"

namespace Json {
class Value;
}  // namespace Json

namespace google_compute_api {
using namespace googleapis;

/**
 * No description provided.
 *
 * @ingroup DataObject
 */
class InstanceGroupManagersSetInstanceTemplateRequest : public client::JsonCppData {
 public:
  /**
   * Creates a new default instance.
   *
   * @return Ownership is passed back to the caller.
   */
  static InstanceGroupManagersSetInstanceTemplateRequest* New();

  /**
   * Standard constructor for an immutable data object instance.
   *
   * @param[in] storage  The underlying data storage for this instance.
   */
  explicit InstanceGroupManagersSetInstanceTemplateRequest(const Json::Value& storage);

  /**
   * Standard constructor for a mutable data object instance.
   *
   * @param[in] storage  The underlying data storage for this instance.
   */
  explicit InstanceGroupManagersSetInstanceTemplateRequest(Json::Value* storage);

  /**
   * Standard destructor.
   */
  virtual ~InstanceGroupManagersSetInstanceTemplateRequest();

  /**
   * Returns a string denoting the type of this data object.
   *
   * @return <code>google_compute_api::InstanceGroupManagersSetInstanceTemplateR
   * equest</code>
   */
  const StringPiece GetTypeName() const {
    return StringPiece("google_compute_api::InstanceGroupManagersSetInstanceTemplateRequest");
  }

  /**
   * Determine if the '<code>instanceTemplate</code>' attribute was set.
   *
   * @return true if the '<code>instanceTemplate</code>' attribute was set.
   */
  bool has_instance_template() const {
    return Storage().isMember("instanceTemplate");
  }

  /**
   * Clears the '<code>instanceTemplate</code>' attribute.
   */
  void clear_instance_template() {
    MutableStorage()->removeMember("instanceTemplate");
  }


  /**
   * Get the value of the '<code>instanceTemplate</code>' attribute.
   */
  const StringPiece get_instance_template() const {
    const Json::Value& v = Storage("instanceTemplate");
    if (v == Json::Value::null) return StringPiece("");
    return StringPiece(v.asCString());
  }

  /**
   * Change the '<code>instanceTemplate</code>' attribute.
   *
   * The URL of the instance template that is specified for this managed
   * instance group. The group uses this template to create all new instances in
   * the managed instance group.
   *
   * @param[in] value The new value.
   */
  void set_instance_template(const StringPiece& value) {
    *MutableStorage("instanceTemplate") = value.data();
  }

 private:
  void operator=(const InstanceGroupManagersSetInstanceTemplateRequest&);
};  // InstanceGroupManagersSetInstanceTemplateRequest
}  // namespace google_compute_api
#endif  // GOOGLE_COMPUTE_API_INSTANCE_GROUP_MANAGERS_SET_INSTANCE_TEMPLATE_REQUEST_H_