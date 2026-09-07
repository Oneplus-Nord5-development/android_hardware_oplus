/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Vibrator.h"

#include <cutils/properties.h>
#include <inttypes.h>
#include <log/log.h>
#include <unistd.h>

#include <thread>

#include "si_vibra_function.h"

#define LIVETAP_DEFAULT_F0 170

namespace aidl {
namespace android {
namespace hardware {
namespace vibrator {

Vibrator::Vibrator() {
    uint32_t deviceType = 0;

    int32_t ret = si_vibra_init(&deviceType);
    if (ret) {
        ALOGE("LiveTap init failed: %d\n", ret);
        return;
    }

    si_vibra_setting_f0(LIVETAP_DEFAULT_F0);
    si_vibra_set_drc_mode(1);
    si_vibra_update_parameter();
    si_vibra_looper_start();

    ALOGI("LiveTap init success: %u\n", deviceType);
}

ndk::ScopedAStatus Vibrator::getCapabilities(int32_t* _aidl_return) {
    *_aidl_return = IVibrator::CAP_ON_CALLBACK | IVibrator::CAP_PERFORM_CALLBACK |
                    IVibrator::CAP_AMPLITUDE_CONTROL | IVibrator::CAP_COMPOSE_EFFECTS;

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::off() {
    mAmplitudeSet = false;
    int32_t ret = si_vibra_off();
    if (ret) {
        ALOGE("LiveTap off failed: %d\n", ret);
        return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_SERVICE_SPECIFIC));
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::on(int32_t timeoutMs,
                                const std::shared_ptr<IVibratorCallback>& callback) {
    if (!mAmplitudeSet) {
        if (timeoutMs <= 100) {
            si_vibra_setAmplitude(50);
        } else {
            si_vibra_setAmplitude(0xff);
        }
    }
    mAmplitudeSet = false;

    int32_t ret = si_vibra_looper_on(timeoutMs);
    if (ret < 0) {
        ALOGE("LiveTap on failed: %d\n", ret);
        return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_SERVICE_SPECIFIC));
    }

    int32_t duration = (ret > 0) ? ret : timeoutMs;
    if (callback != nullptr) {
        std::thread([=] {
            usleep(duration * 1000);
            callback->onComplete();
        }).detach();
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::perform(Effect effect, EffectStrength es,
                                     const std::shared_ptr<IVibratorCallback>& callback,
                                     int32_t* _aidl_return) {
    float strengthFactor;

    if ((effect < Effect::CLICK || effect > Effect::HEAVY_CLICK) &&
        effect != Effect::TEXTURE_TICK) {
        return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
    }

    switch (es) {
        case EffectStrength::LIGHT:
            strengthFactor = 0.65f;
            break;
        case EffectStrength::MEDIUM:
            strengthFactor = 0.80f;
            break;
        case EffectStrength::STRONG:
            strengthFactor = 1.00f;
            break;
        default:
            return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
    }

    uint32_t duration;
    uint8_t baseAmplitude;

    switch (effect) {
        case Effect::TEXTURE_TICK:
            duration = 4;
            baseAmplitude = 32;
            break;
        case Effect::TICK:
            duration = 5;
            baseAmplitude = 40;
            break;
        case Effect::POP:
            duration = 8;
            baseAmplitude = 60;
            break;
        case Effect::CLICK:
            duration = 7;
            baseAmplitude = 54;
            break;
        case Effect::THUD:
            duration = 10;
            baseAmplitude = 65;
            break;
        case Effect::HEAVY_CLICK:
            duration = 12;
            baseAmplitude = 85;
            break;
        case Effect::DOUBLE_CLICK:
            duration = 25;
            baseAmplitude = 54;
            break;
        default:
            duration = 7;
            baseAmplitude = 54;
            break;
    }

    uint8_t finalAmp = static_cast<uint8_t>(baseAmplitude * strengthFactor);
    if (finalAmp < 1) finalAmp = 1;

    mAmplitudeSet = false;
    si_vibra_setAmplitude(finalAmp);

    int32_t ret;
    if (effect == Effect::DOUBLE_CLICK) {
        si_vibra_looper_on(5);
        usleep(15 * 1000);
        si_vibra_setAmplitude(finalAmp);
        si_vibra_looper_on(5);
        ret = duration;
    } else {
        ret = si_vibra_looper_on(duration);
        if (ret < 0) {
            ALOGE("LiveTap looper on failed: %d\n", ret);
            return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_SERVICE_SPECIFIC));
        }
        ret = duration;
    }

    if (callback != nullptr) {
        std::thread([=] {
            usleep(ret * 1000);
            callback->onComplete();
        }).detach();
    }

    *_aidl_return = ret;

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::getSupportedEffects(std::vector<Effect>* _aidl_return) {
    *_aidl_return = {Effect::CLICK,        Effect::DOUBLE_CLICK, Effect::TICK,
                     Effect::THUD,         Effect::POP,          Effect::HEAVY_CLICK,
                     Effect::TEXTURE_TICK};

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::setAmplitude(float amplitude) {
    if (amplitude <= 0.0f || amplitude > 1.0f) {
        return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
    }

    uint8_t tmp = (uint8_t)(amplitude * 0xff);
    if (tmp < 1) tmp = 1;

    int32_t ret = si_vibra_setAmplitude(tmp);
    if (ret) {
        ALOGE("LiveTap set amplitude failed: %d\n", ret);
        return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_SERVICE_SPECIFIC));
    }

    mAmplitudeSet = true;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::setExternalControl(bool enabled __unused) {
    return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
}

ndk::ScopedAStatus Vibrator::getCompositionDelayMax(int32_t* maxDelayMs) {
    if (maxDelayMs == nullptr) {
        return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
    }
    *maxDelayMs = 100;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::getCompositionSizeMax(int32_t* maxSize) {
    if (maxSize == nullptr) {
        return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
    }
    *maxSize = 256;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::getSupportedPrimitives(
        std::vector<CompositePrimitive>* supported) {
    *supported = {
        CompositePrimitive::NOOP,
        CompositePrimitive::CLICK,
        CompositePrimitive::THUD,
        CompositePrimitive::QUICK_FALL,
        CompositePrimitive::LIGHT_TICK,
        CompositePrimitive::LOW_TICK,
    };
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::getPrimitiveDuration(CompositePrimitive primitive,
                                                  int32_t* durationMs) {
    if (durationMs == nullptr) {
        return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
    }
    switch (primitive) {
        case CompositePrimitive::NOOP:
            *durationMs = 0;
            break;
        case CompositePrimitive::CLICK:
            *durationMs = 7;
            break;
        case CompositePrimitive::THUD:
            *durationMs = 10;
            break;
        case CompositePrimitive::LIGHT_TICK:
        case CompositePrimitive::LOW_TICK:
        case CompositePrimitive::QUICK_FALL:
            *durationMs = 4;
            break;
        default:
            return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::compose(const std::vector<CompositeEffect>& composite,
                                     const std::shared_ptr<IVibratorCallback>& callback) {
    if (composite.empty() || composite.size() > 256) {
        return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
    }

    int32_t totalDuration = 0;
    for (const auto& effect : composite) {
        if (effect.delayMs > 100 || effect.scale < 0.0f || effect.scale > 1.0f) {
            return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
        }

        int32_t primDuration = 0;
        switch (effect.primitive) {
            case CompositePrimitive::NOOP:
                primDuration = 0;
                break;
            case CompositePrimitive::CLICK:
                primDuration = 7;
                break;
            case CompositePrimitive::THUD:
                primDuration = 10;
                break;
            case CompositePrimitive::LIGHT_TICK:
            case CompositePrimitive::LOW_TICK:
            case CompositePrimitive::QUICK_FALL:
                primDuration = 4;
                break;
            default:
                return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
        }
        totalDuration += effect.delayMs + primDuration;
    }

    for (const auto& effect : composite) {
        if (effect.delayMs > 0) {
            usleep(effect.delayMs * 1000);
        }

        if (effect.primitive == CompositePrimitive::NOOP) {
            continue;
        }

        uint8_t amp = 0;
        uint32_t duration = 0;
        switch (effect.primitive) {
            case CompositePrimitive::CLICK:
                duration = 7;
                amp = static_cast<uint8_t>(54 * effect.scale);
                break;
            case CompositePrimitive::THUD:
                duration = 10;
                amp = static_cast<uint8_t>(65 * effect.scale);
                break;
            case CompositePrimitive::LIGHT_TICK:
            case CompositePrimitive::LOW_TICK:
            case CompositePrimitive::QUICK_FALL:
                duration = 4;
                amp = static_cast<uint8_t>(32 * effect.scale);
                break;
            default:
                break;
        }
        if (amp < 1) amp = 1;

        mAmplitudeSet = false;
        si_vibra_setAmplitude(amp);
        si_vibra_looper_on(duration);
    }

    if (callback != nullptr) {
        std::thread([=] {
            usleep(totalDuration * 1000);
            callback->onComplete();
        }).detach();
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::getSupportedAlwaysOnEffects(
        std::vector<Effect>* _aidl_return __unused) {
    return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
}

ndk::ScopedAStatus Vibrator::alwaysOnEnable(int32_t id __unused, Effect effect __unused,
                                            EffectStrength strength __unused) {
    return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
}

ndk::ScopedAStatus Vibrator::alwaysOnDisable(int32_t id __unused) {
    return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
}

ndk::ScopedAStatus Vibrator::getResonantFrequency(float* resonantFreqHz) {
    if (resonantFreqHz == nullptr) {
        return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
    }
    *resonantFreqHz = static_cast<float>(LIVETAP_DEFAULT_F0);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::getQFactor(float* qFactor) {
    if (qFactor == nullptr) {
        return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
    }
    *qFactor = 10.0f;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::getFrequencyResolution(float* freqResolutionHz __unused) {
    return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
}

ndk::ScopedAStatus Vibrator::getFrequencyMinimum(float* freqMinimumHz __unused) {
    return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
}

ndk::ScopedAStatus Vibrator::getBandwidthAmplitudeMap(std::vector<float>* _aidl_return __unused) {
    return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
}

ndk::ScopedAStatus Vibrator::getPwlePrimitiveDurationMax(int32_t* durationMs __unused) {
    return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
}

ndk::ScopedAStatus Vibrator::getPwleCompositionSizeMax(int32_t* maxSize __unused) {
    return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
}

ndk::ScopedAStatus Vibrator::getSupportedBraking(std::vector<Braking>* supported __unused) {
    return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
}

ndk::ScopedAStatus Vibrator::composePwle(const std::vector<PrimitivePwle>& composite __unused,
                                         const std::shared_ptr<IVibratorCallback>& callback
                                                 __unused) {
    return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_UNSUPPORTED_OPERATION));
}

}  // namespace vibrator
}  // namespace hardware
}  // namespace android
}  // namespace aidl
