/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkScaledImageGenerator.h"

#include "src/core/SkPixmapPriv.h"

auto SkScaledImageGenerator::MakeFromEncoded(sk_sp<SkData> data, int maxImageSize)
        -> std::unique_ptr<SkImageGenerator> {
    printf("[SkScaledImageGenerator] Making from encoded with max size of %d\n", maxImageSize);
    auto codec = SkCodec::MakeFromData(data);
    if (nullptr == codec) {
        return nullptr;
    }

    return std::unique_ptr<SkImageGenerator>(
            new SkScaledImageGenerator(std::move(codec), data, maxImageSize));
}

static SkImageInfo adjust_info(SkCodec* codec, int maxImageSize) {
    SkImageInfo info = codec->getInfo();
    if (kUnpremul_SkAlphaType == info.alphaType()) {
        info = info.makeAlphaType(kPremul_SkAlphaType);
    }
    if (SkEncodedOriginSwapsWidthHeight(codec->getOrigin())) {
        info = SkPixmapPriv::SwapWidthHeight(info);
    }

    printf("[SkScaledImageGenerator] Created for image with dimensions %dx%d\n",
           info.width(),
           info.height());

    // Alter the advertised dimensions to fit into maxImageSize
    if (info.width() > maxImageSize || info.height() > maxImageSize) {
        float width = info.width();
        float height = info.height();
        float maxSize = maxImageSize;

        printf("[SkScaledImageGenerator]  -> Scaling down.\n");
        const auto scale = std::min(maxSize / width, maxSize / height);
        const auto scaledDimensions =
                SkISize::Make(static_cast<int32_t>(std::lround(scale * width)),
                              static_cast<int32_t>(std::lround(scale * height)));
        info = info.makeDimensions(scaledDimensions);
        printf("[SkScaledImageGenerator]  -> Scaled to %dx%d\n",
               scaledDimensions.width(),
               scaledDimensions.height());
    }

    return info;
}

SkScaledImageGenerator::SkScaledImageGenerator(std::unique_ptr<SkCodec> codec,
                                               sk_sp<SkData> data,
                                               int maxImageSize)
        : INHERITED(adjust_info(codec.get(), maxImageSize))
        , fCodec(std::move(codec))
        , fData(std::move(data))
        , fMaxImageSize(maxImageSize) {}

sk_sp<SkData> SkScaledImageGenerator::onRefEncodedData() { return fData; }

auto SkScaledImageGenerator::needsScaling() -> bool {
    return fCodec->dimensions() == getInfo().dimensions();
}

bool SkScaledImageGenerator::getPixels(const SkImageInfo& info,
                                       void* pixels,
                                       size_t rowBytes,
                                       const SkCodec::Options* options) {
    SkPixmap dst(info, pixels, rowBytes);
    printf("[ImageGenerator] Pixels requested in \n");

    std::function<bool(const SkPixmap&)> decode;

    // Image is returned "as is"
    if (info.dimensions() == fCodec->dimensions()) {
        decode = [this, options](const SkPixmap& pm) {
            SkCodec::Result result = fCodec->getPixels(pm, options);
            switch (result) {
                case SkCodec::kSuccess:
                case SkCodec::kIncompleteInput:
                case SkCodec::kErrorInInput:
                    return true;
                default:
                    return false;
            }
        };
    }

    auto

            printf("[ImageGenerator] getPixels\n");

    return SkPixmapPriv::Orient(dst, fCodec->getOrigin(), decode);
}

bool SkScaledImageGenerator::onGetPixels(const SkImageInfo& requestInfo,
                                         void* requestPixels,
                                         size_t requestRowBytes,
                                         const Options& options) {
    return this->getPixels(requestInfo, requestPixels, requestRowBytes, nullptr);
}

bool SkScaledImageGenerator::onQueryYUVAInfo(
        const SkYUVAPixmapInfo::SupportedDataTypes& supportedDataTypes,
        SkYUVAPixmapInfo* yuvaPixmapInfo) const {
    return fCodec->queryYUVAInfo(supportedDataTypes, yuvaPixmapInfo);
}

bool SkScaledImageGenerator::onGetYUVAPlanes(const SkYUVAPixmaps& yuvaPixmaps) {
    switch (fCodec->getYUVAPlanes(yuvaPixmaps)) {
        case SkCodec::kSuccess:
        case SkCodec::kIncompleteInput:
        case SkCodec::kErrorInInput:
            return true;
        default:
            return false;
    }
}
