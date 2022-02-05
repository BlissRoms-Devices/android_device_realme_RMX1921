/*
 * Copyright (C) 2021 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#define LOG_TAG "android.hardware.biometrics.fingerprint@2.3-service.xt"
#define LOG_VERBOSE "android.hardware.biometrics.fingerprint@2.3-service.xt"
#define FP_PRESS_NOTIFY "/sys/kernel/oppo_display/notify_fppress"
#define DIMLAYER_PATH "/sys/kernel/oppo_display/dimlayer_hbm"
#define PS_MASK "/proc/touchpanel/prox_mask"
#define PS_NEAR "/proc/touchpanel/prox_near"
#define AOD_PRESS "/proc/touchpanel/fod_aod_pressed"
#define DOZING "/proc/touchpanel/DOZE_STATUS"
#define DOZING_FP_HELPER "/proc/touchpanel/fod_aod_listener"
#define ON 1
#define OFF 0

#include "BiometricsFingerprint.h"

#include <inttypes.h>
#include <unistd.h>
#include <utils/Log.h>
#include <fstream>
#include <thread>

namespace android {
namespace hardware {
namespace biometrics {
namespace fingerprint {
namespace V2_3 {
namespace implementation {

BiometricsFingerprint::BiometricsFingerprint() : isEnrolling(false) {
    for(int i=0; i<10; i++) {
        mOppoBiometricsFingerprint = vendor::oppo::hardware::biometrics::fingerprint::V2_1::IBiometricsFingerprint::getService();
        if(mOppoBiometricsFingerprint != nullptr) break;
        sleep(10);
    }
    if(mOppoBiometricsFingerprint == nullptr) exit(0);
}

template <typename T>
static inline void set(const std::string& path, const T& value) {
    std::ofstream file(path);
    file << value;
}

template <typename T>
static inline T get(const std::string& path, const T& def) {
    std::ifstream file(path);
    T result;

    file >> result;
    return file.fail() ? def : result;
}

static bool receivedCancel;
static bool receivedEnumerate;
static uint64_t myDeviceId;
static std::vector<uint32_t> knownFingers;
class OppoClientCallback : public vendor::oppo::hardware::biometrics::fingerprint::V2_1::IBiometricsFingerprintClientCallback {
public:
    sp<android::hardware::biometrics::fingerprint::V2_1::IBiometricsFingerprintClientCallback> mClientCallback;

    OppoClientCallback(sp<android::hardware::biometrics::fingerprint::V2_1::IBiometricsFingerprintClientCallback> clientCallback) : mClientCallback(clientCallback) {}
    Return<void> onEnrollResult(uint64_t deviceId, uint32_t fingerId,
        uint32_t groupId, uint32_t remaining) {
        ALOGE("onEnrollResult %lu %u %u %u", deviceId, fingerId, groupId, remaining);
        if(mClientCallback != nullptr)
            mClientCallback->onEnrollResult(deviceId, fingerId, groupId, remaining);
        return Void();
    }

    Return<void> onAcquired(uint64_t deviceId, vendor::oppo::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo acquiredInfo,
        int32_t vendorCode) {
        ALOGE("onAcquired %lu %d", deviceId, vendorCode);
        if(mClientCallback != nullptr)
            mClientCallback->onAcquired(deviceId, OppoToAOSPFingerprintAcquiredInfo(acquiredInfo), vendorCode);
        return Void();
    }

    Return<void> onAuthenticated(uint64_t deviceId, uint32_t fingerId, uint32_t groupId,
        const hidl_vec<uint8_t>& token) {
        ALOGE("onAuthenticated %lu %u %u", deviceId, fingerId, groupId);
        if(mClientCallback != nullptr){
            if (fingerId!=0)//0 means finger not recognized
                set(DIMLAYER_PATH, OFF);
            mClientCallback->onAuthenticated(deviceId, fingerId, groupId, token);
        }
        return Void();
    }

    Return<void> onError(uint64_t deviceId, vendor::oppo::hardware::biometrics::fingerprint::V2_1::FingerprintError error, int32_t vendorCode) {
        ALOGE("onError %lu %d", deviceId, vendorCode);
        if(error == vendor::oppo::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_CANCELED) {
            receivedCancel = true;
        }
        if(mClientCallback != nullptr)
            mClientCallback->onError(deviceId, OppoToAOSPFingerprintError(error), vendorCode);
        return Void();
    }

    Return<void> onRemoved(uint64_t deviceId, uint32_t fingerId, uint32_t groupId,
        uint32_t remaining) {
        ALOGE("onRemoved %lu %u", deviceId, fingerId);
        if(mClientCallback != nullptr)
            mClientCallback->onRemoved(deviceId, fingerId, groupId, remaining);
        return Void();
    }

    Return<void> onEnumerate(uint64_t deviceId, uint32_t fingerId, uint32_t groupId,
        uint32_t remaining) {
        receivedEnumerate = true;
        ALOGE("onEnumerate %lu %u %u %u", deviceId, fingerId, groupId, remaining);
        if(mClientCallback != nullptr)
            mClientCallback->onEnumerate(deviceId, fingerId, groupId, remaining);
        return Void();
    }

    Return<void> onTouchUp(uint64_t deviceId) {
        set(AOD_PRESS, OFF);
        set(FP_PRESS_NOTIFY, OFF);
        set(PS_NEAR, OFF);
        return Void();
    }

    Return<void> onTouchDown(uint64_t deviceId) {
        if (get(DOZING, OFF)){
            set(AOD_PRESS, ON);
            set(PS_MASK, ON);
        }
        set(FP_PRESS_NOTIFY, ON);
        return Void();
    }

    Return<void> onSyncTemplates(uint64_t deviceId, const hidl_vec<uint32_t>& fingerId, uint32_t remaining) {
        ALOGE("onSyncTemplates %lu %zu %u", deviceId, fingerId.size(), remaining);
        myDeviceId = deviceId;

        for(auto fid : fingerId) {
            ALOGE("\t- %u", fid);
        }
        knownFingers = fingerId;

        return Void();
    }
    Return<void> onFingerprintCmd(int32_t deviceId, const hidl_vec<uint32_t>& groupId, uint32_t remaining) { return Void(); }
    Return<void> onImageInfoAcquired(uint32_t type, uint32_t quality, uint32_t match_score) { return Void(); }
    Return<void> onMonitorEventTriggered(uint32_t type, const hidl_string& data) { return Void(); }
    Return<void> onEngineeringInfoUpdated(uint32_t length, const hidl_vec<uint32_t>& keys, const hidl_vec<hidl_string>& values) { return Void(); }
    Return<void> onUIReady(int64_t deviceId) { return Void(); }

private:

    Return<android::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo> OppoToAOSPFingerprintAcquiredInfo(vendor::oppo::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo info) {
        switch(info) {
            case vendor::oppo::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo::ACQUIRED_GOOD: return android::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo::ACQUIRED_GOOD;
            case vendor::oppo::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo::ACQUIRED_PARTIAL: return android::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo::ACQUIRED_PARTIAL;
            case vendor::oppo::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo::ACQUIRED_INSUFFICIENT: return android::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo::ACQUIRED_INSUFFICIENT;
            case vendor::oppo::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo::ACQUIRED_IMAGER_DIRTY: return android::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo::ACQUIRED_IMAGER_DIRTY;
            case vendor::oppo::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo::ACQUIRED_TOO_SLOW: return android::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo::ACQUIRED_TOO_SLOW;
            case vendor::oppo::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo::ACQUIRED_TOO_FAST: return android::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo::ACQUIRED_TOO_FAST;
            case vendor::oppo::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo::ACQUIRED_VENDOR: return android::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo::ACQUIRED_VENDOR;
            default:
                return android::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo::ACQUIRED_GOOD;
        }
    }

    Return<android::hardware::biometrics::fingerprint::V2_1::FingerprintError> OppoToAOSPFingerprintError(vendor::oppo::hardware::biometrics::fingerprint::V2_1::FingerprintError error) {
        switch(error) {
            case vendor::oppo::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_NO_ERROR: return android::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_NO_ERROR;
            case vendor::oppo::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_HW_UNAVAILABLE: return android::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_HW_UNAVAILABLE;
            case vendor::oppo::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_UNABLE_TO_PROCESS: return android::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_UNABLE_TO_PROCESS;
            case vendor::oppo::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_TIMEOUT: return android::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_TIMEOUT;
            case vendor::oppo::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_NO_SPACE: return android::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_NO_SPACE;
            case vendor::oppo::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_CANCELED: return android::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_CANCELED;
            case vendor::oppo::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_UNABLE_TO_REMOVE: return android::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_UNABLE_TO_REMOVE;
            case vendor::oppo::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_LOCKOUT: return android::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_LOCKOUT;
            case vendor::oppo::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_VENDOR: return android::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_VENDOR;
            default:
                return android::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_NO_ERROR;
        }
    }
};

Return<uint64_t> BiometricsFingerprint::setNotify(
        const sp<IBiometricsFingerprintClientCallback>& clientCallback) {
    ALOGE("setNotify");
    mOppoClientCallback = new OppoClientCallback(clientCallback);
    return mOppoBiometricsFingerprint->setNotify(mOppoClientCallback);
}

Return<RequestStatus> BiometricsFingerprint::OppoToAOSPRequestStatus(vendor::oppo::hardware::biometrics::fingerprint::V2_1::RequestStatus req) {
    switch(req) {
        case vendor::oppo::hardware::biometrics::fingerprint::V2_1::RequestStatus::SYS_UNKNOWN: return RequestStatus::SYS_UNKNOWN;
        case vendor::oppo::hardware::biometrics::fingerprint::V2_1::RequestStatus::SYS_OK: return RequestStatus::SYS_OK;
        case vendor::oppo::hardware::biometrics::fingerprint::V2_1::RequestStatus::SYS_ENOENT: return RequestStatus::SYS_ENOENT;
        case vendor::oppo::hardware::biometrics::fingerprint::V2_1::RequestStatus::SYS_EINTR: return RequestStatus::SYS_EINTR;
        case vendor::oppo::hardware::biometrics::fingerprint::V2_1::RequestStatus::SYS_EIO: return RequestStatus::SYS_EIO;
        case vendor::oppo::hardware::biometrics::fingerprint::V2_1::RequestStatus::SYS_EAGAIN: return RequestStatus::SYS_EAGAIN;
        case vendor::oppo::hardware::biometrics::fingerprint::V2_1::RequestStatus::SYS_ENOMEM: return RequestStatus::SYS_ENOMEM;
        case vendor::oppo::hardware::biometrics::fingerprint::V2_1::RequestStatus::SYS_EACCES: return RequestStatus::SYS_EACCES;
        case vendor::oppo::hardware::biometrics::fingerprint::V2_1::RequestStatus::SYS_EFAULT: return RequestStatus::SYS_EFAULT;
        case vendor::oppo::hardware::biometrics::fingerprint::V2_1::RequestStatus::SYS_EBUSY: return RequestStatus::SYS_EBUSY;
        case vendor::oppo::hardware::biometrics::fingerprint::V2_1::RequestStatus::SYS_EINVAL: return RequestStatus::SYS_EINVAL;
        case vendor::oppo::hardware::biometrics::fingerprint::V2_1::RequestStatus::SYS_ENOSPC: return RequestStatus::SYS_ENOSPC;
        case vendor::oppo::hardware::biometrics::fingerprint::V2_1::RequestStatus::SYS_ETIMEDOUT: return RequestStatus::SYS_ETIMEDOUT;
        default:
            return RequestStatus::SYS_UNKNOWN;
    }
}

Return<uint64_t> BiometricsFingerprint::preEnroll()  {
    ALOGE("preEnroll");
    setFingerprintScreenState(true);
    return mOppoBiometricsFingerprint->preEnroll();
}

Return<RequestStatus> BiometricsFingerprint::enroll(const hidl_array<uint8_t, 69>& hat,
    uint32_t gid, uint32_t timeoutSec)  {
    ALOGE("enroll");
    isEnrolling = true;
    return OppoToAOSPRequestStatus(mOppoBiometricsFingerprint->enroll(hat, gid, timeoutSec));
}

Return<RequestStatus> BiometricsFingerprint::postEnroll()  {
    ALOGE("postEnroll");
    isEnrolling = false;
    setFingerprintScreenState(isEnrolling);
    return OppoToAOSPRequestStatus(mOppoBiometricsFingerprint->postEnroll());
}

Return<uint64_t> BiometricsFingerprint::getAuthenticatorId()  {
    ALOGE("getAuthId");
    return mOppoBiometricsFingerprint->getAuthenticatorId();
}

Return<RequestStatus> BiometricsFingerprint::cancel()  {
    if(OppoToAOSPRequestStatus(mOppoBiometricsFingerprint->cancel()) == RequestStatus::SYS_OK)
       mOppoClientCallback->onError(mOppoBiometricsFingerprint->setNotify(mOppoClientCallback),
           vendor::oppo::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_CANCELED,
           0);
    if (isEnrolling)
        isEnrolling = false;
    else 
        setFingerprintScreenState(false);
    return OppoToAOSPRequestStatus(mOppoBiometricsFingerprint->cancel());
}

Return<RequestStatus> BiometricsFingerprint::enumerate()  {
    receivedEnumerate = false;
    RequestStatus ret = OppoToAOSPRequestStatus(mOppoBiometricsFingerprint->enumerate());
    ALOGE("ENUMERATING");
    if(ret == RequestStatus::SYS_OK && !receivedEnumerate) {
        size_t nFingers = knownFingers.size();
        ALOGE("received fingers, sending our own %zu", nFingers);
        if(nFingers > 0) {
            for(auto finger: knownFingers) {
                mOppoClientCallback->mClientCallback->onEnumerate(
                        myDeviceId,
                        finger,
                        0,
                        --nFingers);

            }
        } else {
            mOppoClientCallback->mClientCallback->onEnumerate(
                    myDeviceId,
                    0,
                    0,
                    0);

        }
    }
    return ret;
}

Return<RequestStatus> BiometricsFingerprint::remove(uint32_t gid, uint32_t fid)  {
    ALOGE("remove");
    return OppoToAOSPRequestStatus(mOppoBiometricsFingerprint->remove(gid, fid));
}

Return<RequestStatus> BiometricsFingerprint::setActiveGroup(uint32_t gid,
    const hidl_string& storePath)  {
    ALOGE("setActiveGroup");
    return OppoToAOSPRequestStatus(mOppoBiometricsFingerprint->setActiveGroup(gid, storePath));
}

Return<RequestStatus> BiometricsFingerprint::authenticate(uint64_t operationId, uint32_t gid)  {
    ALOGE("auth");
    RequestStatus status = OppoToAOSPRequestStatus(mOppoBiometricsFingerprint->authenticate(operationId, gid));
    if (status == RequestStatus::SYS_OK) {
        setFingerprintScreenState(true);
    }
    return OppoToAOSPRequestStatus(mOppoBiometricsFingerprint->authenticate(operationId, gid));
}

Return<bool> BiometricsFingerprint::isUdfps(uint32_t) {
    return true;
}

Return<void> BiometricsFingerprint::onFingerDown(uint32_t, uint32_t, float, float) {
    return Void();
}

Return<void> BiometricsFingerprint::onFingerUp() {
    return Void();
}

Return<void> BiometricsFingerprint::onHideUdfpsOverlay() {
    return Void();
}

Return<void> BiometricsFingerprint::onShowUdfpsOverlay() {
    return Void();
}

void BiometricsFingerprint::setFingerprintScreenState(const bool on) {
    mOppoBiometricsFingerprint->setScreenState(
        on ? vendor::oppo::hardware::biometrics::fingerprint::V2_1::FingerprintScreenState::FINGERPRINT_SCREEN_ON :
            vendor::oppo::hardware::biometrics::fingerprint::V2_1::FingerprintScreenState::FINGERPRINT_SCREEN_OFF
        );
    if ((!isEnrolling)&&on){
            std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(380));//turn display off before enabling dimlayer
            if (!isEnrolling) {
                set(DIMLAYER_PATH, ON);
            }
        }).detach();
        set(DOZING_FP_HELPER, ON);
    } else
    set(DIMLAYER_PATH, on ? ON: OFF);
}

Return<void> BiometricsFingerprint::onHideUdfpsOverlay() {
    return Void();
}

Return<void> BiometricsFingerprint::onShowUdfpsOverlay() {
    return Void();
}

}  // namespace implementation
}  // namespace V2_3
}  // namespace fingerprint
}  // namespace biometrics
}  // namespace hardware
}  // namespace android
