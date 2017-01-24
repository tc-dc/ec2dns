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
#ifndef  GOOGLE_COMPUTE_API_DISK_MOVE_REQUEST_H_
#define  GOOGLE_COMPUTE_API_DISK_MOVE_REQUEST_H_

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
class DiskMoveRequest : public client::JsonCppData {
 public:
  /**
   * Creates a new default instance.
   *
   * @return Ownership is passed back to the caller.
   */
  static DiskMoveRequest* New();

  /**
   * Standard constructor for an immutable data object instance.
   *
   * @param[in] storage  The underlying data storage for this instance.
   */
  explicit DiskMoveRequest(const Json::Value& storage);

  /**
   * Standard constructor for a mutable data object instance.
   *
   * @param[in] storage  The underlying data storage for this instance.
   */
  explicit DiskMoveRequest(Json::Value* storage);

  /**
   * Standard destructor.
   */
  virtual ~DiskMoveRequest();

  /**
   * Returns a string denoting the type of this data object.
   *
   * @return <code>google_compute_api::DiskMoveRequest</code>
   */
  const StringPiece GetTypeName() const {
    return StringPiece("google_compute_api::DiskMoveRequest");
  }

  /**
   * Determine if the '<code>destinationZone</code>' attribute was set.
   *
   * @return true if the '<code>destinationZone</code>' attribute was set.
   */
  bool has_destination_zone() const {
    return Storage().isMember("destinationZone");
  }

  /**
   * Clears the '<code>destinationZone</code>' attribute.
   */
  void clear_destination_zone() {
    MutableStorage()->removeMember("destinationZone");
  }


  /**
   * Get the value of the '<code>destinationZone</code>' attribute.
   */
  const StringPiece get_destination_zone() const {
    const Json::Value& v = Storage("destinationZone");
    if (v == Json::Value::null) return StringPiece("");
    return StringPiece(v.asCString());
  }

  /**
   * Change the '<code>destinationZone</code>' attribute.
   *
   * The URL of the destination zone to move the disk. This can be a full or
   * partial URL. For example, the following are all valid URLs to a zone:
   * - https://www.googleapis.com/compute/v1/projects/project/zones/zone
   * - projects/project/zones/zone
   * - zones/zone.
   *
   * @param[in] value The new value.
   */
  void set_destination_zone(const StringPiece& value) {
    *MutableStorage("destinationZone") = value.data();
  }

  /**
   * Determine if the '<code>targetDisk</code>' attribute was set.
   *
   * @return true if the '<code>targetDisk</code>' attribute was set.
   */
  bool has_target_disk() const {
    return Storage().isMember("targetDisk");
  }

  /**
   * Clears the '<code>targetDisk</code>' attribute.
   */
  void clear_target_disk() {
    MutableStorage()->removeMember("targetDisk");
  }


  /**
   * Get the value of the '<code>targetDisk</code>' attribute.
   */
  const StringPiece get_target_disk() const {
    const Json::Value& v = Storage("targetDisk");
    if (v == Json::Value::null) return StringPiece("");
    return StringPiece(v.asCString());
  }

  /**
   * Change the '<code>targetDisk</code>' attribute.
   *
   * The URL of the target disk to move. This can be a full or partial URL. For
   * example, the following are all valid URLs to a disk:
   * - https://www.googleapis.com/compute/v1/projects/project/zones/zone/disks/d
   * isk
   * - projects/project/zones/zone/disks/disk
   * - zones/zone/disks/disk.
   *
   * @param[in] value The new value.
   */
  void set_target_disk(const StringPiece& value) {
    *MutableStorage("targetDisk") = value.data();
  }

 private:
  void operator=(const DiskMoveRequest&);
};  // DiskMoveRequest
}  // namespace google_compute_api
#endif  // GOOGLE_COMPUTE_API_DISK_MOVE_REQUEST_H_