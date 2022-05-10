/*
 * Copyright (C) 2021 LineageOS Project
 *
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fstream>
#include <stdlib.h>
#include <unistd.h>
#include <vector>
#include <cstdlib>
#include <string.h>

#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>
#include <sys/sysinfo.h>

#include <android-base/properties.h>
#include "property_service.h"
#include "vendor_init.h"

using android::base::GetProperty;
using ::android::base::SetProperty;
using std::string;

std::vector<string> ro_props_default_source_order = {
    "",
    "bootimage.",
    "odm.",
    "product.",
    "system.",
    "system_ext.",
    "vendor."
};

void property_override(char const prop[], char const value[], bool add = true) {
    prop_info *pi;

    pi = (prop_info *)__system_property_find(prop);

    if (pi)
	__system_property_update(pi, value, strlen(value));
    else if (add)
        __system_property_add(prop, strlen(prop), value, strlen(value));
}

void load_dalvikvm_properties() {
  struct sysinfo sys;
  sysinfo(&sys);
  if (sys.totalram > 6144ull * 1024 * 1024) {
    // from - phone-xhdpi-8192-dalvik-heap.mk
    property_override("dalvik.vm.heapstartsize", "24m");
    property_override("dalvik.vm.heaptargetutilization", "0.46");
    property_override("dalvik.vm.heapmaxfree", "48m");
    }
  else if (sys.totalram > 4096ull * 1024 * 1024) {
    // from - phone-xhdpi-6144-dalvik-heap.mk
    property_override("dalvik.vm.heapstartsize", "16m");
    property_override("dalvik.vm.heaptargetutilization", "0.5");
    property_override("dalvik.vm.heapmaxfree", "32m");
    }
  else {
    // from - phone-xhdpi-4096-dalvik-heap.mk
    property_override("dalvik.vm.heapstartsize", "8m");
    property_override("dalvik.vm.heaptargetutilization", "0.7");
    property_override("dalvik.vm.heapmaxfree", "16m");
  }
  property_override("dalvik.vm.heapgrowthlimit", "256m");
  property_override("dalvik.vm.heapsize", "512m");
  property_override("dalvik.vm.heapminfree", "8m");
}

void property_override_dual(char const system_prop[], char const vendor_prop[],
    char const value[])
{
    property_override(system_prop, value);
    property_override(vendor_prop, value);
}

void vendor_load_properties() {
    property_override("ro.build.description","redfin-user 11 SP1A.211105.003 7757856 release-keys");
    property_override("ro.bootimage.build.fingerprint","google/redfin/redfin:12/SQ1A.211205.008/7888514:user/release-keys");
    property_override("ro.build.fingerprint","google/redfin/redfin:12/SQ1A.211205.008/7888514:user/release-keys");
    property_override("ro.odm.build.fingerprint","google/redfin/redfin:12/SQ1A.211205.008/7888514:user/release-keys");
    property_override("ro.product.build.fingerprint","google/redfin/redfin:12/SQ1A.211205.008/7888514:user/release-keys");
    property_override("ro.system.build.fingerprint","google/redfin/redfin:12/SQ1A.211205.008/7888514:user/release-keys");
    property_override("ro.system_ext.build.fingerprint","google/redfin/redfin:12/SQ1A.211205.008/7888514:user/release-keys");
    property_override("ro.vendor.build.fingerprint","google/redfin/redfin:12/SQ1A.211205.008/7888514:user/release-keys");
    property_override("ro.oem_unlock_supported", "0");
    property_override("ro.boot.verifiedbootstate", "green");
    property_override("ro.boot.flash.locked", "1");
    property_override("ro.boot.veritymode", "enforcing");
    property_override("ro.boot.vbmeta.device_state", "locked");

    string oppo_sku = GetProperty("ro.boot.product.hardware.sku", "");

    if (oppo_sku == "nfc_ese"){
        property_override("ro.boot.product.hardware.sku", "RMX1921EU");
        property_override("ro.hardware.nfc_nci", "nqx.default");
        property_override("ro.nfc.port", "I2C");
    } else if (oppo_sku == "secure_element_uicc") {
        property_override("ro.boot.product.hardware.sku", "RMX1921");
    }

  // dalvikvm props
  load_dalvikvm_properties();

}
